#include "application/index_service.hpp"

#include <utility>

#include "application/clock.hpp"
#include "application/index_repository.hpp"
#include "core/index_metadata.hpp"
#include "core/result.hpp"
#include "core/timestamp.hpp"

namespace leta::application {

IndexService::IndexService(IndexRepository& repository, Clock clock)
    : repository_{repository}
    , clock_{std::move(clock)} {}

core::Result<core::IndexMetadata> IndexService::create_index(CreateIndexCommand command) {
    // One reading of the clock, so a new index's createdAt and updatedAt are equal by construction.
    const core::Timestamp now = clock_();
    core::IndexMetadata index{.uid = std::move(command.uid),
                              .primary_key = std::move(command.primary_key),
                              .created_at = now,
                              .updated_at = now};
    const auto added = repository_.get().add(index);
    if (!added) {
        return core::failure(added.error());
    }
    return index;
}

}  // namespace leta::application
