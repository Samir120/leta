# Leta — Quality Strategy

| | |
|---|---|
| Status | Draft v0.2 — **proposed, awaiting owner acceptance** |
| Scope | v1 |
| Last updated | 2026-09-24 |

How we know Leta works. Referenced by S2–S5 and NFR-01 through NFR-14.

---

## 1. Principles

**A requirement with no test is not done.** This is from `01-requirements.md` §3 and it is enforced
by a check that runs in every build and on every CI leg (§3), not by review discipline.

**Tests assert behaviour, not implementation.** A refactor that changes no behaviour breaks no test.
If a test fails during a pure refactor, the test was testing the wrong thing.

**Correctness is tested by oracles wherever one exists.** The interesting parts of this project —
edit distance, tokenization, WAL replay — all have obvious slow reference implementations. Compare
against them on random input rather than hand-writing thirty examples.

**The tests most likely to be skipped are the ones most likely to find bugs.** Crash recovery and
concurrent ingest are both unglamorous and both where real defects live.

---

## 2. Test taxonomy

| Layer | Scope | Speed | Where |
|---|---|---|---|
| Unit | One `core` type, no I/O, no threads | < 1 ms each | `tests/unit/` |
| Property | Randomized input against a reference oracle | seconds | `tests/unit/` |
| Integration | Real HTTP against a real server on a temp data dir | < 5 s each | `tests/integration/` |
| Contract | Generated from `02-api-spec.yaml` | seconds | `tests/contract/` |
| Recovery | Fault injection, kill, restart, assert | tens of seconds | `tests/recovery/` |
| Fuzz | libFuzzer over parser boundaries | continuous | `tests/fuzz/` |
| Benchmark | Fixed dataset, Google Benchmark | minutes | `tests/bench/` |
| Relevance | Query set with expected results | seconds | `tests/relevance/` |

The unit and property layers carry the weight. Integration tests exist to prove the wiring, not to
re-test logic that a unit test already covers — one integration test per endpoint behaviour, not per
edge case.

### Property tests and their oracles

| Under test | Oracle |
|---|---|
| Damerau-Levenshtein automaton (FR-23) | Brute-force edit distance over the whole dictionary |
| Tokenizer (FR-20) | Naive reference implementation; round-trip and idempotence |
| Diacritic folding (FR-24) | ICU or a generated table, compared offline |
| Postings compression (ADR-008) | Round-trip: encode then decode equals the input |
| Postings intersection | `std::set_intersection` over uncompressed lists |
| WAL replay (ADR-009) | An in-memory model of the same operations |
| Ranking rule chain (FR-26) | A naive full sort by the same comparator |

Every one of these compares a fast implementation against an obviously-correct slow one on random
input. This is the highest-yield testing in the project and should be written alongside the fast
implementation, not after it.

---

## 3. Traceability, enforced

Every test carries its requirement ID as a Catch2 tag (ADR-006):

```cpp
TEST_CASE("transposition counts as one edit, not two", "[FR-23]") { ... }
```

The ctest entry `NFR-14.requirement_coverage` (`scripts/check_requirement_coverage.cmake`) reads
every `Must` FR from `01-requirements.md` and the covered tags from `leta_tests --list-tags`, and
**fails on any Must FR that has neither a tagged test nor a line in
`tests/untested-requirements.txt`**. It also fails on a line in that file for an FR that now has a
test, on a line naming anything but a Must FR, and on a tag naming an ID the requirements do not
define. Because it is a ctest entry, it runs in every preset, locally and on every CI leg. That turns
NFR-14 from an intention into a gate, and gives a reviewer a single command that shows what any
requirement is backed by.

`tests/untested-requirements.txt` is, at every commit, exactly the set of Must FRs without a test.
Each line names either the milestone whose tests will cover it, or `untestable` with a one-line
reason why no CI test can check it. Milestone lines disappear as milestones land; a milestone is not
done while a line still names it. `untestable` lines are reviewed at each milestone and should stay
near empty.

