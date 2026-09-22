#include "adapters/config/build_info.hpp"

#include <benchmark/benchmark.h>

namespace {
// Smoke benchmark (M0-T4): exists to prove leta-bench links and runs. It measures a std::string
// build of ~60 bytes, which is not a number anyone should quote; the first benchmark that matters
// is resident memory at M3 (NFR-04).
void version_line_of_build_info(benchmark::State& state) {
    const leta::config::BuildInfo info = leta::config::build_info();
    for ([[maybe_unused]] auto iteration : state) {
        benchmark::DoNotOptimize(leta::config::version_line(info));
    }
}

BENCHMARK(version_line_of_build_info);

}  // namespace

BENCHMARK_MAIN();
