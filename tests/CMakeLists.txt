# M0-T3: the executable's observable FR-68 behaviour, checked with ctest alone. Catch2 unit tests
# arrive in M0-T4 (tests/unit/). The requirement-coverage script (M0-T6) reads Catch2 tags, so T4
# also adds a [FR-68] test on version_line(); the two tests below pin the process, not the function.

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