The gate covers FRs, as NFR-14 words it. NFRs are verified by the benchmark suite, the sanitizer and
fuzz jobs, and image inspection (§5, §6, §8, §10); a test may still carry an NFR tag, and the check
confirms it names a real requirement.

---

## 4. Coverage

Line coverage of `leta_core` ≥ 80% (NFR-14), measured with `llvm-cov`, reported per PR.

The report is the `coverage-report` target of the `coverage` preset
(`scripts/coverage_report.cmake`), run after the coverage tests. It prints `llvm-cov`'s per-file
table for everything under `src/` and fails if the files under `src/core/` — the vendored
`third_party/` excluded — fall below the floor. CI writes the same table into the coverage leg's job
summary. Each report consumes the raw profiles it merges, so a report always describes the binaries
the last test run executed. While `src/core` compiles no code, the gate says so explicitly and
passes; it applies from the first line of core code.

Coverage is a floor, not a target. An uncovered branch in WAL recovery matters more than a covered
accessor, so treat the report as a list of places to look rather than a number to maximize. Do not
write tests to raise the percentage.

---

## 5. Sanitizers

| Build | Runs | Gate |
|---|---|---|
| ASan + UBSan | full unit, property and integration suite | every push |
| TSan | full suite plus a concurrent search-under-ingest stress test | every push |
| MSan | optional, if the toolchain cooperates | not a gate |

TSan matters more here than in most projects because ADR-007 puts a lock-free read path at the centre
of the design. The TSan job must include a test that runs searches and ingestion simultaneously
across threads for several seconds; a TSan build that only runs single-threaded unit tests proves
nothing about the thing at risk.

Sanitizer builds use the glibc-based CI image regardless of the runtime layer chosen in ADR-010.

---

## 6. Fuzzing

Every boundary that consumes bytes from outside the process gets a libFuzzer target (NFR-07):

| Target | Input |
|---|---|
| `fuzz_http_request` | Raw request bytes |
| `fuzz_json_ingest` | Document batch bodies |
| `fuzz_search_params` | Search request bodies |
| `fuzz_settings` | Settings patch bodies |
| `fuzz_wal_replay` | WAL file bytes |
| `fuzz_snapshot_read` | Snapshot file bytes |

The last two matter more than they look: they are the path that runs at startup on files that may
have been truncated by a crash, and a hang or a crash there means a server that will not boot.

Corpora are committed and grow from crashes found. Every crash becomes a permanent regression test
with the crashing input as a fixture. Targets run one hour each nightly and on release branches; a
crash blocks the release.

---

## 7. Crash-recovery testing

The direct test of FR-53, and the one that requires real machinery.

Debug builds compile in a fault-injection hook, controlled by an environment variable, that aborts
the process at a named point:

```
LETA_CRASH_AT=wal_before_fsync | wal_after_fsync | segment_published |
              snapshot_temp_written | snapshot_renamed | wal_truncated
```

The harness, for each point: start the server on a fresh data directory, ingest a known corpus,
acknowledge some writes, trigger the crash, restart, then assert **every acknowledged document is
present and searchable, and no unacknowledged write is visible in a way that contradicts a 5xx the
client received.**

Additionally: truncate the WAL at every byte offset across a range and assert the server either
starts cleanly with a prefix of the data or refuses to start with a clear error — never starts with
silently corrupt data. This is where the CRC in ADR-009 earns its place.

The hook compiles out of release builds entirely.

---

## 8. Benchmarking

Numbers only count if they are reproducible.

**Dataset.** A fixed, committed corpus of 100 000 e-commerce product documents matching the SwadeStack
shape, generated once and stored (or generated from a committed seed). Every benchmark run uses it,
so results are comparable across months.

**Environment.** Recorded with every result: CPU, core count, RAM, kernel, compiler, build type,
container runtime. A number without a machine description is not a result.

**Method.** Google Benchmark for micro-level; a load generator at 50 concurrent clients for the
end-to-end NFR-01 figure. Report p50, p95, p99, p99.9 and max — never the mean, which hides exactly
what the requirement is about.

**What is measured:**

