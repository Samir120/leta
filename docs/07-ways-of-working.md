# Leta — Ways of Working

| | |
|---|---|
| Status | v1.1 |
| Audience | the owner and Claude |
| Last updated | 2026-09-21 |

How the project is actually run: one developer, evenings and weekends, implementation drafted in
Claude Project chats and built locally.

---

## 1. The loop

```
        ┌─ 1. Plan ────────── pick the next task from 04-roadmap.md
        │
        ├─ 2. Spec ────────── chat: agree the task spec (§2). Owner accepts.
        │
        ├─ 3. Implement ───── chat: Claude delivers complete files + tests, inline
        │
        ├─ 4. Verify ──────── owner: build, test, sanitizers. Paste output back.
        │
        ├─ 5. Review ──────── chat: review against the checklist (§4)
        │
        ├─ 6. Land ────────── owner: commit, push, CI green
        │
        └─ 7. Record ──────── Claude emits updated STATE.md; owner replaces it
```

**One task per chat**, named after the task (`M2-T3 typo automaton`). Separate chats for writing a
document, specifying, implementing, reviewing, and debugging a specific failure. Long chats
accumulate superseded code and degrade; when a chat is done, take the state update and open a new one.

**Step 3 is inline.** Claude delivers every repository file in the chat, one block per file with
its path on the first line, in paste order, each with an explanation of what it is for and what to
notice — never as an archive or a bundle. The owner places every file by hand; that is the point of
running the project in chats rather than in an agentic tool.

**Step 4 is the only source of truth.** Claude may pre-check files in a sandbox when one is
available, and says so with the toolchain used; that is evidence, not verification. Whether the
code works on the owner's machine is established only by the owner building it. Claude never
asserts that it does.

---

## 2. Task spec template

Kept in the chat, and summarized in `STATE.md` §4 while the task is active.

```markdown
## M<n>-T<n> — <short title>

**Goal** — one sentence, in terms of observable behaviour.

**Requirements covered** — FR-xx, FR-yy, NFR-zz

**Public interface** — the types and functions this task adds or changes, with signatures.

**Files** — created / modified / deleted.

**Design notes** — algorithm, complexity, data structures. Links to the ADR if one applies.

**Tests** — each with its FR/NFR tag and the behaviour it pins down.
  - [FR-23] terms of length 5 tolerate exactly one edit
  - [FR-23] transposition counts as one edit, not two
  - [FR-23] terms of length 4 tolerate none

**Out of scope** — what this task deliberately does not do, and which task does it.

**Acceptance** — the command that demonstrates it and the output that means success.
```

Rules: no implementation code before the spec is accepted. A task that cannot be specified in half a
page is too big — split it. Estimate in sessions, not hours; anything over three sessions is too big.

---

## 3. Definition of Done

A task is DONE only when every box is true. Claude may not tick a box it cannot verify.

- [ ] Behaviour matches the spec; API matches `02-api-spec.yaml` where relevant
- [ ] Tests exist, tagged with FR/NFR IDs, and the owner has confirmed they pass
- [ ] Clean under `-Wall -Wextra -Werror` on GCC and Clang (NFR-09)
- [ ] ASan + UBSan clean; TSan clean if threading is touched (NFR-06)
- [ ] Fuzz target added or extended if external input is parsed (NFR-07)
- [ ] Public headers documented; non-obvious internals commented
- [ ] No new TODOs without an entry in `STATE.md`
- [ ] Docs updated if behaviour, config, or the API changed
- [ ] Benchmarks re-run and committed if the change touches a hot path
- [ ] `STATE.md` updated

A **milestone** is DONE when every task is DONE, every `Must` requirement in its list has a test
naming its ID, and the demo in `04-roadmap.md` runs from a clean checkout.

---

## 4. Code review checklist

Claude reviews its own output against this before presenting it, and reviews pasted code on request.

**Correctness** — edge cases (empty input, single element, maximum size, Unicode, duplicate keys);
integer overflow and narrowing; every error path actually reachable and tested; no UB.

**Design** — does it belong in this layer (`PROJECT_INSTRUCTIONS.md` §6)? Does anything in `core`
now know about HTTP, JSON, or files? Is the interface minimal? Could a test replace a dependency
without a mock framework?

**Lifetimes** — every `string_view`, `span`, and reference: who owns the storage and does it outlive
the view? Anything captured by a lambda that crosses a thread or segment-swap boundary?

**Concurrency** — documented ownership rule for shared state; justified memory orders; no lock held
across I/O or allocation.

