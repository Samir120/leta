# ADR-007 — Single writer, immutable segments, atomic snapshot swap

| | |
|---|---|
| Status | Proposed |
| Date | 2026-09-03 |
| Requirements | FR-14, NFR-02, NFR-05, NFR-06 |

## Context

NFR-05 is the sharp constraint: search latency must not degrade while a batch is being indexed, which
means **reads never block on writes**. NFR-06 requires a TSan-clean build of the whole suite. FR-14
makes ingestion synchronous from the client's point of view, so a write must be durable and visible
before the HTTP response returns.

A conventional reader-writer lock satisfies none of this well: under sustained ingest, writers starve
readers exactly when the p99 matters.

## Decision

**Exactly one indexer thread per process.** All mutations funnel through it, so no index data
structure needs internal locking and `core` stays single-threaded and pure.

**Readers take an immutable snapshot.** Each index holds an `std::atomic<std::shared_ptr<const
IndexSnapshot>>`. A query does one atomic load at the start and holds that `shared_ptr` for its whole
lifetime. Publication is a single atomic store. An in-flight query finishes against the snapshot it
started with; memory is reclaimed when the last reader drops it.

**A maintenance thread** handles segment merges, snapshot writes, and WAL truncation, publishing
results the same way.

HTTP worker threads enqueue writes to the indexer and wait on a future, which is what makes FR-14's
synchronous contract hold without any shared mutable index state.

## Alternatives considered

**A — reader-writer lock over a mutable index.** Much simpler and would work at low write rates.
Rejected because it violates NFR-05 by construction: a batch of 10 000 documents holds the write lock
long enough to blow the p99 target.

**B — a fully concurrent lock-free index.** Best theoretical throughput. Rejected: high complexity,
hard to prove correct, and no requirement asks for it. `06-coding-standards.md` §7 already forbids
hand-rolled lock-free structures without a dedicated ADR and a TSan stress test.

**C — thread-per-core with sharded indexes.** The right design at high QPS. Rejected as premature:
NFR-11 asks for 2 000 searches/s on a single core and NFR-01 specifies 50 concurrent clients. This
would be the answer if those numbers rose by an order of magnitude.

## Consequences

**Easier:** the read path is wait-free. The TSan surface is two files instead of the whole codebase.
`core` algorithms are testable with no threads at all. Reasoning about visibility is trivial because
nothing published is ever mutated.

**Harder:**
- Memory peaks while an old snapshot is still referenced. A slow query pins a whole snapshot; a
  1 000-result query with a large scan can hold memory for its duration. Bound this with the
  per-connection timeout (NFR-12) and report snapshot memory in `/stats` (FR-61).
- The single indexer is a throughput ceiling. If NFR-02 is missed, parallelize *inside* the indexer
  (build segment shards concurrently, publish once) rather than admitting a second writer.
- The write queue needs a bound, and a full queue needs a defined behaviour: reject with 503 rather
  than grow without limit.

## Revisit when

Measured ingest throughput misses NFR-02, or `/metrics` shows meaningful time spent waiting in the
write queue.
