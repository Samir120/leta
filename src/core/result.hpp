#pragma once

#include <string>
#include <utility>

#include <tl/expected.hpp>

#include "core/error.hpp"

namespace leta::core {

/// The return type of anything that can fail in a way its caller can act on (ADR-005). An alias of
/// the vendored tl::expected, which mirrors C++23 std::expected; adopting C++23 is an edit to this
/// file plus the deletion of the vendored header.
template <typename T, typename E = Error>
using Result = tl::expected<T, E>;

/// The failure branch of a Result: `return core::failure(ErrorCode::Internal, "...");`. Exists so
/// that no call site names tl:: and the C++23 switch stays a one-file edit.
[[nodiscard]] inline tl::unexpected<Error> failure(ErrorCode code, std::string message) {
    return tl::unexpected<Error>{Error{.code = code, .message = std::move(message)}};
}

}  // namespace leta::core
