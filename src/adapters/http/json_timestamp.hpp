#pragma once

#include <string>

#include "core/timestamp.hpp"

namespace leta::http {

/// Appends `timestamp` as a quoted RFC 3339 UTC string to the millisecond, the `date-time` format
/// of 02-api-spec.yaml: "2025-10-09T08:53:20.123Z".
///
/// Precondition: the year is 0000-9999, the range RFC 3339 can write. Every reading of a real clock
/// is.
void append_json_timestamp(std::string& out, core::Timestamp timestamp);

}  // namespace leta::http
