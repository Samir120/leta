# ADR-003 — cpp-httplib behind an `HttpServer` port

| | |
|---|---|
| Status | Proposed |
| Date | 2026-09-03 |
| Requirements | NFR-01, NFR-11, NFR-12, NFR-13, R2 |

## Context

Leta needs an HTTP/1.1 server for 18 operations. The load target is modest and specific: p99 ≤ 10 ms
at **50 concurrent clients** (NFR-01) and ≥ 2 000 simple searches/s on one core (NFR-11). Writing an
event-loop HTTP stack is the most tempting part of this project and, per risk R2 in `STATE.md`, the
most likely to consume the entire budget. The brief's G4 names the index, compression, typo tolerance
and ranking as the from-scratch work — HTTP is not on that list.

## Decision

Use **cpp-httplib** (header-only, thread-per-connection) for v1, behind a project-owned `HttpServer`
port interface in `application`. Handlers never see a cpp-httplib type.

Enforce NFR-12 through the library's payload limit (20 MB) and read/write timeouts (30 s).

## Alternatives considered

**A — Hand-rolled epoll or io_uring loop.** The most interesting option and the best learning value.
Rejected for v1 on scheduling grounds: it is a milestone-sized project on its own, and at 50
concurrent connections it buys nothing measurable. Thread-per-connection at that concurrency costs
about 50 stacks and no meaningful scheduler pressure. The port interface keeps this available as a
post-v1 milestone, where it becomes a genuine, benchmark-justified improvement rather than
speculative work.

**B — Boost.Beast.** Correct and fast, but pulls in a large dependency that threatens NFR-13, and
its async model would shape the whole application layer around it.

**C — A framework (Drogon, oat++).** Brings routing, JSON, and ORM opinions Leta does not want, and
inverts control over the layering in ADR-001.

## Consequences

**Easier:** the HTTP milestone is days, not weeks. One header, no build complications on arm64.

**Harder:** thread-per-connection will not scale to thousands of connections, and the library gives
limited control over buffer reuse — so the HTTP layer will likely be the first bottleneck when
benchmarks run in M11. That is acceptable if measured; it is not acceptable if assumed.

**Follow-up:** M11 must attribute latency between the HTTP layer and the search path, so the budget
in `03-architecture.md` §5 can be validated per stage rather than end to end.

## Revisit when

M11 benchmarks show the HTTP layer costing more than its 0.15 ms budget, or a deployment needs more
than a few hundred concurrent connections.
