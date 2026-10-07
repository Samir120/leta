#include "adapters/http/error_response.hpp"

#include <catch2/catch_test_macros.hpp>

#include "application/http_server.hpp"
#include "core/error.hpp"

namespace {

using leta::application::HttpResponse;
using leta::core::Error;
using leta::core::ErrorCode;
using leta::http::error_for_library_status;
using leta::http::error_response;

}  // namespace

TEST_CASE("an error response carries code, message and type in the FR-71 shape", "[FR-71]") {
    const HttpResponse response =
        error_response(Error{.code = ErrorCode::RouteNotFound, .message = "No route."});
    CHECK(response.status == 404);
    CHECK(response.body
          == R"({"code":"route_not_found","message":"No route.","type":"not_found"})");
}

TEST_CASE("an invalid_request error names its type", "[FR-71]") {
    const HttpResponse response =
        error_response(Error{.code = ErrorCode::MalformedRequest, .message = "Bad."});
    CHECK(response.body
          == R"({"code":"malformed_request","message":"Bad.","type":"invalid_request"})");
}

TEST_CASE("each error code has its own HTTP status", "[FR-71]") {
    CHECK(error_response(Error{.code = ErrorCode::MalformedRequest, .message = ""}).status == 400);
    CHECK(error_response(Error{.code = ErrorCode::PayloadTooLarge, .message = ""}).status == 413);
    CHECK(error_response(Error{.code = ErrorCode::Internal, .message = ""}).status == 500);
}

TEST_CASE("the message is JSON-escaped", "[FR-71]") {
    const HttpResponse response =
        error_response(Error{.code = ErrorCode::Internal, .message = R"(a "b")"});
    CHECK(response.body == R"({"code":"internal","message":"a \"b\"","type":"internal"})");
}

TEST_CASE("statuses the library sets map to codes a client can branch on", "[FR-71]") {
    CHECK(error_for_library_status(404).code == ErrorCode::RouteNotFound);
    CHECK(error_for_library_status(413).code == ErrorCode::PayloadTooLarge);
    CHECK(error_for_library_status(400).code == ErrorCode::MalformedRequest);
    CHECK(error_for_library_status(414).code == ErrorCode::MalformedRequest);
    CHECK(error_for_library_status(431).code == ErrorCode::MalformedRequest);
    CHECK(error_for_library_status(500).code == ErrorCode::Internal);
    CHECK(error_for_library_status(503).code == ErrorCode::Internal);
}
