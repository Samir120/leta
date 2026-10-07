#pragma once

#include <memory>
#include <string_view>

#include <cstdint>

#include "application/http_server.hpp"
#include "core/result.hpp"

namespace leta::http {

/// The HttpServer port on cpp-httplib (ADR-003). The library stays behind Impl: nothing that
/// includes this header sees an httplib type, and httplib.h is compiled into one translation unit
/// of the server.
///
/// Every response it writes carries X-Request-Id (FR-73); every error it writes, including the
/// ones the library raises before any route runs, carries an FR-71 body.
class HttplibServer final : public application::HttpServer {
public:
    /// Throws std::invalid_argument when options.worker_threads is 0: a pool of no threads accepts
    /// connections and never serves them. A startup misconfiguration, caught by main (06 §5).
    explicit HttplibServer(const application::HttpServerOptions& options);
    ~HttplibServer() override;
    HttplibServer(const HttplibServer&) = delete;
    HttplibServer& operator=(const HttplibServer&) = delete;
    HttplibServer(HttplibServer&&) = delete;
    HttplibServer& operator=(HttplibServer&&) = delete;

    void add_route(application::HttpMethod method,
                   std::string_view path,
                   application::HttpHandler handler) override;
    [[nodiscard]] core::Result<std::uint16_t> bind(std::string_view host,
                                                   std::uint16_t port) override;
    [[nodiscard]] core::Result<void> run() override;
    void stop() noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace leta::http
