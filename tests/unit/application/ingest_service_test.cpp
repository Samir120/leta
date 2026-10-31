#include "application/ingest_service.hpp"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <chrono>
#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "adapters/storage/in_memory_index_repository.hpp"
#include "application/index_service.hpp"
#include "core/document.hpp"
#include "core/error.hpp"
#include "core/index_uid.hpp"
#include "core/timestamp.hpp"

namespace {

using leta::application::AddDocumentsCommand;
using leta::application::IndexService;
using leta::application::IngestService;
using leta::core::Document;
using leta::core::ErrorCode;
using leta::core::IndexUid;
using leta::core::Timestamp;
using leta::storage::InMemoryIndexRepository;

constexpr std::chrono::milliseconds FixedTime{1'760'000'000'000};

Timestamp fixed_clock() {
    return Timestamp{FixedTime};
}

AddDocumentsCommand batch_for(std::string_view uid, std::size_t document_count) {
    std::vector<Document> documents(
        document_count,
        Document{.source = R"({"name":"Corsair DDR5"})", .searchable_text = {"Corsair DDR5"}});
    return {.uid = IndexUid::parse(uid).value(), .documents = std::move(documents)};
}

}  // namespace

TEST_CASE("documents sent to a missing index create it, as POST /indexes would", "[FR-02]") {
    InMemoryIndexRepository repository;
    IngestService ingest{repository, fixed_clock};
    IndexService indexes{repository, fixed_clock};
    const auto outcome = ingest.add_documents(batch_for("products", 2));
    REQUIRE(outcome);
    CHECK(outcome->indexed == 2);
    CHECK_FALSE(outcome->primary_key.has_value());
    const auto again = indexes.create_index(
        {.uid = IndexUid::parse("products").value(), .primary_key = std::nullopt});
    REQUIRE_FALSE(again);
    CHECK(again.error().code == ErrorCode::IndexAlreadyExists);
}

TEST_CASE("documents sent to an existing index report its primary key", "[FR-02]") {
    InMemoryIndexRepository repository;
    IngestService ingest{repository, fixed_clock};
    IndexService indexes{repository, fixed_clock};
    REQUIRE(
        indexes.create_index({.uid = IndexUid::parse("products").value(), .primary_key = "sku"}));
    const auto outcome = ingest.add_documents(batch_for("products", 3));
    REQUIRE(outcome);
    CHECK(outcome->indexed == 3);
    CHECK(outcome->primary_key == "sku");
}
