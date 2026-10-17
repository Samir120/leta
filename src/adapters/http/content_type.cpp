#include "adapters/http/content_type.hpp"

#include <algorithm>
#include <string_view>

namespace leta::http {

namespace {

constexpr std::string_view JsonMediaType = "application/json";

// RFC 9110's optional whitespace: spaces and horizontal tabs.
std::string_view trim_whitespace(std::string_view text) {
    constexpr std::string_view Whitespace = " \t";
    const auto first = text.find_first_not_of(Whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(Whitespace);
    return text.substr(first, last - first + 1);
}

char to_ascii_lower(char character) noexcept {
    return (character >= 'A' && character <= 'Z') ? static_cast<char>(character - 'A' + 'a')
                                                  : character;
}

bool equals_ignoring_ascii_case(std::string_view left, std::string_view right) {
    return std::ranges::equal(left, right, [](char left_character, char right_character) {
        return to_ascii_lower(left_character) == to_ascii_lower(right_character);
    });
}

}  // namespace

bool is_json_content_type(std::string_view header) {
    const std::string_view media_type = trim_whitespace(header.substr(0, header.find(';')));
    return equals_ignoring_ascii_case(media_type, JsonMediaType);
}

}  // namespace leta::http
