#pragma once
#include "document.h"
#include "search_server.h"

#include <deque>
#include <string>
#include <vector>

class RequestQueue {
 public:
  explicit RequestQueue(const SearchServer& search_server);

  template <typename DocumentPredicate>
  std::vector<Document> AddFindRequest(const std::string& raw_query,
                                  DocumentPredicate document_predicate);

  std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

  std::vector<Document> AddFindRequest(const std::string& raw_query);

  int GetNoResultRequests() const;

 private:
  struct QueryResult {
    bool success;
  };
  std::deque<QueryResult> requests_;
  const static int min_in_day_ = 1440;
  int time = 0;
  const SearchServer& search_server_;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query,
                                              DocumentPredicate document_predicate) {
  const auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
  requests_.push_back({not result.empty()});
  if (requests_.size() > min_in_day_) {
    requests_.pop_front();
  }
  time++;
  return result;
}