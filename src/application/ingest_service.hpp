#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <cstddef>

#include "application/clock.hpp"
#include "application/index_repository.hpp"
#include "core/document.hpp"
#include "core/index_uid.hpp"
#include "core/result.hpp"

namespace leta::application {

/// POST /indexes/{uid}/documents after the adapter has read it: the target index, and the batch
/// with each document already split into its source bytes and its searchable text.
struct AddDocumentsCommand {
    core::IndexUid uid;
    std::vector<core::Document> documents;
};

/// What a batch did, for the IngestResult of 02-api-spec.yaml.
struct AddDocumentsOutcome {
    std::size_t indexed{};
    /// The index's primary key, if it has one (FR-03; inferred from the first batch from M8).
    std::optional<std::string> primary_key;
};

/// Document ingest. M1: every document of the batch is appended, and a missing index is created
/// (02-api-spec.yaml). Identity by primary key — a repeated key replaces (FR-11) — arrives with the
/// DocumentStore in M3.
class IngestService {
public:
    /// `repository` must outlive the service; `clock` must not be empty.
    IngestService(IndexRepository& repository, Clock clock);

    [[nodiscard]] core::Result<AddDocumentsOutcome> add_documents(AddDocumentsCommand command);

private:
    std::reference_wrapper<IndexRepository> repository_;
    Clock clock_;
};

}  // namespace leta::application
