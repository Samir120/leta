# ADR-006 — Catch2 v3, with FR IDs as tags

| | |
|---|---|
| Status | Proposed |
| Date | 2026-09-03 |
| Requirements | NFR-14, and §3 of `01-requirements.md` |

## Context

`01-requirements.md` §3 states that a requirement with no test is not done, and NFR-14 requires every
`Must` requirement to have at least one test referencing its ID. That is only enforceable if the test
framework can select and enumerate tests by an arbitrary label.

## Decision

**Catch2 v3** for unit and integration tests, driven by `ctest`. Every test carries its requirement
ID as a tag:

```cpp
TEST_CASE("terms of length 5 tolerate exactly one edit", "[FR-23]") { ... }
```

This makes traceability mechanical rather than aspirational:

```bash
./build/leta_tests "[FR-23]"        # run everything covering one requirement
./build/leta_tests --list-tags      # enumerate covered requirements
```

CI parses `--list-tags`, compares it against the `Must` IDs extracted from `01-requirements.md`, and
**fails the build if any Must requirement has no tagged test**. NFR-14 stops being a review item and
becomes a gate.

**Google Benchmark** is used separately for the NFR-01/02/03/11 suite, because it handles iteration
counts, warm-up, and statistics properly. Catch2's `BENCHMARK` is fine for micro-comparisons inside a
unit test but is not the source of the published numbers.

## Alternatives considered

**A — doctest.** Compiles noticeably faster and supports tags. Rejected narrowly: weaker matchers and
generators, and a smaller ecosystem. If test compile time becomes a real drag, this is the fallback
and the migration is mostly mechanical.

**B — GoogleTest.** The most widely used, but filtering is by test name only, so IDs would have to be
embedded in names and matched by substring — workable but fragile. GMock also invites mocking where
this project prefers real in-memory fakes injected through the ports in `03-architecture.md` §2;
having the tool available makes the wrong choice too easy.

## Consequences

**Easier:** the traceability requirement is automated. A reviewer can run one command to see what a
requirement is backed by.

**Harder:** Catch2 v3 compiles slower than doctest. Mitigate by keeping tests in a separate target
from the library, using the precompiled-header support, and not including the framework in any
production translation unit.

**Follow-up:** the requirement-coverage check needs a small script; it is a task in M0.
