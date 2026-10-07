#include "core/result.hpp"

#include <catch2/catch_test_macros.hpp>

#include "core/error.hpp"

namespace {

using leta::core::ErrorCode;
using leta::core::Result;

Result<int> require_positive(int value) {
    if (value <= 0) {
        return leta::core::failure(ErrorCode::MalformedRequest, "not positive");
    }
    return value;
}

}  // namespace

TEST_CASE("failure() puts its code and message on the error branch of a Result", "[FR-71]") {
    const Result<int> result = require_positive(-1);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == ErrorCode::MalformedRequest);
    CHECK(result.error().message == "not positive");
}

TEST_CASE("a Result built from a value carries that value", "[FR-71]") {
    const Result<int> result = require_positive(7);
    REQUIRE(result.has_value());
    CHECK(*result == 7);
}
