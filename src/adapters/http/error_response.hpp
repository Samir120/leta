#pragma once

#include "application/http_server.hpp"
#include "core/error.hpp"

namespace leta::http {

/// The FR-71 response for an Error: the status for its code, and the body
/// `{"code":"…","message":"…","type":"…"}` with `type` derived from the code (ADR-005).
[[nodiscard]] application::HttpResponse error_response(const core::Error& error);

/// The Error for a status the HTTP library produced on its own, before or instead of any route:
/// an unmatched path, an oversized request (NFR-12), an unparseable one. The library writes no
/// body for these; this Error becomes it.
[[nodiscard]] core::Error error_for_library_status(int status);

}  // namespace leta::http
