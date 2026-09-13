# cmake/LetaBuildInfo.cmake
#
# Computes the values baked into build_info_generated.cpp (FR-68, M0-T3 D7):
#
#   LETA_VERSION_STRING   PROJECT_VERSION plus LETA_VERSION_SUFFIX, e.g. "0.1.0-dev"
#   LETA_GIT_COMMIT       short hash, "-dirty" appended when tracked files are modified,
#                         "unknown" outside a git checkout (source tarball, container context)
#   LETA_COMPILER_STRING  "<id> <version>", e.g. "GNU 13.2.0" or "Clang 17.0.6"
#
# The commit is read at configure time. A reconfigure is forced whenever HEAD moves (commit,
# checkout, reset) by depending on the files git touches for that. The remaining hole: an edit
# made after configure is not reflected in "-dirty" until the next reconfigure. Release builds are
# clean checkouts, where this cannot matter; a build-time generator is the upgrade if it ever does.

include_guard(GLOBAL)

set(LETA_VERSION_STRING "${PROJECT_VERSION}${LETA_VERSION_SUFFIX}")
set(LETA_COMPILER_STRING "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
set(LETA_GIT_COMMIT "unknown")

find_package(Git QUIET)

if(Git_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" rev-parse --short=7 HEAD
        RESULT_VARIABLE _leta_git_result
        OUTPUT_VARIABLE _leta_git_commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)

    if(_leta_git_result EQUAL 0)
        set(LETA_GIT_COMMIT "${_leta_git_commit}")

        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" status --porcelain --untracked-files=no
            OUTPUT_VARIABLE _leta_git_status
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(NOT _leta_git_status STREQUAL "")
            string(APPEND LETA_GIT_COMMIT "-dirty")
        endif()

        foreach(_leta_git_file IN ITEMS ".git/HEAD" ".git/logs/HEAD")
            if(EXISTS "${PROJECT_SOURCE_DIR}/${_leta_git_file}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                    "${PROJECT_SOURCE_DIR}/${_leta_git_file}")
            endif()
        endforeach()
    endif()
endif()

message(STATUS "Build info: leta ${LETA_VERSION_STRING} (${LETA_GIT_COMMIT}, ${CMAKE_BUILD_TYPE}, ${LETA_COMPILER_STRING})")
