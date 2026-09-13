# Contributing to Leta

Thanks for considering a contribution. Leta is a single-maintainer project run with a deliberately
tight process — reading this first will save both of us time.

## Before you open anything

- **Check [`docs/00-project-brief.md`](docs/00-project-brief.md) §4.** Filtering, faceting, sorting,
  synonyms, stemming, replication, vector search, and an admin UI are out of v1 scope on purpose.
  Proposing them will get a polite pointer back to that document.
- **Read [`docs/04-roadmap.md`](docs/04-roadmap.md)** for what is being worked on and in what order.
- **Open an issue before a large PR.** Small fixes (typos, obvious bugs, clarifying docs) can go
  straight to a PR; anything larger should start as a conversation.

## Kinds of contribution that are especially welcome

- Bug reports with a minimal reproduction against a specific commit or release.
- Documentation improvements — especially places where the docs are unclear or wrong.
- Additions to the fuzzing corpora under `tests/fuzz/corpus/` after a crash you found.
- Portability fixes on supported platforms (Linux x86-64 and arm64, GCC ≥ 13 and Clang ≥ 17).
- Benchmark reports on hardware different from what's already published, with the machine
  description alongside the numbers.

## Setting up

```bash
git clone https://github.com/Samir120/leta.git
cd leta
cmake --workflow --preset debug
```

Presets: `debug`, `release`, `asan-ubsan`, `tsan`, `coverage`, `fuzz` — the same six names for
configure, build, test and workflow presets, each writing to `build/<preset>/`. `coverage` and
`fuzz` need Clang. Pick the compiler with `CC`/`CXX`; keep personal variants in the gitignored
`CMakeUserPresets.json`. `cmake --preset debug -DLETA_CLANG_TIDY=ON` runs clang-tidy as part of
the build, which is what CI does.

You need CMake ≥ 3.25, GCC ≥ 13 or Clang ≥ 17, Ninja, and Docker (for the integration tests and
container build). Dependencies are fetched by the build; nothing else to install.

Linux only. If you're on Windows or macOS, the supported path is running the container. See
[`docs/adr/010-linux-container-only.md`](docs/adr/010-linux-container-only.md) for why.

## Making a change

1. **Branch from `main`:** `feat/short-title`, `fix/short-title`, `docs/short-title`, or
   `chore/short-title`.
2. **Follow the coding standards** in [`docs/06-coding-standards.md`](docs/06-coding-standards.md).
   Run `clang-format` and `clang-tidy` before pushing.
3. **Tag every new test with the requirement ID it covers:**
   ```cpp
   TEST_CASE("terms of length 5 tolerate one edit", "[FR-23]") { ... }
   ```
   The IDs come from [`docs/01-requirements.md`](docs/01-requirements.md). A CI job fails the build
   if any Must requirement has no tagged test — this is how NFR-14 stays honest.
4. **Use Conventional Commits.** Example:
   ```
   feat(core): Damerau-Levenshtein automaton for typo candidates

   Builds a Levenshtein automaton over the query term and intersects it with
   the term dictionary, so candidate generation is O(matched terms) rather
   than a scan of the dictionary.

   Covers: FR-23
   Refs: ADR-008
   ```
   Types: `feat`, `fix`, `perf`, `refactor`, `test`, `docs`, `build`, `ci`, `chore`.
5. **Keep behaviour changes and refactors in separate commits.** Always.
6. **Update the docs in the same PR** if behaviour, configuration, or the API changes.

## What CI checks

Full matrix in [`docs/05-quality-strategy.md`](docs/05-quality-strategy.md) §10. Every PR gets:

- Build under GCC 13 and Clang 17, Debug and Release, on x86-64 and arm64
- Unit, integration, and contract tests
- AddressSanitizer + UBSan, ThreadSanitizer
- `clang-format` and `clang-tidy`
- Requirement-coverage check (every Must has a tagged test)
- Layering check (`leta_core` links no third-party target — see [ADR-001](docs/adr/001-ports-and-adapters.md))
- Coverage report, floor at 80% of `leta_core`

`main` is protected. PRs are squash-merged. A red `main` is fixed before any new work starts.

## Definition of Done

Every PR is reviewed against the checklist in
[`docs/07-ways-of-working.md`](docs/07-ways-of-working.md) §3. The PR template surfaces it — every
box must be truthfully ticked before merge.

## What Leta writes from scratch, and what it takes as a dependency

To keep the project focused and the code readable, some things are written in-tree even when a
library would be quicker:

- **In-tree:** the inverted index, postings compression, typo tolerance, ranking, and the query
  planner. See [ADR-008](docs/adr/008-index-data-structures.md) and
  [ADR-009](docs/adr/009-persistence-format.md).
- **Dependency:** HTTP, JSON, logging, metrics formatting, testing, benchmarking, and Unicode tables.

Please don't open a PR that replaces the first category with a library — it will be declined for
reasons in the brief, not because the library is bad.

## Bug reports

Include:

- Leta version (`leta --version`) or commit hash
- OS, kernel, container runtime
- A minimal reproduction: the smallest sequence of requests that shows the problem
- What you expected, what you got

Reports that come with a failing test case attached are the fastest to land.

## Security

Please do **not** open a public issue for a suspected vulnerability. See
[`SECURITY.md`](SECURITY.md) for the private reporting route.

## Code of Conduct

Be kind, be direct, argue the technical point and not the person making it. Disagreement is
welcome; abuse and personal attacks are not.

## Licence of your contributions

By opening a pull request you agree that your contribution is licensed under the
[Apache License, Version 2.0](LICENSE), the same licence as the rest of the project. No CLA.
