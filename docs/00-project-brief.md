# Leta — Project Brief

| | |
|---|---|
| Status | Draft v0.3 |
| Owner | Samir (SwadeStack agency) |
| Last updated | 2026-09-06 |

## 1. What Leta is

Leta (Swedish: *to search for*) is a small, fast, self-hosted product search server written in modern C++. It answers typed, typo-tolerant, search-as-you-type queries over a catalog of JSON documents in single-digit milliseconds and exposes a plain HTTP/JSON API so any web backend can use it.

It belongs to the same category as Meilisearch and Typesense: an index that sits next to the system of record (a relational database) and serves the search box, not a database itself.

## 2. Problem

Relational databases are the right place to store a product catalog and the wrong place to serve its search box:

- `LIKE '%term%'` and full-text search do not tolerate typos, do not match prefixes as the user types, and rank poorly on short product names.
- Latency grows with catalog size and query complexity; a search box that fires on every keystroke needs consistent sub-10 ms responses.
- Combining text relevance with structured attributes (brand, socket, price) in one query is awkward in SQL and gets worse as the catalog grows. (Filters and facets are v2 scope; the v1 data model must not prevent them.)

Hosted search services solve this but add per-query cost, external dependency, and data egress. The existing open-source alternatives are excellent but large; a compact engine whose entire design one person can hold in their head is both a practical tool and a strong learning vehicle.

## 3. Goals

**G1 — Usable in production for SwadeStack.** Leta serves the product search for SwadeStack agency (swadestack.com — React/TypeScript frontend, Node.js backend, PostgreSQL). Its catalog (tens of thousands of products across ~80 categories) is the first and primary workload.

**G2 — Fast and predictable.** p99 query latency under 10 ms for a 100k-document index on a modest server, with flat latency as the index grows.

**G3 — Simple to operate.** One container, one data directory, one config file or environment variables, health and metrics endpoints, rebuildable from the source of truth at any time.

**G4 — Serious C++ engineering.** The index, posting-list compression, typo tolerance and ranking are implemented from first principles. The codebase demonstrates modern C++ (C++20), correct concurrency, sanitizer- and fuzz-clean code, and measured performance.

**G5 — Portfolio-grade.** Public repository, clear documentation, reproducible benchmarks, and a live before/after demo on a real catalog.

## 4. Non-goals (binding for v1)

These are deliberately excluded from v1. Each may become a later milestone, but none may be added to v1 without revising this brief.

- Filtering, faceting, and sorting by attributes (v2).
- Synonyms, stop-word lists, stemming (v2+).
- Multi-user auth, API-key scoping, tenant isolation beyond a single master key (v2).
- Replication, clustering, high availability (v3+).
- Vector / semantic / hybrid search.
- Admin web UI.
- Being a primary datastore. Leta may lose its index; the source of truth is always elsewhere.
- Windows-native binary as a supported deployment target. Leta ships as a Linux container image; Windows hosts run it via a Linux container runtime (see `03-architecture.md`, ADR-010).

## 5. Users

- **Primary:** the SwadeStack backend (Node.js) as the API client, and Samir as the operator.
- **Secondary:** any small-to-medium e-commerce or catalog application that wants self-hosted search without running Elasticsearch.
- **Tertiary:** reviewers of the repository (hiring managers, other engineers) evaluating the code and design.

## 6. Success criteria for v1

v1 is done when all of the following are true:

| # | Criterion | Verified by |
|---|---|---|
| S1 | SwadeStack's search box is served by Leta in production for at least two weeks with no rollback | Operator confirmation |
| S2 | p99 search latency ≤ 10 ms on the SwadeStack catalog, measured at the server, at 50 concurrent clients | Benchmark suite (`05-quality-strategy.md`) |
| S3 | Full reindex of the SwadeStack catalog completes in under 60 seconds | Benchmark suite |
| S4 | Relevance evaluation set (≥ 30 real queries) scores ≥ 90% top-3 hit rate | Relevance eval (`05-quality-strategy.md`) |
| S5 | CI is green with ASan/UBSan and TSan builds, and the fuzz targets run clean for 1 h | CI |
| S6 | A stranger can run Leta from the README in under 10 minutes using only the Docker image | Manual walkthrough |

## 7. Constraints and assumptions

- Single developer, evenings and weekends; the roadmap assumes roughly 6–10 hours per week.
- Implementation is executed by an AI coding agent against milestone specifications; design and acceptance decisions stay with the owner.
- Development on Linux only; no Windows or macOS development environment is supported.
- Production: Leta runs co-located with the SwadeStack backend and PostgreSQL, on the host they already share, through that host's Linux container runtime (ADR-010). It shares CPU with both; the S2 benchmark host is dedicated, so production p99 is expected to sit somewhat above the published number.
- Swedish and English catalog text; diacritics must fold for matching (`skarm` matches `skärm`).
- Apache-2.0 licence, public repository from the first commit.

## 8. Related documents

- `01-requirements.md` — what Leta must do
- `02-api-spec.yaml`, `02-api-guide.md` — how clients talk to it
- `03-architecture.md`, `adr/` — how it is built and why
- `04-roadmap.md` — in what order
- `05-quality-strategy.md` — how we know it works
