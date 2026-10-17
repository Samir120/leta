#include "adapters/storage/in_memory_index_repository.hpp"

#include <mutex>
#include <string>

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
    const bool is_new = indexes_.try_emplace(std::string{index.uid.value()}, index).second;
    if (!is_new) {
        return core::failure(core::ErrorCode::IndexAlreadyExists,
                             already_exists_message(index.uid));
    }
    return {};
}

}  // namespace leta::storage
