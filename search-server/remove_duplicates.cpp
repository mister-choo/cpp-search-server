#include "remove_duplicates.h"
#include <string_view>

void RemoveDuplicates(SearchServer& search_server) {
  std::set<int> ids_to_remove;
  std::set<std::set<std::string_view>> word_sets{};
  for (const auto& document_id : search_server) {
    const auto& word_freqs = search_server.GetWordFrequencies(document_id);
    std::set<std::string_view> word_set{};
    for (const auto& [key, _] : word_freqs) {
      word_set.insert(key);
    }
    if (word_sets.count(word_set))
      ids_to_remove.insert(document_id);
    else
      word_sets.insert(word_set);
  }

  for (const auto id : ids_to_remove) {
    std::cout << "Found duplicate document id " << id << std::endl;
    search_server.RemoveDocument(id);
  }
}