#include "application/ingest_service.hpp"

#include <optional>
#include <utility>

#include <cstddef>

#include "application/clock.hpp"
#include "application/index_repository.hpp"
#include "core/index_metadata.hpp"
#include "core/result.hpp"
#include "core/timestamp.hpp"

namespace leta::application {

IngestService::IngestService(IndexRepository& repository, Clock clock)
    : repository_{repository}
    , clock_{std::move(clock)} {}

core::Result<AddDocumentsOutcome> IngestService::add_documents(AddDocumentsCommand command) {
    const std::size_t batch_size = command.documents.size();
    // Read even when the index exists and the stamp goes unused: one path, and the read is cheap.
    const core::Timestamp now = clock_();
    const core::IndexMetadata index_if_missing{.uid = std::move(command.uid),
                                               .primary_key = std::nullopt,
                                               .created_at = now,
                                               .updated_at = now};
    auto stored = repository_.get().add_documents(index_if_missing, std::move(command.documents));
    if (!stored) {
        return core::failure(stored.error());
    }
    return AddDocumentsOutcome{.indexed = batch_size,
                               .primary_key = std::move(stored->primary_key)};
}

}  // namespace leta::application
