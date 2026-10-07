#pragma once

#include <string>

#include <cstdint>

namespace leta::core {

/// Why an operation failed, as a stable value a caller can branch on (ADR-005, FR-71).
///
/// Each enumerator maps 1:1 to a snake_case `code` string on the wire. That string and the HTTP
/// status live in adapters/http/error_response.cpp, because core does not know a wire exists.
/// Grouped by ErrorType so the table in error.cpp reads top to bottom. A shipped code is never
/// renamed or reused: clients branch on it (02-api-guide.md §6).
enum class ErrorCode : std::uint8_t {
    // invalid_request
    MalformedRequest,
    PayloadTooLarge,
    // not_found
    RouteNotFound,
    // internal
    Internal,
};

/// FR-71's `type`: the coarse class a client can handle without knowing every code. A closed set
/// fixed by the requirement; Auth has no code until the master key (FR-64) lands.
enum class ErrorType : std::uint8_t {
    InvalidRequest,
    Auth,
    NotFound,
    Internal,
};

/// A failure as it travels through core and application. `message` is for humans and never carries
/// internals, file paths or memory contents (06-coding-standards.md §5).
struct Error {
    ErrorCode code{ErrorCode::Internal};
    std::string message;
};

/// FR-71's `type` is a function of the code, not a second stored field: one table, so an Error can
/// never carry a code and a type that disagree (ADR-005).
[[nodiscard]] ErrorType error_type(ErrorCode code) noexcept;

}  // namespace leta::core