**Clarity** — would this be readable in six months with no context? Any name that misleads? Any
comment that restates the code?

**Requirements** — which FR/NFR does this satisfy, and is it tested by ID?

---

## 5. Architecture Decision Records

Any decision that is expensive to reverse gets an ADR **before** the code: layering, threading model,
on-disk format, index data structures, error mechanism, dependencies, protocol behaviour.

- Numbered `adr/NNN-short-title.md`, sequential, never renumbered.
- Statuses: `Proposed → Accepted`, later `Superseded by ADR-NNN` or `Deprecated`. Accepted ADRs are
  never edited except to change status; a changed mind is a new ADR.
- Minimum two alternatives considered, each with why it was rejected. An ADR with one option is a
  note, not a decision.
- Consequences section states what becomes harder, not only what becomes easier.
- Template: `adr/000-template.md`. Index of accepted ADRs lives in `03-architecture.md`.

---

## 6. Git

- **Trunk-based.** `main` is always green. Short-lived branches: `feat/m2-typo-automaton`,
  `fix/wal-truncate-race`, `docs/architecture`, `chore/ci-arm64`.
- **Conventional Commits**, with the requirement IDs in the body:
```
  feat(core): Damerau-Levenshtein automaton for typo candidates

  Builds a Levenshtein automaton over the query term and intersects it with
  the term dictionary, so candidate generation is O(matched terms) rather
  than a scan of the dictionary.

  Covers: FR-23
  Refs: ADR-004
```
  Types: `feat`, `fix`, `perf`, `refactor`, `test`, `docs`, `build`, `ci`, `chore`.
- One logical change per commit. Refactors are separate commits from behaviour changes — always.
- PRs even when working alone: the description is a durable record, and CI runs on it. Squash merge.
- Tag releases `v0.1.0`, `v1.0.0`. Semantic versioning, where the public API is the HTTP API in
  `02-api-spec.yaml`, not the C++ symbols.
- `main` is protected: CI must pass. No force pushes.

---

## 7. CI

Defined properly in `05-quality-strategy.md`; the intended shape:

| Job | Trigger | Gate |
|---|---|---|
| Build matrix — {GCC 13, Clang 17} × {Debug, Release} × {x86-64, arm64} | every push | must pass |
| Unit + integration tests | every push | must pass |
| ASan + UBSan | every push | must pass |
| TSan | every push | must pass |
| clang-format + clang-tidy | every push | must pass |
| Coverage report, ≥ 80% core | every push | must pass |
| Benchmark suite on fixed dataset | nightly + release branches | regression alerts |
| Fuzz targets, 1 h each | nightly + release branches | must find nothing (NFR-07) |
| Container build + size check ≤ 30 MB | release branches | must pass |

Same commands available locally as CMake presets, so nothing is CI-only. A red `main` is fixed
before any new work starts.

---

## 8. Documentation maintenance

- The numbered documents live in `docs/` in the repository **and** in Project Knowledge. The repo is
  authoritative; Project Knowledge is a copy that must be refreshed when a document changes.
- After any document change: bump its status line, update the row in `STATE.md` §2, re-upload it
  to the Project, and delete the superseded copy. A stale document in Project Knowledge is worse
  than a missing one, because it will be believed.
- The README is written at M0 and kept true at every milestone, not written at the end. S6 — a
  stranger running Leta in under ten minutes — is a v1 success criterion.
- Keep a `CHANGELOG.md` from the first tagged release, in Keep a Changelog format.

---

## 9. Security and supply chain

- Runs as non-root in the container, no dynamic code, no outbound network calls (NFR-08).
- Master key compared in constant time; never logged, never echoed in errors, never in metrics
  labels (FR-64).
- Dependencies pinned by version and hash where the tooling allows. Dependabot or equivalent on.
- No secrets in the repository. Config by environment variable and CLI flag only (FR-63).
- Error responses never leak paths, versions, or internals to unauthenticated clients.

---

## 10. Working rhythm

- Budget is 6–10 hours a week. Protect it by keeping tasks small enough to finish in one sitting;
  an unfinished task costs the reload cost twice.
- Every session ends with a committed, green `main` and an updated state file. Never stop mid-task
  without writing down where you are — the state file exists for exactly this.
- Prefer finishing a thin vertical slice over half-finishing three horizontal layers. A demo you can
  run is worth more than three components you cannot.
- Publish early. The repository is public from the first commit, so make the first commit one you
  would be happy for a reviewer to read.
