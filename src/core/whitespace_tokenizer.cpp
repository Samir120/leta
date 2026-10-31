#include "core/whitespace_tokenizer.hpp"

#include <string_view>
#include <vector>

#include <cstddef>

namespace leta::core {

namespace {

// ASCII only. Unicode spaces (U+00A0 and friends) are the M2 tokenizer's business.
constexpr std::string_view AsciiWhitespace = " \t\n\v\f\r";

}  // namespace

std::vector<std::string_view> split_on_whitespace(std::string_view text) {
    std::vector<std::string_view> words;
    std::size_t start = text.find_first_not_of(AsciiWhitespace);
    while (start != std::string_view::npos) {
        const std::size_t end = text.find_first_of(AsciiWhitespace, start);
        // When end is npos, substr clamps the count to the rest of the text: the last word.
        words.push_back(text.substr(start, end - start));
        start = text.find_first_not_of(AsciiWhitespace, end);
    }
    return words;
}

}  // namespace leta::core
