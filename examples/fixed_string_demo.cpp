//
// Created by liuxiang on 2025/5/28.
//

#include <iostream>
#include <string>
#include <unordered_map>

#include "nova/base/fixed_string.h"

using namespace nova;

void BasicFunctionalityDemo() {
  std::cout << "\n=== Basic Functionality Demo ===" << std::endl;

  // Test default constructor
  FixedString<32> str1;
  std::cout << "Default constructor: '" << str1.c_str()
            << "', size: " << str1.size() << std::endl;

  // Test constructor from C-style string
  FixedString<32> str2("Hello, World!");
  std::cout << "From C-string: '" << str2.c_str() << "', size: " << str2.size()
            << std::endl;

  // Test constructor from std::string
  std::string stdstr = "Standard string";
  FixedString<32> str3(stdstr);
  std::cout << "From std::string: '" << str3.c_str()
            << "', size: " << str3.size() << std::endl;

  // Test constructor from std::string_view
  std::string_view sv = "String view";
  FixedString<32> str4(sv);
  std::cout << "From string_view: '" << str4.c_str()
            << "', size: " << str4.size() << std::endl;

  // Test assignment
  str1 = "Assigned string";
  std::cout << "After assignment: '" << str1.c_str()
            << "', size: " << str1.size() << std::endl;

  // Test capacity and max_size
  std::cout << "Capacity: " << FixedString<32>::capacity() << std::endl;
  std::cout << "Max size: " << FixedString<32>::max_size << std::endl;
}

void ComparisonDemo() {
  std::cout << "\n=== Comparison Demo ===" << std::endl;

  FixedString<32> str1("Hello");
  FixedString<32> str2("Hello");
  FixedString<32> str3("World");

  std::cout << "str1 == str2: " << (str1 == str2) << std::endl;
  std::cout << "str1 == str3: " << (str1 == str3) << std::endl;
  std::cout << "str1 != str3: " << (str1 != str3) << std::endl;

  // Compare with C-string
  std::cout << "str1 == \"Hello\": " << (str1 == "Hello") << std::endl;
  std::cout << "str1 == \"World\": " << (str1 == "World") << std::endl;

  // Compare with string_view
  std::string_view sv = "Hello";
  std::cout << "str1 == string_view(\"Hello\"): " << (str1 == sv) << std::endl;
}

void ConversionDemo() {
  std::cout << "\n=== Conversion Demo ===" << std::endl;

  FixedString<32> str("Test string");

  // Test different access methods
  std::cout << "c_str(): " << str.c_str() << std::endl;
  std::cout << "data(): " << str.data() << std::endl;
  std::cout << "view(): " << str.view() << std::endl;
  std::cout << "string(): " << str.string() << std::endl;

  // Test empty and clear
  std::cout << "empty(): " << str.empty() << std::endl;
  str.Clear();
  std::cout << "After Clear() - empty(): " << str.empty()
            << ", size: " << str.size() << std::endl;
}

void HashDemo() {
  std::cout << "\n=== Hash Demo ===" << std::endl;

  // Test with unordered_map
  std::unordered_map<FixedString<32>, int, FixedStringHash<32>> map;

  map[FixedString<32>("key1")] = 100;
  map[FixedString<32>("key2")] = 200;
  map[FixedString<32>("key3")] = 300;

  std::cout << "Hash map contents:" << std::endl;
  for (const auto& pair : map) {
    std::cout << "  '" << pair.first.c_str() << "' -> " << pair.second
              << std::endl;
  }

  // Test lookup
  FixedString<32> lookup_key("key2");
  auto it = map.find(lookup_key);
  if (it != map.end()) {
    std::cout << "Found key2: " << it->second << std::endl;
  }
}

void ErrorHandlingDemo() {
  std::cout << "\n=== Error Handling Demo ===" << std::endl;

  try {
    // Try to create a string that's too long
    std::string long_string(50, 'x');  // 50 characters
    FixedString<32> str(long_string);  // Max 32 characters
  } catch (const std::length_error& e) {
    std::cout << "Expected error: " << e.what() << std::endl;
  }

  try {
    // Try assignment that's too long
    FixedString<16> str;
    str = "This string is definitely too long for a 16-character limit";
  } catch (const std::length_error& e) {
    std::cout << "Expected error: " << e.what() << std::endl;
  }
}

void DifferentSizesDemo() {
  std::cout << "\n=== Different Sizes Demo ===" << std::endl;

  FixedString<8> small("Small");
  FixedString<64> medium("Medium sized string");
  FixedString<256> large(
      "This is a much larger string that can hold more content");

  std::cout << "Small (max 8): '" << small.c_str()
            << "', size: " << small.size() << "/" << small.capacity()
            << std::endl;
  std::cout << "Medium (max 64): '" << medium.c_str()
            << "', size: " << medium.size() << "/" << medium.capacity()
            << std::endl;
  std::cout << "Large (max 256): '" << large.c_str()
            << "', size: " << large.size() << "/" << large.capacity()
            << std::endl;
}

void TruncationDemo() {
  std::cout << "\n=== Truncation Demo (Release Mode) ===" << std::endl;

  // Test truncation with different sizes
  std::string long_string =
      "This is a very long string that exceeds the capacity";

  FixedString<16> short_str(long_string);
  std::cout << "Original: '" << long_string
            << "' (length: " << long_string.size() << ")" << std::endl;
  std::cout << "Truncated to 16: '" << short_str.c_str()
            << "' (length: " << short_str.size() << ")" << std::endl;

  FixedString<32> medium_str(long_string);
  std::cout << "Truncated to 32: '" << medium_str.c_str()
            << "' (length: " << medium_str.size() << ")" << std::endl;

  // Test assignment truncation
  FixedString<8> tiny_str;
  tiny_str = "Assignment test with long string";
  std::cout << "Assignment truncated to 8: '" << tiny_str.c_str()
            << "' (length: " << tiny_str.size() << ")" << std::endl;
}

int main() {
  std::cout << "Nova FixedString Demo" << std::endl;
  std::cout << "====================" << std::endl;

  BasicFunctionalityDemo();
  ComparisonDemo();
  ConversionDemo();
  HashDemo();
  ErrorHandlingDemo();
  DifferentSizesDemo();
  TruncationDemo();

  std::cout << "\nDemo completed!" << std::endl;
  return 0;
}