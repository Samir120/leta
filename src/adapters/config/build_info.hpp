#pragma once

#include <string>
#include <string_view>

namespace leta::config {

/// Identity of this binary, fixed at configure time by cmake/LetaBuildInfo.cmake and compiled
/// into the generated build_info_generated.cpp. Every view points at static storage and is valid
/// for the life of the process. Consumed by `leta --version` (FR-68) and, from M6, `GET /version`.
struct BuildInfo {
    std::string_view version;     // PROJECT_VERSION plus pre-release suffix, e.g. "0.1.0-dev"
    std::string_view commit;      // short git hash, "-dirty" if tracked files were modified,
                                  // "unknown" when built outside a git checkout
    std::string_view build_type;  // CMAKE_BUILD_TYPE, e.g. "Debug"
    std::string_view compiler;    // "<id> <version>", e.g. "GNU 13.2.0"
};

/// The values baked into this binary.
[[nodiscard]] BuildInfo build_info() noexcept;

/// The FR-68 one-line form: `leta <version> (<commit>, <build_type>, <compiler>)`.
/// Pure so it can be tested against a fixture without a real build.
[[nodiscard]] std::string version_line(const BuildInfo& info);

}  // namespace leta::config
