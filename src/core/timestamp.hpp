#pragma once

#include <chrono>

namespace leta::core {

/// A UTC instant to the millisecond, the resolution of `createdAt` / `updatedAt` on the wire
/// (02-api-spec.yaml, Index). core holds instants but never reads a clock; the current time comes
/// in through application/clock.hpp, so everything here stays testable at a fixed time.
using Timestamp = std::chrono::sys_time<std::chrono::milliseconds>;

}  // namespace leta::core
