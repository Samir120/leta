#pragma once

#include <algorithm>
#include <functional>
#include <span>
#include <string>
#include <string_view>

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "core/result.hpp"

namespace leta::application {

/// The methods a route can be registered for. Grows when a route needs one (DELETE, PATCH in M6).
enum class HttpMethod : std::uint8_t {
    Get,
    Post,
};

/// One `{name}` segment of a route and the text that filled it in this request.
struct PathParameter {
    std::string_view name;
    std::string_view value;
};

/// One request as a handler sees it. Every view points into the transport's own buffers and is
/// valid only until the handler returns: copy whatever must outlive the call (06 §3).
struct HttpRequest {
    HttpMethod method{HttpMethod::Get};
    std::string_view path;
    std::span<const PathParameter> path_parameters;  // one per `{name}` in the route, any order
    std::string_view body;
    std::string_view request_id;  // FR-73: the same value the response's X-Request-Id carries
};

/// The text that filled the route's `{name}` segment. Empty when the route declares no such
/// segment, which is a mistake in the route: its own tests catch it, because every request then
/// looks like one with an empty segment.
[[nodiscard]] inline std::string_view path_parameter(const HttpRequest& request,
                                                     std::string_view name) noexcept {
    const auto found = std::ranges::find(request.path_parameters, name, &PathParameter::name);
    return found == request.path_parameters.end() ? std::string_view{} : found->value;
}

/// One response. The status is chosen only by code under adapters/http, the single layer that
/// knows what a status code is (ADR-005); application services return Results, never responses.
struct HttpResponse {
    int status{};
    std::string body;  // JSON; the adapter sets Content-Type (FR-70)
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

/// Transport limits, fixed for the life of a server. FR-63 (M9) makes worker_threads configurable
/// as LETA_WORKER_THREADS / --worker-threads (A10).
struct HttpServerOptions {
    // Sized to connections, not cores: a keep-alive connection holds a worker until it closes
    // (ADR-003). Must be at least 1.
    std::size_t worker_threads{64};
    std::size_t max_body_bytes{std::size_t{20} * 1024 * 1024};  // NFR-12
    std::chrono::seconds read_timeout{30};                      // NFR-12
    std::chrono::seconds write_timeout{30};                     // NFR-12
};

/// The HTTP server port (ADR-003). Owned here so that routes and services never see the library
/// behind it; adapters/http/httplib_server.hpp is the one implementation.
///
/// Lifecycle, all on one thread except stop(): add_route() for every route, bind() once, then
/// run(), which blocks until stop() is called from another thread.
class HttpServer {
public:
    HttpServer() = default;
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&) = delete;
    HttpServer& operator=(HttpServer&&) = delete;
    virtual ~HttpServer() = default;

    /// `path` is matched exactly, except that a segment written `{name}` — the syntax of
    /// 02-api-spec.yaml — matches any one segment, which the handler reads with
    /// path_parameter(request, "name"). No library's pattern syntax reaches this layer.
    /// Not thread-safe: call before run().
    virtual void add_route(HttpMethod method, std::string_view path, HttpHandler handler) = 0;

    /// Opens the listening socket. Port 0 asks the kernel for a free one; the result is the port
    /// actually bound, which is how tests run servers side by side.
    [[nodiscard]] virtual core::Result<std::uint16_t> bind(std::string_view host,
                                                           std::uint16_t port) = 0;

    /// Serves until stop(). Connections that arrive between bind() and run() wait in the kernel's
    /// backlog and are served once this starts.
    [[nodiscard]] virtual core::Result<void> run() = 0;

    /// Stops accepting and makes run() return. Safe to call from any thread.
    virtual void stop() noexcept = 0;
};

}  // namespace leta::application
