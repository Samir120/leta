#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "core/result.hpp"

namespace leta::core {

/// An index's name, valid by construction: 1 to 64 characters of [a-zA-Z0-9_-] (FR-01). parse() is
/// the only way to get one, so code holding an IndexUid never re-checks it. The narrow alphabet is
/// what makes a uid safe to echo into messages and, from M7, to use as a directory name
/// (03-architecture.md §7).
class IndexUid {
public:
    [[nodiscard]] static Result<IndexUid> parse(std::string_view text);

    [[nodiscard]] std::string_view value() const noexcept {
        return value_;
    }

private:
    explicit IndexUid(std::string value)
        : value_{std::move(value)} {}

    std::string value_;
};

}  // namespace leta::core
