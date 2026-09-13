# cmake/LetaCompileOptions.cmake
#
# Warnings and language standard for Leta's own compiled targets (M0-T3 D4). They sit on an
# INTERFACE target that every compiled leta_* target links PRIVATE, so nothing here propagates to a
# consumer or to a CPM-built dependency; -Werror on somebody else's code is how builds break on a
# new compiler release.
#
# Adding a flag is a `build:` commit with a one-line reason. The first five flags are mandated by
# 06-coding-standards.md §1; the rest are the additions accepted in M0-T3.

include_guard(GLOBAL)

add_library(leta_compile_options INTERFACE)
target_compile_features(leta_compile_options INTERFACE cxx_std_20)

set(_leta_warnings
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    # GCC's -Wconversion does not imply this one in C++; Clang's does. Explicit keeps both equal.
    -Wsign-conversion
    -Wold-style-cast
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wimplicit-fallthrough
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wcast-align)

set(_leta_gnu_only_warnings
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op)

target_compile_options(leta_compile_options INTERFACE
    ${_leta_warnings}
    "$<$<CXX_COMPILER_ID:GNU>:${_leta_gnu_only_warnings}>"
    "$<$<BOOL:${LETA_WERROR}>:-Werror>")

if(LETA_CLANG_TIDY)
    find_program(LETA_CLANG_TIDY_EXECUTABLE NAMES clang-tidy clang-tidy-18 clang-tidy-17)
    if(NOT LETA_CLANG_TIDY_EXECUTABLE)
        message(FATAL_ERROR "LETA_CLANG_TIDY=ON but no clang-tidy executable was found on PATH.")
    endif()
endif()

# Every compiled project target calls this once. It is what makes "project target" mean the same
# thing everywhere: our warnings, and clang-tidy when asked for. Dependencies never see either.
function(leta_enable_diagnostics target)
    target_link_libraries(${target} PRIVATE leta_compile_options)
    if(LETA_CLANG_TIDY)
        # The target property rather than CMAKE_CXX_CLANG_TIDY: the global variable would also
        # lint every dependency's sources. Configuration comes from .clang-tidy at the repo root.
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${LETA_CLANG_TIDY_EXECUTABLE}")
    endif()
endfunction()
