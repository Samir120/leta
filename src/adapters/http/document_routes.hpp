#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <chrono>
#include <cstddef>

#include "application/http_server.hpp"
#include "application/ingest_service.hpp"
#include "core/document.hpp"
#include "core/result.hpp"

namespace leta::http {

/// Reads a POST /indexes/{uid}/documents body: a JSON array of 1 to 10 000 objects (FR-14,
/// 02-api-spec.yaml), each valid JSON throughout and nested at most 128 levels. Each object keeps
/// the exact bytes the client sent as its source (ADR-004); its top-level string values become its
/// searchable text, the M1 stand-in for searchable attributes. Anything else fails with
/// MalformedRequest. Public so that it can be fuzzed directly (NFR-07).
[[nodiscard]] core::Result<std::vector<core::Document>> parse_document_batch(std::string_view body);

/// The IngestResult object of 02-api-spec.yaml.
[[nodiscard]] std::string ingest_result_json(std::size_t received,
                                             const application::AddDocumentsOutcome& outcome,
                                             std::chrono::milliseconds processing_time);

/// The whole of POST /indexes/{uid}/documents once the transport has checked the Content-Type: 200
/// with the IngestResult, or the FR-71 error. `uid` is the path segment as received.
[[nodiscard]] application::HttpResponse add_documents_response(application::IngestService& service,
                                                               std::string_view uid,
                                                               std::string_view body);

/// Registers POST /indexes/{uid}/documents. `service` must outlive `server`: the route holds a
/// reference to it.
void register_document_routes(application::HttpServer& server, application::IngestService& service);

}  // namespace leta::http
