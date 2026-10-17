#pragma once

#include <map>
#include <mutex>
#include <string>

#include "application/index_repository.hpp"
#include "core/index_metadata.hpp"
#include "core/result.hpp"

namespace leta::storage {

/// The IndexRepository until persistence (M7): a map in process memory, gone when the process
/// exits.
///
/// One mutex guards it. ADR-007's single indexer owns document data; index metadata is a few bytes
/// written once per create, and it moves onto the indexer at M7, when a create becomes a WAL
/// record. Until then this lock is all of its concurrency, and it lives here rather than in the
/// service so that swapping the store swaps the locking with it.
class InMemoryIndexRepository final : public application::IndexRepository {
public:
    [[nodiscard]] core::Result<void> add(const core::IndexMetadata& index) override;

private:
    std::mutex mutex_;
    std::map<std::string, core::IndexMetadata> indexes_;
};

}  // namespace leta::storage
