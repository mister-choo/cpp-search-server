#include "document.h"

std::ostream& operator<<(std::ostream& out, const Document& document) {
  using namespace std::string_literals;
  out << "{ "s
      << "document_id = "s << document.id << ", "s
      << "relevance = "s << document.relevance << ", "s
      << "rating = "s << document.rating << " }"s;
  return out;
}
Document::Document(int id, double relevance, int rating)
    : id(id), relevance(relevance), rating(rating) {}

bool Document::operator==(const Document& oth) const {
  return id == oth.id and relevance == oth.relevance and rating == oth.rating;
}
bool Document::operator<(const Document& oth) const {
  if (id == oth.id) {
    if (relevance == oth.relevance) {
      return rating < oth.rating;
    }
    return relevance < oth.relevance;
  }
  return id < oth.id;
}
