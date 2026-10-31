#pragma once

#include <string_view>
#include <vector>

namespace leta::core {

/// THROWAWAY (04-roadmap.md, M1). Splits `text` into the runs of characters between ASCII
/// whitespace, and does nothing else: no case folding (FR-25), no diacritic folding (FR-24), no
/// punctuation handling, so `DDR5,` is one word and `Corsair` does not match `corsair`. M2 replaces
/// it with the tokenizer proper, one path for documents and queries alike (FR-20).
///
/// The views point into `text`, which must outlive them.
[[nodiscard]] std::vector<std::string_view> split_on_whitespace(std::string_view text);

}  // namespace leta::core
