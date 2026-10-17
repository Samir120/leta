#include "adapters/http/index_routes.hpp"

#include <string>

#include <chrono>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "adapters/storage/in_memory_index_repository.hpp"
#include "application/http_server.hpp"
#include "application/index_service.hpp"
#include "core/error.hpp"
#include "core/index_metadata.hpp"
#include "core/index_uid.hpp"
#include "core/timestamp.hpp"

namespace {

using leta::application::HttpResponse;
using leta::application::IndexService;
using leta::core::ErrorCode;
using leta::core::IndexMetadata;
using leta::core::IndexUid;
using leta::core::Timestamp;
using leta::http::create_index_response;
using leta::http::index_json;
using leta::http::parse_create_index_request;
using leta::storage::InMemoryIndexRepository;

constexpr std::chrono::milliseconds FixedTime{1'760'000'000'000};  // 2025-10-09T08:53:20.000Z

Timestamp fixed_clock() {
    return Timestamp{FixedTime};
}

ErrorCode error_code_for(const std::string& body) {
    const auto command = parse_create_index_request(body);
    REQUIRE_FALSE(command);
    return command.error().code;
}

}  // namespace

TEST_CASE("a body with only a uid asks for an index with no primary key yet", "[FR-70]") {
    const auto command = parse_create_index_request(R"({"uid":"products"})");
    REQUIRE(command);
    CHECK(command->uid.value() == "products");
    CHECK_FALSE(command->primary_key.has_value());
}

TEST_CASE("primaryKey may be a string or null, in any field order", "[FR-70]") {
    const auto named = parse_create_index_request(R"({"primaryKey":"id","uid":"products"})");
    REQUIRE(named);
    CHECK(named->primary_key == "id");
    const auto unnamed = parse_create_index_request(R"({"uid":"products","primaryKey":null})");
    REQUIRE(unnamed);
    CHECK_FALSE(unnamed->primary_key.has_value());
}

TEST_CASE("an unknown body field is refused, and the message names it", "[FR-72]") {
    const auto command =
        parse_create_index_request(R"({"uid":"products","filterableAttributes":[]})");
    REQUIRE_FALSE(command);
    CHECK(command.error().code == ErrorCode::UnknownField);
    CHECK(command.error().message == "Unknown field `filterableAttributes`.");
}

TEST_CASE("a uid outside FR-01's alphabet is refused as invalid_index_uid", "[FR-01]") {
    CHECK(error_code_for(R"({"uid":"my index"})") == ErrorCode::InvalidIndexUid);
    CHECK(error_code_for(R"({"uid":""})") == ErrorCode::InvalidIndexUid);
}

TEST_CASE("a missing, mistyped or repeated field is a malformed request", "[FR-70]") {
    const std::string body = GENERATE(as<std::string>{},
                                      R"({})",
                                      R"({"primaryKey":"id"})",
                                      R"({"uid":42})",
                                      R"({"uid":"products","primaryKey":7})",
                                      R"({"uid":"a","uid":"b"})");
    CAPTURE(body);
    CHECK(error_code_for(body) == ErrorCode::MalformedRequest);
}

TEST_CASE("a body that is not exactly one JSON object is a malformed request", "[FR-70]") {
    const std::string body = GENERATE(as<std::string>{},
                                      "",
                                      "not json",
                                      "[]",
                                      R"("products")",
                                      R"({"uid":"products")",
                                      R"({"uid":"products"} {})",
                                      "{\"uid\":\"\xff\"}");
    CAPTURE(body);
    CHECK(error_code_for(body) == ErrorCode::MalformedRequest);
}

TEST_CASE("an index is written as the Index object of the API spec", "[FR-70]") {
    const IndexMetadata index{.uid = IndexUid::parse("products").value(),
                              .primary_key = "id",
                              .created_at = Timestamp{FixedTime},
                              .updated_at = Timestamp{FixedTime + std::chrono::milliseconds{5}}};
    CHECK(
        index_json(index)
        == R"({"uid":"products","primaryKey":"id","createdAt":"2025-10-09T08:53:20.000Z","updatedAt":"2025-10-09T08:53:20.005Z"})");
}

TEST_CASE("creating an index answers 201, creating it again answers 409", "[FR-02]") {
    InMemoryIndexRepository repository;
    IndexService service{repository, fixed_clock};
    const HttpResponse created = create_index_response(service, R"({"uid":"products"})");
    CHECK(created.status == 201);
    const HttpResponse again = create_index_response(service, R"({"uid":"products"})");
    CHECK(again.status == 409);
    CHECK(
        again.body
        == R"({"code":"index_already_exists","message":"Index `products` already exists.","type":"invalid_request"})");
}
