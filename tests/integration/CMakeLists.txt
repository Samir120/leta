# leta_integration_tests — real HTTP against a real server on an ephemeral loopback port
# (05-quality-strategy.md §2). It proves the wiring; logic belongs in tests/unit. One file per
# endpoint group; the server runs in-process on its own thread.

add_executable(leta_integration_tests
    http_server_test.cpp)
target_link_libraries(leta_integration_tests PRIVATE
    leta_adapters
    httplib::httplib  # for httplib::Client only; the server is reached through the port
    Catch2::Catch2WithMain)
leta_enable_diagnostics(leta_integration_tests)

# Each test is expected to finish well under 5 s (05 §2). The timeout is the backstop for a server
# that never starts, which would otherwise hang ctest for its 1500 s default.
include(Catch)
catch_discover_tests(leta_integration_tests
    TEST_PREFIX "integration."
    PROPERTIES TIMEOUT 30)