| Metric | Target | Requirement |
|---|---|---|
| Search latency, p50 / p99 | 2 ms / 10 ms | NFR-01, S2 |
| Search latency under concurrent ingest | no p99 degradation | NFR-05 |
| Indexing throughput | ≥ 5 000 docs/s | NFR-02 |
| Startup restore, 100k docs | ≤ 15 s | NFR-03, Q5 |
| Resident memory | ≤ 10× raw JSON | NFR-04 |
| Single-core search throughput | ≥ 2 000 /s | NFR-11 |
| Full reindex of the SwadeStack catalog | ≤ 60 s | S3 |
| Per-stage latency breakdown | fills `03-architecture.md` §5 | — |

**Regression policy.** Benchmarks run nightly and on release branches. A regression above 10% on any
metric opens an issue; above 25% blocks the release. Results are committed so the history is
inspectable, which is also what makes the published numbers credible to a reader of the repository.

---

## 9. Relevance evaluation

The test that Leta is *good*, as distinct from correct. S4 requires ≥ 30 real queries scoring ≥ 90%
top-3 hit rate.

**Building the set.** Take real queries from the SwadeStack search box, weighted toward the head but
including a deliberate tail: misspellings, Swedish text with diacritics, partial SKUs, brand plus
spec combinations, and single-character prefixes. For each, the owner records the product ids that
*should* appear in the top 3. This is manual judgement work and cannot be automated.

**Start collecting now.** This has weeks of lead time, gates v1 completion, and is the single most
common thing to leave until it is too late.

**Format**, committed as JSON:

```json
{ "query": "corsiar ddr5 600", "expect_top3": [4471, 4472], "note": "typo + prefix" }
```

**Metric.** Primary: proportion of queries where at least one expected id is in the top 3. Secondary:
mean reciprocal rank, which is more sensitive and catches relevance drifting downward before the
primary metric moves.

**Cadence.** Runs from M5 onward. Every milestone reports the score and the delta. A drop is treated
as a regression and investigated before new work — relevance decays silently otherwise, and by the
time a human notices, the cause is twenty commits back.

---

## 10. CI

| Job | Trigger | Gate |
|---|---|---|
| Build {GCC 13, Clang 17} × {Debug, Release} × {x86-64, arm64} | push | blocks |
| Unit + property + integration + contract | push | blocks |
| ASan + UBSan | push | blocks |
| TSan, including search-under-ingest stress | push | blocks |
| clang-format + clang-tidy | push | blocks |
| Coverage ≥ 80% core | push | blocks |
| Requirement-coverage check (§3), a ctest entry on every leg | push | blocks |
| `leta_core` links no third-party target (ADR-001) | push | blocks |
| Relevance evaluation | push, from M5 | reports; blocks on a drop |
| Benchmarks on the fixed dataset | nightly, release branches | reports; blocks > 25% regression |
| Fuzz targets, 1 h each | nightly, release branches | blocks on crash |
| Image build + size ≤ 30 MB | release branches | blocks |

Every job is runnable locally through the same CMake preset. Nothing is CI-only, because a check you
cannot reproduce on your machine is a check you will eventually disable.

`main` stays green. A red `main` is fixed before any new work starts.

---

## 11. Release gates

`v1.0.0` ships only when every success criterion in `00-project-brief.md` §6 is demonstrably met:

- [ ] **S1** — two weeks serving SwadeStack production search, no rollback *(M13)*
- [ ] **S2** — p99 ≤ 10 ms at 50 concurrent clients, published with machine details *(M11)*
- [ ] **S3** — full SwadeStack reindex under 60 s *(M13)*
- [ ] **S4** — ≥ 30 query relevance set at ≥ 90% top-3 *(M5 onward)*
- [ ] **S5** — ASan/UBSan/TSan green; fuzz targets clean for 1 h *(M11)*
- [ ] **S6** — a stranger runs Leta from the README in under 10 minutes, timed *(M12)*

S6 is verified by an actual person who has not seen the project, on a machine that is not yours,
while you watch without helping. Every place they hesitate is a documentation defect.
