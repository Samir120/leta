// libFuzzer target: raw HTTP request bytes through the whole transport — cpp-httplib's parser, the
// routes, the FR-70/71/73 hooks, every handler (NFR-07, 05-quality-strategy.md §6).

#include <cstddef>
#include <cstdint>

#include "adapters/http/document_routes.hpp"
#include "adapters/http/httplib_server.hpp"
#include "adapters/http/index_routes.hpp"
#include "adapters/http/system_routes.hpp"
#include "adapters/storage/in_memory_index_repository.hpp"
#include "application/health_service.hpp"
#include "application/http_server.hpp"
#include "application/index_service.hpp"
#include "application/ingest_service.hpp"
#include "core/timestamp.hpp"
#include "fuzz/fuzz_input.hpp"

namespace {

leta::core::Timestamp epoch_clock() {
    return leta::core::Timestamp{};
}

// Everything a request can reach, wired as main wires it, in declaration order.
struct ServerUnderFuzz {
    leta::application::HealthService health;
    leta::storage::InMemoryIndexRepository indexes;
    leta::application::IndexService index_service{indexes, epoch_clock};
    leta::application::IngestService ingest_service{indexes, epoch_clock};
    leta::http::HttplibServer server{leta::application::HttpServerOptions{}};

    ServerUnderFuzz() {
        health.mark_available();
        leta::http::register_system_routes(server, health);
        leta::http::register_index_routes(server, index_service);
        leta::http::register_document_routes(server, ingest_service);
    }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    // A fresh server per input. libFuzzer assumes a run depends on its input alone: an index
    // created by an earlier input would make a crash unreproducible from its own file, and indexes
    // piling up across millions of runs would end in an out-of-memory report that is not a bug.
    ServerUnderFuzz target;
    static_cast<void>(target.server.serve_raw(leta::fuzzing::as_text(data, size)));
    return 0;
}
