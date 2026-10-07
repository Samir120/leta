#include "adapters/http/json_string.hpp"

#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

namespace {

using leta::http::append_json_string;

std::string as_json(std::string_view value) {
    std::string out;
    append_json_string(out, value);
    return out;
}

}  // namespace

TEST_CASE("plain text is quoted and otherwise unchanged", "[FR-71]") {
    CHECK(as_json("Index not found.") == R"("Index not found.")");
}

TEST_CASE("append_json_string appends to what is already there", "[FR-71]") {
    std::string out{"x:"};
    append_json_string(out, "y");
    CHECK(out == R"(x:"y")");
}

TEST_CASE("quote and backslash are escaped", "[FR-71]") {
    CHECK(as_json(R"(say "hi" \ bye)") == R"("say \"hi\" \\ bye")");
}

TEST_CASE("control characters use JSON's short escape where it has one", "[FR-71]") {
    CHECK(as_json("a\nb\tc\rd\be\ff") == R"("a\nb\tc\rd\be\ff")");
}

TEST_CASE("other C0 controls, NUL included, become \\u00XX escapes", "[FR-71]") {
    CHECK(as_json(std::string_view{"a\0b\x1f", 4}) == R"("a\u0000b\u001f")");
}

TEST_CASE("UTF-8 and DEL pass through unchanged", "[FR-71]") {
    CHECK(as_json("Skärm \x7f") == "\"Skärm \x7f\"");
}
