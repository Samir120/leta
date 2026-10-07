#pragma once

#include <atomic>

#include <cstdint>

namespace leta::application {

/// FR-60's two answers.
enum class Readiness : std::uint8_t {
    Starting,   // restoring from disk; GET /health answers 503
    Available,  // queries can be served; GET /health answers 200
};

/// Whether the process can serve queries yet. Starts as Starting; startup marks it Available when
/// restore has finished (M7) — immediately, until there is anything to restore.
class HealthService {
public:
    [[nodiscard]] Readiness readiness() const noexcept;

    /// Called once, by the startup thread, after restore. Never reverts: v1 has no degraded state.
    void mark_available() noexcept;

private:
    // Written once by the startup thread, read by every HTTP worker. Release/acquire: a worker that
    // reads Available also sees every index the startup thread published before marking it (M7).
    std::atomic<bool> is_available_{false};
};

}  // namespace leta::application
