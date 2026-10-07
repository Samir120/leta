#pragma once

#include <string>
#include <string_view>

namespace leta::http {
/// Appends `value` to `out` as a quoted JSON string (RFC 8259 §7). Quote, backslash and the C0
/// controls are escaped; every other byte, UTF-8 included, is copied verbatim.
///
/// Precondition: `value` is valid UTF-8. Text that echoes client input must be validated before it
/// reaches a response; nothing in M1-T1 echoes any.
void append_json_string(std::string& out, std::string_view value);

}  // namespace leta::http
