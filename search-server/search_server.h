#pragma once

#include <algorithm>
#include <cmath>
#include <execution>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include "concurrent_map.h"
#include "document.h"
#include "string_processing.h"

using std::string_literals::operator""s;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const int BUCKET_COUNT = 1000;
constexpr double eps = 1e-6;
class SearchServer {
 public:
  // friend void RemoveDuplicates(SearchServer& search_server);
  template <typename StringContainer>
  explicit SearchServer(const StringContainer& stop_words);

  explicit SearchServer(const std::string& stop_words_text);

  explicit SearchServer(const std::string_view& stop_words_text);

  void AddDocument(int document_id,
                   const std::string_view& document,
                   DocumentStatus status,
                   const std::vector<int>& ratings);

  // todo
  template <typename DocumentPredicate>
  std::vector<Document> FindTopDocuments(const std::string_view& raw_query,
                                         DocumentPredicate pred) const;

  template <typename ExecutionPolicy, typename DocumentPredicate>
  std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy,
                                         const std::string_view& raw_query,
                                         DocumentPredicate pred) const;

  std::vector<Document> FindTopDocuments(const std::string_view& raw_query,
                                         DocumentStatus status) const;

  std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;

  template <typename ExecutionPolicy>
  std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy,
                                         const std::string_view& raw_query,
                                         DocumentStatus status) const;

  template <typename ExecutionPolicy>
  std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy,
                                         const std::string_view& raw_query) const;

  int GetDocumentCount() const;

  // int GetDocumentId(int index) const;

  using MatchedWords = std::tuple<std::vector<std::string_view>, DocumentStatus>;

  MatchedWords MatchDocument(const std::string_view& raw_query, int document_id) const;

  MatchedWords MatchDocument(std::execution::sequenced_policy seq,
                             const std::string_view& raw_query,
                             int document_id) const;

  MatchedWords MatchDocument(std::execution::parallel_policy par,
                             const std::string_view& raw_query,
                             int document_id) const;

  typename std::set<int>::const_iterator begin() const;

  typename std::set<int>::const_iterator end() const;

  const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

  void RemoveDocument(int document_id);

  void RemoveDocument(std::execution::parallel_policy par, int document_id);

  void RemoveDocument(std::execution::sequenced_policy seq, int document_id);

 private:
  struct DocumentData {
    int rating;
    DocumentStatus status;
  };
  static void DeleteCopies(std::vector<std::string_view>& vec);

  const std::set<std::string, std::less<>> stop_words_;

  std::map<std::string, std::map<int, double>, std::less<>> word_to_document_freqs_;

  std::map<int, DocumentData> documents_;

  std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;

  std::set<int> document_ids_;

  bool IsStopWord(const std::string_view& word) const;

  static bool IsValidWord(const std::string_view& word);

  std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view& text) const;

  static int ComputeAverageRating(const std::vector<int>& ratings);

  struct QueryWord {
    std::string_view data;
    bool is_minus;
    bool is_stop;
  };

  QueryWord ParseQueryWord(const std::string_view& text) const;

  struct Query {
    std::set<std::string_view> plus_words;
    std::set<std::string_view> minus_words;
  };

  struct NewQuery {
    std::vector<std::string_view> plus_words;
    std::vector<std::string_view> minus_words;
  };

  Query ParseQuery(const std::string_view& text) const;

  NewQuery ParseQuery(std::execution::parallel_policy par, const std::string_view& text) const;

  double ComputeWordInverseDocumentFreq(const std::string_view& word) const;

  template <typename DocumentPredicate>
  std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate pred) const;

  template <typename ExecutionPolicy, typename DocumentPredicate>
  std::vector<Document> FindAllDocuments(const ExecutionPolicy& policy,
                                         const NewQuery& query,
                                         DocumentPredicate pred) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
  if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
    throw std::invalid_argument("Some of stop words are invalid"s);
  }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query,
                                                     DocumentPredicate pred) const {
  const auto query = ParseQuery(raw_query);

  auto matched_documents = FindAllDocuments(query, pred);

  std::sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
              if (std::abs(lhs.relevance - rhs.relevance) < eps) {
                return lhs.rating > rhs.rating;
              }
              return lhs.relevance > rhs.relevance;
            });
  if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
    matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
  }

  return matched_documents;
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy,
                                                     const std::string_view& raw_query,
                                                     DocumentPredicate pred) const {
  if constexpr (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {
    return FindTopDocuments(raw_query, pred);
  } else {

    auto query = ParseQuery(policy, raw_query);
    DeleteCopies(query.plus_words);
    DeleteCopies(query.minus_words);

    auto matched_documents = FindAllDocuments(policy, query, pred);

    std::sort(policy, matched_documents.begin(), matched_documents.end(),
              [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) < eps) {
                  return lhs.rating > rhs.rating;
                }
                return lhs.relevance > rhs.relevance;
              });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
      matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
  }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query,
                                                     DocumentPredicate pred) const {
  std::map<int, double> document_to_relevance;
  for (const auto& word : query.plus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
    for (const auto& [document_id, term_freq] :
         word_to_document_freqs_.at(std::string(word))) {
      const auto& document_data = documents_.at(document_id);
      if (pred(document_id, document_data.status, document_data.rating)) {
        document_to_relevance[document_id] += term_freq * inverse_document_freq;
      }
    }
  }

  for (const auto& word : query.minus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    for (const auto& [document_id, _] : word_to_document_freqs_.at(std::string(word))) {
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

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const ExecutionPolicy& policy,
                                                     const NewQuery& query,
                                                     DocumentPredicate pred) const {

  ConcurrentMap<int, double> document_to_relevance(BUCKET_COUNT);

  std::for_each(
      policy, query.plus_words.cbegin(), query.plus_words.cend(),
      [this, &w = word_to_document_freqs_, &document_to_relevance, &pred](const auto& word) {
        if (w.count(word) != 0) {
          const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

          for (const auto& [document_id, term_freq] : w.at(std::string(word))) {
            const auto& document_data = documents_.at(document_id);
            if (pred(document_id, document_data.status, document_data.rating)) {
              document_to_relevance[document_id].ref_to_value +=
                  term_freq * inverse_document_freq;
            }
          }
        }
      });

  std::for_each(policy, query.minus_words.cbegin(), query.minus_words.cend(),
                [&w = word_to_document_freqs_, &document_to_relevance](const auto& word) {
                  if (w.count(word) != 0)
                    for (const auto& [document_id, _] : w.at(std::string(word))) {
                      document_to_relevance.erase(document_id);
                    }
                });

  const auto results = document_to_relevance.BuildFlatContainer();
  std::vector<Document> matched_documents(results.size());

  std::transform(policy, results.begin(), results.end(), matched_documents.begin(),
                 [&d = documents_](const auto& el) {
                   return Document{el.first, el.second, d.at(el.first).rating};
                 });
  return matched_documents;
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy,
                                                     const std::string_view& raw_query,
                                                     DocumentStatus status) const {
  return FindTopDocuments(
      policy, raw_query,
      [status]([[maybe_unused]] int document_id, DocumentStatus document_status,
               [[maybe_unused]] int rating) { return document_status == status; });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy,
                                                     const std::string_view& raw_query) const {
  return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}
