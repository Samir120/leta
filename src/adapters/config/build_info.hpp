#pragma once

#include <string>
#include <string_view>

namespace leta::config {
struct BuildInfo {
  std::string_view version;
  std::string_view commit;
  std::string_view build_type;
  std::string_view compiler;
};

[[nodiscard]] BuildInfo build_info() noexcept;

[[nodiscard]] std::string version_line(const BuildInfo &info);
} // namespace leta::config
