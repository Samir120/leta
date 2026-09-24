# One subdirectory per test layer of 05-quality-strategy.md §2, each its own executable, added
# when the milestone that needs it arrives: unit since M0-T4; bench behind LETA_BUILD_BENCHMARKS;
# integration, contract, recovery, relevance and fuzz from M1 onward.
#
# Also here, because both are about the tests rather than any one layer of them, the two halves of
# NFR-14 (M0-T6): the requirement-coverage check, a ctest entry that runs in every preset, and the
# coverage-report target, which exists in the coverage preset only.
#
# The two entries below are M0-T3's process-level FR-68 checks, run through ctest alone with no
# framework. tests/unit/ pins version_line() the function; these pin `leta --version` the process.

add_test(NAME FR-68.version_line COMMAND leta --version)
set_tests_properties(FR-68.version_line PROPERTIES
    PASS_REGULAR_EXPRESSION
        "^leta [0-9]+\\.[0-9]+\\.[0-9]+(-[0-9A-Za-z.]+)? \\(([0-9a-f]+(-dirty)?|unknown), [A-Za-z]+, [A-Za-z]+ [0-9.]+\\)\n$")

# Exit status 2 and a usage line for any other invocation (A6). WILL_FAIL would accept any
# non-zero status; the shell wrapper pins the exact one. Linux only, like the rest of the project.
add_test(NAME FR-68.usage_exit_code
    COMMAND sh -c "\"$1\"; test $? -eq 2" leta-usage "$<TARGET_FILE:leta>")
set_tests_properties(FR-68.usage_exit_code PROPERTIES
    PASS_REGULAR_EXPRESSION "usage: leta --version")

add_subdirectory(unit)

if(LETA_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()

# ---- NFR-14, requirement coverage ---------------------------------------------------------------
# Every Must FR has a test tagged with its ID, or a line in untested-requirements.txt
# (scripts/check_requirement_coverage.cmake). A ctest entry rather than a CI job, so it runs in
# every preset and nothing about it is CI-only. Each Catch2 executable joins
# LETA_TEST_EXECUTABLES as it arrives (integration and contract from M1), '|'-separated.

add_test(NAME NFR-14.requirement_coverage
    COMMAND "${CMAKE_COMMAND}"
        "-DLETA_REQUIREMENTS=${PROJECT_SOURCE_DIR}/docs/01-requirements.md"
        "-DLETA_UNTESTED=${CMAKE_CURRENT_SOURCE_DIR}/untested-requirements.txt"
        "-DLETA_TEST_EXECUTABLES=$<TARGET_FILE:leta_tests>"
        -P "${PROJECT_SOURCE_DIR}/scripts/check_requirement_coverage.cmake")

# ---- NFR-14, line coverage (coverage preset only) -----------------------------------------------
# `cmake --build --preset coverage --target coverage-report`, after `ctest --preset coverage`.
# Deliberately not a dependency of anything: it reads the profiles the last test run wrote, and
# rebuilding a binary first would make those profiles stale.

if(LETA_INSTRUMENTATION STREQUAL "coverage")
    # The raw profile format belongs to one LLVM major version. Search the compiler's own directory
    # first (clang++-17 resolves to /usr/lib/llvm-17/bin on Ubuntu), then PATH, trying the versioned
    # name before the plain one in each directory. Not found does not fail the configure: the
    # preset still builds and tests, and the report target says what to install.
    file(REAL_PATH "${CMAKE_CXX_COMPILER}" _leta_real_cxx)
    cmake_path(GET _leta_real_cxx PARENT_PATH _leta_llvm_bin)
    string(REGEX MATCH "^[0-9]+" _leta_llvm_major "${CMAKE_CXX_COMPILER_VERSION}")
    find_program(LETA_LLVM_PROFDATA NAMES "llvm-profdata-${_leta_llvm_major}" llvm-profdata
        NAMES_PER_DIR HINTS "${_leta_llvm_bin}")
    find_program(LETA_LLVM_COV NAMES "llvm-cov-${_leta_llvm_major}" llvm-cov
        NAMES_PER_DIR HINTS "${_leta_llvm_bin}")

    # LETA_PROFILE_DIR must match LLVM_PROFILE_FILE in the coverage test preset (CMakePresets.json).
    # The floor is NFR-14's; raising it is a requirement change, not an edit here.
    add_custom_target(coverage-report
        COMMAND "${CMAKE_COMMAND}"
            "-DLETA_LLVM_PROFDATA=${LETA_LLVM_PROFDATA}"
            "-DLETA_LLVM_COV=${LETA_LLVM_COV}"
            "-DLETA_PROFILE_DIR=${PROJECT_BINARY_DIR}/profiles"
            "-DLETA_OBJECTS=$<TARGET_FILE:leta_tests>|$<TARGET_FILE:leta>"
            "-DLETA_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DLETA_OUTPUT_DIR=${PROJECT_BINARY_DIR}/coverage-report"
            -DLETA_CORE_MIN_LINE_PERCENT=80
            -P "${PROJECT_SOURCE_DIR}/scripts/coverage_report.cmake"
        COMMENT "Coverage report and the 80 % leta_core line floor (NFR-14)"
        USES_TERMINAL
        VERBATIM)
endif()
