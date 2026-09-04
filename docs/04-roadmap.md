# Leta — Roadmap

| | |
|---|---|
| Status | Draft v0.1 — **proposed, awaiting owner acceptance** |
| Scope | v1 |
| Last updated | 2026-09-03 |

---

## 1. How to read this

A **milestone** is a shippable increment with a demo you can run from a clean checkout. It is done
when every task inside it meets the Definition of Done in `07-ways-of-working.md` §3, every `Must`
requirement it claims has a test naming its ID, and the exit demo works.

Estimates are in **sessions** of roughly three focused hours. The brief budgets 6–10 hours per week,
so two to three sessions a week. Estimates are for planning, not commitments; record actuals in
`STATE.md` and let the ratio inform later milestones rather than pretending the first guess was right.

## 2. Sequencing principle

**Vertical before horizontal.** M1 builds a thin end-to-end slice — HTTP in, one document indexed,
one exact term searched, JSON out — before any component is built properly. Every milestone after it
deepens a slice that already runs.

This costs a little rework in M1 and buys three things: the API contract in `02-api-spec.yaml` is
exercised from week two instead of month four, every milestone has a demo, and integration problems
surface while they are cheap.

The alternative — building the tokenizer, then the index, then ranking, then the HTTP layer — defers
all integration risk to the end and gives you nothing runnable for months. For a solo project on
evenings, that is also a motivation problem, not just an engineering one.

## 3. Release checkpoints

| Tag | After | What it is |
|---|---|---|
| `v0.1.0` | M7 | Search works, data survives restart. Public, honest about being incomplete. |
| `v0.9.0` | M11 | Feature-complete against v1 requirements, benchmarks published. |
| `v1.0.0` | M12 | Packaged, documented, S6 verified. |
| — | M13 | Running the SwadeStack search box in production; S1 clock starts. |

Publishing at `v0.1.0` matters. A repository that has been public and progressing for months reads
very differently from one that appears fully formed the week before a job application.

---

## 4. Milestones

### M0 — Scaffold and CI · 4 sessions

Nothing but the machinery. No product code.

- CMake ≥ 3.25 with presets (`debug`, `release`, `asan`, `tsan`, `coverage`); CPM.cmake wired
- Layer targets `leta_core`, `leta_application`, `leta_adapters`, with the CI check that `leta_core`
  links no third-party target (ADR-001)
- clang-format and clang-tidy configs; Catch2 v3 and Google Benchmark; empty `leta --version` binary
- CI matrix {GCC 13, Clang 17} × {Debug, Release} × {x86-64, arm64}
- The requirement-coverage script from ADR-006: parse `--list-tags`, diff against `Must` IDs, fail on a gap
- Dockerfile skeleton, non-root, multi-arch; README with a build section

**Closes:** FR-68, NFR-09, and the scaffolding for NFR-06/13/14
**Exit demo:** `cmake --workflow --preset ci` green on both compilers; `docker run …/leta --version` prints version, commit, build type, compiler.

### M1 — Walking skeleton · 4 sessions

The thinnest end-to-end path, in memory only, with everything hardcoded that can be.

- HTTP server behind the `HttpServer` port (ADR-003); `GET /health`; JSON error model (FR-71)
- `POST /indexes`, `POST /indexes/{uid}/documents`, `POST /indexes/{uid}/search`
- Whitespace-split tokenizer, `std::map<term, vector<DocId>>`, single-term exact match only
- `X-Request-Id`, structured request logging

**Closes:** FR-70, FR-71, FR-73, FR-60 (partial)
**Exit demo:** create an index, post three documents, search one word, get a hit — over real HTTP.
**Explicitly throwaway:** the tokenizer and index here are placeholders. Say so in the code.

### M2 — Text pipeline · 4 sessions

- Unicode normalization, NFKD combining-mark strip, diacritic folding (FR-24), case folding (FR-25)
- Tokenizer proper: word and number handling, the same path for documents and queries (FR-20)
- Property tests against a naive reference implementation; fuzz target on the tokenizer

**Closes:** FR-20, FR-24, FR-25
**Exit demo:** `skarm` finds `Skärm`; `MSI` finds `msi`.

