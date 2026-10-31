#include "adapters/http/document_routes.hpp"

#include <chrono>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include "adapters/http/index_routes.hpp"
#include "adapters/storage/in_memory_index_repository.hpp"
#include "application/http_server.hpp"
#include "application/index_service.hpp"
#include "application/ingest_service.hpp"
#include "core/timestamp.hpp"
#include "integration/running_server.hpp"

namespace {

using leta::application::HttpServer;
using leta::application::IndexService;
using leta::application::IngestService;
using leta::storage::InMemoryIndexRepository;
using leta::testing::RunningServer;

constexpr const char* JsonContentType = "application/json";
constexpr const char* DocumentsPath = "/indexes/products/documents";
constexpr const char* ThreeDocuments =
    R"([{"id":1,"name":"Corsair DDR5 6000"},{"id":2,"name":"MSI B650"},{"id":3,"name":"Kingston Fury DDR5"}])";

leta::core::Timestamp fixed_clock() {
    return leta::core::Timestamp{std::chrono::milliseconds{1'760'000'000'000}};
}

// Index and document routes over one repository, so what one route creates the other sees. Built
// before the server and destroyed after it.
struct DocumentApi {
    InMemoryIndexRepository repository;
    IndexService indexes{repository, fixed_clock};
    IngestService ingest{repository, fixed_clock};
    RunningServer server{[this](HttpServer& routes) {
        leta::http::register_index_routes(routes, indexes);
        leta::http::register_document_routes(routes, ingest);
    }};
};

}  // namespace

TEST_CASE("POST /indexes/{uid}/documents indexes a batch, creating the index, and answers 200",
          "[FR-02][FR-70]") {
    const DocumentApi api;
    auto client = api.server.client();
    const auto result = client.Post(DocumentsPath, ThreeDocuments, JsonContentType);
    REQUIRE(result);
    CHECK(result->status == 200);
    CHECK(result->body.starts_with(R"({"received":3,"indexed":3,"processingTimeMs":)"));
    CHECK(result->get_header_value("Content-Type") == JsonContentType);
    // The route created the index: creating it again is a conflict.
    const auto created = client.Post("/indexes", R"({"uid":"products"})", JsonContentType);
    REQUIRE(created);
    CHECK(created->status == 409);
}

TEST_CASE("documents posted to an index created with a primary key report it", "[FR-02]") {
    const DocumentApi api;
    auto client = api.server.client();
    REQUIRE(client.Post("/indexes", R"({"uid":"products","primaryKey":"id"})", JsonContentType));
    const auto result = client.Post(DocumentsPath, ThreeDocuments, JsonContentType);
    REQUIRE(result);
    CHECK(result->status == 200);
    CHECK(result->body.starts_with(
        R"({"received":3,"indexed":3,"primaryKey":"id","processingTimeMs":)"));
}

TEST_CASE("a uid outside FR-01's alphabet in the path answers 400 invalid_index_uid",
          "[FR-01][FR-71]") {
    const DocumentApi api;
    auto client = api.server.client();
    const auto result = client.Post("/indexes/bad.uid/documents", ThreeDocuments, JsonContentType);
    REQUIRE(result);
    CHECK(result->status == 400);
    CHECK(
        result->body
        == R"({"code":"invalid_index_uid","message":"An index uid is 1 to 64 characters, each an ASCII letter, digit, `_` or `-`.","type":"invalid_request"})");
}
