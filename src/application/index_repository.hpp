#pragma once

#include "core/index_metadata.hpp"
#include "core/result.hpp"

namespace leta::application {

/// Where index metadata lives (ADR-001: application owns the port, adapters/storage implements it).
/// In memory until M7; from then each create is an `index_meta` WAL record (ADR-009) behind this
/// same interface.
///
/// Called from every HTTP worker at once: implementations must be safe for that.
class IndexRepository {
public:
    IndexRepository() = default;
    IndexRepository(const IndexRepository&) = delete;
    IndexRepository& operator=(const IndexRepository&) = delete;
    IndexRepository(IndexRepository&&) = delete;
    IndexRepository& operator=(IndexRepository&&) = delete;
    virtual ~IndexRepository() = default;

    /// Stores `index` under its uid. When the uid is taken, fails with IndexAlreadyExists and
    /// stores nothing. Check and insert are one atomic step: two concurrent creates of one uid
    /// cannot both succeed.
    [[nodiscard]] virtual core::Result<void> add(const core::IndexMetadata& index) = 0;
};

}  // namespace leta::application