### M3 — Inverted index and multi-term search · 6 sessions

- `DocumentStore`: raw-byte storage, primary key map, dense `DocId` assignment (ADR-004)
- Sorted term dictionary and postings with positions and attribute ids (ADR-008)
- Segment model, immutable once published; tombstones for replace and delete
- AND semantics with the partial-match lower tier (FR-21); insertion-order results for empty `q`
- Batch add, get by key, delete by key, delete all; dotted paths and string arrays (FR-16)

**Closes:** FR-01, FR-02, FR-10 – FR-13, FR-16, FR-17, FR-21
**Exit demo:** 10 000 real SwadeStack products indexed; multi-word queries return correct sets.
**Measure here, not later:** resident memory against NFR-04. Discovering a 10× overrun in M11 is expensive.

### M4 — Prefix and typo tolerance · 8 sessions

The technical heart of the project.

- Prefix expansion on the last term via dictionary range scan (FR-22)
- Damerau-Levenshtein automaton, walked over the sorted dictionary with range skipping (FR-23)
- Length thresholds (≥ 5 → one edit, ≥ 9 → two), per-index configurable
- Candidate cap ordered by edit distance then document frequency (ADR-008)
- Property test: automaton results identical to a brute-force edit-distance scan over the dictionary

**Closes:** FR-22, FR-23
**Exit demo:** the `02-api-guide.md` §5.2 example — `corsiar ddr5 600` finds the Corsair DDR5 kit.

### M5 — Ranking · 5 sessions

- The FR-26 rule chain in order, evaluated lazily and short-circuiting on the first discriminating rule
- Proximity from positions; exactness distinguishing whole-word from prefix; attribute rank from
  `searchableAttributes` order; stable `DocId` tiebreak
- Top-k selection without sorting the full candidate set
- **First relevance evaluation run** against the S4 query set

**Closes:** FR-26
**Exit demo:** the eval set scores are recorded; every subsequent milestone reports the delta.

### M6 — Full API surface · 6 sessions

- Every remaining operation in `02-api-spec.yaml`: index CRUD, stats, settings read/patch/reset
- Pagination with limits (FR-27), `estimatedTotalHits` and `processingTimeMs` (FR-28),
  `attributesToRetrieve` (FR-29), `displayedAttributes` (FR-41)
- Strict unknown-field rejection naming the offending field (FR-72)
- Settings model including `filterableAttributes` and `sortableAttributes`, stored and validated but
  inert (FR-32) — this is what makes v2 a server upgrade rather than a reindex
- Integration test suite driving the spec end to end; fuzz target on the query parser

**Closes:** FR-04, FR-27 – FR-29, FR-40 – FR-43, FR-72, FR-74
**Exit demo:** a schemathesis-style run against `02-api-spec.yaml` finds no contract violations.

### M7 — Persistence · 8 sessions

- WAL with framing, CRC32C, group commit, fsync before acknowledgement (FR-50, ADR-009)
- Snapshot write with the temp-fsync-rename-fsync-dir sequence; WAL truncation (FR-51)
- Startup restore, 503 on `/health` until ready (FR-52, FR-60)
- Format version headers and the refusal path for newer versions (FR-55)
- **Crash-recovery test harness** — fault injection at each fsync point, kill, restart, assert
  recovery to the last acknowledged write (FR-53)
- Manual snapshot endpoint (FR-54); fuzz target on WAL and snapshot readers
- **Resolve Q5**: measure restore time against NFR-03. If it misses, take ADR-009 alternative A.

**Closes:** FR-50 – FR-55, NFR-03
**Exit demo:** kill -9 mid-batch, restart, every acknowledged document is present and searchable.
**→ tag `v0.1.0`.**

### M8 — Settings and reindex · 4 sessions

- Automatic reindex when an indexing-affecting setting changes (FR-44), built as a new snapshot and
  published atomically so search stays available throughout
- Settings reset (FR-45); partial document update (FR-15)
- Primary key inference from the first batch (FR-03)

**Closes:** FR-03, FR-15, FR-44, FR-45

### M9 — Operations · 5 sessions

