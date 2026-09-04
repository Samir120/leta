# ADR-002 — CMake with presets and CPM.cmake for dependencies

| | |
|---|---|
| Status | Proposed |
| Date | 2026-09-03 |
| Requirements | NFR-09, NFR-13, S6 |

## Context

The build must work on GCC 13 and Clang 17, on x86-64 and arm64, on Linux and under WSL2 on Windows
11 (NFR-09, and the brief's §7 development environment). S6 requires a stranger to run Leta in under
ten minutes. The container image must stay under 30 MB (NFR-13). The dependency count is small —
roughly six libraries.

## Decision

CMake ≥ 3.25 with `CMakePresets.json`, and **CPM.cmake** (a single vendored `.cmake` file over
`FetchContent`) for dependencies, each pinned by git tag and hash.

Presets define the build configurations so that local and CI commands are identical:
`debug`, `release`, `asan-ubsan`, `tsan`, `coverage`, `fuzz`. Every command in the README and in CI
is `cmake --preset <name>` followed by `cmake --build`.

## Alternatives considered

**A — vcpkg manifest mode.** Better binary caching, first-class on Windows, and a large catalogue.
Rejected because it adds a toolchain a contributor must install and bootstrap before the first build,
which works against S6, and because arm64 triplet handling adds friction for a target that must work.

**B — Conan.** Same objection as vcpkg plus a Python dependency.

**C — Plain FetchContent with no wrapper.** Works, but duplicates version handling across
dependencies and has no deduplication. CPM is a single file that fixes both and can be dropped later
with no migration.

**D — System packages via `find_package` only.** Rejected: GCC 13 and Clang 17 are newer than the
default packages on common distributions, and reproducibility across two architectures would suffer.

## Consequences

**Easier:** `git clone && cmake --preset release && cmake --build --preset release` with only CMake
and a compiler installed. Identical behaviour on every platform. Version pinning is explicit and
reviewable in one file.

**Harder:** dependencies are built from source, so cold CI builds are slow. Mitigated with `ccache`
and a CI cache keyed on the dependency manifest. A dependency with a bad CMake export requires a
local patch.

**Follow-up:** M0 sets up presets, CPM, ccache in CI, and the `leta_core` no-dependency check from
ADR-001.

## Revisit when

Cold CI build time exceeds ~10 minutes, or a needed dependency does not build cleanly from source.
