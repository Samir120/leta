#include "adapters/http/index_routes.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <simdjson.h>

#include "adapters/http/error_response.hpp"
#include "adapters/http/json_string.hpp"
#include "adapters/http/json_timestamp.hpp"
#include "application/http_server.hpp"
#include "application/index_service.hpp"
#include "core/error.hpp"
#include "core/index_metadata.hpp"
#include "core/index_uid.hpp"
#include "core/result.hpp"

namespace leta::http {

namespace {

using application::CreateIndexCommand;
using core::ErrorCode;

constexpr std::string_view UidField = "uid";
constexpr std::string_view PrimaryKeyField = "primaryKey";
constexpr const char* NotJson = "The request body is not valid JSON.";
constexpr int CreatedStatus = 201;

// The body as read so far. Fields arrive in any order, so whether `uid` is present can only be
// decided after the last one.
struct CreateIndexFields {
    std::optional<std::string> uid;
    bool has_primary_key{false};
    std::optional<std::string> primary_key;  // empty with has_primary_key: the client sent null
};

auto malformed(std::string message) {
    return core::failure(ErrorCode::MalformedRequest, std::move(message));
}

auto field_must_be(std::string_view field_name, std::string_view expectation) {
    std::string message{"Field `"};
    message.append(field_name).append("` must be ").append(expectation).append(".");
    return malformed(std::move(message));
}

// A repeated key is legal JSON with no agreed meaning; refusing it beats guessing which one won.
auto appears_more_than_once(std::string_view field_name) {
    std::string message{"Field `"};
    message.append(field_name).append("` appears more than once.");
    return malformed(std::move(message));
}

// Echoing the key is safe: iterate() has already validated the whole body as UTF-8.
auto unknown_field(std::string_view field_name) {
    std::string message{"Unknown field `"};
    message.append(field_name).append("`.");
    return core::failure(ErrorCode::UnknownField, std::move(message));
}

core::Result<void> read_uid(simdjson::ondemand::value& value, CreateIndexFields& fields) {
    if (fields.uid) {
        return appears_more_than_once(UidField);
    }
    std::string_view uid;
    if (value.get_string().get(uid) != simdjson::SUCCESS) {
        return field_must_be(UidField, "a string");
    }
    // The view points into the parser's string buffer, which dies with the parser (ADR-004).
    fields.uid = std::string{uid};
    return {};
}

core::Result<void> read_primary_key(simdjson::ondemand::value& value, CreateIndexFields& fields) {
    if (fields.has_primary_key) {
        return appears_more_than_once(PrimaryKeyField);
    }
    fields.has_primary_key = true;
    bool is_null = false;
    if (value.is_null().get(is_null) != simdjson::SUCCESS) {
        return field_must_be(PrimaryKeyField, "a string or null");
    }
    if (is_null) {
        return {};
    }
    std::string_view primary_key;
    if (value.get_string().get(primary_key) != simdjson::SUCCESS) {
        return field_must_be(PrimaryKeyField, "a string or null");
    }
    fields.primary_key = std::string{primary_key};  // copied out of the parser's buffer (ADR-004)
    return {};
}

core::Result<void> read_field(simdjson::ondemand::field& field, CreateIndexFields& fields) {
    std::string_view key;
    if (field.unescaped_key().get(key) != simdjson::SUCCESS) {
        return malformed(NotJson);
    }
    if (key == UidField) {
        return read_uid(field.value(), fields);
    }
    if (key == PrimaryKeyField) {
        return read_primary_key(field.value(), fields);
    }
    return unknown_field(key);
}

core::Result<CreateIndexFields> read_fields(simdjson::ondemand::object& object) {
    CreateIndexFields fields;
    for (auto field_result : object) {
        simdjson::ondemand::field field;
        // simdjson's get() exists only on an rvalue result (5.0.4); the loop variable is named, so
        // it is moved into the call. It is not touched again, so nothing reads a moved-from object.
        if (std::move(field_result).get(field) != simdjson::SUCCESS) {
            return malformed(NotJson);
        }
        if (auto read = read_field(field, fields); !read) {
            return core::failure(read.error());
        }
    }
    return fields;
}

core::Result<CreateIndexCommand> to_command(CreateIndexFields fields) {
    if (!fields.uid) {
        return malformed("Field `uid` is required.");
    }
    auto uid = core::IndexUid::parse(*fields.uid);
    if (!uid) {
        return core::failure(uid.error());
    }
    return CreateIndexCommand{.uid = std::move(*uid), .primary_key = std::move(fields.primary_key)};
}

}  // namespace

core::Result<CreateIndexCommand> parse_create_index_request(std::string_view body) {
    // simdjson may read up to SIMDJSON_PADDING bytes past the end of its input, so the body is
    // copied into a padded buffer. A create-index body is a few dozen bytes; ingest bodies (M1-T3)
    // are the ones worth parsing in place.
    const simdjson::padded_string padded{body};
    simdjson::ondemand::parser parser;
    simdjson::ondemand::document document;
    if (parser.iterate(padded).get(document) != simdjson::SUCCESS) {
        return malformed(NotJson);
    }
    simdjson::ondemand::object object;
    if (document.get_object().get(object) != simdjson::SUCCESS) {
        return malformed("The request body must be a JSON object.");
    }
    auto fields = read_fields(object);
    if (!fields) {
        return core::failure(fields.error());
    }
    // On demand reads only what it is asked for. This is what refuses `{"uid":"a"} trailing`.
    if (!document.at_end()) {
        return malformed(NotJson);
    }
    return to_command(std::move(*fields));
}

std::string index_json(const core::IndexMetadata& index) {
    std::string body{R"({"uid":)"};
    append_json_string(body, index.uid.value());
    body.append(R"(,"primaryKey":)");
    if (index.primary_key) {
        // Valid UTF-8, as append_json_string requires: it came through the parser.
        append_json_string(body, *index.primary_key);
    } else {
        body.append("null");
    }
    body.append(R"(,"createdAt":)");
    append_json_timestamp(body, index.created_at);
    body.append(R"(,"updatedAt":)");
    append_json_timestamp(body, index.updated_at);
    body.push_back('}');
    return body;
}

application::HttpResponse create_index_response(application::IndexService& service,
                                                std::string_view body) {
    auto command = parse_create_index_request(body);
    if (!command) {
        return error_response(command.error());
    }
    const auto index = service.create_index(std::move(*command));
    if (!index) {
        return error_response(index.error());
    }
    return {.status = CreatedStatus, .body = index_json(*index)};
}

void register_index_routes(application::HttpServer& server, application::IndexService& service) {
    server.add_route(application::HttpMethod::Post,
                     "/indexes",
                     [&service](const application::HttpRequest& request) {
                         return create_index_response(service, request.body);
                     });
}

}  // namespace leta::http
