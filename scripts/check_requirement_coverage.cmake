# NFR-14 as a gate (05-quality-strategy.md §3, ADR-006): every Must FR in docs/01-requirements.md
# has a Catch2 test tagged with its ID, or a line in tests/untested-requirements.txt naming the
# milestone that owes one (or why no CI test can check it). Registered as the ctest entry
# NFR-14.requirement_coverage in tests/CMakeLists.txt, so it runs in every preset, locally and on
# every CI leg. Run it alone, with the summary:
#
#   ctest --preset debug -R NFR-14 -V
#
# Script mode (cmake -P). Inputs, all -D:
#   LETA_REQUIREMENTS       path to docs/01-requirements.md
#   LETA_UNTESTED           path to tests/untested-requirements.txt
#   LETA_TEST_EXECUTABLES   Catch2 executables to ask for --list-tags, separated by '|'
#
# Fails, reporting every problem at once, on:
#   - a Must FR with no tagged test and no line in the untested list
#   - an untested-list line for an FR that now has a tagged test: the list only ever shrinks
#   - an untested-list line that is malformed, duplicated, or names anything but a Must FR
#   - a tag naming an ID that 01-requirements.md does not define; a typo such as [FR-230] would
#     otherwise cover nothing and leave the requirement it meant silently uncovered
#
# The gate is on FRs, as NFR-14 words it. NFRs are verified by benchmarks, CI jobs and image
# inspection; an NFR tag on a test is allowed and is checked only for existence.

cmake_minimum_required(VERSION 3.25)

foreach(input IN ITEMS LETA_REQUIREMENTS LETA_UNTESTED LETA_TEST_EXECUTABLES)
    if("${${input}}" STREQUAL "")
        message(FATAL_ERROR "check_requirement_coverage: -D${input}=<value> is required")
    endif()
endforeach()

# Reads a text file as a list with exactly one element per line. `;`, `[` and `]` are CMake list
# syntax, and all three occur in the requirements table (`["*"]`, `[a-zA-Z0-9_-]{1,64}`). Left in,
# they would split one row in two or glue two rows into one. Nothing matched below uses them.
function(read_lines path out_var)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "check_requirement_coverage: ${path} does not exist")
    endif()
    file(READ "${path}" content)
    string(REPLACE ";" "," content "${content}")
    string(REPLACE "[" "(" content "${content}")
    string(REPLACE "]" ")" content "${content}")
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE "\n" ";" content "${content}")
    set(${out_var} "${content}" PARENT_SCOPE)
endfunction()

set(errors "")

# ---- 1. Requirements: every defined ID, and the Must FRs ----------------------------------------
# A row is `| <ID> | <priority> | ...`, the same shape in the FR and the NFR table.

read_lines("${LETA_REQUIREMENTS}" requirement_lines)
set(defined_ids "")
set(must_frs "")
foreach(line IN LISTS requirement_lines)
    if("${line}" MATCHES "^\\|[ ]*((N?FR)-[0-9]+)[ ]*\\|[ ]*([MSCW])[ ]*\\|")
        list(APPEND defined_ids "${CMAKE_MATCH_1}")
        if(CMAKE_MATCH_2 STREQUAL "FR" AND CMAKE_MATCH_3 STREQUAL "M")
            list(APPEND must_frs "${CMAKE_MATCH_1}")
        endif()
    endif()
endforeach()

# Parsing nothing would pass vacuously. Refuse instead: the table format has changed.
if("${must_frs}" STREQUAL "")
    message(FATAL_ERROR
        "check_requirement_coverage: no Must FR rows found in ${LETA_REQUIREMENTS}. Has the "
        "table format changed? Rows must read `| FR-nn | M | ... |`.")
endif()

# ---- 2. Tags carried by the tests ---------------------------------------------------------------

string(REPLACE "|" ";" executables "${LETA_TEST_EXECUTABLES}")
set(tagged_ids "")
foreach(executable IN LISTS executables)
    execute_process(COMMAND "${executable}" --list-tags
        RESULT_VARIABLE exit_code
        OUTPUT_VARIABLE listing
        ERROR_VARIABLE listing_errors)
    if(NOT exit_code EQUAL 0)
        message(FATAL_ERROR
            "check_requirement_coverage: `${executable} --list-tags` failed (${exit_code}):\n"
            "${listing}${listing_errors}")
    endif()
    # Catch2 matches tags case-insensitively, so [fr-23] runs under "[FR-23]" and counts here too.
    string(TOUPPER "${listing}" listing)
    string(REPLACE "[" "<" listing "${listing}")
    string(REPLACE "]" ">" listing "${listing}")
    string(REGEX MATCHALL "<N?FR-[0-9]+>" tags "${listing}")
    foreach(tag IN LISTS tags)
        string(REGEX REPLACE "[<>]" "" id "${tag}")
        list(APPEND tagged_ids "${id}")
    endforeach()
