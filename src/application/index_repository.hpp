#pragma once

#include <vector>

#include "core/document.hpp"
#include "core/index_metadata.hpp"
#include "core/result.hpp"

namespace leta::application {

/// Where indexes live: their metadata and, since M1-T3, their documents (ADR-001: application owns
/// the port, adapters/storage implements it). In memory until M7; from then each change is a WAL
/// record (ADR-009) behind this same interface.
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

    /// Adds `documents`, in order, to the index named `index_if_missing.uid`, first storing
    /// `index_if_missing` when no index has that uid: POST /indexes/{uid}/documents creates a
    /// missing index (02-api-spec.yaml). Lookup, creation and insertion are one atomic step, as in
    /// add(). Returns the index's metadata as stored: the existing index's, when there was one.
    [[nodiscard]] virtual core::Result<core::IndexMetadata> add_documents(
        const core::IndexMetadata& index_if_missing,
        std::vector<core::Document> documents) = 0;
};

}  // namespace leta::application