- `/stats` and `/indexes/{uid}/stats` including index memory and indexing state (FR-61)
- Prometheus registry and text encoder; per-route histograms, index sizes, WAL size, snapshot
  durations (FR-62)
- Config from environment and CLI flags, flags winning (FR-63); master key with constant-time
  comparison (FR-64); structured JSON logs (FR-65)
- Graceful shutdown within 10 s (FR-66)

**Closes:** FR-61 – FR-66
**Exit demo:** a Grafana panel of `leta_search_duration_seconds` under load.

### M10 — Highlighting · 3 sessions

- `attributesToHighlight` producing `_formatted`, tags configurable (FR-30)
- Correct spans for prefix and typo matches, which is where this gets interesting
- `attributesToCrop` if time allows (FR-31, `Could`)

**Closes:** FR-30, possibly FR-31

### M11 — Performance and hardening · 10 sessions

The milestone most likely to be underestimated, and the one that produces the portfolio artifacts.

- Benchmark suite on a fixed committed dataset; fill in the real numbers in the latency budget table
  in `03-architecture.md` §5
- Postings block compression tuning; profile-driven optimization of whatever actually dominates
- Meet NFR-01, NFR-02, NFR-04, NFR-05, NFR-11 — or document honestly where and why a target is missed
- Full sanitizer sweep; fuzz targets run for an hour each on release branches (NFR-07)
- Load test at 50 concurrent clients with concurrent ingest, proving NFR-05
- Coverage to ≥ 80% of core (NFR-14)

**Closes:** NFR-01, NFR-02, NFR-04 – NFR-07, NFR-11, NFR-12, NFR-14
**→ tag `v0.9.0`.**

### M12 — Packaging and release · 5 sessions

- Runtime layer decision from ADR-010, measured both ways against NFR-13
- Multi-arch image, non-root, volume, `HEALTHCHECK` (FR-67)
- README rewritten for a stranger; the S6 ten-minute walkthrough, timed by someone who is not you
- Apache-2.0 headers, `CHANGELOG.md`, published benchmark results, architecture diagram

**Closes:** FR-67, NFR-08, NFR-13, S6
**→ tag `v1.0.0`.**

### M13 — SwadeStack integration · 4 sessions plus a two-week soak

- Search document mapping and the reindex script (`02-api-guide.md` §3, §4.1)
- Outbox table, worker, and the SQL fallback path (§4.2, §5.1)
- Frontend: search-as-you-type dropdown with abort handling, and the results page
- Deploy, monitor, iterate on relevance against real queries

**Closes:** S1, S3, S4
**Exit:** two weeks in production with no rollback.

---

## 5. Totals and honesty about the calendar

| | Sessions | At 2.5 sessions/week |
|---|---|---|
| M0 – M7 (to `v0.1.0`) | 45 | ~18 weeks |
| M8 – M12 (to `v1.0.0`) | 27 | ~11 weeks |
| M13 | 4 + soak | ~4 weeks |
| **Total** | **76** | **~33 weeks** |

Roughly eight months at the stated budget, which is what a project of this scope actually costs and
is worth writing down rather than discovering. Two consequences: the `v0.1.0` checkpoint at M7 is
important, and if the calendar needs compressing, the honest levers are cutting M10 (highlighting is
`Should`), deferring FR-15 and FR-31, and shortening the M11 optimization pass — not skipping M7's
crash tests or M11's sanitizer work.

## 6. Critical path and risks

The path runs M0 → M1 → M3 → M4 → M5, since ranking depends on positions from the index and typo
expansion depends on the dictionary layout. M2 can slip alongside M3. M9 and M10 are independent of
each other and can be reordered freely.

| Risk | Milestone | Mitigation |
|---|---|---|
| Typo expansion latency blows the budget | M4 | Candidate cap designed in from the start (ADR-008); FST is the escape hatch |
| Restore time misses NFR-03 (Q5) | M7 | Measure at M7, not M11; ADR-009 alternative A is pre-agreed |
| Memory exceeds NFR-04 | M3 | Measure at M3 while the structures are still cheap to change |
| M11 underestimated | M11 | It is already the largest estimate; expect it to be the one that grows |
| Relevance set not ready | M5 | Start collecting SwadeStack queries now — it has weeks of lead time and gates S4 |
