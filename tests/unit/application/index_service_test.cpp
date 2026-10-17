#include "application/index_service.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include "adapters/storage/in_memory_index_repository.hpp"
#include "core/error.hpp"
#include "core/index_uid.hpp"
#include "core/timestamp.hpp"

namespace {

using leta::application::CreateIndexCommand;
using leta::application::IndexService;
using leta::core::ErrorCode;
using leta::core::IndexUid;
using leta::core::Timestamp;
using leta::storage::InMemoryIndexRepository;

constexpr std::chrono::milliseconds FixedTime{1'760'000'000'000};

Timestamp fixed_clock() {
    return Timestamp{FixedTime};
}

CreateIndexCommand command_for(std::string_view uid,
                               std::optional<std::string> primary_key = std::nullopt) {
    return {.uid = IndexUid::parse(uid).value(), .primary_key = std::move(primary_key)};
}

}  // namespace

TEST_CASE("a new index is stamped with the clock's time, created and updated alike", "[FR-02]") {
    InMemoryIndexRepository repository;
    IndexService service{repository, fixed_clock};
    const auto index = service.create_index(command_for("products"));
    REQUIRE(index);
    CHECK(index->uid.value() == "products");
    CHECK(index->created_at == Timestamp{FixedTime});
    CHECK(index->updated_at == index->created_at);
}

TEST_CASE("the primary key named at creation is kept", "[FR-02]") {
    InMemoryIndexRepository repository;
    IndexService service{repository, fixed_clock};
    const auto index = service.create_index(command_for("products", "sku"));
    REQUIRE(index);
    CHECK(index->primary_key == "sku");
}

TEST_CASE("creating an index whose uid exists fails with index_already_exists", "[FR-02]") {
    InMemoryIndexRepository repository;
    IndexService service{repository, fixed_clock};
    REQUIRE(service.create_index(command_for("products")));
    const auto again = service.create_index(command_for("products"));
    REQUIRE_FALSE(again);
    CHECK(again.error().code == ErrorCode::IndexAlreadyExists);
}
