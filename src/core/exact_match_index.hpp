#pragma once

#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <cstddef>

#include "core/doc_id.hpp"
#include "core/document.hpp"

namespace leta::core {

/// THROWAWAY (04-roadmap.md, M1): one index's documents and a term → documents map, built with
/// split_on_whitespace() and matched exactly. It exists so the walking skeleton can answer a
/// one-word search over real HTTP (M1-T4). M3 replaces it with the DocumentStore (ADR-004) and
/// immutable segments with sorted postings (ADR-008); nothing here is meant to survive that.
///
/// Not thread-safe: its owner serialises access (in M1, the in-memory repository's lock).
class ExactMatchIndex {
public:
    /// Stores `document` under the next DocId and makes every word of its searchable text findable.
    /// Returns that DocId.
    DocId add(Document document);

    [[nodiscard]] std::size_t document_count() const noexcept;

    /// The documents whose searchable text contains `term` exactly, in insertion order, each once.
    /// Valid until the next add().
    [[nodiscard]] std::span<const DocId> documents_containing(std::string_view term) const;

    /// The bytes stored for `document_id`. Precondition: add() returned it. Valid until the next
    /// add().
    [[nodiscard]] std::string_view source(DocId document_id) const;

private:
    void record(std::string_view term, DocId document_id);

    std::vector<std::string> sources_;  // indexed by DocId: dense ids make the id the position
    // std::less<> lets find() take a string_view without building a std::string.
    std::map<std::string, std::vector<DocId>, std::less<>> postings_;
};

}  // namespace leta::core
