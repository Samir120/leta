#include "adapters/http/error_response.hpp"

#include <exception>
#include <string>
#include <string_view>
#include <utility>

#include "adapters/http/json_string.hpp"
#include "application/http_server.hpp"
#include "core/error.hpp"

namespace leta::http {

namespace {

struct WireError {
    std::string_view code;
    int status;
};

// Status is per code, not per type: 02-api-guide.md §6 gives one type (invalid_request) three
// statuses, which a type-keyed table cannot express.
WireError wire_error(core::ErrorCode code) noexcept {
    switch (code) {
        case core::ErrorCode::MalformedRequest:
            return {.code = "malformed_request", .status = 400};
        case core::ErrorCode::PayloadTooLarge:
            return {.code = "payload_too_large", .status = 413};
        case core::ErrorCode::RouteNotFound:
            return {.code = "route_not_found", .status = 404};
        case core::ErrorCode::Internal:
            return {.code = "internal", .status = 500};
    }
    std::terminate();
}

std::string_view wire_type(core::ErrorType type) noexcept {
    switch (type) {
        case core::ErrorType::InvalidRequest:
            return "invalid_request";
        case core::ErrorType::Auth:
            return "auth";
        case core::ErrorType::NotFound:
            return "not_found";
        case core::ErrorType::Internal:
            return "internal";
    }
    std::terminate();
}

}  // namespace

application::HttpResponse error_response(const core::Error& error) {
    const WireError wire = wire_error(error.code);
    std::string body{R"({"code":")"};
    body.append(wire.code).append(R"(","message":)");
    append_json_string(body, error.message);
    body.append(R"(,"type":")").append(wire_type(core::error_type(error.code))).append(R"("})");
    return {.status = wire.status, .body = std::move(body)};
}

core::Error error_for_library_status(int status) {
    if (status == 404) {
        return {.code = core::ErrorCode::RouteNotFound,
                .message = "No route matches this method and path."};
    }
    // cpp-httplib also answers 413 for an over-long request URI, so the message names no one limit.
    if (status == 413) {
        return {.code = core::ErrorCode::PayloadTooLarge,
                .message = "The request exceeds a size limit."};
    }
    if (status >= 500) {
        return {.code = core::ErrorCode::Internal, .message = "Internal error."};
    }
    return {.code = core::ErrorCode::MalformedRequest,
            .message = "The request could not be parsed."};
}

}  // namespace leta::http
