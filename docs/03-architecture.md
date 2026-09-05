# Leta — Architecture

| | |
|---|---|
| Status | Draft v0.1 — **proposed, awaiting owner acceptance** |
| Scope | v1 |
| Last updated | 2026-09-03 |

Nine ADRs are proposed alongside this document (`adr/001`–`adr/009`). They are **Proposed**, not
Accepted: this document describes the design they imply so it can be reviewed as a whole. Accepting
or overturning them is the gate on starting M0.

---

## 1. System context

```
┌──────────────┐    HTTP/JSON    ┌───────────┐   read/write   ┌──────────┐
│  SwadeStack     │ ──────────────▶ │           │ ─────────────▶ │  /data   │
│  backend     │                 │   Leta    │                │  WAL +   │
│  (Node.js)   │ ◀────────────── │           │ ◀───────────── │ snapshots│
└──────┬───────┘   hits + meta   └─────┬─────┘                └──────────┘
       │                               │
       │ source of truth               │ /metrics
       ▼                               ▼
┌──────────────┐                 ┌───────────┐
│  PostgreSQL  │                 │Prometheus │
└──────────────┘                 └───────────┘
```

Leta is a **derived, disposable index**. PostgreSQL is the source of truth. Any corruption,
upgrade problem, or format question is answered by wiping `/data` and reindexing. This single
assumption is what allows v1 to skip replication, transactions across indexes, and repair tooling.

The browser never talks to Leta (`02-api-guide.md` §5.1). One Express route forwards, and falls back
to SQL when Leta is unreachable.

---

## 2. Process and layering

One process, one binary, one data directory. Internally, ports and adapters:

```
┌──────────────────────────────────────────────────────────────┐
│ adapters                                                     │
│  http/      routing, request parse, error mapping, auth       │
│  storage/   WAL writer, snapshot reader/writer, data dir      │
│  config/    env + CLI flags → Config                          │
│  metrics/   registry + Prometheus text encoder                │
│  logging/   structured JSON to stdout                         │
└───────────────────────────┬──────────────────────────────────┘
                            │ depends on
┌───────────────────────────▼──────────────────────────────────┐
│ application                                                  │
│  IndexService      create/list/inspect/delete indexes         │
│  IngestService     validate → WAL → build segment → publish   │
│  SearchService     query → plan → execute → rank → project    │
│  SettingsService   read/patch/reset, trigger reindex          │
│  ports:  DocumentStore, WriteAheadLog, SnapshotStore, Clock   │
└───────────────────────────┬──────────────────────────────────┘
                            │ depends on
┌───────────────────────────▼──────────────────────────────────┐
│ core   (standard library only)                               │
│  text/     normalization, diacritic folding, tokenization     │
│  index/    term dictionary, postings, segment, snapshot       │
│  query/    planner, prefix expansion, typo automaton          │
│  rank/     rule chain                                         │
└──────────────────────────────────────────────────────────────┘
```

The dependency rule is one-directional and enforced by the build: `core` links no third-party
target, and CI fails if it gains one. `core` does not know that HTTP, JSON, files, or threads exist.
Everything in `core` is testable with a string in and a struct out.

`application` owns the port interfaces; `adapters` implement them. Swapping cpp-httplib for an
epoll loop, or the snapshot format for a different one, touches one directory.

See **ADR-001** (layering), **ADR-003** (HTTP), **ADR-005** (errors).

---

## 3. Threading model

```
   HTTP worker threads (N = worker_threads, default = hardware_concurrency)
        │  reads: lock-free
        │  writes: enqueue + wait
        ▼
   ┌─────────────────────────────────────────────────────────┐
   │  atomic shared_ptr<const IndexSnapshot>  per index       │
   └─────────────────────────────────────────────────────────┘
        ▲ publish (single atomic store)
        │
   indexer thread (exactly one per process)
        │  drains the write queue, appends to WAL, fsyncs,
        │  builds a new segment, publishes a new snapshot
        ▼
   maintenance thread
        │  segment merge, snapshot write, WAL truncation
```

- **Readers never block on writers** (NFR-05). A query loads the snapshot pointer once at the start
  and holds that `shared_ptr` for its whole lifetime. A concurrent publish creates a new snapshot;
  the in-flight query finishes against the old one and releases it. Memory is reclaimed when the
  last reader drops it.
- **Exactly one writer.** All mutations funnel through the indexer thread, so no index data
  structure needs internal locking. Concurrency lives in two files, not scattered through `core`.
- Ingestion is synchronous from the client's point of view (FR-14): the HTTP thread enqueues the
  batch and waits on a future that the indexer completes after the WAL fsync and the publish.
- Every shared declaration carries a comment naming which thread writes it and under what ordering.

See **ADR-007**.

---

