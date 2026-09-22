# One subdirectory per test layer of 05-quality-strategy.md §2, each its own executable, added
# when the milestone that needs it arrives: unit since M0-T4; bench behind LETA_BUILD_BENCHMARKS;
# integration, contract, recovery, relevance and fuzz from M1 onward.
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
