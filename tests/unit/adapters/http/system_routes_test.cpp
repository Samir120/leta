#include "adapters/http/system_routes.hpp"

#include <catch2/catch_test_macros.hpp>

#include "application/health_service.hpp"
#include "application/http_server.hpp"

namespace {

using leta::application::HttpResponse;
using leta::application::Readiness;
using leta::http::health_response;

}  // namespace

TEST_CASE("health answers 200 available once the server can serve queries", "[FR-60]") {
    const HttpResponse response = health_response(Readiness::Available);
    CHECK(response.status == 200);
    CHECK(response.body == R"({"status":"available"})");
}

TEST_CASE("health answers 503 starting while the server cannot serve queries yet", "[FR-60]") {
    const HttpResponse response = health_response(Readiness::Starting);
    CHECK(response.status == 503);
    CHECK(response.body == R"({"status":"starting"})");
}
