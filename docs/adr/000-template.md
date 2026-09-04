# ADR-000 — <short, decision-shaped title>

| | |
|---|---|
| Status | Proposed \| Accepted \| Superseded by ADR-NNN \| Deprecated |
| Date | YYYY-MM-DD |
| Deciders | Owner |
| Requirements | FR-xx, NFR-yy |

> Copy to `adr/NNN-short-title.md`. Numbers are sequential and never reused.
> An accepted ADR is not edited except to change its status. A changed mind is a new ADR that
> supersedes the old one, so the reasoning that was true at the time stays readable.

## Context

What forces are at play. Constraints from the brief, the requirements, the toolchain, the schedule,
the deployment target. What is true today that makes this decision necessary now. No solutions here.

## Decision

One or two sentences, in the active voice: *Leta stores postings as delta-encoded varints in
immutable per-segment blocks.* Then the detail needed to implement it.

## Alternatives considered

At least two, each with a real reason for rejection. An ADR with one option is a note, not a decision.

**A — <name>**
What it is. Why it was rejected — and what would have to change for it to become right.

**B — <name>**
Same.

## Consequences

**Easier:** what this unlocks.

**Harder:** what this costs — and state it honestly. An ADR with no downsides is an ADR that has not
been thought through.

**Follow-up work:** tasks this creates, and what to measure to find out whether the decision was right.

## Revisit when

The concrete signal that would make this worth reopening: a benchmark number, a catalog size, a
requirement change.
