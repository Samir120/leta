# leta_bench — the Google Benchmark executable behind LETA_BUILD_BENCHMARKS (ADR-006). The tree
# mirrors src/ like tests/unit/. Real benchmarks arrive with the code they measure (memory at M3,
# the latency budget at M11); the one registered today is a smoke benchmark that proves the wiring.
#
# It is not a ctest test — benchmarks measure, they do not assert — except for the smoke entry
# below, which runs each benchmark for a single iteration so a broken link or a crash on startup
# fails the build's test step rather than being discovered at M11.

add_executable(leta_bench
    adapters/config/build_info_bench.cpp)
target_link_libraries(leta_bench PRIVATE leta_adapters benchmark::benchmark)
leta_enable_diagnostics(leta_bench)

add_test(NAME bench.smoke COMMAND leta_bench --benchmark_min_time=1x)
