# CMakeLists.txt
#
# Root of the Leta build. Configure only through a preset (CMakePresets.json):
#
#   cmake --workflow --preset debug      # configure + build + test
#
# Everything a target needs is decided in the cmake/ modules included below; the per-layer
# CMakeLists.txt under src/ only declare targets and sources. See ADR-002 and M0-T3.

cmake_minimum_required(VERSION 3.25)

project(leta
    VERSION 0.1.0
    DESCRIPTION "Small, fast, self-hosted product search server"
    HOMEPAGE_URL "https://github.com/Samir120/leta"
    LANGUAGES CXX)

# Pre-release suffix appended to PROJECT_VERSION (D7). The release commit for v0.1.0 sets this
# to "" and is tagged; the commit after it bumps VERSION above and restores "-dev".
set(LETA_VERSION_SUFFIX "-dev")

# A compiler choice arrives through CC/CXX (D2) and is read only when a build directory is first
# configured. Re-running a preset with a different CXX keeps the cached compiler silently, so a
# "Clang" run can build with GCC and report green. Refuse instead.
if(DEFINED ENV{CXX})
    find_program(_leta_requested_cxx "$ENV{CXX}" NO_CACHE)
    if(_leta_requested_cxx)
        file(REAL_PATH "${_leta_requested_cxx}" _leta_requested_cxx)
        file(REAL_PATH "${CMAKE_CXX_COMPILER}" _leta_cached_cxx)
        if(NOT _leta_requested_cxx STREQUAL _leta_cached_cxx)
            message(FATAL_ERROR
                "CXX=$ENV{CXX}, but ${CMAKE_BINARY_DIR} was configured with ${CMAKE_CXX_COMPILER}. "
                "Delete that directory, or use a preset with its own binaryDir "
                "(CMakeUserPresets.json).")
        endif()
    endif()
endif()

if(NOT CMAKE_BUILD_TYPE)
    message(FATAL_ERROR
        "CMAKE_BUILD_TYPE is unset. Configure through a preset, e.g. `cmake --preset debug` "
        "(see CMakePresets.json for the six canonical presets).")
endif()

# ---- Options ---------------------------------------------------------------------------------

set(LETA_INSTRUMENTATION "none" CACHE STRING
    "Global instrumentation, mirrors the preset names: none, asan-ubsan, tsan, coverage, fuzz")
set_property(CACHE LETA_INSTRUMENTATION PROPERTY STRINGS none asan-ubsan tsan coverage fuzz)

option(LETA_WERROR "Treat warnings as errors on project targets (NFR-09)" ON)
option(LETA_CLANG_TIDY "Run clang-tidy on project targets as part of the build" OFF)
option(LETA_BUILD_BENCHMARKS "Fetch Google Benchmark and build tests/bench (ADR-006)" OFF)

# ---- Global settings -------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Order matters: instrumentation applies directory-wide flags and must precede every target and
# every dependency, so that sanitizers instrument dependencies too.
include(LetaInstrumentation)
include(LetaCompileOptions)
include(LetaBuildInfo)
include(LetaLayeringCheck)

# Third-party code is fetched and configured here, before any Leta target exists, so a layer's
# CMakeLists.txt only ever names targets that are already defined (cmake/LetaDependencies.cmake).
include(LetaDependencies)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)

# Runs after this directory and every subdirectory have been processed, so it sees the final
# link graph (ADR-001).
cmake_language(DEFER CALL leta_check_layering)
