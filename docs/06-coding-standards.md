# Leta — Coding Standards

| | |
|---|---|
| Status | v1 |
| Applies to | all C++ in the repository |
| Last updated | 2026-09-03 |

Rules here are enforced in review and, where possible, by tooling. Where a rule is marked
**[ADR]**, the decision is still open and listed in `STATE.md` §5.

---

## 1. Language and toolchain

- **C++20.** No compiler extensions. `-std=c++20 -Wall -Wextra -Werror -Wshadow -Wconversion`.
- Must build on **GCC ≥ 13 and Clang ≥ 17**, x86-64 and arm64 (NFR-09). Anything that works on only
  one compiler is a bug, not a preference.
- MSVC is a portability check only, allowed to fail (NFR-10). Do not contort the code for it.
- C++23 features are permitted only when both required compilers support them in their oldest
  supported version. `std::expected` is the live example — see §5. **[ADR D5]**

## 2. Layout, naming, formatting

```
leta/
├── include/leta/          # public headers, if a public library API is offered
├── src/
│   ├── core/              # pure domain: tokenizer, index, postings, ranking, typo
│   ├── application/       # services orchestrating core; owns storage interfaces
│   ├── adapters/
│   │   ├── http/
│   │   ├── storage/
│   │   ├── config/
│   │   └── metrics/
│   └── main.cpp
├── tests/{unit,integration,fuzz,bench}/
├── docs/                  # the numbered specification set
└── cmake/
```

- Files: `snake_case.cpp` / `.hpp`. One primary type per file, file named after it.
- Types and concepts: `PascalCase`. Functions, variables, members: `snake_case`.
- Private members: trailing underscore — `postings_`. Never Hungarian, never `m_`.
- Constants and enumerators: `PascalCase` (`enum class TokenKind { Word, Number };`).
- Macros: forbidden except include guards (and `#pragma once` replaces those) and build-generated
  version strings.
- Namespaces mirror directories: `leta::core`, `leta::http`. No `using namespace` in a header, ever.
- `#pragma once` in every header.
- Include order: own header, then C++ standard library, then third party, then project. Each group
  alphabetized and separated by a blank line.
- Formatting is `clang-format` only. 100 columns. Never hand-format; never argue about it in review.

## 3. Types and interfaces

- `const` by default. `constexpr` where it costs nothing.
- Pass by `std::string_view` and `std::span` for non-owning views; by value for cheap types;
  by `const&` for expensive ones. Return by value and trust the compiler.
- **Never store a `string_view` or `span` whose owner may die first.** Lifetimes across the
  segment-swap boundary (§7) are the highest-risk area of this codebase — document them.
- `explicit` on every single-argument constructor.
- Strong types over primitives where confusion is possible: `DocId`, `TermId`, `AttributeRank`
  as distinct types, not bare `uint32_t`. Mixing them up silently is a whole class of bug removed.
- `enum class` always; never plain `enum`.
- Rule of zero. If a type needs a destructor, it probably should own a resource wrapper instead.
- Public headers of any shipped library use **pimpl** to keep implementation details and heavy
  includes out of the interface.

## 4. Memory and resources

- **No raw `new`/`delete`.** No owning raw pointers. `unique_ptr` by default, `shared_ptr` only
  where ownership is genuinely shared (the reader snapshot in §7 is the legitimate case).
- Raw pointers and references are non-owning parameters only.
- Every OS resource (fd, mmap, socket) is wrapped in a small RAII type on first use. No exceptions.
- Hot paths do not allocate. Reserve up front, reuse buffers, prefer arena or pool allocation for
  per-query scratch. Where an allocation is unavoidable in a hot path, comment why.
- Data structures on the search path are laid out for cache locality: contiguous storage, indices
  instead of pointers, structure-of-arrays where it wins. Measure before claiming it wins.

## 5. Errors

Three categories, handled differently:

| Category | Example | Mechanism |
|---|---|---|
| Expected, caller can act | malformed JSON body, unknown index UID, bad query parameter | Returned value type, no exception |
| Unexpected, unrecoverable at this layer | out of memory, corrupt snapshot on startup | Exception, caught at a boundary |
| Programmer error | broken invariant, impossible enum value | `assert` in debug; explicit `std::terminate` path in release for anything that would otherwise be UB |

- The returned value type is `Result<T, Error>` **[ADR D5]** — `std::expected` if the compiler
  matrix allows, otherwise a vendored or project-local equivalent with the same shape.
