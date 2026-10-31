#include "core/exact_match_index.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "core/doc_id.hpp"
#include "core/document.hpp"

namespace {

using leta::core::DocId;
using leta::core::Document;
using leta::core::ExactMatchIndex;

Document document_with(std::string source, std::vector<std::string> searchable_text) {
    return {.source = std::move(source), .searchable_text = std::move(searchable_text)};
}

std::vector<DocId> ids_containing(const ExactMatchIndex& index, std::string_view term) {
    const auto found = index.documents_containing(term);
    return {found.begin(), found.end()};
}

}  // namespace

TEST_CASE("documents get dense ids in insertion order, from 0", "[ADR-004]") {
    ExactMatchIndex index;
    CHECK(index.add(document_with("{}", {})) == DocId{0});
    CHECK(index.add(document_with("{}", {})) == DocId{1});
    CHECK(index.document_count() == 2);
}

TEST_CASE("a document's source is kept byte for byte", "[ADR-004]") {
    ExactMatchIndex index;
    const std::string source = R"({"id": 1, "price": 12.50, "name": "Sk\u00e4rm"})";
    const DocId document_id = index.add(document_with(source, {}));
    CHECK(index.source(document_id) == source);
}

TEST_CASE("a word finds every document containing it, in insertion order", "[M1]") {
    ExactMatchIndex index;
    index.add(document_with("{}", {"Corsair DDR5"}));
    index.add(document_with("{}", {"MSI B650"}));
    index.add(document_with("{}", {"Kingston", "DDR5 kit"}));
    CHECK(ids_containing(index, "DDR5") == std::vector{DocId{0}, DocId{2}});
    CHECK(ids_containing(index, "B650") == std::vector{DocId{1}});
    CHECK(ids_containing(index, "kit") == std::vector{DocId{2}});
    CHECK(ids_containing(index, "Asus").empty());
}

TEST_CASE("a word repeated within one document lists that document once", "[M1]") {
    ExactMatchIndex index;
    index.add(document_with("{}", {"DDR5 DDR5", "DDR5"}));
    CHECK(ids_containing(index, "DDR5") == std::vector{DocId{0}});
}

TEST_CASE("matching is exact: case is significant until M2 folds it", "[M1]") {
    ExactMatchIndex index;
    index.add(document_with("{}", {"Corsair"}));
    CHECK(ids_containing(index, "Corsair") == std::vector{DocId{0}});
    CHECK(ids_containing(index, "corsair").empty());
}
