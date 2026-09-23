<!--
Title: a Conventional Commit subject, e.g. `feat(core): Damerau-Levenshtein automaton for typo candidates`.
PRs are squash-merged, so the title becomes the commit on main.
-->

## What and why

<!-- The observable behaviour this changes, and why. One paragraph. -->

Covers: <!-- requirement IDs this completes, e.g. FR-23 — or "none" -->
Refs: <!-- ADRs and issues, e.g. ADR-008, #12 -->

## Verification

<!-- The commands you ran and the lines of output that show they passed. CI re-runs them on every
leg; this records what you saw locally. -->

## Definition of Done — [`docs/07-ways-of-working.md`](../docs/07-ways-of-working.md) §3

A box that does not apply is ticked and marked *n/a*.

- [ ] Behaviour matches the spec; the API matches `docs/02-api-spec.yaml` where relevant
- [ ] Tests exist, are tagged with their FR/NFR IDs, and pass
- [ ] Clean under `-Wall -Wextra -Werror` on GCC and Clang (NFR-09)
- [ ] ASan + UBSan clean; TSan clean if threading is touched (NFR-06)
- [ ] Fuzz target added or extended if external input is parsed (NFR-07)
- [ ] Public headers documented; non-obvious internals commented
- [ ] No new TODO without a tracked issue
- [ ] Docs updated if behaviour, configuration or the API changed
- [ ] Benchmarks re-run and committed if a hot path changed
- [ ] Behaviour changes and refactors are in separate commits
