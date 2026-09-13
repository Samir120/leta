# cmake/LetaLayeringCheck.cmake
#
# Build-enforced layering (ADR-001), run at the end of every configure via cmake_language(DEFER)
# from the root CMakeLists.txt, so it sees the final link graph. Two independent halves:
#
#   1. Link graph — leta_core links only project targets (named leta_*), its include directories
#      all lie under src/, and its only SYSTEM include directory is src/core/third_party. That
#      directory is the ADR-005 exception: vendored, copied-in headers (tl::expected), never a
#      linked target. It is named here rather than silently allowed.
#
#   2. Include direction — dependencies point inward. A file under src/core may include only the
#      standard library (<name>, no dot, no slash), the named third-party headers (<tl/expected.hpp>),
#      and "core/...". A file under src/application may not include "adapters/...". Adapters,
#      main.cpp and tests are unchecked. The link graph cannot catch a core -> adapters include,
#      because both layers are project targets sharing one include root (A5); this half can.
#
# Every failure is a FATAL_ERROR naming the offending target, file, or include line.

include_guard(GLOBAL)

set(LETA_SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(LETA_CORE_THIRD_PARTY_DIR "${LETA_SRC_DIR}/core/third_party")
set(LETA_CORE_THIRD_PARTY_HEADERS "tl/expected.hpp")

# Unwraps the two generator expressions CMake itself adds to link and include lists. Anything else
# is left as-is and therefore fails the checks below — the safe direction for an unknown shape.
function(_leta_unwrap_genex out_var value)
    string(REGEX REPLACE "^\\$<(LINK_ONLY|BUILD_INTERFACE):(.*)>$" "\\2" value "${value}")
    set(${out_var} "${value}" PARENT_SCOPE)
endfunction()

function(_leta_check_core_link_graph)
    foreach(property IN ITEMS LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(entries leta_core ${property})
        if(NOT entries)
            continue()
        endif()
        foreach(entry IN LISTS entries)
            _leta_unwrap_genex(entry "${entry}")
            if(NOT entry MATCHES "^leta_")
                message(FATAL_ERROR
                    "ADR-001: leta_core links '${entry}' (${property}). core links the standard "
                    "library and nothing else. Vendored headers go under src/core/third_party as "
                    "files, never as targets; everything else belongs in application or adapters.")
            endif()
        endforeach()
    endforeach()

    foreach(property IN ITEMS INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(directories leta_core ${property})
        if(NOT directories)
            continue()
        endif()
        foreach(directory IN LISTS directories)
            _leta_unwrap_genex(directory "${directory}")
            cmake_path(IS_PREFIX LETA_SRC_DIR "${directory}" NORMALIZE is_under_src)
            if(NOT is_under_src)
                message(FATAL_ERROR
                    "ADR-001: leta_core has include directory '${directory}' (${property}) outside "
                    "${LETA_SRC_DIR}. core may not see third-party headers through include paths "
                    "either.")
            endif()
        endforeach()
    endforeach()

    get_target_property(system_directories leta_core INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
    if(system_directories)
        foreach(directory IN LISTS system_directories)
            _leta_unwrap_genex(directory "${directory}")
            cmake_path(COMPARE "${directory}" EQUAL "${LETA_CORE_THIRD_PARTY_DIR}" is_third_party)
            if(NOT is_third_party)
                message(FATAL_ERROR
                    "ADR-001/ADR-005: leta_core has SYSTEM include directory '${directory}'. The "
                    "only one permitted is ${LETA_CORE_THIRD_PARTY_DIR}.")
            endif()
        endforeach()
    endif()
endfunction()

function(_leta_fail_include file line why)
    file(RELATIVE_PATH relative "${PROJECT_SOURCE_DIR}" "${file}")
    message(FATAL_ERROR "ADR-001: ${relative}: ${line}\n  ${why}")
endfunction()

function(_leta_check_core_includes file)
    file(STRINGS "${file}" include_lines REGEX "^[ \t]*#[ \t]*include")
    foreach(line IN LISTS include_lines)
        if(line MATCHES "^[ \t]*#[ \t]*include[ \t]*<([^>]+)>")
            set(header "${CMAKE_MATCH_1}")
            if(NOT header MATCHES "^[a-z_]+$" AND NOT header IN_LIST LETA_CORE_THIRD_PARTY_HEADERS)
                _leta_fail_include("${file}" "${line}"
                    "core may include only the standard library (<name>) or the vendored "
                    "${LETA_CORE_THIRD_PARTY_HEADERS}.")
            endif()
        elseif(line MATCHES "^[ \t]*#[ \t]*include[ \t]*\"([^\"]+)\"")
            set(header "${CMAKE_MATCH_1}")
            if(NOT header MATCHES "^core/")
                _leta_fail_include("${file}" "${line}"
                    "core may include only \"core/...\" headers; dependencies point inward.")
            endif()
        endif()
    endforeach()
endfunction()

function(_leta_check_application_includes file)
    file(STRINGS "${file}" include_lines REGEX "^[ \t]*#[ \t]*include[ \t]*\"adapters/")
    foreach(line IN LISTS include_lines)
        _leta_fail_include("${file}" "${line}"
            "application may not include \"adapters/...\"; it owns the port interfaces that "
            "adapters implement.")
    endforeach()
endfunction()

function(_leta_check_include_direction)
    file(GLOB_RECURSE core_files CONFIGURE_DEPENDS
        "${LETA_SRC_DIR}/core/*.hpp" "${LETA_SRC_DIR}/core/*.cpp")
    # Vendored files are not subject to the rule; they are the exception the rule names.
    list(FILTER core_files EXCLUDE REGEX "^${LETA_CORE_THIRD_PARTY_DIR}/")
    foreach(file IN LISTS core_files)
        _leta_check_core_includes("${file}")
    endforeach()

    file(GLOB_RECURSE application_files CONFIGURE_DEPENDS
        "${LETA_SRC_DIR}/application/*.hpp" "${LETA_SRC_DIR}/application/*.cpp")
    foreach(file IN LISTS application_files)
        _leta_check_application_includes("${file}")
    endforeach()
endfunction()

function(leta_check_layering)
    if(NOT TARGET leta_core)
        message(FATAL_ERROR "ADR-001: target leta_core does not exist; the layering check has nothing to check.")
    endif()
    _leta_check_core_link_graph()
    _leta_check_include_direction()
    message(STATUS "Layering check (ADR-001): leta_core links only project targets; includes point inward")
endfunction()
