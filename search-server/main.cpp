﻿#include "document.h"
#include "paginator.h"
#include "read_input_functions.h"
#include "remove_duplicates.h"
#include "request_queue.h"
#include "search_server.h"
#include "string_processing.h"

using std::string_literals::operator""s;

void AddDocument(SearchServer& search_server,
                 int document_id,
                 const std::string& document,
                 DocumentStatus status,
                 const std::vector<int>& ratings) {
  search_server.AddDocument(document_id, document, status, ratings);
}

using namespace std;

int main() {
  SearchServer search_server("and with"s);

  AddDocument(search_server, 1, "funny pet nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
  AddDocument(search_server, 2, "funny pet curly hair"s, DocumentStatus::ACTUAL, {1, 2});

  // дубликат документа 2, будет удалён
  AddDocument(search_server, 3, "funny pet curly hair"s, DocumentStatus::ACTUAL, {1, 2});

  // отличие только в стоп-словах, считаем дубликатом
  AddDocument(search_server, 4, "funny pet curly hair"s, DocumentStatus::ACTUAL, {1, 2});

  // множество слов такое же, считаем дубликатом документа 1
  AddDocument(search_server, 5, "funny funny pet nasty nasty rat"s, DocumentStatus::ACTUAL,
              {1, 2});

  // добавились новые слова, дубликатом не является
  AddDocument(search_server, 6, "funny pet not very nasty rat"s, DocumentStatus::ACTUAL,
              {1, 2});

  // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
  AddDocument(search_server, 7, "very nasty rat not very funny pet"s,
              DocumentStatus::ACTUAL, {1, 2});

  // есть не все слова, не является дубликатом
  AddDocument(search_server, 8, "pet rat rat rat"s, DocumentStatus::ACTUAL,
              {1, 2});

  // слова из разных документов, не является дубликатом
  AddDocument(search_server, 9, "nasty rat curly hair"s, DocumentStatus::ACTUAL, {1, 2});

  cout << "Before duplicates removed: "s << search_server.GetDocumentCount() << endl;
  RemoveDuplicates(search_server);
  cout << "After duplicates removed: "s << search_server.GetDocumentCount() << endl;
}