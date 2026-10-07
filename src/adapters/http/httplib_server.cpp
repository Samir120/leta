#include "adapters/http/httplib_server.hpp"

#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>

#include <httplib.h>

#include "adapters/http/error_response.hpp"
#include "adapters/http/request_id.hpp"
#include "application/http_server.hpp"
#include "core/error.hpp"
#include "core/result.hpp"

namespace leta::http {

namespace {

using application::HttpHandler;
using application::HttpMethod;
using application::HttpRequest;
using application::HttpResponse;
using application::HttpServerOptions;

constexpr const char* JsonContentType = "application/json";

void write_response(httplib::Response& out, const HttpResponse& response) {
    out.status = response.status;
    out.set_content(response.body, JsonContentType);
}

// The inbound id when it is safe to echo, a fresh one otherwise (FR-73).
std::string resolve_request_id(const httplib::Request& request) {
    const std::string inbound = request.get_header_value(RequestIdHeader);
    return is_acceptable_request_id(inbound) ? inbound : generate_request_id();
}

// The id is set before the handler runs, so a handler that throws still leaves it in place for
// the exception hook to log and for the client to see.
httplib::Server::Handler adapt(HttpMethod method, HttpHandler handler) {
    return [method, handler = std::move(handler)](const httplib::Request& request,
                                                  httplib::Response& response) {
        const std::string request_id = resolve_request_id(request);
        response.set_header(RequestIdHeader, request_id);
        const HttpRequest neutral{.method = method,
                                  .path = request.path,
                                  .body = request.body,
                                  .request_id = request_id};
        write_response(response, handler(neutral));
    };
}

// Every response with status >= 400 passes through here, including those our routes wrote; they
// already carry an FR-71 body and are left alone. What remains is a status cpp-httplib set on its
// own (404, 413, 400 …), which gets its FR-71 body here.
httplib::Server::HandlerResponse fill_library_error(const httplib::Request& /*request*/,
                                                    httplib::Response& response) {
    if (!response.body.empty()) {
        return httplib::Server::HandlerResponse::Unhandled;
    }
    write_response(response, error_response(error_for_library_status(response.status)));
    return httplib::Server::HandlerResponse::Handled;
}

std::string describe(const std::exception_ptr& exception) {
    try {
        std::rethrow_exception(exception);
    } catch (const std::exception& error) {
        return error.what();
    } catch (...) {
        return "an exception not derived from std::exception";
    }
}

// A handler threw. The detail goes to the server log, correlated by request id; the client gets
// only `internal` (06-coding-standards.md §5). stderr until structured logging lands (M1-T4).
void report_handler_exception(const httplib::Request& /*request*/,
                              httplib::Response& response,
                              const std::exception_ptr& exception) {
    std::string line{"leta: unhandled exception in request "};
    line.append(response.get_header_value(RequestIdHeader))
        .append(": ")
        .append(describe(exception))
        .append("\n");
    static_cast<void>(std::fputs(line.c_str(), stderr));
    write_response(response,
                   error_response(core::Error{.code = core::ErrorCode::Internal,
                                              .message = "Internal error."}));
}

// Every response leaves through here, including those cpp-httplib writes before any route runs (a
// malformed request line, an oversized body): the one place FR-73 can be guaranteed.
void ensure_request_id(const httplib::Request& request, httplib::Response& response) {
    if (!response.has_header(RequestIdHeader)) {
        response.set_header(RequestIdHeader, resolve_request_id(request));
    }
}

// ADR-003 follow-up, verified against 0.59.0: the default queue is a dynamic pool (base
// max(8, cores - 1), growing to 4x under load, unbounded job queue). Base == max pins the fixed
// pool ADR-003 sized; max_queued_requests stays 0 (unbounded), so a connection beyond the pool
// waits for a worker instead of being refused.
void configure_worker_pool(httplib::Server& server, std::size_t worker_threads) {
    assert(worker_threads > 0 && "HttpServerOptions::worker_threads must be at least 1");
    server.new_task_queue = [worker_threads] {
        // cpp-httplib takes ownership of the pool it is handed; its API leaves no other way.
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        return new httplib::ThreadPool(worker_threads, worker_threads);
    };
}

void configure_limits(httplib::Server& server, const HttpServerOptions& options) {
    server.set_payload_max_length(options.max_body_bytes);
    server.set_read_timeout(static_cast<std::time_t>(options.read_timeout.count()));
    server.set_write_timeout(static_cast<std::time_t>(options.write_timeout.count()));
}

void install_hooks(httplib::Server& server) {
    server.set_error_handler(fill_library_error);
    server.set_exception_handler(report_handler_exception);
    server.set_post_routing_handler(ensure_request_id);
}

std::string cannot_listen_message(std::string_view host, std::uint16_t port) {
    std::string message{"Cannot listen on "};
    message.append(host).append(":").append(std::to_string(port)).append(".");
    return message;
}

}  // namespace

struct HttplibServer::Impl {
    httplib::Server server;
};

HttplibServer::HttplibServer(const HttpServerOptions& options)
    : impl_{std::make_unique<Impl>()} {
    configure_worker_pool(impl_->server, options.worker_threads);
    configure_limits(impl_->server, options);
    install_hooks(impl_->server);
}

HttplibServer::~HttplibServer() = default;

void HttplibServer::add_route(HttpMethod method, std::string_view path, HttpHandler handler) {
    const std::string pattern{path};
    httplib::Server::Handler adapted = adapt(method, std::move(handler));
    switch (method) {
        case HttpMethod::Get:
            impl_->server.Get(pattern, std::move(adapted));
            return;
        case HttpMethod::Post:
            impl_->server.Post(pattern, std::move(adapted));
            return;
    }
}

core::Result<std::uint16_t> HttplibServer::bind(std::string_view host, std::uint16_t port) {
    const std::string host_name{host};
    if (port == 0) {
        const int bound_port = impl_->server.bind_to_any_port(host_name);
        if (bound_port < 0) {
            return core::failure(core::ErrorCode::Internal, cannot_listen_message(host, port));
        }
        return static_cast<std::uint16_t>(bound_port);
    }
    if (!impl_->server.bind_to_port(host_name, port)) {
        return core::failure(core::ErrorCode::Internal, cannot_listen_message(host, port));
    }
    return port;
}

core::Result<void> HttplibServer::run() {
    if (!impl_->server.listen_after_bind()) {
        return core::failure(core::ErrorCode::Internal,
                             "The HTTP listener failed while accepting connections.");
    }
    return {};
}

void HttplibServer::stop() noexcept {
    impl_->server.stop();
}

}  // namespace leta::http
