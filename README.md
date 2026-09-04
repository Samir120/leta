# Leta

> Small, fast, self-hosted product search server in modern C++.

Leta answers typed, typo-tolerant, search-as-you-type queries over a catalog of JSON documents in
single-digit milliseconds, over a plain HTTP/JSON API. Same category as Meilisearch and Typesense:
an index that sits next to your system of record and serves the search box — not a database itself.

**Status: pre-alpha.** The specification set is complete; implementation is in progress. There is
no released binary yet. See [the roadmap](docs/04-roadmap.md) and [`CHANGELOG.md`](CHANGELOG.md)
for where things stand.

## Why another search server

- **Small enough to hold in your head.** One binary, one data directory, one config file. The
  design fits in a document; the codebase is meant to as well.
- **Self-hosted, no per-query cost, no data leaving your network.**
- **Modern C++ engineering, not a black box.** The inverted index, postings compression,
  typo tolerance and ranking are implemented from first principles and documented as such.

## What v1 looks like once shipped

```bash
docker run -d --name leta \
  -p 127.0.0.1:7700:7700 \
  -v leta-data:/data \
  -e LETA_MASTER_KEY=change-me \
  ghcr.io/samir120/leta:1

curl -s localhost:7700/health
# {"status":"available"}
```

Then create an index, add documents, search:

```bash
H='Authorization: Bearer change-me'

curl -X POST localhost:7700/indexes -H "$H" -H 'Content-Type: application/json' \
  -d '{"uid":"products","primaryKey":"id"}'

curl -X POST localhost:7700/indexes/products/documents -H "$H" -H 'Content-Type: application/json' \
  -d '[{"id":1,"name":"Corsair Vengeance DDR5 32GB 6000MHz","brand":"Corsair"}]'

curl -X POST localhost:7700/indexes/products/search -H "$H" -H 'Content-Type: application/json' \
  -d '{"q":"corsiar ddr","limit":3}'
```

Full walkthrough with a real-world integration: [`docs/02-api-guide.md`](docs/02-api-guide.md).

## Design targets

For a 100 000-document catalog on a modest 4-vCPU host:

| | Target |
|---|---|
| Search latency (p50 / p99) | 2 ms / 10 ms at 50 concurrent clients |
| Indexing throughput | ≥ 5 000 documents/s |
| Startup restore time | ≤ 15 s |
| Single-core search throughput | ≥ 2 000 queries/s |
| Container image size | ≤ 30 MB |

Full requirements — functional and non-functional — in [`docs/01-requirements.md`](docs/01-requirements.md).

## What Leta is not

- **Not a primary datastore.** Documents live in your database of record; Leta's index is derived
  and rebuildable. If the index is lost, you re-feed the source.
- **Not v2 yet.** No filtering, faceting, sorting, synonyms, stemming, replication, vector search,
  or admin UI. The v1 non-goals are enumerated in [`docs/00-project-brief.md`](docs/00-project-brief.md) §4.
- **Not for the open internet.** Leta is designed to sit behind a backend, not to be exposed
  directly. See [`SECURITY.md`](SECURITY.md).

## Building from source

```bash
git clone https://github.com/Samir120/leta.git
cd leta
cmake --workflow --preset dev          # configure + build + unit tests
./build/dev/src/leta --version
```

Requirements: **CMake ≥ 3.25**, **GCC ≥ 13** or **Clang ≥ 17**, **Ninja**. Linux only. Windows and
macOS hosts should use the container (see [ADR-010](docs/adr/010-linux-container-only.md)).

Dependencies are fetched at configure time. Nothing to install manually.

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

## Contributing

Bug reports, well-scoped feature requests, and pull requests are welcome. Please read
[`CONTRIBUTING.md`](CONTRIBUTING.md) first — Leta's process is deliberate, and PRs that ignore it
tend to bounce.

For security issues, do **not** open a public issue: see [`SECURITY.md`](SECURITY.md).

## Licence

[Apache-2.0](LICENSE). Public repository from the first commit.
