#pragma once

#include <string>
#include <string_view>

#include "application/http_server.hpp"
#include "application/index_service.hpp"
#include "core/index_metadata.hpp"
#include "core/result.hpp"

namespace leta::http {

/// Reads a POST /indexes body: `{"uid": string, "primaryKey": string | null}`, `uid` required.
/// Unknown fields fail with UnknownField naming the field (FR-72); a uid outside FR-01's alphabet
/// with InvalidIndexUid; anything else that is not exactly that object with MalformedRequest.
/// Public so that it can be fuzzed directly (NFR-07).
[[nodiscard]] core::Result<application::CreateIndexCommand> parse_create_index_request(
    std::string_view body);

/// The Index object of 02-api-spec.yaml.
[[nodiscard]] std::string index_json(const core::IndexMetadata& index);

/// The whole of POST /indexes once the transport has checked the Content-Type: 201 with the new
/// Index, or the FR-71 error.
[[nodiscard]] application::HttpResponse create_index_response(application::IndexService& service,
                                                              std::string_view body);

/// Registers POST /indexes. `service` must outlive `server`: the route holds a reference to it.
void register_index_routes(application::HttpServer& server, application::IndexService& service);

}  // namespace leta::http
