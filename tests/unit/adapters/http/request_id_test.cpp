#include "adapters/http/request_id.hpp"

#include <string>

#include <catch2/catch_test_macros.hpp>

namespace {

using leta::http::format_request_id;
using leta::http::generate_request_id;
using leta::http::is_acceptable_request_id;
using leta::http::MaxRequestIdLength;

}  // namespace

TEST_CASE("ids of letters, digits, dot, dash and underscore are echoed", "[FR-73]") {
    CHECK(is_acceptable_request_id("3f2b9c1e-7a4d-4e8b-9c3a-0d1e2f3a4b5c"));
    CHECK(is_acceptable_request_id("swadestack.search_42"));
    CHECK(is_acceptable_request_id(std::string(MaxRequestIdLength, 'a')));
}

TEST_CASE("empty, over-long and log-breaking ids are not echoed", "[FR-73]") {
    CHECK_FALSE(is_acceptable_request_id(""));
    CHECK_FALSE(is_acceptable_request_id(std::string(MaxRequestIdLength + 1, 'a')));
    CHECK_FALSE(is_acceptable_request_id("two words"));
    CHECK_FALSE(is_acceptable_request_id("forged\nline"));
    CHECK_FALSE(is_acceptable_request_id(R"("quoted")"));
    CHECK_FALSE(is_acceptable_request_id("sök"));
}

TEST_CASE("a request id is 32 lowercase hex digits, high word first", "[FR-73]") {
    CHECK(format_request_id(0, 0) == std::string(32, '0'));
    CHECK(format_request_id(0x0123456789abcdefU, 0xfedcba9876543210U)
          == "0123456789abcdeffedcba9876543210");
}

TEST_CASE("generated ids are acceptable and differ from one call to the next", "[FR-73]") {
    const std::string first = generate_request_id();
    const std::string second = generate_request_id();
    CHECK(first.size() == 32);
    CHECK(is_acceptable_request_id(first));
    CHECK(first != second);
}
