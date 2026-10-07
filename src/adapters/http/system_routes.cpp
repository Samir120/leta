#include "adapters/http/system_routes.hpp"

#include <exception>

#include "application/health_service.hpp"
#include "application/http_server.hpp"

namespace leta::http {

application::HttpResponse health_response(application::Readiness readiness) {
    switch (readiness) {
        case application::Readiness::Available:
            return {.status = 200, .body = R"({"status":"available"})"};
        case application::Readiness::Starting:
            return {.status = 503, .body = R"({"status":"starting"})"};
    }
    std::terminate();
}

void register_system_routes(application::HttpServer& server,
                            const application::HealthService& health) {
    server.add_route(application::HttpMethod::Get,
                     "/health",
                     [&health](const application::HttpRequest& /*request*/) {
                         return health_response(health.readiness());
                     });
}

}  // namespace leta::http
