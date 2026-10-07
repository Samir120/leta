#include "adapters/http/json_string.hpp"

#include <string>
#include <string_view>

#include <cstddef>

namespace leta::http {

namespace {

constexpr std::string_view HexDigits = "0123456789abcdef";

// RFC 8259 §7: U+0000 through U+001F must be escaped; from here up, nothing need be.
constexpr unsigned char FirstUnescapedByte = 0x20;

void append_unicode_escape(std::string& out, unsigned char byte) {
    const std::size_t value = byte;
    out.append("\\u00");
    out.push_back(HexDigits[value >> 4U]);
    out.push_back(HexDigits[value & 0x0FU]);
}

void append_escaped(std::string& out, char character) {
    switch (character) {
        case '"':
            out.append("\\\"");
            return;
        case '\\':
            out.append("\\\\");
            return;
        case '\b':
            out.append("\\b");
            return;
        case '\f':
            out.append("\\f");
            return;
        case '\n':
            out.append("\\n");
            return;
        case '\r':
            out.append("\\r");
            return;
        case '\t':
            out.append("\\t");
            return;
        default:
            break;
    }
    const auto byte = static_cast<unsigned char>(character);
    if (byte < FirstUnescapedByte) {
        append_unicode_escape(out, byte);
        return;
    }
    out.push_back(character);
}

}  // namespace

void append_json_string(std::string& out, std::string_view value) {
    out.push_back('"');
    for (const char character : value) {
        append_escaped(out, character);
    }
    out.push_back('"');
}

}  // namespace leta::http
