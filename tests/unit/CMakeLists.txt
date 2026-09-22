# leta_tests — the one Catch2 executable for unit and property tests (ADR-006). The tree under
# tests/unit/ mirrors src/: the test for src/adapters/config/build_info.cpp is
# tests/unit/adapters/config/build_info_test.cpp. Sources are listed, never globbed, so adding a
# test is a visible edit here.
#
# Every TEST_CASE carries its requirement ID as a tag (NFR-14):
#
#   ./build/debug/bin/leta_tests "[FR-68]"      # everything covering one requirement
#   ./build/debug/bin/leta_tests --list-tags    # what the coverage check (M0-T6) reads

add_executable(leta_tests
    adapters/config/build_info_test.cpp)
target_link_libraries(leta_tests PRIVATE leta_adapters Catch2::Catch2WithMain)
leta_enable_diagnostics(leta_tests)

# One ctest entry per TEST_CASE, discovered by running the binary once after it links, so a failing
# case appears by name in ctest output instead of as one opaque "leta_tests" failure.
include(Catch)
catch_discover_tests(leta_tests TEST_PREFIX "unit.")
