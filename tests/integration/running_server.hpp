#pragma once

#include <functional>
#include <thread>

#include <cstdint>

#include <httplib.h>

#include "adapters/http/httplib_server.hpp"
#include "adapters/http/system_routes.hpp"
#include "application/health_service.hpp"
#include "application/http_server.hpp"

namespace leta::testing {

/// Routes a test registers on top of GET /health, before the server binds.
using AddRoutes = std::function<void(application::HttpServer&)>;

inline constexpr const char* Loopback = "127.0.0.1";

/// A real server on an ephemeral loopback port, serving on its own thread for the life of one test.
/// GET /health is always registered and reports available. Whatever the extra routes reference
/// must be declared before the RunningServer, so that it outlives the server's workers.
class RunningServer {
public:
    explicit RunningServer(const AddRoutes& add_routes = {},
                           const application::HttpServerOptions& options = {})
        : server_{options}
        , port_{start_listening(add_routes)}
        ,
        // A failure to serve shows up as the test's own request failing.
        runner_{[this] { static_cast<void>(server_.run()); }} {}

    ~RunningServer() {
        server_.stop();
        runner_.join();
    }

    RunningServer(const RunningServer&) = delete;
    RunningServer& operator=(const RunningServer&) = delete;
    RunningServer(RunningServer&&) = delete;
    RunningServer& operator=(RunningServer&&) = delete;

    [[nodiscard]] httplib::Client client() const {
        return httplib::Client{Loopback, port_};
    }

private:
    // Runs during construction, after health_ and server_ exist and before the thread starts.
    std::uint16_t start_listening(const AddRoutes& add_routes) {
        health_.mark_available();
        http::register_system_routes(server_, health_);
        if (add_routes) {
            add_routes(server_);
        }
        return server_.bind(Loopback, 0).value();
    }

    application::HealthService health_;
    http::HttplibServer server_;
    std::uint16_t port_;
    std::thread runner_;  // declared last: the thread starts only once everything it touches exists
};

}  // namespace leta::testing
