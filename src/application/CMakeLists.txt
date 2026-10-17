# src/application/CMakeLists.txt
#
# leta_application — services and the port interfaces adapters implement (ADR-001). STATIC since
# M1-T1, when health_service.cpp became its first compiled code (D6).

add_library(leta_application STATIC
    clock.hpp
    health_service.hpp
    health_service.cpp
    http_server.hpp
    index_repository.hpp
    index_service.hpp
    index_service.cpp)
target_link_libraries(leta_application PUBLIC leta_core)
leta_enable_diagnostics(leta_application)
