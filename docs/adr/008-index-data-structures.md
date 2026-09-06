# ADR-008 — Sorted term dictionary and block-compressed postings

| | |
|---|---|
| Status | Accepted |
| Date | 2026-09-03 |
| Accepted | 2026-09-06 |
| Requirements | FR-21, FR-22, FR-23, FR-26, FR-63, NFR-01, NFR-04 |

## Context

This is the core design decision of the project and the part G4 explicitly asks to be built from
first principles.

Four access patterns must be served by the term dictionary:

| Pattern | Requirement |
|---|---|
| Exact term lookup | FR-21 |
| Prefix lookup on the last query term | FR-22 |
| Fuzzy lookup within 1 or 2 edits | FR-23 |
| Iteration for merges and statistics | internal |

And ranking (FR-26) needs proximity and attribute rank, so postings must carry positions and
attribute ids, not just document ids.

## Decision

**Term dictionary, per segment.** All terms concatenated into one sorted byte blob, with a parallel
`uint32` offset array.

- Exact: binary search.
- Prefix: binary search for the lower bound, then a range scan to the end of the prefix.
- Typo: a Damerau-Levenshtein automaton for the query term, walked over the sorted array. Because the
  array is sorted, whole ranges that the automaton cannot accept are skipped rather than tested.

**Postings, per term.** Document ids ascending, delta-encoded as varints in fixed blocks of 128, with
a skip list over block boundaries. Each occurrence carries `(attribute_id, position)`.

**Intersection.** Leapfrog, driven by the shortest posting list, using the skip list to advance
without decoding skipped blocks.

## Alternatives considered

**A — hash map from term to postings.** O(1) exact lookup, but supports neither prefix nor automaton
traversal, so it would need a second structure alongside it. Rejected as strictly more machinery for
less capability.

**B — trie or DAWG.** The most natural fit for automaton intersection, since the automaton and the
trie are walked in lockstep. Rejected for v1 on memory and cache behaviour: pointer-chasing through a
trie is much worse than a linear scan of a sorted blob at this corpus size, and serialization is more
work.

**C — FST, as in Lucene and Tantivy.** Smaller and faster than any of the above for exactly these
operations. Rejected for v1 on implementation cost: shared-prefix output logic is subtle and would
consume a milestone on its own. **This is the planned upgrade**, gated on measurement rather than on
taste.

## Consequences

**Easier:** one contiguous, cache-friendly structure serving all four access patterns. Trivially
serializable. The automaton walk over a sorted dictionary is a well-documented technique with a
naive oracle available for property testing.

**Harder:**
- A one-character prefix scans a large range. Mitigate with a candidate cap.
- Two-typo expansion on terms of length ≥ 9 (FR-23) can accept a wide slice of the dictionary. This
  is the dominant latency risk in the budget in `03-architecture.md` §5.
- Positions inflate postings memory, which is the second-largest contributor to NFR-04 after raw
  document storage.

**Mitigation, specified now:** cap candidates per query term, ordered by edit distance ascending then
document frequency descending. The cap is a **server-level configuration value**
(`LETA_MAX_CANDIDATES_PER_TERM`, FR-63) with a documented default; it is deliberately not a field in
the `Settings` schema of `02-api-spec.yaml`, so the contract is untouched, and a per-index override
is deferred until a user needs one. Capping makes `estimatedTotalHits` an estimate, which is why
FR-28 names it that.

## Revisit when

Candidate expansion exceeds its 0.60 ms budget on the SwadeStack catalog, or resident memory approaches
NFR-04. Either signal makes the FST worth building.
