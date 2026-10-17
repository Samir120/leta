#include "core/index_uid.hpp"

#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "core/error.hpp"

namespace {

using leta::core::ErrorCode;
using leta::core::IndexUid;

}  // namespace

TEST_CASE("a uid of 1 to 64 letters, digits, _ and - is accepted unchanged", "[FR-01]") {
    const std::string uid =
        GENERATE(as<std::string>{}, "a", "products", "Products_2026-v1", std::string(64, 'x'));
    CAPTURE(uid);
    const auto parsed = IndexUid::parse(uid);
    REQUIRE(parsed);
    CHECK(parsed->value() == uid);
}

TEST_CASE("any other uid is refused as invalid_index_uid", "[FR-01]") {
    const std::string uid = GENERATE(as<std::string>{},
                                     "",
                                     std::string(65, 'x'),
                                     "my index",
                                     "a/b",
                                     "a.b",
                                     "skärm",
                                     "products\n");
    CAPTURE(uid);
    const auto parsed = IndexUid::parse(uid);
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().code == ErrorCode::InvalidIndexUid);
}
