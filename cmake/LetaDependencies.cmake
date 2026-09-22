# Every third-party dependency Leta builds against, in one file (ADR-002, M0-T4). Each entry:
#
#   - is fetched as a release tarball and verified against a SHA-256 before extraction. A tag can
#     be moved; a hash cannot, so the build is reproducible byte for byte and needs no git;
#   - is marked SYSTEM (CPM passes it through to FetchContent), which keeps its headers out of
#     -Werror and clang-tidy. Our warnings never reach it anyway: they sit on leta_compile_options,
#     which only Leta's own targets link (cmake/LetaCompileOptions.cmake);
#   - carries its licence and the reason it exists (06-coding-standards.md §10).
#
# Included from the root after LetaInstrumentation, so the directory-wide sanitizer flags of
# M0-T3 D5 instrument the dependencies as well as Leta's own code.
#
# Updating a pin: change the tag in the URL and VERSION, download the tarball, replace the hash.
# No dependency bot understands these pins (ADR-002); the manual review happens at each release
# checkpoint and is recorded in STATE.md.
#
# Not here: the vendored tl::expected header (ADR-005). It is a file under src/core/third_party,
# not a target — a target linked into leta_core is exactly what the layering check rejects.

include_guard(GLOBAL)

# Never substitute a system copy for a pin; reproducibility over convenience.
set(CPM_USE_LOCAL_PACKAGES OFF)

# Downloads land in <build>/_deps. Setting CPM_SOURCE_CACHE in the environment (for example
# ~/.cache/CPM) shares one download and extraction across the six preset build directories; CPM
# reads the variable itself.
include(CPM)

# ---- Catch2 v3 — unit, property and integration tests (ADR-006). BSL-1.0. ----------------------
CPMAddPackage(
    NAME Catch2
    VERSION 3.16.0
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.0.tar.gz
    URL_HASH SHA256=0957cae5821b17ce07f0833aaa52b5137643a8382203221f363a8303c109af34
    SYSTEM YES
    OPTIONS
        "CATCH_INSTALL_DOCS OFF"
        "CATCH_INSTALL_EXTRAS OFF")

# catch_discover_tests() lives in the source tree's extras/, which is not on the module path.
list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")

# ---- Google Benchmark — the NFR-01/02/03/11 numbers (ADR-006). Apache-2.0. --------------------
# Opt-in (LETA_BUILD_BENCHMARKS): it is a compiled library that only tests/bench needs, and the
# everyday presets should not pay for it.
if(LETA_BUILD_BENCHMARKS)
    CPMAddPackage(
        NAME benchmark
        VERSION 1.9.5
        URL https://github.com/google/benchmark/archive/refs/tags/v1.9.5.tar.gz
        URL_HASH SHA256=9631341c82bac4a288bef951f8b26b41f69021794184ece969f8473977eaa340
        SYSTEM YES
        OPTIONS
            "BENCHMARK_ENABLE_TESTING OFF"
            "BENCHMARK_ENABLE_GTEST_TESTS OFF"
            "BENCHMARK_ENABLE_INSTALL OFF"
            "BENCHMARK_INSTALL_DOCS OFF"
            # Their -Werror on a compiler newer than they tested is their build breaking ours.
            "BENCHMARK_ENABLE_WERROR OFF")
endif()
