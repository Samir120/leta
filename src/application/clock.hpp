#pragma once

#include <functional>

#include "core/timestamp.hpp"

namespace leta::application {

/// The current time, injected. Services never read a clock themselves, so tests pin time to a
/// constant and core stays free of it (ADR-001). main passes the system clock.
using Clock = std::function<core::Timestamp()>;

}  // namespace leta::application
