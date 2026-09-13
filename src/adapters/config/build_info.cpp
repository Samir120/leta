#include "adapters/config/build_info.hpp"

#include <string>

namespace leta::config {

std::string version_line(const BuildInfo& info) {
    std::string line{"leta "};
    line.append(info.version)
        .append(" (")
        .append(info.commit)
        .append(", ")
        .append(info.build_type)
        .append(", ")
        .append(info.compiler)
        .append(")");
    return line;
}

}  // namespace leta::config
