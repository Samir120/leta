#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "application/index_repository.hpp"
#include "core/document.hpp"
#include "core/exact_match_index.hpp"
#include "core/index_metadata.hpp"
#include "core/result.hpp"

namespace leta::storage {

/// The IndexRepository until persistence (M7): a map in process memory, gone when the process
/// exits.
///
/// One mutex guards it. ADR-007's single indexer takes over document data in M3 and index metadata
/// in M7, when a create becomes a WAL record. Until then this lock is all of the concurrency, and
/// it lives here rather than in the services so that swapping the store swaps the locking with it.
/// It serialises every ingest, tokenizing included: fine for the walking skeleton, and exactly what
/// the indexer replaces.
class InMemoryIndexRepository final : public application::IndexRepository {
public:
    [[nodiscard]] core::Result<void> add(const core::IndexMetadata& index) override;
    [[nodiscard]] core::Result<core::IndexMetadata> add_documents(
        const core::IndexMetadata& index_if_missing,
        std::vector<core::Document> documents) override;

private:
    // An index and its documents together, so one lookup under one lock finds both.
    struct StoredIndex {
        core::IndexMetadata metadata;
        core::ExactMatchIndex documents;  // throwaway (M1), see core/exact_match_index.hpp
    };

    std::mutex mutex_;
    std::map<std::string, StoredIndex> indexes_;
};

}  // namespace leta::storage
