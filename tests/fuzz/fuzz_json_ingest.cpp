// libFuzzer target: POST /indexes/{uid}/documents bodies (NFR-07, 05-quality-strategy.md §6).
// Beyond the sanitizers it checks one property of its own: what the parser keeps reads back the
// same.

#include <string>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "adapters/http/document_routes.hpp"
#include "core/document.hpp"
#include "fuzz/fuzz_input.hpp"

namespace {

// A document the parser kept, wrapped as a batch of one, must come back unchanged: the same bytes
// and the same searchable text. A slice cut a byte short or long breaks it.
bool survives_round_trip(const leta::core::Document& document) {
    std::string batch{"["};
    batch.append(document.source).push_back(']');
    const auto again = leta::http::parse_document_batch(batch);
    return again && again->size() == 1 && again->front().source == document.source
           && again->front().searchable_text == document.searchable_text;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const auto documents = leta::http::parse_document_batch(leta::fuzzing::as_text(data, size));
    if (!documents) {
        return 0;
    }
    for (const leta::core::Document& document : *documents) {
        if (!survives_round_trip(document)) {
            std::abort();  // libFuzzer reports the abort as a crash and saves the input
        }
    }
    return 0;
}