## 4. Data model

```
Index
 ├── uid, primaryKey, Settings
 ├── DocumentStore          docId → raw JSON bytes (verbatim, FR-10/16/17)
 ├── primary key map        key → docId
 └── IndexSnapshot (immutable, atomically swapped)
      ├── segments[]        each immutable once published
      │    ├── term dictionary   sorted terms + offsets, one blob
      │    ├── postings          per term: docIds + (attribute, position)
      │    └── docId range
      ├── tombstones        bitmap of deleted docIds
      └── docCount, memory accounting
```

`docId` is a dense internal `uint32_t` assigned in insertion order, which makes it both the array
index and the stable tiebreak required by FR-26. The client's primary key (string or integer) maps
to it and is never exposed internally.

**Documents are stored as the raw bytes the client sent.** This satisfies FR-10 (unknown fields
stored and returned), FR-16 (nested objects and arrays verbatim), and FR-17 (primary key returned in
the submitted JSON type) with no type-fidelity bugs, and makes retrieval a slice copy. Projection
for `displayedAttributes` and `attributesToRetrieve` parses on the way out, for at most `limit`
documents per query. See **ADR-004**.

Segments follow the Lucene model: a batch produces a new immutable segment; deletes set a tombstone
bit; a background merge compacts small segments. Nothing published is ever mutated, which is what
makes the lock-free read path safe.

---

## 5. Search path

`POST /indexes/products/search  {"q": "corsiar ddr5 600", "limit": 3}`

```
1. http      parse body, reject unknown fields (FR-72), enforce limits (NFR-12)
2. service   load snapshot pointer (one atomic load)
3. text      normalize → fold diacritics (FR-24) → lowercase (FR-25) → tokenize (FR-20)
                ["corsiar", "ddr5", "600"]
4. plan      last term is a prefix (FR-22); earlier terms are whole tokens
             per term, expand candidates against each segment's dictionary:
               exact          binary search
               prefix         binary search + range scan
               typo (FR-23)   Damerau-Levenshtein automaton walked over the
                              sorted dictionary, skipping non-matching ranges
                ["corsair"(1 typo), "corsiar"(0)] × ["ddr5"] × ["6000mhz", "600w", …]
5. execute   intersect postings, AND semantics (FR-21); collect partial matches
             into a lower tier used only if full matches < limit
6. rank      rule chain in FR-26 order: matched terms ↓, typos ↑, attribute rank ↑,
             proximity ↑, exactness ↓, docId ↑ — evaluated lazily, short-circuiting
             on the first discriminating rule
7. project   top-k only: fetch raw document, apply displayedAttributes,
             build _formatted if requested (FR-30)
8. http      serialize, add estimatedTotalHits + processingTimeMs (FR-28), X-Request-Id
```

**Latency budget** for the p50 ≤ 2 ms target (NFR-01), 100k documents:

| Stage | Budget |
|---|---|
| HTTP read + body parse | 0.15 ms |
| Normalize + tokenize | 0.05 ms |
| Candidate expansion (3 terms × segments) | 0.60 ms |
| Postings intersection | 0.70 ms |
| Ranking + top-k selection | 0.20 ms |
| Document fetch + projection + serialize | 0.30 ms |
| **Total** | **2.0 ms** |

This budget is a design constraint, not a measurement. Each stage gets a benchmark in M11 and the
table is updated with real numbers. The dominant risk is candidate expansion when a two-typo term
(FR-23, length ≥ 9) matches a large slice of the dictionary; the mitigation is a cap on candidates
per term, ordered by edit distance then frequency.

---

## 6. Write path

`POST /indexes/products/documents  [ … 1000 docs … ]`

```
1. http      size check (FR-14: ≤ 10 000 docs / 20 MB) before allocating
2. parse     simdjson, streaming; extract primary key and searchable fields
3. validate  primary key present and of a permitted type; UTF-8 valid
4. enqueue   hand to the indexer thread, HTTP thread waits
5. WAL       append framed records, CRC, fsync  ← the acknowledgement point (FR-50)
6. build     assign docIds, tokenize searchable attributes, build a segment
7. publish   atomic store of the new snapshot
8. respond   200 with the count
```

The fsync at step 5 is what FR-53 means by "recoverable to the last acknowledged write": the client
is told 200 only after the bytes are durable. Everything after step 5 is reconstructible from the
WAL, so a crash between 5 and 7 loses nothing.

Replacing a document by primary key (FR-11) is a tombstone on the old docId plus an append, which is
what makes reindexing into a live index safe (`02-api-guide.md` §4.1).

---

## 7. Persistence and recovery

**WAL record framing:**

