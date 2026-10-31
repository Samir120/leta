#include "adapters/storage/in_memory_index_repository.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <chrono>
#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "core/document.hpp"
#include "core/error.hpp"
#include "core/index_metadata.hpp"
#include "core/index_uid.hpp"
#include "core/timestamp.hpp"

namespace {

using leta::core::Document;
using leta::core::ErrorCode;
using leta::core::IndexMetadata;
using leta::core::IndexUid;
using leta::core::Timestamp;
using leta::storage::InMemoryIndexRepository;

IndexMetadata index_named(std::string_view uid,
                          std::optional<std::string> primary_key = std::nullopt,
                          Timestamp created_at = Timestamp{}) {
    return {.uid = IndexUid::parse(uid).value(),
            .primary_key = std::move(primary_key),
            .created_at = created_at,
            .updated_at = created_at};
}

std::vector<Document> one_document() {
    std::vector<Document> documents;
    documents.push_back(
        Document{.source = R"({"name":"MSI B650"})", .searchable_text = {"MSI B650"}});
    return documents;
}

}  // namespace

TEST_CASE("the repository holds more than one index, each under its own uid", "[FR-01]") {
    InMemoryIndexRepository repository;
    CHECK(repository.add(index_named("products")));
    CHECK(repository.add(index_named("articles")));
}

TEST_CASE("an index whose uid is taken is refused as index_already_exists", "[FR-01]") {
    InMemoryIndexRepository repository;
    REQUIRE(repository.add(index_named("products")));
    const auto again = repository.add(index_named("products"));
    REQUIRE_FALSE(again);
    CHECK(again.error().code == ErrorCode::IndexAlreadyExists);
    CHECK(again.error().message == "Index `products` already exists.");
}

TEST_CASE("concurrent creates of one uid: exactly one succeeds", "[FR-01][NFR-06]") {
    constexpr std::size_t ThreadCount = 8;
    InMemoryIndexRepository repository;
    // One slot per thread, so the threads share nothing but the repository under test. No Catch2
    // assertion runs on a thread: they are not thread-safe.
    std::array<bool, ThreadCount> succeeded{};
    {
        std::vector<std::jthread> threads;
        threads.reserve(ThreadCount);
        for (std::size_t slot = 0; slot < ThreadCount; ++slot) {
            threads.emplace_back([&repository, &succeeded, slot] {
                succeeded.at(slot) = repository.add(index_named("products")).has_value();
            });
        }
    }  // every jthread joins here
    CHECK(std::ranges::count(succeeded, true) == 1);
}

TEST_CASE("adding documents to a missing index creates it from the metadata given", "[FR-02]") {
    InMemoryIndexRepository repository;
    const auto stored = repository.add_documents(index_named("products", "sku"), one_document());
    REQUIRE(stored);
    CHECK(stored->uid.value() == "products");
    CHECK(stored->primary_key == "sku");
    const auto again = repository.add(index_named("products"));
    REQUIRE_FALSE(again);
    CHECK(again.error().code == ErrorCode::IndexAlreadyExists);
}

TEST_CASE("adding documents to an existing index keeps its metadata", "[FR-02]") {
    constexpr Timestamp CreatedAt{std::chrono::milliseconds{1'000}};
    InMemoryIndexRepository repository;
    REQUIRE(repository.add(index_named("products", "sku", CreatedAt)));
    const auto stored = repository.add_documents(index_named("products"), one_document());
    REQUIRE(stored);
    CHECK(stored->primary_key == "sku");
    CHECK(stored->created_at == CreatedAt);
}

TEST_CASE("concurrent batches into one missing index all land and create it once", "[NFR-06]") {
    constexpr std::size_t ThreadCount = 8;
    InMemoryIndexRepository repository;
    std::array<bool, ThreadCount> landed{};
    {
        std::vector<std::jthread> threads;
        threads.reserve(ThreadCount);
        for (std::size_t slot = 0; slot < ThreadCount; ++slot) {
            threads.emplace_back([&repository, &landed, slot] {
                landed.at(slot) =
                    repository.add_documents(index_named("products"), one_document()).has_value();
            });
        }
    }  // every jthread joins here
    CHECK(std::ranges::all_of(landed, [](bool has_landed) { return has_landed; }));
    CHECK_FALSE(repository.add(index_named("products")));
}
