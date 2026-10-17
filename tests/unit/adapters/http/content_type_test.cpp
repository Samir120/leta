#include "adapters/http/content_type.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

using leta::http::is_json_content_type;

}  // namespace

TEST_CASE("application/json is JSON, with or without parameters, in any case", "[FR-70]") {
    CHECK(is_json_content_type("application/json"));
    CHECK(is_json_content_type("application/json; charset=utf-8"));
    CHECK(is_json_content_type("Application/JSON"));
    CHECK(is_json_content_type(" application/json \t"));
}

TEST_CASE("anything else, including no Content-Type at all, is not", "[FR-70]") {
    CHECK_FALSE(is_json_content_type(""));
    CHECK_FALSE(is_json_content_type("text/plain"));
    CHECK_FALSE(is_json_content_type("application/x-www-form-urlencoded"));
    CHECK_FALSE(is_json_content_type("application/jsonx"));
    CHECK_FALSE(is_json_content_type("application/json-patch+json"));
}
