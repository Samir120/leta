# ADR-004 — simdjson for ingest; documents stored as raw bytes

| | |
|---|---|
| Status | Proposed |
| Date | 2026-09-03 |
| Requirements | FR-10, FR-16, FR-17, NFR-02, NFR-07 |

## Context

Two JSON problems, with different characteristics.

**Ingest** is a hot path. NFR-02 asks for ≥ 5 000 documents/s at roughly 1 KB each, and FR-14 allows
batches of 10 000 documents or 20 MB. It is also a hostile boundary and a required fuzz target
(NFR-07).

**Retrieval** is cold by comparison: at most `limit` documents per query, capped at 1 000 (FR-27).

Three requirements constrain storage independently of the parser. FR-10 says unknown fields are
stored and returned. FR-16 says nested objects and arrays are kept verbatim. FR-17 says a primary key
comes back in the JSON type it was submitted as. Together these say: whatever we store must round-trip
the client's document exactly.

## Decision

Parse incoming batches with **simdjson**'s on-demand API. Store **the exact bytes of each document**
in a per-segment arena, indexed by `docId`. Parse again on the way out, only for the documents in the
result page, to apply `displayedAttributes` and `attributesToRetrieve`.

Response serialization is a small hand-written writer rather than a second library dependency: output
shapes are few, fixed by `02-api-spec.yaml`, and the error body in FR-71 needs to be produceable when
things are already going wrong.

## Alternatives considered

**A — nlohmann/json for both directions.** Far more ergonomic and the obvious default. Rejected on
two grounds: parse throughput is roughly an order of magnitude below simdjson, which puts NFR-02 at
risk on its own; and a DOM round-trip re-formats numbers, so a client's `1.10` or a large integer can
come back changed, breaking FR-17 in a way that is easy to miss and painful to debug.

**B — RapidJSON.** Fast and proven, but the in-situ API is sharp-edged, and the project has been
quiet for years. No advantage over simdjson that matters here.

**C — glaze.** Excellent throughput and serialization in one library, but leans on C++23 and reflection
patterns, which conflicts with the C++20 baseline in `06-coding-standards.md` §1.

## Consequences

**Easier:** ingest throughput has headroom. Type fidelity (FR-17) is free rather than defended by
tests. Document retrieval is a slice copy. The fuzz target is one function over a byte buffer.

**Harder:** simdjson yields `string_view`s into the source buffer, which must outlive extraction —
exactly the lifetime hazard flagged in `06-coding-standards.md` §3, and it needs a comment at every
such boundary. The on-demand API is forward-only, so field access order matters and the extraction
code is less obvious than a DOM walk. Storing raw bytes costs the full JSON size in memory, which is
comfortable inside NFR-04's 10× allowance but is the largest single contributor to it.

**Follow-up:** measure projection cost against its 0.30 ms slice of the latency budget
(`03-architecture.md` §5) during M11.

## Revisit when

Projection exceeds its latency budget, at which point store a pre-parsed columnar form of just the
displayed attributes alongside the raw bytes.
