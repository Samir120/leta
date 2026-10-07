#include "application/health_service.hpp"

#include <atomic>

namespace leta::application {

Readiness HealthService::readiness() const noexcept {
    return is_available_.load(std::memory_order_acquire) ? Readiness::Available
                                                         : Readiness::Starting;
}

void HealthService::mark_available() noexcept {
    is_available_.store(true, std::memory_order_release);
}

}  // namespace leta::application
