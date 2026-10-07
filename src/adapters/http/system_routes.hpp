#pragma once

#include "application/health_service.hpp"
#include "application/http_server.hpp"

namespace leta::http {

/// FR-60: 200 `{"status":"available"}` or 503 `{"status":"starting"}` (02-api-spec.yaml /health).
[[nodiscard]] application::HttpResponse health_response(application::Readiness readiness);

/// Registers GET /health. `health` must outlive `server`: the route holds a reference to it.
void register_system_routes(application::HttpServer& server,
                            const application::HealthService& health);

}  // namespace leta::http
