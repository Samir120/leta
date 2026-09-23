# Changelog

All notable changes to Leta are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). The versioned surface is the HTTP API
defined in [`docs/02-api-spec.yaml`](docs/02-api-spec.yaml), not the C++ symbols.

Entries reference the requirement IDs they close, e.g. `Prefix matching on the last query term
(FR-22)`, so a release can be checked against [`docs/01-requirements.md`](docs/01-requirements.md).

## [Unreleased]

Pre-alpha. `leta --version` is the only runnable behaviour.

### Added
- Specification set: project brief, requirements, OpenAPI 3.1 contract and guide, architecture,
  roadmap, quality strategy, coding standards, ways of working (`docs/`).
- Architecture Decision Records 001–010 (`docs/adr/`), all *Accepted*.
- Repository layout, editor configuration, lint configuration, and public-facing files
  (`README`, `CONTRIBUTING`, `SECURITY`, `LICENSE`, `NOTICE`).
- Build system: CMake presets `debug`, `release`, `asan-ubsan`, `tsan`, `coverage` and `fuzz`, and
  layer targets with a configure-time layering check (ADR-001, ADR-002).
- `leta --version` prints version, commit, build type and compiler (FR-68).
- Test dependencies pinned by hash: Catch2 3.16.0 and, opt-in, Google Benchmark 1.9.5; `tl::expected`
  1.3.1 vendored (ADR-005, ADR-006).
- Continuous integration on GCC 13 and Clang 17, x86-64 and arm64, with sanitizer, coverage, fuzz,
  clang-tidy and clang-format checks.

<!--
Release template — copy for each tag. Delete empty sections.

## [0.1.0] - YYYY-MM-DD

### Added        new features
### Changed      changes to existing behaviour
### Deprecated   features that will be removed
### Removed      features removed in this release
### Fixed        bug fixes
### Security     vulnerability fixes

[Unreleased]: https://github.com/Samir120/leta/compare/v0.1.0...HEAD
[0.1.0]:      https://github.com/Samir120/leta/releases/tag/v0.1.0
-->
