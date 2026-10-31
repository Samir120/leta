#pragma once

#include <cstdint>

namespace leta::core {

/// A document's internal identity: dense, assigned in insertion order from 0, never exposed on the
/// wire (ADR-004, 03-architecture.md). Dense is what lets it be an array index and the stable
/// tiebreak FR-26 needs. The client's primary key maps to it from M3 on.
///
/// An enum class rather than a bare integer: a DocId cannot be mixed up with a count or a position
/// without a cast that says so.
enum class DocId : std::uint32_t {
};

}  // namespace leta::core
