# cmake/LetaInstrumentation.cmake
#
# Directory-wide instrumentation selected by LETA_INSTRUMENTATION (M0-T3 D5). Applied at directory
# scope before any target or dependency exists, so that sanitizers instrument the dependencies as
# well as Leta's own code; ASan and TSan would otherwise be blind in exactly the boundary code that
# talks to them. Warnings are deliberately NOT here: they live on leta_compile_options and never
# reach a dependency.
#
# Instrumented modes keep CMAKE_BUILD_TYPE=Debug so assertions stay on (RelWithDebInfo would define
# NDEBUG) and add -O1, the level the sanitizer projects recommend.

include_guard(GLOBAL)

set(_leta_instrumentation_modes none asan-ubsan tsan coverage fuzz)
if(NOT LETA_INSTRUMENTATION IN_LIST _leta_instrumentation_modes)
    message(FATAL_ERROR
        "LETA_INSTRUMENTATION='${LETA_INSTRUMENTATION}' is not one of: ${_leta_instrumentation_modes}")
endif()

# libstdc++ precondition checks (bounds, iterator validity) in every Debug build, instrumented or
# not. Unlike _GLIBCXX_DEBUG this does not change the ABI, so it is safe to mix with dependencies.
add_compile_definitions($<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)

if(LETA_INSTRUMENTATION STREQUAL "none")
    return()
endif()

function(_leta_require_clang mode reason)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR
            "LETA_INSTRUMENTATION=${mode} requires Clang (${reason}); the compiler is "
            "${CMAKE_CXX_COMPILER_ID}. Configure with CC=clang CXX=clang++.")
    endif()
endfunction()

# Every instrumented mode: keep frame pointers so sanitizer stack traces are readable.
add_compile_options(-fno-omit-frame-pointer)

if(LETA_INSTRUMENTATION STREQUAL "asan-ubsan")
    add_compile_options(-O1 -fsanitize=address,undefined -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)

elseif(LETA_INSTRUMENTATION STREQUAL "tsan")
    add_compile_options(-O1 -fsanitize=thread)
    add_link_options(-fsanitize=thread)

elseif(LETA_INSTRUMENTATION STREQUAL "coverage")
    _leta_require_clang(coverage "llvm-cov, 05-quality-strategy.md §4")
    # -O0 (Debug default) so that line attribution is exact.
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)

elseif(LETA_INSTRUMENTATION STREQUAL "fuzz")
    _leta_require_clang(fuzz "libFuzzer")
    # fuzzer-no-link instruments everything without supplying main(). Each fuzz executable adds
    # -fsanitize=fuzzer itself when the targets exist (M2 onward, NFR-07).
    add_compile_options(-O1 -fsanitize=address,undefined,fuzzer-no-link -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
endif()
