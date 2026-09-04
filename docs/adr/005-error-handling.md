# ADR-005 — `leta::Result<T, Error>` over a vendored `expected`

| | |
|---|---|
| Status | Proposed |
| Date | 2026-09-03 |
| Requirements | FR-71, NFR-09 |

## Context

Leta has three error categories (`06-coding-standards.md` §5), and the largest — expected failures a
caller can act on, such as malformed JSON, an unknown index UID, or an out-of-range parameter — needs
a return-value mechanism rather than exceptions. FR-71 fixes the wire shape: a stable `code`, a
`type`, and a human-readable `message`.

`std::expected` is the natural choice and is C++23. NFR-09 pins the compiler matrix at GCC ≥ 13 and
Clang ≥ 17 on two architectures, and `06-coding-standards.md` §1 sets the language baseline at C++20.
Raising the whole project to C++23 for one type widens the risk surface across four toolchain
combinations for no other benefit.

## Decision

Vendor **`tl::expected`** (single header, public domain) and alias it in one project header:

```cpp
// src/core/result.hpp
template <typename T, typename E = Error>
using Result = tl::expected<T, E>;

struct Error {
    ErrorCode code;     // enum class; names map 1:1 to the FR-71 snake_case codes
    ErrorType type;     // invalid_request | auth | not_found | internal
    std::string message;
};
```

`ErrorCode` to wire-string and `ErrorType` to HTTP status are two tables in the HTTP adapter, which
stays the only layer that knows what a status code is.

## Alternatives considered

**A — move to C++23 and use `std::expected`.** The right long-term answer and a one-line change once
taken, because `tl::expected` mirrors the standard API deliberately. Rejected now only on toolchain
risk; revisit at M11 when the CI matrix has a track record.

**B — a hand-written `Result`.** Rejected. Writing a monadic result type is fiddly, easy to get
subtly wrong around references and `void`, and is not on the from-scratch list that G4 actually cares
about (index, compression, typo tolerance, ranking).

**C — exceptions throughout.** Rejected. A rejected query parameter is control flow, not an
exceptional condition, and throwing on it would put unwinding in a path that runs thousands of times
a second.

## Consequences

**Easier:** error handling is uniform and visible in signatures; the FR-71 body is produced by one
function; no exception unwinding on expected paths.

**Harder:** one more dependency. Callers must handle results explicitly, which is the point but adds
noise — mitigate with `[[nodiscard]]` on every returning function so an ignored error is a compile
error, not a silent one.

## Revisit when

`std::expected` is verified working on all four NFR-09 toolchain combinations. The switch is then a
single edit to `result.hpp`.
