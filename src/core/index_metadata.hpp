#pragma once

#include <optional>
#include <string>

#include "core/index_uid.hpp"
#include "core/timestamp.hpp"

namespace leta::core {

/// What the server knows about an index apart from its documents: the Index object of
/// 02-api-spec.yaml. Settings (FR-04) join it when their routes do (M6).
struct IndexMetadata {
    IndexUid uid;
    /// FR-03: named at creation, or empty until inferred from the first batch (M8).
    std::optional<std::string> primary_key;
    Timestamp created_at;
    Timestamp updated_at;
};

}  // namespace leta::core
