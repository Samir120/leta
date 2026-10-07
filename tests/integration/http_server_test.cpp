#include "application/http_server.hpp"

#include <functional>
#include <stdexcept>
#include <string>
#include <thread>

#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include "adapters/http/httplib_server.hpp"
#include "adapters/http/request_id.hpp"
#include "adapters/http/system_routes.hpp"
#include "application/health_service.hpp"

namespace {

using leta::application::HealthService;
using leta::application::HttpMethod;
using leta::application::HttpRequest;
using leta::application::HttpResponse;
using leta::application::HttpServer;
using leta::application::HttpServerOptions;
using leta::http::HttplibServer;
using leta::http::is_acceptable_request_id;
using leta::http::RequestIdHeader;

using AddRoutes = std::function<void(HttpServer&)>;

constexpr const char* Loopback = "127.0.0.1";

std::uint16_t start_listening(HttplibServer& server,
                              HealthService& health,
                              const AddRoutes& add_routes) {
    health.mark_available();
    leta::http::register_system_routes(server, health);
    if (add_routes) {
        add_routes(server);
    }
    return server.bind(Loopback, 0).value();
}

// A real server on an ephemeral loopback port, serving on its own thread for the life of one test.
// runner_ is declared last, so the thread starts only once everything it touches exists.
class RunningServer {
public:
    explicit RunningServer(const AddRoutes& add_routes = {}, const HttpServerOptions& options = {})
        : server_{options}
        , port_{start_listening(server_, health_, add_routes)}
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
    HealthService health_;
    HttplibServer server_;
    std::uint16_t port_;
    std::thread runner_;
};

void add_echo_route(HttpServer& routes) {
    routes.add_route(HttpMethod::Post, "/echo", [](const HttpRequest& request) {
        std::string body{request.request_id};
        body.append(" ").append(request.body);
        return HttpResponse{.status = 200, .body = body};
    });
}

}  // namespace

TEST_CASE("GET /health answers 200 available, as JSON", "[FR-60]") {
    const RunningServer server;
    auto client = server.client();
    const auto result = client.Get("/health");
    REQUIRE(result);
    CHECK(result->status == 200);
    CHECK(result->body == R"({"status":"available"})");
    CHECK(result->get_header_value("Content-Type") == "application/json");
}

TEST_CASE("a request without X-Request-Id gets a generated one back", "[FR-73]") {
    const RunningServer server;
    auto client = server.client();
    const auto result = client.Get("/health");
    REQUIRE(result);
    const std::string id = result->get_header_value(RequestIdHeader);
    CHECK(id.size() == 32);
    CHECK(is_acceptable_request_id(id));
}

TEST_CASE("an acceptable inbound X-Request-Id is echoed unchanged", "[FR-73]") {
    const RunningServer server;
    auto client = server.client();
    const auto result =
        client.Get("/health", httplib::Headers{{RequestIdHeader, "swadestack-7f3a.1"}});
    REQUIRE(result);
    CHECK(result->get_header_value(RequestIdHeader) == "swadestack-7f3a.1");
}

TEST_CASE("an unacceptable inbound X-Request-Id is replaced, not echoed", "[FR-73]") {
    const RunningServer server;
    auto client = server.client();
    const auto result = client.Get("/health", httplib::Headers{{RequestIdHeader, "two words"}});
    REQUIRE(result);
    const std::string id = result->get_header_value(RequestIdHeader);
    CHECK(id != "two words");
    CHECK(is_acceptable_request_id(id));
}

TEST_CASE("a handler sees the body and the same request id the client gets", "[FR-73]") {
    const RunningServer server{add_echo_route};
    auto client = server.client();
    const std::string body = R"({"q":"skärm"})";
    const auto result = client.Post("/echo", body, "application/json");
    REQUIRE(result);
    const std::string id = result->get_header_value(RequestIdHeader);
    CHECK(result->body == id + " " + body);
}

TEST_CASE("an unknown route answers 404 with an FR-71 body and a request id", "[FR-71]") {
    const RunningServer server;
    auto client = server.client();
    const auto result = client.Get("/no-such-route");
    REQUIRE(result);
    CHECK(result->status == 404);
    CHECK(
        result->body
        == R"({"code":"route_not_found","message":"No route matches this method and path.","type":"not_found"})");
    CHECK(result->get_header_value("Content-Type") == "application/json");
    CHECK(is_acceptable_request_id(result->get_header_value(RequestIdHeader)));
}

TEST_CASE("an exception in a handler becomes 500 internal without its detail", "[FR-71]") {
    const RunningServer server{[](HttpServer& routes) {
        routes.add_route(HttpMethod::Get,
                         "/throws",
                         [](const HttpRequest& /*request*/) -> HttpResponse {
                             throw std::runtime_error{"secret detail"};
                         });
    }};
    auto client = server.client();
    const auto result = client.Get("/throws");
    REQUIRE(result);
    CHECK(result->status == 500);
    CHECK(result->body == R"({"code":"internal","message":"Internal error.","type":"internal"})");
    CHECK(is_acceptable_request_id(result->get_header_value(RequestIdHeader)));
}

TEST_CASE("a body over the configured limit is refused with 413 payload_too_large",
          "[NFR-12][FR-71]") {
    HttpServerOptions options;
    options.max_body_bytes = 16;
    const RunningServer server{add_echo_route, options};
    auto client = server.client();
    const auto result = client.Post("/echo", std::string(64, 'x'), "application/json");
    REQUIRE(result);
    CHECK(result->status == 413);
    CHECK(
        result->body
        == R"({"code":"payload_too_large","message":"The request exceeds a size limit.","type":"invalid_request"})");
}

TEST_CASE("a server with no worker threads is refused at construction", "[ADR-003]") {
    HttpServerOptions options;
    options.worker_threads = 0;
    CHECK_THROWS_AS(HttplibServer{options}, std::invalid_argument);
}
