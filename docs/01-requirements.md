# Leta — Requirements

| | |
|---|---|
| Status | Draft v0.1 |
| Scope | v1 unless marked otherwise |
| Last updated | 2026-09-03 |

Priority uses MoSCoW: **M**ust, **S**hould, **C**ould, **W**on't (this release). IDs are stable; never renumber, only deprecate.

## 1. Functional requirements

### 1.1 Indexes

| ID | Pri | Requirement |
|---|---|---|
| FR-01 | M | The server hosts one or more named indexes; index UIDs are `[a-zA-Z0-9_-]{1,64}`. |
| FR-02 | M | An index can be created, listed, inspected and deleted via the API. |
| FR-03 | M | Each index has a `primaryKey` (the document field used as identity). It is set at creation or inferred from the first batch (first field named `id` or ending in `id`, case-insensitive). |
| FR-04 | M | Each index has settings (see §1.4) with sensible defaults; an index is usable immediately after creation with no settings. |

### 1.2 Documents

| ID | Pri | Requirement |
|---|---|---|
| FR-10 | M | Documents are JSON objects. Any field may be present; unknown fields are stored and returned but only searchable attributes are indexed. |
| FR-11 | M | Documents can be added in batches (array of objects). A document whose primary key already exists is replaced in full. |
| FR-12 | M | A single document can be fetched by primary key and deleted by primary key. |
| FR-13 | M | All documents in an index can be deleted in one call. |
| FR-14 | M | Ingestion is synchronous in v1: the call returns after the batch is indexed and durably logged. Batches up to 10 000 documents or 20 MB are accepted. |
| FR-15 | S | Partial update (merge fields into an existing document) via a dedicated endpoint. |
| FR-16 | M | Nested objects and arrays are stored verbatim; string values inside arrays of strings are indexed; nested objects are indexed via dotted paths (`specs.type`). |
| FR-17 | M | Primary key values may be integers or strings and are returned in the same JSON type as submitted. |

### 1.3 Search

| ID | Pri | Requirement |
|---|---|---|
| FR-20 | M | `q` is tokenised the same way as documents; a query with an empty `q` returns documents in insertion order. |
| FR-21 | M | Multi-word queries match documents containing all query terms (AND semantics), with documents matching only some terms returned after full matches when fewer than `limit` full matches exist. |
| FR-22 | M | The last query term is matched as a prefix (search-as-you-type); earlier terms match whole tokens. |
| FR-23 | M | Typo tolerance: terms of length ≥ 5 tolerate 1 edit, ≥ 9 tolerate 2 edits (Damerau-Levenshtein; transposition counts as one). Thresholds are per-index settings. |
| FR-24 | M | Diacritic folding is applied to both documents and queries (`å→a`, `ä→a`, `ö→o`, `é→e`, …, Unicode NFKD strip of combining marks) when the index setting is on (default on). |
| FR-25 | M | Case-insensitive matching. |
| FR-26 | M | Ranking orders results by, in sequence: number of matched query terms (desc), total typo count (asc), attribute rank of the best match (searchable attribute order), proximity of matched terms (asc), exactness (whole-word > prefix), then a stable tiebreak on insertion order. |
| FR-27 | M | Pagination via `limit` (default 20, max 1000) and `offset` (max 10 000). |
| FR-28 | M | The response reports `estimatedTotalHits` and server-side `processingTimeMs`. |
| FR-29 | S | `attributesToRetrieve` limits returned fields. |
| FR-30 | S | Highlighting: `attributesToHighlight` returns a `_formatted` copy of each hit with matched terms wrapped in configurable tags (default `<em>…</em>`). |
| FR-31 | C | `attributesToCrop` returns a window of text around the first match for long fields. |
| FR-32 | W | Filtering (`filter` expression on attributes), faceting (`facets`), and sorting (`sort`). **v2.** The document store and settings model must reserve `filterableAttributes` and `sortableAttributes` so v2 requires no reindex. |
| FR-33 | W | Synonyms, stop words, stemming. **v2+.** |

### 1.4 Settings

| ID | Pri | Requirement |
|---|---|---|
| FR-40 | M | `searchableAttributes`: ordered list of attribute paths to index; `["*"]` (default) indexes every string field in document field order. Order defines attribute rank for FR-26. |
| FR-41 | M | `displayedAttributes`: attributes returned in hits; default `["*"]`. |
| FR-42 | M | `typoTolerance`: `{enabled, minWordSizeForTypos: {oneTypo, twoTypos}, disableOnWords, disableOnAttributes}`. |
| FR-43 | M | `language`: `{foldDiacritics: bool}`. Default true. |
| FR-44 | M | Changing a setting that affects indexing (FR-40, FR-42 word lists, FR-43) triggers an automatic reindex of that index from the stored documents. |
| FR-45 | M | Settings can be read and partially updated; a reset endpoint restores defaults. |

### 1.5 Persistence

