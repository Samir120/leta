#include "adapters/storage/in_memory_index_repository.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>

#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "core/error.hpp"
#include "core/index_metadata.hpp"
#include "core/index_uid.hpp"
#include "core/timestamp.hpp"

namespace {

using leta::core::ErrorCode;
using leta::core::IndexMetadata;
using leta::core::IndexUid;
using leta::core::Timestamp;
using leta::storage::InMemoryIndexRepository;

IndexMetadata index_named(std::string_view uid) {
    return {.uid = IndexUid::parse(uid).value(),
            .primary_key = std::nullopt,
            .created_at = Timestamp{},
            .updated_at = Timestamp{}};
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
