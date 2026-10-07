# src/adapters/CMakeLists.txt
#
# leta_adapters — HTTP, storage, config, metrics, logging (ADR-001). STATIC from the start because
# build info is real, compiled code (M0-T3 D6).

# Values come from cmake/LetaBuildInfo.cmake. The generated file is never edited or committed.
configure_file(
    config/build_info_generated.cpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/config/build_info_generated.cpp"
    @ONLY)

add_library(leta_adapters STATIC
    config/build_info.hpp
    config/build_info.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/config/build_info_generated.cpp"
    http/error_response.hpp
    http/error_response.cpp
    http/httplib_server.hpp
    http/httplib_server.cpp
    http/json_string.hpp
    http/json_string.cpp
    http/request_id.hpp
    http/request_id.cpp
    http/system_routes.hpp
    http/system_routes.cpp)
target_link_libraries(leta_adapters
    PUBLIC leta_application
    # PRIVATE: only httplib_server.cpp includes httplib.h. Nothing that includes an adapters header
    # sees an httplib type (ADR-003); the integration tests link it themselves for the client.
    PRIVATE httplib::httplib)
leta_enable_diagnostics(leta_adapters)
