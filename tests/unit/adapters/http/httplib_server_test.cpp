#include "adapters/http/httplib_server.hpp"

#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "application/http_server.hpp"

namespace {

using leta::application::HttpMethod;
using leta::application::HttpRequest;
using leta::application::HttpResponse;
using leta::application::HttpServerOptions;
using leta::application::path_parameter;
using leta::http::HttplibServer;

// Answers with its `word` parameter and, as a check, one the route never declared.
HttpResponse echo_parameters(const HttpRequest& request) {
    std::string body{R"({"word":")"};
    body.append(path_parameter(request, "word"))
        .append(R"(","undeclared":")")
        .append(path_parameter(request, "undeclared"))
        .append(R"("})");
    return {.status = 200, .body = std::move(body)};
}

std::string get(HttplibServer& server, std::string_view path) {
    std::string request{"GET "};
    request.append(path).append(" HTTP/1.1\r\nHost: leta\r\n\r\n");
    return server.serve_raw(request);
}

}  // namespace

TEST_CASE("a {name} segment matches one path segment and reaches the handler by name",
          "[ADR-003]") {
    HttplibServer server{HttpServerOptions{}};
    server.add_route(HttpMethod::Get, "/echo/{word}/tail", echo_parameters);
    const std::string response = get(server, "/echo/hello/tail");
    CHECK(response.starts_with("HTTP/1.1 200 OK\r\n"));
    CHECK(response.ends_with(R"({"word":"hello","undeclared":""})"));
}

TEST_CASE("a path with a segment too many or too few matches no route", "[FR-71][FR-73]") {
    HttplibServer server{HttpServerOptions{}};
    server.add_route(HttpMethod::Get, "/echo/{word}/tail", echo_parameters);
    for (const std::string_view path : {"/echo/hello/tail/more", "/echo/tail"}) {
        CAPTURE(path);
        const std::string response = get(server, path);
        CHECK(response.starts_with("HTTP/1.1 404 Not Found\r\n"));
        CHECK(response.find("X-Request-Id: ") != std::string::npos);
        CHECK(response.ends_with(
            R"({"code":"route_not_found","message":"No route matches this method and path.","type":"not_found"})"));
    }
}
