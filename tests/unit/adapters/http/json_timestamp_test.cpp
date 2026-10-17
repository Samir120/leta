#include "adapters/http/json_timestamp.hpp"

#include <string>

#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include "core/timestamp.hpp"

namespace {

using leta::core::Timestamp;
using std::chrono::milliseconds;

std::string json_timestamp(Timestamp timestamp) {
    std::string out;
    leta::http::append_json_timestamp(out, timestamp);
    return out;
}

}  // namespace

TEST_CASE("timestamps are quoted RFC 3339 UTC strings to the millisecond", "[FR-70]") {
    CHECK(json_timestamp(Timestamp{}) == R"("1970-01-01T00:00:00.000Z")");
    CHECK(json_timestamp(Timestamp{milliseconds{1'760'000'000'123}})
          == R"("2025-10-09T08:53:20.123Z")");
}

TEST_CASE("the last millisecond of a leap day lands on the 29th", "[FR-70]") {
    CHECK(json_timestamp(Timestamp{milliseconds{1'709'251'199'999}})
          == R"("2024-02-29T23:59:59.999Z")");
}
