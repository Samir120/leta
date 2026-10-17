#include "adapters/http/index_routes.hpp"

#include <chrono>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include "adapters/storage/in_memory_index_repository.hpp"
#include "application/http_server.hpp"
#include "application/index_service.hpp"
#include "core/timestamp.hpp"
#include "integration/running_server.hpp"

namespace {

using leta::application::HttpServer;
using leta::application::IndexService;
using leta::storage::InMemoryIndexRepository;
using leta::testing::RunningServer;

constexpr const char* JsonContentType = "application/json";
constexpr const char* ProductsBody = R"({"uid":"products"})";

leta::core::Timestamp fixed_clock() {
    return leta::core::Timestamp{std::chrono::milliseconds{1'760'000'000'000}};
}

// The routes and what they reference, in declaration order: repository and service are built
// before the server and destroyed after it.
struct IndexApi {
    InMemoryIndexRepository repository;
    IndexService service{repository, fixed_clock};
    RunningServer server{
        [this](HttpServer& routes) { leta::http::register_index_routes(routes, service); }};
};

}  // namespace

TEST_CASE("POST /indexes creates an index and answers 201 with it, as JSON", "[FR-02][FR-70]") {
    const IndexApi api;
    auto client = api.server.client();
    const auto result = client.Post("/indexes", ProductsBody, JsonContentType);
    REQUIRE(result);
    CHECK(result->status == 201);
    CHECK(
        result->body
        == R"({"uid":"products","primaryKey":null,"createdAt":"2025-10-09T08:53:20.000Z","updatedAt":"2025-10-09T08:53:20.000Z"})");
    CHECK(result->get_header_value("Content-Type") == JsonContentType);
}

TEST_CASE("POST /indexes for a uid that exists answers 409 index_already_exists", "[FR-02]") {
    const IndexApi api;
    auto client = api.server.client();
    REQUIRE(client.Post("/indexes", ProductsBody, JsonContentType));
    const auto result = client.Post("/indexes", ProductsBody, JsonContentType);
    REQUIRE(result);
    CHECK(result->status == 409);
    CHECK(
        result->body
        == R"({"code":"index_already_exists","message":"Index `products` already exists.","type":"invalid_request"})");
}

TEST_CASE("a POST body not declared as JSON is refused before the route runs", "[FR-70]") {
    const IndexApi api;
    auto client = api.server.client();
    const auto refused = client.Post("/indexes", ProductsBody, "text/plain");
    REQUIRE(refused);
    CHECK(refused->status == 400);
    CHECK(
        refused->body
        == R"({"code":"malformed_request","message":"The request body must be JSON, sent with Content-Type: application/json.","type":"invalid_request"})");
    // Nothing was created: the same uid is still free.
    const auto created = client.Post("/indexes", ProductsBody, JsonContentType);
    REQUIRE(created);
    CHECK(created->status == 201);
}
