# tests/fuzz — libFuzzer targets over every boundary that reads bytes from outside the process
# (NFR-07, 05-quality-strategy.md §6). Configured in the fuzz preset only (tests/CMakeLists.txt):
# cmake/LetaInstrumentation.cmake already compiles everything there with ASan, UBSan and
# -fsanitize=fuzzer-no-link, and linking with -fsanitize=fuzzer adds libFuzzer's main().
#
# A target is <name>.cpp here plus a corpus/<name>/ directory of committed seeds. Each gets two
# ctest entries:
#   NFR-07.<name>.replay  every committed input once (-runs=0). Deterministic: the regression suite
#                         every fixed crash joins, as a file in corpus/<name>/.
#   NFR-07.<name>.smoke   LETA_FUZZ_SMOKE_SECONDS of fuzzing from those seeds, so every PR's fuzz
#                         leg fuzzes a little. New inputs and crash files land in the build tree;
#                         nothing is ever written into the source tree.
# The hour per target NFR-07 asks for is a scheduled CI job (M11).

set(LETA_FUZZ_SMOKE_SECONDS 20)

function(leta_add_fuzz_target name)
    add_executable(${name} ${name}.cpp fuzz_input.hpp)
    target_link_libraries(${name} PRIVATE leta_adapters)
    target_link_options(${name} PRIVATE -fsanitize=fuzzer)
    # Test support is included from tests/, as in the integration tests (A5): "fuzz/fuzz_input.hpp".
    target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/tests")
    leta_enable_diagnostics(${name})

    set(seeds "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${name}")
    set(findings "${CMAKE_CURRENT_BINARY_DIR}/findings/${name}")
    set(artifacts "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${name}/")
    file(MAKE_DIRECTORY "${findings}" "${artifacts}")

    add_test(NAME NFR-07.${name}.replay
        COMMAND ${name} -runs=0 "-artifact_prefix=${artifacts}" "${seeds}")
    # libFuzzer adds what it discovers to the FIRST corpus directory it is given: the build tree's.
    add_test(NAME NFR-07.${name}.smoke
        COMMAND ${name} -max_total_time=${LETA_FUZZ_SMOKE_SECONDS} -timeout=10
            "-artifact_prefix=${artifacts}" "${findings}" "${seeds}")
    # The backstop for a hang outside libFuzzer's own per-input timeout.
    set_tests_properties(NFR-07.${name}.smoke PROPERTIES TIMEOUT 120)
endfunction()

leta_add_fuzz_target(fuzz_http_request)
leta_add_fuzz_target(fuzz_json_ingest)
leta_add_fuzz_target(fuzz_create_index)
