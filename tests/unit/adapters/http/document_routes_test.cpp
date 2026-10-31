#include "adapters/http/document_routes.hpp"

#include <optional>
#include <string>
#include <vector>

#include <chrono>
#include <cstddef>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "adapters/storage/in_memory_index_repository.hpp"
#include "application/ingest_service.hpp"
#include "core/error.hpp"
#include "core/index_metadata.hpp"
#include "core/index_uid.hpp"
#include "core/timestamp.hpp"

namespace {

using leta::application::AddDocumentsOutcome;
using leta::application::IngestService;
using leta::core::ErrorCode;
using leta::core::IndexMetadata;
using leta::core::IndexUid;
using leta::core::Timestamp;
using leta::http::add_documents_response;
using leta::http::ingest_result_json;
using leta::http::parse_document_batch;
using leta::storage::InMemoryIndexRepository;

Timestamp fixed_clock() {
    return Timestamp{std::chrono::milliseconds{1'760'000'000'000}};
}

leta::core::Error error_for(const std::string& body) {
    const auto batch = parse_document_batch(body);
    REQUIRE_FALSE(batch);
    return batch.error();
}

// `count` empty objects: the smallest valid documents.
std::string batch_of_empty_documents(std::size_t count) {
    std::string body{"["};
    for (std::size_t written = 0; written < count; ++written) {
        body.append(written == 0 ? "{}" : ",{}");
    }
    body.push_back(']');
    return body;
}

// One document whose field holds `depth` nested arrays: the outermost at depth 1.
std::string nested_arrays(std::size_t depth) {
    std::string body{R"([{"deep":)"};
    body.append(depth, '[').append(depth, ']').append("}]");
    return body;
}

}  // namespace

TEST_CASE("each document keeps the exact bytes the client sent", "[ADR-004]") {
    const auto batch = parse_document_batch(
        R"([{"id":1,"name":"Corsair DDR5"},{"id":"b2","price":12.50,"specs":{"type":"DDR5"}}])");
    REQUIRE(batch);
    REQUIRE(batch->size() == 2);
    CHECK(batch->at(0).source == R"({"id":1,"name":"Corsair DDR5"})");
    CHECK(batch->at(1).source == R"({"id":"b2","price":12.50,"specs":{"type":"DDR5"}})");
}

TEST_CASE("whitespace between documents belongs to neither", "[ADR-004]") {
    const auto batch = parse_document_batch("[ {\"a\":\"x\"} ,\n {\"b\":\"y\"} \n]");
    REQUIRE(batch);
    REQUIRE(batch->size() == 2);
    CHECK(batch->at(0).source == R"({"a":"x"})");
    CHECK(batch->at(1).source == R"({"b":"y"})");
}

TEST_CASE("top-level string values become the searchable text, unescaped", "[M1]") {
    const auto batch = parse_document_batch(
        R"([{"id":"b2","name":"Sk\u00e4rm 27","price":1299,"tags":["ram"],"specs":{"type":"DDR5"}}])");
    REQUIRE(batch);
    REQUIRE(batch->size() == 1);
    CHECK(batch->at(0).searchable_text == std::vector<std::string>{"b2", "Sk\u00e4rm 27"});
    // The source keeps the escape as sent; only the searchable text is unescaped.
    CHECK(batch->at(0).source.find(R"(\u00e4)") != std::string::npos);
}

TEST_CASE("a batch of 10000 documents is accepted, 10001 refused", "[M1]") {
    // FR-14's limit. The [FR-14] tag lands with the rest of FR-14 in M3 (A12).
    CHECK(parse_document_batch(batch_of_empty_documents(10'000)));
    const auto refused = error_for(batch_of_empty_documents(10'001));
    CHECK(refused.code == ErrorCode::MalformedRequest);
    CHECK(refused.message == "A batch holds at most 10000 documents.");
}

TEST_CASE("documents nest up to 128 levels; one level deeper is refused", "[M1]") {
    CHECK(parse_document_batch(nested_arrays(128)));
    const auto refused = error_for(nested_arrays(129));
    CHECK(refused.code == ErrorCode::MalformedRequest);
    CHECK(refused.message == "A document nests more than 128 levels deep.");
}

TEST_CASE("a body that is not an array of valid objects is refused as malformed", "[M1]") {
    const std::string body = GENERATE(as<std::string>{},
                                      "",
                                      "not json",
                                      R"({"id":1})",
                                      R"([{"id":1}] [])",
                                      // Values on demand would skip unread, and so never check:
                                      R"([{"id":tru}])",
                                      R"([{"price":1.2.3}])",
                                      R"([{"price":1e400}])",
                                      R"([{"name":"bad \q escape"}])",
                                      R"([{"bad \q key":1}])",
                                      R"([{"nested":{"flag":nul}}])",
                                      R"([{"list":[1,2,fals]}])");
    CAPTURE(body);
    CHECK(error_for(body).code == ErrorCode::MalformedRequest);
}

TEST_CASE("refusals name what is wrong", "[FR-71]") {
    CHECK(error_for(R"({"id":1})").message
          == "The request body must be a JSON array of documents.");
    CHECK(error_for("[]").message == "A batch holds at least one document.");
    CHECK(error_for(R"([{"id":1},2])").message
          == "The batch's element at index 1 is not a JSON object.");
}

TEST_CASE("the IngestResult carries counts, time, and the primary key when there is one", "[M1]") {
    CHECK(ingest_result_json(3,
                             AddDocumentsOutcome{.indexed = 3, .primary_key = std::nullopt},
                             std::chrono::milliseconds{12})
          == R"({"received":3,"indexed":3,"processingTimeMs":12})");
    CHECK(ingest_result_json(1,
                             AddDocumentsOutcome{.indexed = 1, .primary_key = "sku"},
                             std::chrono::milliseconds{0})
          == R"({"received":1,"indexed":1,"primaryKey":"sku","processingTimeMs":0})");
}

TEST_CASE("a valid batch answers 200 with its IngestResult", "[M1]") {
    InMemoryIndexRepository repository;
    IngestService service{repository, fixed_clock};
    const auto response =
        add_documents_response(service,
                               "products",
                               R"([{"name":"Corsair DDR5"},{"name":"MSI B650"}])");
    CHECK(response.status == 200);
    CHECK(response.body.starts_with(R"({"received":2,"indexed":2,"processingTimeMs":)"));
}

TEST_CASE("a uid outside FR-01's alphabet is refused before the body is read", "[FR-01]") {
    InMemoryIndexRepository repository;
    IngestService service{repository, fixed_clock};
    const auto response = add_documents_response(service, "bad.uid", "not even json");
    CHECK(response.status == 400);
    CHECK(response.body.starts_with(R"({"code":"invalid_index_uid")"));
}

TEST_CASE("a malformed batch answers 400 and creates nothing", "[FR-71]") {
    InMemoryIndexRepository repository;
    IngestService service{repository, fixed_clock};
    const auto response = add_documents_response(service, "products", "[]");
    CHECK(response.status == 400);
    CHECK(
        response.body
        == R"({"code":"malformed_request","message":"A batch holds at least one document.","type":"invalid_request"})");
    // The uid is still free: the refused batch created no index.
    CHECK(repository.add(IndexMetadata{.uid = IndexUid::parse("products").value(),
                                       .primary_key = std::nullopt,
                                       .created_at = Timestamp{},
                                       .updated_at = Timestamp{}}));
}
