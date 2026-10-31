#include "adapters/storage/in_memory_index_repository.hpp"

#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "core/document.hpp"
#include "core/error.hpp"
#include "core/index_metadata.hpp"
#include "core/index_uid.hpp"
#include "core/result.hpp"

namespace leta::storage {

namespace {

// Safe to echo: a uid is ASCII letters, digits, `_` and `-` by construction (FR-01).
std::string already_exists_message(const core::IndexUid& uid) {
    std::string message{"Index `"};
    message.append(uid.value()).append("` already exists.");
    return message;
}

}  // namespace

core::Result<void> InMemoryIndexRepository::add(const core::IndexMetadata& index) {
    const std::scoped_lock lock{mutex_};
    // try_emplace is the check and the insert in one call, under one lock: the IndexRepository
    // contract that two concurrent creates of one uid cannot both succeed.
    const bool is_new = indexes_
                            .try_emplace(std::string{index.uid.value()},
                                         StoredIndex{.metadata = index, .documents = {}})
                            .second;
    if (!is_new) {
        return core::failure(core::ErrorCode::IndexAlreadyExists,
                             already_exists_message(index.uid));
    }
    return {};
}

core::Result<core::IndexMetadata> InMemoryIndexRepository::add_documents(
    const core::IndexMetadata& index_if_missing,
    std::vector<core::Document> documents) {
    const std::scoped_lock lock{mutex_};
    // try_emplace inserts only when the uid is free, so finding and creating are one step under the
    // lock: a concurrent POST /indexes either sees this index or creates it first.
    StoredIndex& stored =
        indexes_
            .try_emplace(std::string{index_if_missing.uid.value()},
                         StoredIndex{.metadata = index_if_missing, .documents = {}})
            .first->second;
    for (core::Document& document : documents) {
        stored.documents.add(std::move(document));
    }
    return stored.metadata;
}

}  // namespace leta::storage
