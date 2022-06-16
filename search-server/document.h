#pragma once

#include <iostream>

struct Document {
  Document() = default;

  Document(int id, double relevance, int rating);

  int id = 0;
  double relevance = 0.0;
  int rating = 0;

  bool operator<(const Document& oth) const;
  bool operator==(const Document& oth) const;
};

std::ostream& operator<<(std::ostream& out, const Document& document);

enum class DocumentStatus {
  ACTUAL,
  IRRELEVANT,
  BANNED,
  REMOVED,
};