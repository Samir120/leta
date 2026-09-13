# src/core/CMakeLists.txt
#
# leta_core — the pure domain (ADR-001). Links the standard library and nothing else; the configure
# fails if anything else appears here (cmake/LetaLayeringCheck.cmake).
#
# INTERFACE until the first .cpp lands (M1/M2). Promotion is then three edits in this file:
# `add_library(leta_core STATIC <sources>)`, the INTERFACE keywords below become PUBLIC, and
# `leta_enable_diagnostics(leta_core)` is added.

add_library(leta_core INTERFACE)

# The include root for the whole tree. Project includes are written from src/, so the layer is
# visible on every include line: "core/result.hpp", "adapters/config/build_info.hpp" (A5).
target_include_directories(leta_core INTERFACE "${PROJECT_SOURCE_DIR}/src")

# The ADR-005 exception: vendored, copied-in headers (tl::expected), included as <tl/expected.hpp>.
# SYSTEM keeps them out of -Werror and out of clang-tidy (M0-T3 D9). The layering check permits
# this directory and no other as a SYSTEM include.
target_include_directories(leta_core SYSTEM INTERFACE "${PROJECT_SOURCE_DIR}/src/core/third_party")