| ID | Pri | Requirement |
|---|---|---|
| FR-50 | M | Every accepted document write and settings change is appended to a write-ahead log before the call returns. |
| FR-51 | M | The server periodically writes a snapshot of each index (documents + settings) and truncates the log. Snapshot interval and size thresholds are configurable. |
| FR-52 | M | On startup the server restores every index from snapshot + log and rebuilds the in-memory search structures. |
| FR-53 | M | A crash at any point leaves the data directory recoverable to the last acknowledged write. |
| FR-54 | S | A manual snapshot endpoint (`POST /snapshots`) for backup before upgrades. |
| FR-55 | S | The on-disk format carries a version number; the server refuses to start on an unknown newer version and can migrate from any earlier v1 format. |

### 1.6 Operations

| ID | Pri | Requirement |
|---|---|---|
| FR-60 | M | `GET /health` returns 200 when the server can serve queries; 503 during startup restore. |
| FR-61 | M | `GET /stats` and `GET /indexes/{uid}/stats` report document count, index memory, last snapshot time, and whether indexing is in progress. |
| FR-62 | M | `GET /metrics` exposes Prometheus text format: request counts/latency histograms per route, index sizes, WAL size, snapshot durations. |
| FR-63 | M | Configuration by environment variables (`LETA_*`) and CLI flags; flags win. Settings: bind address, port, data dir, master key, snapshot interval, log level/format, worker threads. |
| FR-64 | S | Optional master key: when `LETA_MASTER_KEY` is set, every route except `/health` requires `Authorization: Bearer <key>`. When unset, the server logs a warning at startup and accepts all requests. |
| FR-65 | M | Structured JSON logs to stdout with request id, route, status, duration. |
| FR-66 | M | Graceful shutdown on SIGTERM/SIGINT: stop accepting connections, finish in-flight requests, flush the log, exit within 10 s. |
| FR-67 | M | Distributed as a multi-arch (`linux/amd64`, `linux/arm64`) OCI image with a non-root user, a declared volume for the data directory, and a `HEALTHCHECK`. |
| FR-68 | M | `leta --version` prints version, git commit, build type and compiler. |

### 1.7 API conventions

| ID | Pri | Requirement |
|---|---|---|
| FR-70 | M | JSON request and response bodies; `Content-Type: application/json`. |
| FR-71 | M | Errors return a JSON body `{ "code": "<snake_case>", "message": "<human readable>", "type": "<invalid_request|auth|not_found|internal>" }` with an appropriate HTTP status. |
| FR-72 | M | Unknown query parameters or body fields are rejected with 400 and the offending field named. |
| FR-73 | M | Every response carries an `X-Request-Id` header (generated or echoed). |
| FR-74 | M | The API is versioned by path prefix only if a breaking change is ever needed; v1 uses no prefix. |

## 2. Non-functional requirements

| ID | Pri | Requirement | Measurement |
|---|---|---|---|
| NFR-01 | M | Search latency: p50 ≤ 2 ms, p99 ≤ 10 ms for a 100k-document index of e-commerce products, 50 concurrent clients, on a 4-vCPU / 8 GB host. | Benchmark suite, CI-run on fixed dataset |
| NFR-02 | M | Indexing throughput ≥ 5 000 documents/s for typical product documents (~1 KB). | Benchmark suite |
| NFR-03 | M | Startup restore of a 100k-document index ≤ 15 s. | Benchmark suite |
| NFR-04 | M | Resident memory ≤ 10× the raw JSON size of the indexed documents. | Benchmark suite |
| NFR-05 | M | Search latency does not degrade during indexing of a batch (reads never block on writes). | Benchmark: search p99 under concurrent ingest |
| NFR-06 | M | No memory errors or data races: ASan, UBSan and TSan builds pass the full test suite in CI. | CI |
| NFR-07 | M | The HTTP parser, JSON ingestion and query parser have libFuzzer targets; no crashes after 1 h per target on release branches. | CI (scheduled) |
| NFR-08 | M | Runs as non-root in the container; no dynamic code, no outbound network calls. | Image inspection |
| NFR-09 | M | Builds with GCC ≥ 13 and Clang ≥ 17 on Linux x86-64 and arm64 with `-Wall -Wextra -Werror`. | CI matrix |
| NFR-10 | S | Builds with MSVC 2022 (not a deployment target; portability check only). | CI, allowed to fail |
| NFR-11 | M | A single core can handle ≥ 2 000 simple searches/s. | Benchmark suite |
| NFR-12 | M | Request body limit and per-connection timeouts prevent trivial DoS (defaults: 20 MB, 30 s). | Test |
| NFR-13 | S | Container image ≤ 30 MB. | CI |
| NFR-14 | M | Unit-test line coverage of the core library ≥ 80%; every FR marked M has at least one test referencing its ID. | CI coverage report |

## 3. Traceability

Each milestone in `04-roadmap.md` lists the FR/NFR IDs it completes. Each test names the ID it covers in its test name or a tag (`[FR-23]`). A requirement with no test is not done.
