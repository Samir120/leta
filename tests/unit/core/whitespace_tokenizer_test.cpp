#include "core/whitespace_tokenizer.hpp"

#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

using leta::core::split_on_whitespace;
using Words = std::vector<std::string_view>;

}  // namespace

TEST_CASE("text splits into the runs between ASCII whitespace", "[M1]") {
    CHECK(split_on_whitespace("Corsair DDR5 6000") == Words{"Corsair", "DDR5", "6000"});
}

TEST_CASE("runs of whitespace, leading and trailing included, make no empty words", "[M1]") {
    CHECK(split_on_whitespace(" \tMSI \n\r B650  ") == Words{"MSI", "B650"});
}

TEST_CASE("empty or all-whitespace text has no words", "[M1]") {
    CHECK(split_on_whitespace("").empty());
    CHECK(split_on_whitespace(" \t\n\v\f\r").empty());
}

TEST_CASE("nothing is folded or stripped yet: case, diacritics, punctuation stay (M2)", "[M1]") {
    CHECK(split_on_whitespace("Sk\u00e4rm, 27\"") == Words{"Sk\u00e4rm,", "27\""});
}