- **No exceptions across the HTTP boundary.** Every handler catches, maps to the error model in
  FR-71 (`{code, message, type}`), logs with the request id, and returns a status.
- No error codes as `int`. No `errno` leaking upward. No exceptions used for control flow.
- Error messages are for humans and never leak internals, paths, or memory contents to clients.
  The detail goes to the log, correlated by `X-Request-Id`.

## 6. Input handling

Everything crossing the process boundary is hostile: HTTP bytes, JSON documents, query strings,
settings payloads, and the on-disk WAL and snapshot files.

- Parsers are total functions: every input either parses or returns an error. Never UB, never abort.
- Enforce the limits from FR-14 and NFR-12 (batch ≤ 10 000 docs / 20 MB, 30 s timeout) at the
  earliest possible point, before allocation.
- Every parser gets a libFuzzer target and a seed corpus (NFR-07).
- Validate UTF-8 explicitly. Invalid sequences are rejected with a named error, not normalized away.
- Reject unknown fields rather than ignoring them (FR-72), and name the offending field.

## 7. Concurrency

The concurrency model is decided in **[ADR D7]**; these rules hold regardless of the outcome.

- Concurrency lives in as few places as possible. Core algorithms are single-threaded, pure, and
  testable without threads.
- **Reads never block on writes** (NFR-05). The intended shape: writes build a new immutable index
  segment; readers acquire a `shared_ptr` snapshot at query start and hold it for the query; the
  swap is a single atomic operation.
- Shared mutable state requires an explicit, documented ownership rule stated in a comment at the
  declaration: which thread writes, which threads read, under what synchronization.
- `std::atomic` requires an explicit memory order and a comment justifying anything weaker than
  `seq_cst`. No hand-rolled lock-free structures without an ADR and a TSan-clean stress test.
- No sleeping as synchronization. No mutex held across I/O or an allocation. Lock ordering
  documented if more than one lock exists.
- Anything touching threading ships TSan-clean (NFR-06).

## 8. Testing

- Framework **[ADR D6]**; must support tags so tests carry FR IDs.
- Every `Must` requirement has at least one test naming its ID (NFR-14):
  `TEST_CASE("terms of length 5 tolerate one edit", "[FR-23]")`.
- Core library line coverage ≥ 80% (NFR-14). Coverage is a floor, not a goal — an untested branch in
  recovery code matters more than a tested getter.
- Layers of tests:
  - **Unit** — core types, no I/O, milliseconds.
  - **Integration** — real HTTP against a real server on a temp data dir, exercising the endpoints in `02-api-spec.yaml`.
  - **Property/randomized** — tokenizer round-trips, edit-distance oracle comparison against a naive implementation, WAL replay against a reference model.
  - **Crash recovery** — kill the process at chosen points mid-write and assert FR-53: recoverable to the last acknowledged write. This is the test most likely to be skipped and the one most likely to find a real bug.
  - **Fuzz** — HTTP parser, JSON ingest, query parser (NFR-07).
  - **Benchmark** — fixed dataset, committed results, the NFR-01/02/03/11 numbers.
- Tests assert behaviour, not implementation. A refactor that breaks no behaviour breaks no test.
- No sleeps in tests. Inject the clock.

## 9. Performance

- No performance claim without a measurement. "Faster" is meaningless without a number, a dataset,
  and a machine description.
- Benchmarks use a fixed, committed dataset so results are comparable across commits.
- Optimize only what a profile shows. Record in the commit message what the profile said.
- Where a fast path is genuinely obscure, keep the obvious implementation next to it as the
  reference used by the tests.

## 10. Dependencies

- Every new dependency needs a one-line justification and a note on licence compatibility with
  Apache-2.0.
- Things Leta writes itself, per brief G4: the inverted index, postings compression, typo tolerance,
  ranking, and the query planner.
- Things Leta may take as dependencies: HTTP, JSON, logging, metrics formatting, testing,
  benchmarking, and Unicode tables. **[ADR D2]** records the exact boundary.
- Pinned versions. No floating tags. Reproducible builds.
- No dynamic code loading, no plugins, no outbound network calls at runtime (NFR-08).

## 11. Documentation in code

- Every public header entity gets a comment covering: what it does, what it does not do, ownership
  and lifetime of anything it borrows, thread-safety, and complexity where non-obvious.
- Algorithm implementations cite their source: the paper, the chapter, or the ADR.
- The `why` comment is the valuable one. If a line looks wrong but is correct, explain it or someone
  will "fix" it later.
