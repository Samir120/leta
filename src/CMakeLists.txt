# src/CMakeLists.txt
#
# One subdirectory per layer, inner to outer (ADR-001). Each declares exactly one target and its
# sources; flags, checks and generated values come from cmake/.

add_subdirectory(core)
add_subdirectory(application)
add_subdirectory(adapters)

add_executable(leta main.cpp)
target_link_libraries(leta PRIVATE leta_adapters)
leta_enable_diagnostics(leta)
