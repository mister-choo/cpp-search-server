#include "read_input_functions.h"
#include <iostream>

using std::string, std::cin;
int ReadLineWithNumber() {
  int result;
  cin >> result;
  ReadLine();
  return result;
}
std::string ReadLine() {
  string s;
  getline(cin, s);
  return s;
}