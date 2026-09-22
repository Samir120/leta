The single exception to "`leta_core` links the standard library and nothing else" (ADR-001):
headers **copied** into the tree, never fetched as CMake targets.

| Header | Version | Licence | Source |
|---|---|---|---|
| `tl/expected.hpp` | 1.3.1 | CC0-1.0 | https://github.com/TartanLlama/expected (ADR-005) |

Rules for anything placed here:

- A permissive licence (CC0, MIT, BSD, Apache-2.0) with an entry in the repository `NOTICE`.
- The file is vendored verbatim and refreshed by replacement; it is never edited in place. To
  refresh: download the tagged file from upstream, check its SHA-256 against the upstream release,
  replace the file, update the version in this table and in `NOTICE`, one `build:` commit.
- Included as `<tl/expected.hpp>` — angle brackets, path relative to this directory. This directory
  is a SYSTEM include directory, which is what keeps vendored code out of `-Werror` and clang-tidy.
- The layering check (`cmake/LetaLayeringCheck.cmake`) lists the permitted headers by name.
  Adding a file here means adding it to that list, in the same commit, with the reason.

`cmake/CPM.cmake` is also vendored, but it is build tooling, not code `leta_core` sees, so it
lives under `cmake/` and is governed by ADR-002 rather than this directory's rules.
