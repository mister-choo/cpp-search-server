#include "string_processing.h"



std::vector<std::string_view> SplitIntoWords(const std::string_view& text) {
  std::vector<std::string_view> output;
  size_t first = 0;

  while (first < text.size()) {
    const auto second = text.find_first_of(' ', first);
    if (first != second) {
      output.emplace_back(text.substr(first, second - first));
    }
    if (second == std::string_view::npos) {
      break;
    }
    first = second + 1;
  }
  return output;
}