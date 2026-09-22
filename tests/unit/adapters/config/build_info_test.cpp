#include "adapters/config/build_info.hpp"

#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

namespace {

using leta::config::build_info;
using leta::config::BuildInfo;
using leta::config::version_line;

// The same shape tests/CMakeLists.txt pins on the real process, applied here to the function, so
// the two ends of FR-68 cannot drift apart without one of them failing by name.
constexpr std::string_view VersionLinePattern =
    R"(leta [0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.]+)? )"
    R"(\(([0-9a-f]+(-dirty)?|unknown), [A-Za-z]+, [A-Za-z]+ [0-9.]+\))";

}  // namespace

TEST_CASE("version_line renders every field in the FR-68 order", "[FR-68]") {
    const BuildInfo info{
        .version = "1.2.3-rc.1",
        .commit = "a1b2c3d",
        .build_type = "Release",
        .compiler = "Clang 17.0.6",
    };
    CHECK(version_line(info) == "leta 1.2.3-rc.1 (a1b2c3d, Release, Clang 17.0.6)");
}

TEST_CASE("version_line keeps a -dirty commit suffix verbatim", "[FR-68]") {
    const BuildInfo info{
        .version = "0.1.0-dev",
        .commit = "ebe708f-dirty",
        .build_type = "Debug",
        .compiler = "GNU 13.2.0",
    };
    CHECK(version_line(info) == "leta 0.1.0-dev (ebe708f-dirty, Debug, GNU 13.2.0)");
}

TEST_CASE("the compiled-in build info has every field populated", "[FR-68]") {
    const BuildInfo info = build_info();
    CHECK_FALSE(info.version.empty());
    CHECK_FALSE(info.commit.empty());
    CHECK_FALSE(info.build_type.empty());
    CHECK_FALSE(info.compiler.empty());
}

TEST_CASE("the compiled-in version line has the FR-68 shape", "[FR-68]") {
    using Catch::Matchers::Matches;
    CHECK_THAT(version_line(build_info()), Matches(std::string{VersionLinePattern}));
}
