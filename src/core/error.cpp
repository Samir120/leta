#include "core/error.hpp"

#include <exception>

namespace leta::core {

ErrorType error_type(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::MalformedRequest:
        case ErrorCode::PayloadTooLarge:
            return ErrorType::InvalidRequest;
        case ErrorCode::RouteNotFound:
            return ErrorType::NotFound;
        case ErrorCode::Internal:
            return ErrorType::Internal;
    }
    // Reachable only through a value outside the enumeration, a programmer error (06 §5). It is
    // also what tells GCC this function cannot fall off its end.
    std::terminate();
}

}  // namespace leta::core
