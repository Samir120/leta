# ADR-009 — WAL framing and snapshot format

| | |
|---|---|
| Status | Proposed |
| Date | 2026-09-03 |
| Requirements | FR-50, FR-51, FR-52, FR-53, FR-55, NFR-03 |

## Context

FR-53 sets the durability contract precisely: a crash at any point leaves the data directory
recoverable **to the last acknowledged write**. Combined with FR-14's synchronous ingestion, that
means the client sees 200 only after the bytes are durable.

The counterweight is that Leta is explicitly a derived, disposable index. PostgreSQL is the source of
truth and the documented disaster-recovery plan is to wipe `/data` and reindex. That permits a much
simpler format than a real database needs.

## Decision

**WAL record framing:**

```
┌────────┬─────────┬────────┬────────┬──────────┬──────────────┐
│ magic  │ version │  type  │ length │  crc32c  │   payload    │
│  4 B   │   2 B   │  2 B   │  4 B   │   4 B    │   length B   │
└────────┴─────────┴────────┴────────┴──────────┴──────────────┘
```

Types: `document_upsert`, `document_delete`, `documents_clear`, `settings_patch`, `index_create`,
`index_delete`. Replay is idempotent.

`fsync` before acknowledging (FR-50), with **group commit**: concurrent batches waiting on the
indexer share one fsync, so throughput is not one sync per batch.

**Recovery** reads forward and stops at the first frame failing magic, length, or CRC, then truncates
there. A torn trailing record is the expected state after a crash, not corruption — the CRC is what
makes the difference detectable instead of silently loading garbage.

**Snapshot** contains documents and settings, not the inverted index. Written to a temporary file,
fsynced, renamed, then the directory fsynced, and only then is the WAL truncated. The rename is the
commit point.

Both files begin with a `LETA` magic and a format version. The server refuses to start on an unknown
newer version and migrates from earlier v1 formats (FR-55).

## Alternatives considered

**A — persist the built index alongside the documents.** Restore becomes a load instead of a rebuild,
which removes the NFR-03 risk entirely. Rejected for v1: a much larger and more brittle format, and
every future change to the index structures becomes a migration. **This is the designated fallback**
if the tension below is not resolved by measurement.

**B — snapshot only, no WAL.** Simpler, and defensible given the source of truth is elsewhere.
Rejected: it violates FR-53 directly, since everything since the last snapshot would be lost.

**C — embed SQLite or an existing KV store for durability.** Would solve this in an afternoon.
Rejected on two grounds: it moves the most interesting remaining systems problem out of the project
(G4), and it works against the 30 MB image budget.

## Consequences

**Easier:** a small format that is easy to fuzz, easy to inspect with `xxd`, and easy to reason
about. Wipe-and-reindex is always a valid recovery path, so no repair tooling is needed.

**Harder:** restore time is rebuild time. See the open tension below.

**Open tension (tracked as Q5).** NFR-03 requires a 100 000-document restore in ≤ 15 s, while NFR-02
sets ingest throughput at ≥ 5 000 docs/s — which would take 20 s for the same corpus. The two are
reconcilable because restore is a different path: no HTTP, no validation, no per-batch fsync, and
parallelizable across segments. But this is an argument, not a measurement, and it must be measured
in M7. If it does not hold, take alternative A.

## Revisit when

M7 benchmarks restore time, or NFR-02 group-commit measurements show fsync dominating ingest.
