#pragma once

#include <string>

namespace leta::http {

/// FR-70: whether a Content-Type header value names JSON. The media type is compared ignoring
/// ASCII case (RFC 9110 §8.3.1) and parameters such as `; charset=utf-8` are ignored: JSON is UTF-8
/// by definition (RFC 8259 §8.1), and the parser rejects anything that is not.
[[nodiscard]] bool is_json_content_type(std::string_view header);

}  // namespace leta::http
