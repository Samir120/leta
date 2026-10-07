#include "adapters/http/request_id.hpp"

#include <algorithm>
#include <random>
#include <string>
#include <string_view>

#include <cstddef>
#include <cstdint>

namespace leta::http {

namespace {

constexpr std::string_view HexDigits = "0123456789abcdef";
constexpr unsigned BitsPerHexDigit = 4;
constexpr unsigned HexDigitsPerWord = 16;

bool is_request_id_character(char character) noexcept {
    const bool is_lower = character >= 'a' && character <= 'z';
    const bool is_upper = character >= 'A' && character <= 'Z';
    const bool is_digit = character >= '0' && character <= '9';
    return is_lower || is_upper || is_digit || character == '-' || character == '_'
           || character == '.';
}

void append_hex_word(std::string& out, std::uint64_t word) {
    for (unsigned position = 0; position < HexDigitsPerWord; ++position) {
        const unsigned shift = (HexDigitsPerWord - 1U - position) * BitsPerHexDigit;
        out.push_back(HexDigits[(word >> shift) & 0xFU]);
    }
}

}  // namespace

bool is_acceptable_request_id(std::string_view candidate) noexcept {
    return !candidate.empty() && candidate.size() <= MaxRequestIdLength
           && std::ranges::all_of(candidate, is_request_id_character);
}

std::string format_request_id(std::uint64_t high, std::uint64_t low) {
    std::string id;
    // Two words of hex digits. Computed in size_t, the type reserve() takes, so nothing is widened
    // after the multiplication (bugprone-implicit-widening-of-multiplication-result).
    id.reserve(std::size_t{2} * HexDigitsPerWord);
    append_hex_word(id, high);
    append_hex_word(id, low);
    return id;
}

std::string generate_request_id() {
    // One engine per thread: HTTP workers share nothing, so there is no lock and nothing for TSan
    // to find. Seeded once per thread from the OS.
    thread_local std::mt19937_64 engine{std::random_device{}()};
    const std::uint64_t high = engine();
    const std::uint64_t low = engine();
    return format_request_id(high, low);
}

}  // namespace leta::http