```
┌────────┬─────────┬────────┬────────┬──────────┬──────────────┐
│ magic  │ version │  type  │ length │  crc32c  │   payload    │
│  4 B   │   2 B   │  2 B   │  4 B   │   4 B    │   length B   │
└────────┴─────────┴────────┴────────┴──────────┴──────────────┘
```

Types: `document_upsert`, `document_delete`, `documents_clear`, `settings_patch`, `index_create`,
`index_delete`. Replay is idempotent, so a partial trailing record is truncated and ignored — the
CRC is what makes a torn write detectable rather than silently corrupting.

**Snapshot** contains documents and settings, not the inverted index. On startup the server replays
snapshot + WAL and rebuilds the search structures (FR-52), serving 503 on `/health` until done
(FR-60).

> **Open tension.** NFR-03 requires a 100k-document restore in ≤ 15 s, while NFR-02 sets ingest
> throughput at ≥ 5 000 docs/s — which would take 20 s for the same corpus. Restore is a different
> path (no HTTP, no validation, no per-batch fsync, and parallelizable across segments), so it
> should comfortably exceed the ingest rate. But this needs to be measured by M7, and if it does not
> hold, the fix is to serialize the built index into the snapshot rather than rebuilding it. Flagged
> in `STATE.md` §6 as Q5.

The format carries a version number; the server refuses to start on a newer unknown version and
migrates from earlier v1 formats (FR-55). See **ADR-009**.

---

## 8. Index data structures

**Term dictionary (per segment).** Terms concatenated into one sorted blob with an offset array.
Exact lookup is a binary search; prefix is a binary search plus a range scan; typo candidates come
from walking the sorted array under a Damerau-Levenshtein automaton, which lets whole ranges be
skipped. An FST would be smaller and faster for typo expansion and is the planned upgrade, with the
trigger being a measured candidate-expansion cost above its budget in §5.

**Postings.** Per term, docIds in ascending order, delta-encoded as varints in fixed blocks with a
skip list over block boundaries. Positions and attribute ids are stored per occurrence because FR-26
requires proximity and attribute rank. Block-level compression means intersection can skip whole
blocks without decoding them.

**Intersection.** Leapfrog over the shortest posting list first, using the skip list to advance.

See **ADR-008**.

---

## 9. Cross-cutting concerns

**Configuration** — `Config` is built once at startup from environment then CLI flags (flags win,
FR-63), validated, and passed by const reference. No global access, no re-reading, no hot reload in
v1.

**Errors** — `leta::Result<T, Error>` through core and application; exceptions only for
unrecoverable conditions, caught at the HTTP boundary. `Error` carries the stable `code`, the
`type`, and a human message; the HTTP adapter is the only place that knows about status codes
(FR-71). Internal detail goes to the log, correlated by `X-Request-Id`. See **ADR-005**.

**Observability** — every request logs one structured JSON line (FR-65). Metrics are a small
in-process registry with counters and histograms, encoded to Prometheus text on scrape (FR-62);
writing the ~200 lines of encoder avoids a heavy dependency and the 30 MB image limit (NFR-13).

**Auth** — when `LETA_MASTER_KEY` is set, a middleware checks `Authorization: Bearer` on every route
except `/health`, comparing in constant time. Unset means open, with a startup warning (FR-64).

**Shutdown** — SIGTERM/SIGINT stops the listener, drains in-flight requests, flushes the WAL,
writes a final snapshot if cheap, and exits within 10 s (FR-66).

---

## 10. What this architecture deliberately does not do

- No async ingestion or task queue. FR-14 makes ingestion synchronous; the outbox pattern lives in
  the client (`02-api-guide.md` §4.2).
- No query cache. It would hide the real latency and complicate invalidation. Revisit only if
  benchmarks show repeated identical queries dominate.
- No memory-mapped index. Everything is in RAM (NFR-04 permits 10× raw JSON). Simpler, and the
  target catalogs fit.
- No plugin system, no scripting, no dynamic loading (NFR-08).

---

## 11. ADR index

| ADR | Decision | Status |
|---|---|---|
| ADR-001 | Ports and adapters layering, enforced by the build | Proposed |
| ADR-002 | CMake + presets + CPM.cmake for dependencies | Proposed |
| ADR-003 | cpp-httplib behind an `HttpServer` port | Proposed |
| ADR-004 | simdjson for ingest; documents stored as raw bytes | Proposed |
| ADR-005 | `leta::Result<T, Error>` over `tl::expected` | Proposed |
| ADR-006 | Catch2 v3 with FR-ID tags | Proposed |
| ADR-007 | Single writer, immutable segments, snapshot swap | Proposed |
| ADR-008 | Sorted term dictionary + block-compressed postings | Proposed |
| ADR-009 | WAL framing and snapshot format | Proposed |
| ADR-010 | Linux container as the only deployment artifact | Proposed |
