#include <exception>
#include <string>
#include <string_view>

#include <cstdint>
#include <cstdio>

#include "adapters/config/build_info.hpp"
#include "adapters/http/httplib_server.hpp"
#include "adapters/http/system_routes.hpp"
#include "application/health_service.hpp"
#include "application/http_server.hpp"

namespace {

constexpr std::string_view VersionFlag = "--version";

// Fixed until FR-63 (M9) brings LETA_* configuration (A6, amended in M1-T1). All interfaces,
// because the container publishes the port; 7700 is the port the API spec, Dockerfile and README
// name.
constexpr std::string_view ListenHost = "0.0.0.0";
constexpr std::uint16_t ListenPort = 7700;

void print_line(std::string_view text) {
    std::string line{"leta: "};
    line.append(text).append("\n");
    // Nothing useful to do if writing to stderr fails; the exit status is the contract.
    static_cast<void>(std::fputs(line.c_str(), stderr));
}

int serve() {
    const leta::application::HttpServerOptions options{};
    leta::application::HealthService health;
    leta::http::HttplibServer server{options};
    leta::http::register_system_routes(server, health);

    const auto bound_port = server.bind(ListenHost, ListenPort);
    if (!bound_port) {
        print_line(bound_port.error().message);
        return 1;
    }
    // Nothing to restore until M7, so the server is ready as soon as it can accept (FR-60).
    health.mark_available();

    std::string listening{"listening on "};
    listening.append(ListenHost)
        .append(":")
        .append(std::to_string(*bound_port))
        .append(" (")
        .append(std::to_string(options.worker_threads))
        .append(" worker threads)");
    print_line(listening);

    const auto stopped = server.run();
    if (!stopped) {
        print_line(stopped.error().message);
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    // A6: no arguments serves; --version identifies the binary (FR-68); anything else is a usage
    // error. Flags and environment proper arrive with FR-63 in M9.
    if (argc == 1) {
        try {
            return serve();
        } catch (const std::exception& error) {
            // Unexpected and unrecoverable at startup, e.g. the worker pool could not be created
            // (06-coding-standards.md §5). main is the boundary that catches it.
            print_line(error.what());
            return 1;
        }
    }
    if (argc == 2 && std::string_view{argv[1]} == VersionFlag) {
        std::puts(leta::config::version_line(leta::config::build_info()).c_str());
        return 0;
    }
    static_cast<void>(std::fputs("usage: leta [--version]\n", stderr));
    return 2;
}
// This deliberately long comment exists only to break the format check, and it runs well past one hundred columns.
