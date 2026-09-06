# ADR-009 — WAL framing, segment rotation and snapshot format

| | |
|---|---|
| Status | Accepted |
| Date | 2026-09-03 |
| Accepted | 2026-09-06 |
| Requirements | FR-03, FR-14, FR-50, FR-51, FR-52, FR-53, FR-55, NFR-03 |

## Context

FR-53 sets the durability contract precisely: a crash at any point leaves the data directory
recoverable **to the last acknowledged write**. Combined with FR-14's synchronous ingestion, that
means the client sees 200 only after the bytes are durable.

The counterweight is that Leta is explicitly a derived, disposable index. PostgreSQL is the source of
truth and the documented disaster-recovery plan is to wipe `/data` and reindex. That permits a much
simpler format than a real database needs.

Two facts from ADR-007 shape the format: writes continue while the maintenance thread is writing a
snapshot, and readers of on-disk data run at startup on files that a crash may have left torn.

## Decision

**Layout — one directory per index.** `<data>/indexes/<uid>/` holds that index's WAL segments
(`wal-000001.log`, `wal-000002.log`, …) and its snapshot (`snapshot.leta`). Index creation is the
directory's creation. Index deletion renames the directory to `<uid>.deleting` and then removes it;
the rename is the commit point, so a crash mid-delete leaves either a whole index or a trash
directory swept at startup — never a half-deleted one. Index lifecycle is filesystem state, not WAL
records, and every WAL is scoped to one index.

**WAL record framing:**

```
┌────────┬─────────┬────────┬────────┬──────────┬──────────────┐
│ magic  │ version │  type  │ length │  crc32c  │   payload    │
│  4 B   │   2 B   │  2 B   │  4 B   │   4 B    │   length B   │
└────────┴─────────┴────────┴────────┴──────────┴──────────────┘
```

`crc32c` covers every header byte except itself, plus the payload. `length` is bounded by the
largest accepted request body (FR-14: 20 MB) plus framing; a frame declaring more is corrupt and is
rejected **before any allocation**, so a garbage header can never turn into a multi-gigabyte read.

Types: `index_meta` (primary key and timestamps — written at creation and again if the key is
inferred from the first batch, FR-03), `document_upsert` (one record per accepted batch, so record,
fsync and acknowledgement are one unit), `document_delete`, `documents_clear`, `settings_patch`.
Replay is idempotent.

`fsync` before acknowledging (FR-50), with **group commit**: concurrent batches for the same index
waiting on the indexer share one fsync, so throughput is not one sync per batch.

**Rotation and snapshot.** A snapshot is always taken at a segment boundary. The indexer closes
segment N and opens N+1; the maintenance thread then writes the state as of the end of segment N —
documents and settings, not the inverted index — to a temporary file, fsyncs it, renames it over
`snapshot.leta`, fsyncs the directory, and only then deletes segments ≤ N. The rename is the commit
point. The snapshot header records N. Ingest never stalls for a snapshot, and the crash-test fault
point formerly called `wal_truncated` is `wal_segments_deleted`.

**Recovery** loads the snapshot, then replays every segment numbered above its N, in order. In the
**newest** segment, reading stops at the first frame failing magic, length bound, or CRC and
truncates there: a torn trailing record is the expected state after a crash, and the CRC is what
makes it detectable instead of silently loading garbage. A bad frame in any **older** segment, or a
gap in the segment sequence, is real corruption: the server refuses to start with a clear error, and
the remedy is the documented wipe-and-reindex.

Both file kinds begin with a `LETA` magic and a format version. The server refuses to start on an
unknown newer version and migrates from earlier v1 formats (FR-55).

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

**D — one process-wide WAL with per-index snapshots.** One fsync stream and group commit across
indexes. Rejected: truncation would have to wait for every index to snapshot, and index lifecycle
would need to be replayed as records. Per-index directories let each index recover independently.

**E — stall ingest for the duration of a snapshot instead of rotating.** Simplest possible
truncation. Rejected: a 100k-document snapshot is a second or more of blocked writes, for the cost
of a segment counter.

## Consequences

**Easier:** a small format that is easy to fuzz, easy to inspect with `xxd`, and easy to reason
about. Wipe-and-reindex is always a valid recovery path, so no repair tooling is needed. Each index
is self-contained on disk, so backup of one index is a directory copy.

**Harder:** restore time is rebuild time — see the open tension below. Group commit spans one index,
so batches to different indexes fsync separately; SwadeStack has one index, so nothing is lost in
practice. Segment sequencing is one more invariant the recovery reader must check and the fuzz target
must exercise.

**Follow-up:** `05-quality-strategy.md` §7 renames the `wal_truncated` fault point and adds
`wal_segment_rotated`. The M7 spec fixes the snapshot file's internal layout.

**Open tension (tracked as Q5).** NFR-03 requires a 100 000-document restore in ≤ 15 s, while NFR-02
sets ingest throughput at ≥ 5 000 docs/s — which would take 20 s for the same corpus. The two are
reconcilable because restore is a different path: no HTTP, no validation, no per-batch fsync, and
parallelizable across segments. But this is an argument, not a measurement, and it must be measured
in M7. If it does not hold, take alternative A.

## Revisit when

M7 benchmarks restore time, or NFR-02 group-commit measurements show fsync dominating ingest.
