#include "application/health_service.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

using leta::application::HealthService;
using leta::application::Readiness;

}  // namespace

TEST_CASE("a new HealthService reports Starting", "[FR-60]") {
    const HealthService health;
    CHECK(health.readiness() == Readiness::Starting);
}

TEST_CASE("mark_available switches readiness to Available", "[FR-60]") {
    HealthService health;
    health.mark_available();
    CHECK(health.readiness() == Readiness::Available);
}
