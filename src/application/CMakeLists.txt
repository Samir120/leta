# src/application/CMakeLists.txt
#
# leta_application — services and the port interfaces adapters implement (ADR-001).
#
# INTERFACE until the first .cpp lands (M1). Promotion: `add_library(leta_application STATIC
# <sources>)`, INTERFACE -> PUBLIC below, plus `leta_enable_diagnostics(leta_application)`.

add_library(leta_application INTERFACE)
target_link_libraries(leta_application INTERFACE leta_core)
