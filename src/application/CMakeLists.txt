# src/application/CMakeLists.txt
#
# leta_application — services and the port interfaces adapters implement (ADR-001). STATIC since
# M1-T1, when health_service.cpp became its first compiled code (D6).

add_library(leta_application STATIC
    health_service.hpp
    health_service.cpp
    http_server.hpp)
target_link_libraries(leta_application PUBLIC leta_core)
leta_enable_diagnostics(leta_application)
