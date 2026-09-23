# Leta

[![CI](https://github.com/Samir120/leta/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Samir120/leta/actions/workflows/ci.yml)
[![Licence: Apache-2.0](https://img.shields.io/badge/licence-Apache--2.0-blue.svg)](LICENSE)
[![Status: pre-alpha](https://img.shields.io/badge/status-pre--alpha-orange.svg)](docs/04-roadmap.md)

> Small, fast, self-hosted product search server in modern C++.

Leta answers typed, typo-tolerant, search-as-you-type queries over a catalog of JSON documents in
single-digit milliseconds, over a plain HTTP/JSON API. Same category as Meilisearch and Typesense:
an index that sits next to your system of record and serves the search box — not a database itself.

## Status

**Pre-alpha. The only runnable behaviour is `leta --version`.**

The specification set under [`docs/`](docs/) is complete, and the build system, test harness and
CI are in place. Product code begins at milestone M1. Progress is tracked milestone by milestone in
[`docs/04-roadmap.md`](docs/04-roadmap.md) and summarised in [`CHANGELOG.md`](CHANGELOG.md).

*What v1 will look like* and *Design targets* below describe the target, not the present.
*Building from source* describes what works today.

## Why another search server

- **Small enough to hold in your head.** One binary, one data directory, one config file. The
  design fits in a document; the codebase is meant to as well.
- **Self-hosted.** No per-query cost, no external dependency, no data leaving your network.
- **Modern C++ engineering, not a black box.** The inverted index, postings compression, typo
  tolerance and ranking are implemented from first principles and documented as such — every
  structural choice has an [ADR](docs/adr/) explaining the alternatives considered.

## What v1 will look like

```bash
docker run -d --name leta \
  -p 127.0.0.1:7700:7700 \
  -v leta-data:/data \
  -e LETA_MASTER_KEY=change-me \
  ghcr.io/samir120/leta:1

curl -s localhost:7700/health
# {"status":"available"}
```

Create an index, add documents, search:

```bash
H='Authorization: Bearer change-me'

curl -X POST localhost:7700/indexes -H "$H" -H 'Content-Type: application/json' \
  -d '{"uid":"products","primaryKey":"id"}'

curl -X POST localhost:7700/indexes/products/documents -H "$H" -H 'Content-Type: application/json' \
  -d '[{"id":1,"name":"Corsair Vengeance DDR5 32GB 6000MHz","brand":"Corsair"}]'

curl -X POST localhost:7700/indexes/products/search -H "$H" -H 'Content-Type: application/json' \
  -d '{"q":"corsiar ddr","limit":3}'
```

```json
{
  "hits": [{ "id": 1, "name": "Corsair Vengeance DDR5 32GB 6000MHz", "brand": "Corsair" }],
  "query": "corsiar ddr",
  "limit": 3, "offset": 0,
  "estimatedTotalHits": 1,
  "processingTimeMs": 0.4
}
```

`corsiar` matched `Corsair` with one typo; `ddr` matched `DDR5` as a prefix because it is the last
term. Swedish and English diacritics fold, so `skarm` finds `Skärm`.

The API contract is [`docs/02-api-spec.yaml`](docs/02-api-spec.yaml) (OpenAPI 3.1). A full
walkthrough, including how to keep the index in sync with a PostgreSQL source of truth, is in
[`docs/02-api-guide.md`](docs/02-api-guide.md).

### Operating it

Configuration is by `LETA_*` environment variables or CLI flags, flags winning. The full list
arrives with milestone M9; one setting deserves a warning now.

**`LETA_WORKER_THREADS`** (`--worker-threads`), default **64**. Leta serves HTTP from a fixed pool
of threads, and a keep-alive connection holds its thread until the connection closes. Node's HTTP
agent keeps connections alive by default, so size the pool to the number of connections your
backend holds open, not to the number of CPU cores. With more open connections than threads, the
surplus waits for a free thread and latency degrades sharply rather than gracefully. `/metrics`
reports how many connections are waiting for a worker, so the condition is visible.
[ADR-003](docs/adr/003-http-library.md) explains the trade-off.

## Design targets

For a 100 000-document catalog on a modest 4-vCPU host:

| | Target |
|---|---|
| Search latency, p50 / p99 | 2 ms / 10 ms at 50 concurrent clients |
| Search latency under concurrent ingest | no p99 degradation — reads never block on writes |
| Indexing throughput | ≥ 5 000 documents/s |
| Startup restore, 100k documents | ≤ 15 s |
| Single-core search throughput | ≥ 2 000 queries/s |
| Container image | ≤ 30 MB (stretch) |

Benchmarks will be published with machine details and a committed dataset so they can be
reproduced. Until then these are targets, not results. The binding list, functional and
non-functional, is [`docs/01-requirements.md`](docs/01-requirements.md).

## What Leta is not

- **Not a primary datastore.** Documents live in your database of record; Leta's index is derived
  and rebuildable. If the index is lost, you re-feed the source.
- **Not v2 yet.** No filtering, faceting, sorting, synonyms, stemming, replication, vector search,
  or admin UI. The v1 non-goals are listed in
  [`docs/00-project-brief.md`](docs/00-project-brief.md) §4 and are binding.
- **Not for the open internet.** Leta is designed to sit behind a backend, not to be exposed
  directly. See [`SECURITY.md`](SECURITY.md).

## Repository layout

```
docs/            specification set, numbered in reading order, plus adr/
include/leta/    public headers (kept minimal; pimpl)
src/core/        pure domain — tokenizer, index, postings, ranker, typo automaton. No third-party deps.
src/application/ services orchestrating core; owns the storage and HTTP port interfaces
src/adapters/    http, storage, config, metrics — replaceable I/O layers
tests/           unit, integration, contract, recovery, relevance, fuzz, bench, data
cmake/           build modules
scripts/         CI enforcement: layering check, requirement-coverage check
```

Dependencies point inward only — `src/core` links nothing but the standard library, and a CI job
fails the build if that changes. Why: [ADR-001](docs/adr/001-ports-and-adapters.md).

## Building from source

Linux only. You need **CMake ≥ 3.25**, **GCC ≥ 13** or **Clang ≥ 17**, **Ninja** and git; the build
fetches and hash-checks everything else. Windows and macOS hosts should use the container (see
[ADR-010](docs/adr/010-linux-container-only.md)).

```bash
git clone https://github.com/Samir120/leta.git
cd leta
cmake --workflow --preset debug        # configure + build + tests
./build/debug/bin/leta --version
```

Six presets, same names for `cmake --preset`, `--build --preset`, `ctest --preset` and
`--workflow --preset`: `debug`, `release`, `asan-ubsan`, `tsan`, `coverage`, `fuzz`. The last two
need Clang. Each writes to `build/<preset>/`. The compiler comes from `CC`/`CXX`; personal overrides
go in `CMakeUserPresets.json`, which is gitignored.

CI runs the same presets with the same commands on exactly those minimum versions — GCC 13 and
Clang 17 on x86-64 and arm64, and one leg on CMake 3.25 — so code that only a newer toolchain
accepts fails there rather than for you ([`ci.yml`](.github/workflows/ci.yml)).

## Documentation

Everything worth knowing about Leta is in [`docs/`](docs/), numbered in reading order.

| Document | Role |
|---|---|
| [`00-project-brief.md`](docs/00-project-brief.md) | Scope, goals, non-goals |
| [`01-requirements.md`](docs/01-requirements.md) | Functional and non-functional requirements (binding) |
| [`02-api-spec.yaml`](docs/02-api-spec.yaml) | OpenAPI 3.1 contract (normative) |
| [`02-api-guide.md`](docs/02-api-guide.md) | API guide with an end-to-end integration walkthrough |
| [`03-architecture.md`](docs/03-architecture.md) | Components, layering, threading, latency budget |
| [`04-roadmap.md`](docs/04-roadmap.md) | Milestones and sequencing |
| [`05-quality-strategy.md`](docs/05-quality-strategy.md) | Testing, fuzzing, benchmarking, relevance eval, CI gates |
| [`06-coding-standards.md`](docs/06-coding-standards.md) | C++ style, error handling, concurrency |
| [`07-ways-of-working.md`](docs/07-ways-of-working.md) | How this project is worked on |
| [`adr/`](docs/adr/) | Architecture Decision Records — the reasoning behind the design |

## About this project

Leta is a solo side project, built in public, and the first production workload is the product
search for [swadestack.com](https://swadestack.com). It is also a deliberate exercise in doing a
small systems project properly: written specifications before code, requirement IDs traced to
tests, decisions recorded as ADRs, and benchmarks that can be reproduced. Implementation is drafted
with an AI pair and reviewed, built and accepted by the maintainer; every design and acceptance
decision is human.

## Contributing

Bug reports, well-scoped feature requests, and pull requests are welcome. Please read
[`CONTRIBUTING.md`](CONTRIBUTING.md) first — the process is deliberate, and PRs that skip it tend
to bounce.

For security issues, do **not** open a public issue: see [`SECURITY.md`](SECURITY.md).

## Licence

[Apache-2.0](LICENSE). Public repository from the first commit.
