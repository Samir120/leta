#include "core/error.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

using leta::core::error_type;
using leta::core::ErrorCode;
using leta::core::ErrorType;

}  // namespace

TEST_CASE("codes about the shape of a request are invalid_request error", "[FR-71]") {
    CHECK(error_type(ErrorCode::MalformedRequest) == ErrorType::InvalidRequest);
    CHECK(error_type(ErrorCode::PayloadTooLarge) == ErrorType::InvalidRequest);
}

TEST_CASE("route_not_found is a not_found error", "[FR-71]") {
    CHECK(error_type(ErrorCode::RouteNotFound) == ErrorType::NotFound);
}

TEST_CASE("internal is an internal error", "[FR-71]") {
    CHECK(error_type(ErrorCode::Internal) == ErrorType::Internal);
}
