# ADR-001 — Ports and adapters layering, enforced by the build

| | |
|---|---|
| Status | Accepted |
| Date | 2026-09-03 |
| Accepted | 2026-09-06 |
| Requirements | G4, NFR-06, NFR-14 |

## Context

Leta is one small binary, so a layered architecture could reasonably be called overkill. Three things
make it worth the cost anyway. G4 asks for a codebase that demonstrates serious engineering to
reviewers. NFR-14 requires 80% coverage of the core library, which is only cheap if core logic has no
I/O. And the highest-risk decisions in this project — the HTTP library, the on-disk format, the index
data structures — are exactly the ones most likely to be replaced after benchmarks.

## Decision

Three layers with dependencies pointing inward only: `core` (pure domain), `application` (services
and port interfaces), `adapters` (HTTP, storage, config, metrics, logging).

`core` links against the standard library and nothing else. The CMake target for `leta_core` declares
no third-party dependency, and CI fails the build if one appears. The single documented exception is
the vendored `tl::expected` header inside `core` (ADR-005): a copied file, not a linked target.

`application` defines the port interfaces it needs (`WriteAheadLog`, `SnapshotStore`, `Clock`);
`adapters` implement them. Dependencies are injected through constructors — no singletons, no
service locator.

`DocumentStore` — the in-memory raw-byte document arena of ADR-004 — is a `core` type, not a port.
Every byte of it lives in RAM (`03-architecture.md` §10), and a port with exactly one in-memory
implementation is the ceremony alternative B rejects. It becomes a port only if a disk-backed store
is ever introduced.

## Alternatives considered

**A — Flat structure, one `src/` directory.** Faster to start and honest about the project's size.
Rejected because core algorithms would end up depending on the JSON and HTTP types they were handed,
making unit tests require a server and a temp directory. That cost compounds across 12 milestones.

**B — Full Clean Architecture with use-case objects and DTOs at every boundary.** Rejected as
ceremony: with one developer and 18 API operations, the mapping layers would outweigh the logic.
Three layers is the smallest structure that gets the testability benefit.

## Consequences

**Easier:** core is testable with a string in and a struct out, in milliseconds, with no fixtures.
Swapping the HTTP library or storage format touches one directory. The layering is self-documenting
for a reviewer.

**Harder:** some data crosses a boundary that a flat design would pass directly, which costs a
conversion. Interfaces must be defined before implementations, which front-loads design work.

**Follow-up:** M0 adds the CMake target structure and the CI check that `leta_core` has no
third-party link dependency.

## Revisit when

Never, realistically. Reversing this mid-project would be a rewrite; that is the point of deciding it
first. The one narrower trigger is a disk-backed document store, which would promote `DocumentStore`
to a port without touching the layering itself.
