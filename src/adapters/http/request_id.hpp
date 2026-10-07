#pragma once

#include <string>
#include <string_view>

#include <cstddef>
#include <cstdint>

namespace leta::http {

/// FR-73. A `const char*` so it converts to the std::string the transport API takes.
inline constexpr const char* RequestIdHeader = "X-Request-Id";

/// Longest inbound id that is echoed. Longer ids are replaced, not truncated: a truncated id no
/// longer matches anything in the caller's own logs.
inline constexpr std::size_t MaxRequestIdLength = 128;

/// Whether an inbound X-Request-Id is echoed verbatim: 1 to 128 characters of [A-Za-z0-9._-]. The
/// id goes into every log line for its request (FR-65), so anything that could break or forge a
/// log line is replaced rather than escaped.
[[nodiscard]] bool is_acceptable_request_id(std::string_view candidate) noexcept;

/// 32 lowercase hex digits, `high` first. Separate from generation so the format is testable.
[[nodiscard]] std::string format_request_id(std::uint64_t high, std::uint64_t low);

/// A fresh id for a request that brought none, or an unacceptable one. For correlation only:
/// unique in practice, but neither secret nor unguessable.
[[nodiscard]] std::string generate_request_id();

}  // namespace leta::http
