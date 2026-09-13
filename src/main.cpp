#include <string_view>

#include <cstdio>

#include "adapters/config/build_info.hpp"

namespace {

constexpr std::string_view VersionFlag = "--version";

}  // namespace

int main(int argc, char* argv[]) {
    // FR-68. Flag and environment parsing proper arrives with FR-63 in M9; until then the binary
    // only identifies itself (A6).
    if (argc == 2 && std::string_view{argv[1]} == VersionFlag) {
        std::puts(leta::config::version_line(leta::config::build_info()).c_str());
        return 0;
    }
    // Nothing useful to do if writing the usage line fails; the exit status is the contract.
    static_cast<void>(std::fputs("usage: leta --version\n", stderr));
    return 2;
}
