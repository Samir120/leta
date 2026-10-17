#include "core/index_uid.hpp"

#include <algorithm>
#include <string>
#include <string_view>

#include <cstddef>

#include "core/error.hpp"
#include "core/result.hpp"

namespace leta::core {

namespace {

constexpr std::size_t MaxUidLength = 64;

// ASCII ranges spelled out instead of std::isalnum, whose answer depends on the C locale and whose
// argument must first be cast to unsigned char. FR-01 means ASCII, whatever the locale.
constexpr bool is_uid_character(char character) noexcept {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z')
           || (character >= '0' && character <= '9') || character == '_' || character == '-';
}

}  // namespace

Result<IndexUid> IndexUid::parse(std::string_view text) {
    const bool has_valid_length = !text.empty() && text.size() <= MaxUidLength;
    if (!has_valid_length || !std::ranges::all_of(text, is_uid_character)) {
        return failure(
            ErrorCode::InvalidIndexUid,
            "An index uid is 1 to 64 characters, each an ASCII letter, digit, `_` or `-`.");
    }
    return IndexUid{std::string{text}};
}

}  // namespace leta::core
