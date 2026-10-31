#include "core/exact_match_index.hpp"

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cstddef>
#include <cstdint>

#include "core/doc_id.hpp"
#include "core/document.hpp"
#include "core/whitespace_tokenizer.hpp"

namespace leta::core {

DocId ExactMatchIndex::add(Document document) {
    // 2^32 documents is far past what NFR-04's memory allows on one host; M3's DocumentStore owns
    // the real bound.
    const DocId document_id{static_cast<std::uint32_t>(sources_.size())};
    for (const std::string& text : document.searchable_text) {
        for (const std::string_view word : split_on_whitespace(text)) {
            record(word, document_id);
        }
    }
    sources_.push_back(std::move(document.source));
    return document_id;
}

std::size_t ExactMatchIndex::document_count() const noexcept {
    return sources_.size();
}

std::span<const DocId> ExactMatchIndex::documents_containing(std::string_view term) const {
    const auto found = postings_.find(term);
    if (found == postings_.end()) {
        return {};
    }
    return found->second;
}

std::string_view ExactMatchIndex::source(DocId document_id) const {
    return sources_.at(static_cast<std::size_t>(document_id));
}

void ExactMatchIndex::record(std::string_view term, DocId document_id) {
    std::vector<DocId>& documents = postings_[std::string{term}];
    // Ids only grow, so a word repeated within one document can only find its own id at the back.
    if (documents.empty() || documents.back() != document_id) {
        documents.push_back(document_id);
    }
}

}  // namespace leta::core
