#include <iostream>
#include <string>

#include "nova/base/fixed_array.h"

void print_array(const nova::FixedArray<int, 5>& arr) {
  std::cout << "Array contents: ";
  for (const auto& item : arr) {
    std::cout << item << " ";
  }
  std::cout << "\nSize: " << arr.size() << "\n";
}

int main() {
  // Create a fixed array
  nova::FixedArray<int, 5> arr;

  // Test push_back
  std::cout << "Testing push_back:\n";
  arr.push_back(1);
  arr.push_back(2);
  arr.push_back(3);
  print_array(arr);

  // Test emplace_back
  std::cout << "\nTesting emplace_back:\n";
  arr.emplace_back(4);
  arr.emplace_back(5);
  print_array(arr);

  // Test element access
  std::cout << "\nTesting element access:\n";
  std::cout << "First element: " << arr.front() << "\n";
  std::cout << "Last element: " << arr.back() << "\n";
  std::cout << "Element at index 2: " << arr[2] << "\n";

  // Test iterators
  std::cout << "\nTesting iterators:\n";
  std::cout << "Forward iteration: ";
  for (auto it = arr.begin(); it != arr.end(); ++it) {
    std::cout << *it << " ";
  }
  std::cout << "\nReverse iteration: ";
  for (auto it = arr.rbegin(); it != arr.rend(); ++it) {
    std::cout << *it << " ";
  }
  std::cout << "\n";

  // Test pop_back
  std::cout << "\nTesting pop_back:\n";
  arr.pop_back();
  print_array(arr);

  // Test clear
  std::cout << "\nTesting clear:\n";
  arr.clear();
  std::cout << "Is empty: " << (arr.empty() ? "true" : "false") << "\n";

  // Test with custom type
  std::cout << "\nTesting with std::string:\n";
  nova::FixedArray<std::string, 3> str_arr;
  str_arr.emplace_back("Hello");
  str_arr.emplace_back("World");
  str_arr.emplace_back("!");

  for (const auto& str : str_arr) {
    std::cout << str << " ";
  }
  std::cout << "\n";

  return 0;
}