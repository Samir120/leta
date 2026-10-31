// libFuzzer target: POST /indexes bodies (NFR-07, 05-quality-strategy.md §6). The property is the
// sanitizers': no crash, no leak, no undefined behaviour, whatever the bytes.

#include <cstddef>
#include <cstdint>

#include "adapters/http/index_routes.hpp"
#include "fuzz/fuzz_input.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    static_cast<void>(leta::http::parse_create_index_request(leta::fuzzing::as_text(data, size)));
    return 0;
}
