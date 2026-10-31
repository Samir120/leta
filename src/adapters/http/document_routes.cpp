#include "adapters/http/document_routes.hpp"

#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <chrono>
#include <cstddef>

#include <simdjson.h>

#include "adapters/http/error_response.hpp"
#include "adapters/http/json_string.hpp"
#include "application/http_server.hpp"
#include "application/ingest_service.hpp"
#include "core/document.hpp"
#include "core/error.hpp"
#include "core/index_uid.hpp"
#include "core/result.hpp"

namespace leta::http {

namespace {

using application::AddDocumentsOutcome;
using core::ErrorCode;
using simdjson::ondemand::json_type;

constexpr std::size_t MaxBatchDocuments = 10'000;  // FR-14; maxItems in 02-api-spec.yaml
// Product documents nest a handful of levels. The bound keeps validation's recursion shallow on
// every worker thread and far inside the parser's own limit (simdjson::DEFAULT_MAX_DEPTH, 1024).
// 128 is also serde_json's default.
constexpr std::size_t MaxNestingDepth = 128;
constexpr std::string_view UidParameter = "uid";
constexpr const char* NotJson = "The request body is not valid JSON.";
constexpr int OkStatus = 200;

auto malformed(std::string message) {
    return core::failure(ErrorCode::MalformedRequest, std::move(message));
}

auto not_an_object(std::size_t index) {
    std::string message{"The batch's element at index "};
    message.append(std::to_string(index)).append(" is not a JSON object.");
    return malformed(std::move(message));
}

auto too_many_documents() {
    std::string message{"A batch holds at most "};
    message.append(std::to_string(MaxBatchDocuments)).append(" documents.");
    return malformed(std::move(message));
}

auto nested_too_deeply() {
    std::string message{"A document nests more than "};
    message.append(std::to_string(MaxNestingDepth)).append(" levels deep.");
    return malformed(std::move(message));
}

// raw_json() ends where the next token starts, so its slice can carry the whitespace between the
// object's `}` and the `,` or `]` after it. The object itself ends at its `}`.
std::string_view without_trailing_whitespace(std::string_view json) {
    const std::size_t last = json.find_last_not_of(" \t\n\r");  // RFC 8259's four
    return last == std::string_view::npos ? std::string_view{} : json.substr(0, last + 1);
}

// A scalar is checked by reading it. Any JSON number reads as a double; one beyond its range
// (1e400) does not, and is refused.
bool reads_as_scalar(simdjson::ondemand::value& value, json_type type) {
    switch (type) {
        case json_type::number: {
            double number = 0;
            return value.get_double().get(number) == simdjson::SUCCESS;
        }
        case json_type::string: {
            std::string_view text;
            return value.get_string().get(text) == simdjson::SUCCESS;
        }
        case json_type::boolean: {
            bool parsed_boolean = false;
            return value.get_bool().get(parsed_boolean) == simdjson::SUCCESS;
        }
        case json_type::null: {
            bool is_null = false;
            return value.is_null().get(is_null) == simdjson::SUCCESS && is_null;
        }
        case json_type::array:
        case json_type::object:
        case json_type::unknown:
            return false;
    }
    std::terminate();
}

// On demand checks an atom (true, false, null, a number) or a string's escapes only when the value
// is read, and skips what nobody reads: `{"a":tru}` would be stored, and later served, as sent
// (ADR-004). So every value is read once, here. `depth` counts from the document: its top-level
// field values are at depth 1. The three functions recurse through one another; validate_value
// checks MaxNestingDepth before anything else, which bounds the recursion.
// NOLINTBEGIN(misc-no-recursion)
core::Result<void> validate_value(simdjson::ondemand::value& value, std::size_t depth);

core::Result<void> validate_array(simdjson::ondemand::value& value, std::size_t depth) {
    simdjson::ondemand::array array;
    if (value.get_array().get(array) != simdjson::SUCCESS) {
        return malformed(NotJson);
    }
    for (auto element_result : array) {
        simdjson::ondemand::value element;
        if (std::move(element_result).get(element) != simdjson::SUCCESS) {
            return malformed(NotJson);
        }
        if (const auto valid = validate_value(element, depth + 1); !valid) {
            return core::failure(valid.error());
        }
    }
    return {};
}

core::Result<void> validate_object(simdjson::ondemand::value& value, std::size_t depth) {
    simdjson::ondemand::object object;
    if (value.get_object().get(object) != simdjson::SUCCESS) {
        return malformed(NotJson);
    }
    for (auto field_result : object) {
        simdjson::ondemand::field field;
        std::string_view key;  // read to check its escapes, not kept
        if (std::move(field_result).get(field) != simdjson::SUCCESS
            || field.unescaped_key().get(key) != simdjson::SUCCESS) {
            return malformed(NotJson);
        }
        if (const auto valid = validate_value(field.value(), depth + 1); !valid) {
            return core::failure(valid.error());
        }
    }
    return {};
}

core::Result<void> validate_value(simdjson::ondemand::value& value, std::size_t depth) {
    if (depth > MaxNestingDepth) {
        return nested_too_deeply();
    }
    json_type type{};
    if (value.type().get(type) != simdjson::SUCCESS) {
        return malformed(NotJson);
    }
    if (type == json_type::array) {
        return validate_array(value, depth);
    }
    if (type == json_type::object) {
        return validate_object(value, depth);
    }
    if (!reads_as_scalar(value, type)) {
        return malformed(NotJson);
    }
    return {};
}

// NOLINTEND(misc-no-recursion)

// The field pass over one document: every top-level string value becomes searchable text, and
// every other value is validated.
core::Result<std::vector<std::string>> read_searchable_text(simdjson::ondemand::object& object) {
    std::vector<std::string> searchable_text;
    for (auto field_result : object) {
        simdjson::ondemand::field field;
        std::string_view key;  // read to check its escapes, not kept
        if (std::move(field_result).get(field) != simdjson::SUCCESS
            || field.unescaped_key().get(key) != simdjson::SUCCESS) {
            return malformed(NotJson);
        }
        json_type type{};
        if (field.value().type().get(type) != simdjson::SUCCESS) {
            return malformed(NotJson);
        }
        if (type != json_type::string) {
            if (const auto valid = validate_value(field.value(), 1); !valid) {
                return core::failure(valid.error());
            }
            continue;
        }
        std::string_view text;
        if (field.value().get_string().get(text) != simdjson::SUCCESS) {
            return malformed(NotJson);
        }
        // The view points into the parser's string buffer, which dies with the parser (ADR-004).
        searchable_text.emplace_back(text);
    }
    return searchable_text;
}

// Two passes over one object, because on demand reads forward only: raw_json() takes the source
// while the object is untouched, then reset() rewinds it for the field pass. The rescan is the
// price ADR-004 names for storing raw bytes with a forward-only parser.
core::Result<core::Document> read_document(simdjson::ondemand::value& element, std::size_t index) {
    simdjson::ondemand::object object;
    if (element.get_object().get(object) != simdjson::SUCCESS) {
        return not_an_object(index);
    }
    std::string_view source;
    bool has_fields = false;  // unused: an empty object simply yields no fields below
    if (object.raw_json().get(source) != simdjson::SUCCESS
        || object.reset().get(has_fields) != simdjson::SUCCESS) {
        return malformed(NotJson);
    }
    auto searchable_text = read_searchable_text(object);
    if (!searchable_text) {
        return core::failure(searchable_text.error());
    }
    // Copied: the view points into this request's padded copy of the body.
    return core::Document{.source = std::string{without_trailing_whitespace(source)},
                          .searchable_text = std::move(*searchable_text)};
}

core::Result<std::vector<core::Document>> read_documents(simdjson::ondemand::array& array) {
    std::vector<core::Document> documents;
    for (auto element_result : array) {
        if (documents.size() == MaxBatchDocuments) {
            return too_many_documents();
        }
        simdjson::ondemand::value element;
        // get() exists only on an rvalue result in simdjson 5.0.4 (A18).
        if (std::move(element_result).get(element) != simdjson::SUCCESS) {
            return malformed(NotJson);
        }
        auto document = read_document(element, documents.size());
        if (!document) {
            return core::failure(document.error());
        }
        documents.push_back(std::move(*document));
    }
    if (documents.empty()) {
        return malformed("A batch holds at least one document.");
    }
    return documents;
}

}  // namespace

core::Result<std::vector<core::Document>> parse_document_batch(std::string_view body) {
    // Copied into a padded buffer like every other body (A18, amended in M1-T3): simdjson may read
    // up to SIMDJSON_PADDING bytes past the input's end, and the transport's buffer is not ours to
    // pad. The copy is one memcpy of at most 20 MB (NFR-12), ahead of a parse that reads every
    // byte.
    const simdjson::padded_string padded{body};
    simdjson::ondemand::parser parser;
    simdjson::ondemand::document document;
    if (parser.iterate(padded).get(document) != simdjson::SUCCESS) {
        return malformed(NotJson);
    }
    simdjson::ondemand::array array;
    if (document.get_array().get(array) != simdjson::SUCCESS) {
        return malformed("The request body must be a JSON array of documents.");
    }
    auto documents = read_documents(array);
    if (!documents) {
        return core::failure(documents.error());
    }
    // On demand reads only what it is asked for. This is what refuses `[{}] trailing`.
    if (!document.at_end()) {
        return malformed(NotJson);
    }
    return documents;
}

std::string ingest_result_json(std::size_t received,
                               const AddDocumentsOutcome& outcome,
                               std::chrono::milliseconds processing_time) {
    std::string body{R"({"received":)"};
    body.append(std::to_string(received))
        .append(R"(,"indexed":)")
        .append(std::to_string(outcome.indexed));
    // `replaced` joins with replace-by-key (FR-11, M3). `primaryKey` is optional and not nullable
    // in IngestResult, so it is left out while the index has none.
    if (outcome.primary_key) {
        body.append(R"(,"primaryKey":)");
        append_json_string(body, *outcome.primary_key);  // valid UTF-8: it came through the parser
    }
    body.append(R"(,"processingTimeMs":)")
        .append(std::to_string(processing_time.count()))
        .push_back('}');
    return body;
}

application::HttpResponse add_documents_response(application::IngestService& service,
                                                 std::string_view uid,
                                                 std::string_view body) {
    // processingTimeMs is a duration read at this edge; no instant is kept anywhere (A19, amended
    // in M1-T3).
    const auto started = std::chrono::steady_clock::now();
    auto index_uid = core::IndexUid::parse(uid);
    if (!index_uid) {
        return error_response(index_uid.error());
    }
    auto documents = parse_document_batch(body);
    if (!documents) {
        return error_response(documents.error());
    }
    const std::size_t received = documents->size();
    const auto outcome =
        service.add_documents({.uid = std::move(*index_uid), .documents = std::move(*documents)});
    if (!outcome) {
        return error_response(outcome.error());
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    return {.status = OkStatus, .body = ingest_result_json(received, *outcome, elapsed)};
}

void register_document_routes(application::HttpServer& server,
                              application::IngestService& service) {
    server.add_route(application::HttpMethod::Post,
                     "/indexes/{uid}/documents",
                     [&service](const application::HttpRequest& request) {
                         return add_documents_response(
                             service,
                             application::path_parameter(request, UidParameter),
                             request.body);
                     });
}

}  // namespace leta::http
