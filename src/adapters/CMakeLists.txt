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
    "${CMAKE_CURRENT_BINARY_DIR}/config/build_info_generated.cpp")
target_link_libraries(leta_adapters PUBLIC leta_application)
leta_enable_diagnostics(leta_adapters)
