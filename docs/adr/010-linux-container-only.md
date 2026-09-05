# ADR-010 — Linux container as the only deployment artifact

| | |
|---|---|
| Status | Proposed |
| Date | 2026-09-03 |
| Requirements | FR-67, NFR-08, NFR-09, NFR-10, NFR-13, S6 |
| Note | This is the decision the brief §4 refers to as "ADR-002". See §Numbering below. |

## Context

The brief's §7 states the environment plainly: development on Linux only; production on a Windows
Server host running a Linux container runtime. §4 already declares a Windows-native binary a
non-goal.

FR-67 requires a multi-arch OCI image with a non-root user, a declared volume, and a `HEALTHCHECK`.
NFR-13 caps the image at 30 MB. S6 requires a stranger to be running Leta within ten minutes using
only the image.

## Decision

The **only** supported artifact is a multi-arch OCI image for `linux/amd64` and `linux/arm64`. No
Windows-native binary, no installer, no service wrapper. Windows hosts run Leta through a Linux
container runtime. NFR-10's MSVC job remains a portability canary that is allowed to fail; it exists
to catch accidental GCC/Clang-isms, not to produce a shippable binary.

Multi-stage build: a full toolchain image compiles, a minimal runtime layer ships. The runtime layer
is settled by measurement at M12, between two candidates:

| Option | Image size | Risk |
|---|---|---|
| Alpine + musl, static binary on `scratch` | smallest, comfortably under NFR-13 | musl's allocator performs poorly under multithreaded server load; would need mimalloc, and TSan/ASan behave differently |
| Debian build + distroless runtime | larger base, may crowd the 30 MB cap | glibc allocator, matches the CI sanitizer environment |

Build both at M12 and choose on measured NFR-01 latency and NFR-13 size, not on preference.

## Alternatives considered

**A — ship a native Windows binary as well.** Rejected. It doubles the platform surface for
persistence in particular: `fsync`/`fdatasync` semantics, directory fsync for the snapshot rename in
ADR-009, and the socket layer all differ. The cost would exceed a whole milestone, and the stated
production host already runs containers.

**B — a `.tar.gz` of a static Linux binary alongside the image.** Cheap to produce and a reasonable
later addition, but it is a second artifact to test, sign, and document, for no user identified in
brief §5. Deferred, not rejected.

## Consequences

**Easier:** one platform, one set of syscalls, one reproducible artifact. Persistence code targets
POSIX only. The S6 walkthrough is a single `docker run`.

**Harder:** Windows-only users must install a container runtime first, which becomes a documented
prerequisite in the README rather than an unstated assumption.

## Numbering

The brief §4 forward-references "ADR-002" for this decision, written before the ADR sequence existed.
ADR-002 is now the build-system decision. Per `07-ways-of-working.md` §5 ADR numbers are never
reused or renumbered, so the brief was corrected to point at ADR-010 (2026-09-05, closing Q6).
