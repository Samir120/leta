#pragma once

#include <functional>
#include <optional>
#include <string>

#include "application/clock.hpp"
#include "application/index_repository.hpp"
#include "core/index_metadata.hpp"
#include "core/index_uid.hpp"
#include "core/result.hpp"

namespace leta::application {

/// POST /indexes after the adapter has read it: a uid that already satisfies FR-01, and the primary
/// key if the client named one.
struct CreateIndexCommand {
    core::IndexUid uid;
    std::optional<std::string> primary_key;
};

/// The index lifecycle (FR-02): create today; list, inspect and delete arrive with their routes
/// (M6).
class IndexService {
public:
    /// `repository` must outlive the service; `clock` must not be empty.
    IndexService(IndexRepository& repository, Clock clock);

    /// The index as stored. Fails with IndexAlreadyExists, storing nothing, when the uid is taken.
    [[nodiscard]] core::Result<core::IndexMetadata> create_index(CreateIndexCommand command);

private:
    std::reference_wrapper<IndexRepository> repository_;
    Clock clock_;
};

}  // namespace leta::application