endforeach()
list(REMOVE_DUPLICATES tagged_ids)

foreach(id IN LISTS tagged_ids)
    if(NOT id IN_LIST defined_ids)
        list(APPEND errors "a test is tagged ${id}, which docs/01-requirements.md does not define")
    endif()
endforeach()

# ---- 3. The untested list -----------------------------------------------------------------------
# One line per FR: `<FR-ID> <due> [note]`, where <due> is a milestone (M1 … M13) or `untestable`.

read_lines("${LETA_UNTESTED}" untested_lines)
set(listed_ids "")
set(pending_milestones "")
set(untestable_count 0)
set(line_number 0)
foreach(line IN LISTS untested_lines)
    math(EXPR line_number "${line_number} + 1")
    set(where "tests/untested-requirements.txt:${line_number}")
    if("${line}" MATCHES "^[ \t]*(#.*)?$")
        continue()
    endif()
    if(NOT "${line}" MATCHES "^(FR-[0-9]+)[ \t]+(M[0-9]+|untestable)([ \t]+(.*))?$")
        list(APPEND errors
            "${where}: expected `<FR-ID> <milestone|untestable> [note]`, got `${line}`")
        continue()
    endif()
    set(id "${CMAKE_MATCH_1}")
    set(due "${CMAKE_MATCH_2}")
    string(STRIP "${CMAKE_MATCH_4}" note)

    if(id IN_LIST listed_ids)
        list(APPEND errors "${where}: ${id} is listed twice")
    elseif(NOT id IN_LIST must_frs)
        list(APPEND errors "${where}: ${id} is not a Must FR; this list holds untested Must FRs only")
    elseif(id IN_LIST tagged_ids)
        list(APPEND errors "${where}: ${id} has a tagged test now; delete this line")
    elseif(due STREQUAL "untestable" AND note STREQUAL "")
        list(APPEND errors "${where}: `untestable` needs a one-line reason")
    endif()

    list(APPEND listed_ids "${id}")
    if(due STREQUAL "untestable")
        math(EXPR untestable_count "${untestable_count} + 1")
    else()
        list(APPEND pending_milestones "${due}")
    endif()
endforeach()

# ---- 4. Every Must FR is either tested or listed ------------------------------------------------

set(tested_count 0)
foreach(id IN LISTS must_frs)
    if(id IN_LIST tagged_ids)
        math(EXPR tested_count "${tested_count} + 1")
    elseif(NOT id IN_LIST listed_ids)
        list(APPEND errors
            "${id} is a Must requirement with no test tagged [${id}] and no line in "
            "tests/untested-requirements.txt")
    endif()
endforeach()

# ---- 5. Verdict ---------------------------------------------------------------------------------

if(NOT "${errors}" STREQUAL "")
    list(LENGTH errors error_count)
    list(JOIN errors "\n  - " error_text)
    message(FATAL_ERROR
        "Requirement coverage (NFR-14): ${error_count} problem(s)\n  - ${error_text}\n"
        "Tag each test with the ID it covers, e.g. TEST_CASE(\"...\", \"[FR-23]\"), and keep "
        "tests/untested-requirements.txt equal to the set of Must FRs that have no test yet.")
endif()

list(LENGTH must_frs must_count)
list(LENGTH pending_milestones pending_count)
set(milestones "${pending_milestones}")
list(REMOVE_DUPLICATES milestones)
list(SORT milestones COMPARE NATURAL)
set(breakdown "")
foreach(milestone IN LISTS milestones)
    set(owed "${pending_milestones}")
    list(FILTER owed INCLUDE REGEX "^${milestone}$")
    list(LENGTH owed owed_count)
    list(APPEND breakdown "${milestone} ${owed_count}")
endforeach()
list(JOIN breakdown ", " breakdown)

message(STATUS "Requirement coverage (NFR-14): ${tested_count} of ${must_count} Must FRs have a tagged test")
message(STATUS "  pending ${pending_count}: ${breakdown}")
message(STATUS "  untestable ${untestable_count}")
