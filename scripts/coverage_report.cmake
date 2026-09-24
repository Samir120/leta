# The coverage half of NFR-14 (05-quality-strategy.md §4): merge the raw profiles a coverage test
# run left behind, print llvm-cov's table for Leta's own sources, and fail if line coverage of
# leta_core — every file under src/core except the vendored third_party/ — is below the floor.
# Run through the target tests/CMakeLists.txt defines, after the tests:
#
#   cmake --workflow --preset coverage                         # configure, build, run the tests
#   cmake --build --preset coverage --target coverage-report
#
# Each report consumes the profiles it merges, so a second report needs a second test run. That
# keeps profiles from an older binary out of a newer report, where llvm-cov would silently mix them.
#
# Files are selected from `llvm-cov export` JSON by path rather than with llvm-cov's own filters:
# third-party sources (Catch2, compiled with the same instrumentation) live wherever CPM's cache
# is, and a path prefix is the one test that is certain. A header no instrumented translation unit
# includes has no coverage mapping at all and counts as neither covered nor uncovered; once
# leta_core has .cpp files every one of them is compiled, so that blind spot is limited to headers
# nothing includes.
#
# Script mode (cmake -P). Inputs, all -D:
#   LETA_LLVM_PROFDATA, LETA_LLVM_COV   same LLVM major version as the compiler
#   LETA_PROFILE_DIR                    where LLVM_PROFILE_FILE (CMakePresets.json) writes .profraw
#   LETA_OBJECTS                        instrumented executables, separated by '|'
#   LETA_SOURCE_DIR                     repository root
#   LETA_OUTPUT_DIR                     receives leta.profdata and report.txt
#   LETA_CORE_MIN_LINE_PERCENT          the floor, an integer

cmake_minimum_required(VERSION 3.25)

foreach(input IN ITEMS LETA_LLVM_PROFDATA LETA_LLVM_COV LETA_PROFILE_DIR LETA_OBJECTS
                       LETA_SOURCE_DIR LETA_OUTPUT_DIR LETA_CORE_MIN_LINE_PERCENT)
    if("${${input}}" STREQUAL "")
        message(FATAL_ERROR "coverage_report: -D${input}=<value> is required")
    endif()
endforeach()

foreach(tool IN ITEMS LETA_LLVM_PROFDATA LETA_LLVM_COV)
    if("${${tool}}" MATCHES "-NOTFOUND$")
        message(FATAL_ERROR
            "coverage_report: ${tool} was not found when the coverage preset was configured. "
            "Install the LLVM tools of your Clang's major version (Debian/Ubuntu: llvm-<major>, "
            "Arch and Fedora: llvm), then re-run `cmake --preset coverage`.")
    endif()
endforeach()

# ---- 1. Merge, then consume, the raw profiles ---------------------------------------------------

file(GLOB raw_profiles "${LETA_PROFILE_DIR}/*.profraw")
if("${raw_profiles}" STREQUAL "")
    message(FATAL_ERROR
        "coverage_report: no .profraw files in ${LETA_PROFILE_DIR}. Run `ctest --preset coverage` "
        "first; each report consumes the profiles it merges.")
endif()

file(MAKE_DIRECTORY "${LETA_OUTPUT_DIR}")
set(profdata "${LETA_OUTPUT_DIR}/leta.profdata")
execute_process(
    COMMAND "${LETA_LLVM_PROFDATA}" merge -sparse ${raw_profiles} -o "${profdata}"
    COMMAND_ERROR_IS_FATAL ANY)
file(REMOVE ${raw_profiles})

# llvm-cov takes the first binary positionally and every further one after -object.
string(REPLACE "|" ";" objects "${LETA_OBJECTS}")
list(POP_FRONT objects first_object)
set(object_args "${first_object}")
foreach(object IN LISTS objects)
    list(APPEND object_args -object "${object}")
endforeach()

# ---- 2. Per-file line summaries: project files, and the core total ------------------------------

execute_process(
    COMMAND "${LETA_LLVM_COV}" export -summary-only "-instr-profile=${profdata}" ${object_args}
    OUTPUT_VARIABLE export_json
    COMMAND_ERROR_IS_FATAL ANY)

set(src_dir "${LETA_SOURCE_DIR}/src")
set(core_dir "${LETA_SOURCE_DIR}/src/core")
set(vendored_dir "${LETA_SOURCE_DIR}/src/core/third_party")
set(project_files "")
set(core_lines 0)
set(core_lines_covered 0)

string(JSON file_count LENGTH "${export_json}" data 0 files)
if(file_count GREATER 0)
    math(EXPR last_index "${file_count} - 1")
    foreach(index RANGE ${last_index})
        string(JSON filename GET "${export_json}" data 0 files ${index} filename)
        cmake_path(IS_PREFIX src_dir "${filename}" NORMALIZE is_project)
        cmake_path(IS_PREFIX vendored_dir "${filename}" NORMALIZE is_vendored)
        if(NOT is_project OR is_vendored)
            continue()
        endif()
        list(APPEND project_files "${filename}")

        cmake_path(IS_PREFIX core_dir "${filename}" NORMALIZE is_core)
        if(is_core)
            string(JSON lines GET "${export_json}" data 0 files ${index} summary lines count)
            string(JSON covered GET "${export_json}" data 0 files ${index} summary lines covered)
            math(EXPR core_lines "${core_lines} + ${lines}")
            math(EXPR core_lines_covered "${core_lines_covered} + ${covered}")
        endif()
    endforeach()
endif()

# ---- 3. Table for the reader --------------------------------------------------------------------

if("${project_files}" STREQUAL "")
    set(report "No Leta source under src/ was compiled into an instrumented binary.\n")
else()
    execute_process(
        COMMAND "${LETA_LLVM_COV}" report "-instr-profile=${profdata}" ${object_args}
                ${project_files}
        OUTPUT_VARIABLE report
        COMMAND_ERROR_IS_FATAL ANY)
endif()

# ---- 4. The gate --------------------------------------------------------------------------------

set(floor "${LETA_CORE_MIN_LINE_PERCENT}")
if(core_lines EQUAL 0)
    set(passed TRUE)
    set(verdict
        "leta_core line coverage: nothing to measure — no file under src/core is compiled into an "
        "instrumented binary yet (expected while leta_core is INTERFACE, until M1/M2). The ${floor} % "
        "floor applies from the first line of core code.")
    string(JOIN "" verdict ${verdict})
else()
    math(EXPR hundredths "${core_lines_covered} * 10000 / ${core_lines}")
    math(EXPR whole "${hundredths} / 100")
    math(EXPR fraction "${hundredths} % 100")
    if(fraction LESS 10)
        set(fraction "0${fraction}")
    endif()
    math(EXPR needed "${floor} * ${core_lines}")
    math(EXPR achieved "${core_lines_covered} * 100")
    if(achieved GREATER_EQUAL needed)
        set(passed TRUE)
    else()
        set(passed FALSE)
    endif()
    set(verdict
        "leta_core line coverage: ${whole}.${fraction} % (${core_lines_covered} of ${core_lines} "
        "lines), floor ${floor} %")
    string(JOIN "" verdict ${verdict})
endif()

string(APPEND report "\n${verdict}\n")
file(WRITE "${LETA_OUTPUT_DIR}/report.txt" "${report}")
message(NOTICE "${report}")

if(NOT passed)
    message(FATAL_ERROR "Coverage gate (NFR-14): ${verdict}")
endif()
