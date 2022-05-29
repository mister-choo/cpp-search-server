#pragma once

#include "document.h"
#include "string_processing.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <iterator>


using std::string_literals::operator""s;
const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
 public:
  //friend void RemoveDuplicates(SearchServer& search_server);
  template <typename StringContainer>
  explicit SearchServer(const StringContainer& stop_words);

  explicit SearchServer(const std::string& stop_words_text);

  void AddDocument(int document_id,
                   const std::string& document,
                   DocumentStatus status,
                   const std::vector<int>& ratings);

  template <typename DocumentPredicate>
  std::vector<Document> FindTopDocuments(const std::string& raw_query,
                                         DocumentPredicate pred) const;

  std::vector<Document> FindTopDocuments(const std::string& raw_query,
                                         DocumentStatus status) const;

  std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

  int GetDocumentCount() const;

  //int GetDocumentId(int index) const;

  using MatchedWords = std::tuple<std::vector<std::string>, DocumentStatus>;

  MatchedWords MatchDocument(const std::string& raw_query, int document_id) const;

  typename std::set<int>::const_iterator begin() const;

  typename std::set<int>::const_iterator end() const;

  const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

  void RemoveDocument(int document_id);

 private:
  struct DocumentData {
    int rating;
    DocumentStatus status;
  };

  const std::set<std::string> stop_words_;

  std::map<std::string, std::map<int, double>> word_to_document_freqs_;
  std::map<int, DocumentData> documents_;

  std::map<int, std::map<std::string, double>> document_to_word_freqs_;

  std::set<int> document_ids_;

  /// std::map<int, std::set<std::string>> document_to_words_;

  //std::map<std::set<std::string>, std::set<int>> words_to_document_;

  bool IsStopWord(const std::string& word) const;

  static bool IsValidWord(const std::string& word);

  std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

  static int ComputeAverageRating(const std::vector<int>& ratings);

  struct QueryWord {
    std::string data;
    bool is_minus;
    bool is_stop;
  };

  QueryWord ParseQueryWord(const std::string& text) const;

  struct Query {
    std::set<std::string> plus_words;
    std::set<std::string> minus_words;
  };

  Query ParseQuery(const std::string& text) const;
  //friend void RemoveDuplicates(SearchServer& search_server);
  // Existence required
  double ComputeWordInverseDocumentFreq(const std::string& word) const;

  template <typename DocumentPredicate>
  std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate pred) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
  if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
    throw std::invalid_argument("Some of stop words are invalid"s);
  }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query,
                                                     DocumentPredicate pred) const {
  const auto query = ParseQuery(raw_query);

  auto matched_documents = FindAllDocuments(query, pred);

  std::sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
              if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
                return lhs.rating > rhs.rating;
              }
              return lhs.relevance > rhs.relevance;
            });
  if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
    matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
  }

  return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query,
                                                     DocumentPredicate pred) const {
  std::map<int, double> document_to_relevance;
  for (const std::string& word : query.plus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
    for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
      const auto& document_data = documents_.at(document_id);
      if (pred(document_id, document_data.status, document_data.rating)) {
        document_to_relevance[document_id] += term_freq * inverse_document_freq;
      }
    }
  }

  for (const std::string& word : query.minus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
      document_to_relevance.erase(document_id);
    }
  }

  std::vector<Document> matched_documents;
  matched_documents.reserve(document_to_relevance.size());
  for (const auto [document_id, relevance] : document_to_relevance) {
    matched_documents.emplace_back(document_id, relevance, documents_.at(document_id).rating);
  }
  return matched_documents;
}