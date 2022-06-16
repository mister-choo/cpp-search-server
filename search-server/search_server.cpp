#include "search_server.h"
#include <algorithm>
#include <execution>
#include <numeric>
#include <utility>

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {}

SearchServer::SearchServer(const std::string_view& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {}

void SearchServer::AddDocument(int document_id,
                               const std::string_view& document,
                               DocumentStatus status,
                               const std::vector<int>& ratings) {
  if ((document_id < 0) || (documents_.count(document_id) > 0)) {
    throw std::invalid_argument("Invalid document_id"s);
  }
  const auto words = SplitIntoWordsNoStop(document);

  const double inv_word_count = 1.0 / words.size();
  for (const auto& word : words) {
    word_to_document_freqs_[std::string(word)][document_id] += inv_word_count;
    document_to_word_freqs_[document_id][std::move(word)] += inv_word_count;
  }

  documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
  document_ids_.insert(document_id);
}

int SearchServer::GetDocumentCount() const {
  return documents_.size();
}

SearchServer::MatchedWords SearchServer::MatchDocument(const std::string_view& raw_query,
                                                       int document_id) const {
  const auto query = ParseQuery(raw_query);
  std::vector<std::string_view> matched_words;
  for (const auto& word : query.minus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    if (word_to_document_freqs_.at(std::string(word)).count(document_id)) {
      matched_words.clear();
      return {matched_words, documents_.at(document_id).status};
    }
  }

  for (const auto& word : query.plus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    if (word_to_document_freqs_.at(std::string(word)).count(document_id)) {
      matched_words.push_back(word);
    }
  }

  return {matched_words, documents_.at(document_id).status};
}

SearchServer::MatchedWords SearchServer::MatchDocument(
    [[maybe_unused]] std::execution::sequenced_policy seq,
    const std::string_view& raw_query,
    int document_id) const {

  return MatchDocument(raw_query, document_id);
}



SearchServer::MatchedWords SearchServer::MatchDocument(
    [[maybe_unused]] std::execution::parallel_policy par,
    const std::string_view& raw_query,
    int document_id) const {

  const auto query = ParseQuery(par, raw_query);

  if (query.plus_words.empty() or
      any_of(query.minus_words.begin(), query.minus_words.end(),
             [&w = word_to_document_freqs_, document_id](const auto& word) {
               return w.count(word) && w.at(std::string(word)).count(document_id);
             })) {
    return MatchedWords{std::vector<std::string_view>{}, documents_.at(document_id).status};
  }
  std::vector<std::string_view> matched_words(query.plus_words.size());
  const auto it = copy_if(std::execution::par, query.plus_words.begin(),
                          query.plus_words.end(), matched_words.begin(),
                          [&w = word_to_document_freqs_, document_id](const auto& word) {
                            return w.count(word) && w.at(std::string(word)).count(document_id);
                          });
  matched_words.erase(it, matched_words.end());
  DeleteCopies(matched_words);
  return MatchedWords{matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const std::string_view& word) const {
  return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string_view& word) {
  // A valid word must not contain special characters
  return std::none_of(word.begin(), word.end(), [](char c) { return c >= '\0' && c < ' '; });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(
    const std::string_view& text) const {
  std::vector<std::string_view> words;
  for (const auto& word : SplitIntoWords(text)) {
    if (!IsValidWord(word)) {
      throw std::invalid_argument("Word "s + std::string(word) + " is invalid"s);
    }
    if (!IsStopWord(word)) {
      words.push_back(word);
    }
  }
  return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
  if (ratings.empty()) {
    return 0;
  }
  return std::accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string_view& text) const {
  if (text.empty()) {
    throw std::invalid_argument("Query word is empty"s);
  }
  std::string_view word = text;
  bool is_minus = false;
  if (word[0] == '-') {
    is_minus = true;
    word = word.substr(1);
  }
  if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
    throw std::invalid_argument("Query word "s + std::string(text) + " is invalid");
  }

  return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view& text) const {
  Query result;
  for (const auto& word : SplitIntoWords(text)) {
    const auto query_word = ParseQueryWord(word);
    if (!query_word.is_stop) {
      if (query_word.is_minus) {
        result.minus_words.insert(query_word.data);
      } else {
        result.plus_words.insert(query_word.data);
      }
    }
  }
  return result;
}

SearchServer::NewQuery SearchServer::ParseQuery(std::execution::parallel_policy par,
                                                const std::string_view& text) const {
  NewQuery res;
  for (const auto& word : SplitIntoWords(text)) {
    const auto query_word = ParseQueryWord(word);
    if (!query_word.is_stop) {
      if (query_word.is_minus) {
        res.minus_words.push_back(query_word.data);
      } else {
        res.plus_words.push_back(query_word.data);
      }
    }
  }
  return res;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view& word) const {
  return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(std::string(word)).size());
}
typename std::set<int>::const_iterator SearchServer::end() const {
  return document_ids_.end();
}
typename std::set<int>::const_iterator SearchServer::begin() const {
  return document_ids_.begin();
}
const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(
    int document_id) const {
  static const std::map<std::string_view, double> empty_map{};
  if (document_to_word_freqs_.count(document_id) == 0) {
    return empty_map;
  }

  return document_to_word_freqs_.at(document_id);
}
void SearchServer::RemoveDocument(int document_id) {
  for (const auto& [word, _] : document_to_word_freqs_[document_id]) {
    word_to_document_freqs_[std::string(word)].erase(document_id);
  }
  documents_.erase(document_id);

  document_to_word_freqs_.erase(document_id);

  document_ids_.erase(document_id);
}

void SearchServer::RemoveDocument([[maybe_unused]] std::execution::sequenced_policy seq,
                                  int document_id) {
  RemoveDocument(document_id);
}

using str_ptr = const std::string_view*;

void SearchServer::RemoveDocument([[maybe_unused]] std::execution::parallel_policy par,
                                  int document_id) {

  std::vector<str_ptr> res(document_to_word_freqs_[document_id].size());
  std::transform(std::execution::par, document_to_word_freqs_[document_id].begin(),
                 document_to_word_freqs_[document_id].end(), res.begin(),
                 [](const auto& el) { return &(el.first); });

  std::for_each(std::execution::par, res.begin(), res.end(),
                [&w_to_d = word_to_document_freqs_, document_id](const auto& el) {
                  w_to_d.at(std::string(*el)).erase(document_id);
                });

  documents_.erase(document_id);

  document_to_word_freqs_.erase(document_id);

  document_ids_.erase(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query,
                                                     DocumentStatus status) const {
  return FindTopDocuments(
      raw_query, [status]([[maybe_unused]] int document_id, DocumentStatus document_status,
                          [[maybe_unused]] int rating) { return document_status == status; });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query) const {
  return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}
void SearchServer::DeleteCopies(std::vector<std::string_view>& vec) {
  std::sort(std::execution::par, vec.begin(), vec.end());
  const auto last = std::unique(std::execution::par, vec.begin(), vec.end());
  vec.erase(last, vec.end());
}