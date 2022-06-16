#include "process_queries.h"
#include <algorithm>
#include <execution>
#include <functional>
#include <list>
#include <string>
#include <utility>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server,
                                                  const std::vector<std::string>& queries) {
  std::vector<std::vector<Document>> res(queries.size());
  /*for (const std::string& query : queries) {
    res.push_back(search_server.FindTopDocuments(query));
  }*/
  std::transform(
      std::execution::par, queries.begin(), queries.end(), res.begin(),
      [&search_server](const auto& query) { return search_server.FindTopDocuments(query); });
  return res;
}

/*template <typename T>
std::set<T> getUnion(const std::set<T>& a, const std::set<T>& b) {
  std::set<T> result = a;
  result.insert(b.begin(), b.end());
  return result;
}*/

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server,
                                         const std::vector<std::string>& queries) {
  std::list<Document> res;
  for (const auto& el : ProcessQueries(search_server, queries)) {
    std::list<Document> temp(el.begin(), el.end());
    res.splice(res.end(), temp);
  }
  return res;
}