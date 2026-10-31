#pragma once

#include <string_view>

#include <cstddef>
#include <cstdint>

namespace leta::fuzzing {

/// libFuzzer's input as the text every Leta boundary reads. A view: valid for one run.
inline std::string_view as_text(const std::uint8_t* data, std::size_t size) noexcept {
    // libFuzzer passes unsigned bytes and the parsers read char: the same bytes, another type.
    return {reinterpret_cast<const char*>(data), size};
}

}  // namespace leta::fuzzing
