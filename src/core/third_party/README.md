The single exception to "`leta_core` links the standard library and nothing else" (ADR-001):
headers **copied** into the tree, never fetched as CMake targets. Today that is `tl/expected.hpp`
(ADR-005, arrives in M0-T4).

Rules for anything placed here:

- A permissive licence (CC0, MIT, BSD, Apache-2.0) with a line in the repository `NOTICE`.
- The file is vendored verbatim and refreshed by replacement; it is never edited in place.
- Included as `<tl/expected.hpp>` — angle brackets, path relative to this directory. This directory
  is a SYSTEM include directory, which is what keeps vendored code out of `-Werror` and clang-tidy.
- The layering check (`cmake/LetaLayeringCheck.cmake`) lists the permitted headers by name.
  Adding a file here means adding it to that list, in the same commit, with the reason.
