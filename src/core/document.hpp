#pragma once

#include <string>
#include <vector>

namespace leta::core {

/// A document on its way into an index, after the adapter has read the JSON: core never sees JSON
/// itself (ADR-001).
struct Document {
    /// The exact bytes the client sent for this document, returned as they are (ADR-004).
    std::string source;
    /// The text to make searchable. In M1 the adapter puts the top-level string values here; the
    /// searchable-attributes setting (FR-04) decides from M6, dotted paths from M3 (FR-16).
    std::vector<std::string> searchable_text;
};

}  // namespace leta::core
