#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "nova/base/flat_hash_map.h"

using namespace nova::static_impl;

// Test data structures
struct TestData {
  int id;
  std::string name;
  double value;

  TestData() = default;
  TestData(int i, const std::string& n, double v) : id(i), name(n), value(v) {}

  void Print() const {
    std::cout << "TestData{id=" << id << ", name=\"" << name
              << "\", value=" << value << "}" << std::endl;
  }
};

// Basic functionality demo
void BasicFlatHashMapDemo() {
  std::cout << "\n=== Basic FlatHashMap Demo ===" << std::endl;

  // Create a FlatHashMap with capacity 16
  FlatHashMap<int, std::string, 16> map;

  std::cout << "Created FlatHashMap with capacity: " << map.max_size()
            << std::endl;
  std::cout << "Initial size: " << map.size() << std::endl;
  std::cout << "Is empty: " << map.empty() << std::endl;

  // Insert some elements
  auto result1 = map.insert({1, "One"});
  std::cout << "Inserted (1, \"One\"): "
            << (result1.second ? "success" : "failed") << std::endl;

  auto result2 = map.insert({2, "Two"});
  std::cout << "Inserted (2, \"Two\"): "
            << (result2.second ? "success" : "failed") << std::endl;

  auto result3 = map.emplace(3, "Three");
  std::cout << "Emplaced (3, \"Three\"): "
            << (result3.second ? "success" : "failed") << std::endl;

  // Try to insert duplicate
  auto result4 = map.insert({1, "One_Duplicate"});
  std::cout << "Inserted duplicate (1, \"One_Duplicate\"): "
            << (result4.second ? "success" : "failed") << std::endl;

  std::cout << "\nAfter insertions:" << std::endl;
  std::cout << "Size: " << map.size() << std::endl;
  std::cout << "Load factor: " << std::fixed << std::setprecision(2)
            << map.load_factor() << std::endl;

  // Access elements
  std::cout << "\nAccessing elements:" << std::endl;
  std::cout << "map[1] = \"" << map[1] << "\"" << std::endl;
  std::cout << "map[2] = \"" << map[2] << "\"" << std::endl;
  std::cout << "map[3] = \"" << map[3] << "\"" << std::endl;

  // Using operator[] to insert new element
  map[4] = "Four";
  std::cout << "Added map[4] = \"Four\"" << std::endl;
  std::cout << "New size: " << map.size() << std::endl;

  // Find elements
  std::cout << "\nFinding elements:" << std::endl;
  auto it = map.find(2);
  if (it != map.end()) {
    std::cout << "Found key 2: \"" << it->second << "\"" << std::endl;
  }

  auto it_not_found = map.find(99);
  if (it_not_found == map.end()) {
    std::cout << "Key 99 not found (as expected)" << std::endl;
  }

  // Check contains
  std::cout << "Contains key 3: " << map.contains(3) << std::endl;
  std::cout << "Contains key 99: " << map.contains(99) << std::endl;
}

// Iterator demo
void IteratorDemo() {
  std::cout << "\n=== Iterator Demo ===" << std::endl;

  FlatHashMap<std::string, int, 32> map;

  // Insert some elements
  map["apple"] = 5;
  map["banana"] = 3;
  map["cherry"] = 8;
  map["date"] = 2;
  map["elderberry"] = 12;

  std::cout << "Inserted " << map.size() << " elements" << std::endl;

  // Iterate using range-based for loop
  std::cout << "\nUsing range-based for loop:" << std::endl;
  for (const auto& [key, value] : map) {
    std::cout << "  \"" << key << "\" -> " << value << std::endl;
  }

  // Iterate using iterators
  std::cout << "\nUsing iterators:" << std::endl;
  for (auto it = map.begin(); it != map.end(); ++it) {
    std::cout << "  \"" << it->first << "\" -> " << it->second << std::endl;
  }

  // Const iteration
  std::cout << "\nUsing const iterators:" << std::endl;
  for (auto it = map.cbegin(); it != map.cend(); ++it) {
    std::cout << "  \"" << it->first << "\" -> " << it->second << std::endl;
  }
}

// Complex object demo
void ComplexObjectDemo() {
  std::cout << "\n=== Complex Object Demo ===" << std::endl;

  FlatHashMap<int, TestData, 64> map;

  // Insert complex objects
  map.emplace(1, 1, "First", 1.1);
  map.emplace(2, 2, "Second", 2.2);
  map.emplace(3, 3, "Third", 3.3);

  // Using try_emplace
  auto result = map.try_emplace(4, 4, "Fourth", 4.4);
  std::cout << "try_emplace result: "
            << (result.second ? "inserted" : "existed") << std::endl;

  // Try to emplace with existing key
  auto result2 = map.try_emplace(2, 999, "Should not insert", 999.9);
  std::cout << "try_emplace existing key: "
            << (result2.second ? "inserted" : "existed") << std::endl;

  std::cout << "\nStored objects:" << std::endl;
  for (const auto& [key, data] : map) {
    std::cout << "Key " << key << ": ";
    data.Print();
  }

  // Access and modify
  std::cout << "\nModifying object at key 2:" << std::endl;
  map.at(2).value = 22.22;
  map.at(2).name = "Second_Modified";
  map.at(2).Print();
}

// Erase demo
void EraseDemo() {
  std::cout << "\n=== Erase Demo ===" << std::endl;

  FlatHashMap<int, std::string, 16> map;

  // Insert elements
  for (int i = 1; i <= 8; ++i) {
    map[i] = "Value_" + std::to_string(i);
  }

  std::cout << "Initial size: " << map.size() << std::endl;
  std::cout << "Elements: ";
  for (const auto& [key, value] : map) {
    std::cout << key << " ";
  }
  std::cout << std::endl;

  // Erase by key
  size_t erased = map.erase(3);
  std::cout << "\nErased key 3: " << erased << " elements removed" << std::endl;
  std::cout << "New size: " << map.size() << std::endl;

  // Erase by iterator
  auto it = map.find(5);
  if (it != map.end()) {
    std::cout << "Erasing key " << it->first << " by iterator" << std::endl;
    map.erase(it);
  }

  std::cout << "Size after iterator erase: " << map.size() << std::endl;
  std::cout << "Remaining elements: ";
  for (const auto& [key, value] : map) {
    std::cout << key << " ";
  }
  std::cout << std::endl;

  // Clear all
  map.clear();
  std::cout << "\nAfter clear():" << std::endl;
  std::cout << "Size: " << map.size() << std::endl;
  std::cout << "Is empty: " << map.empty() << std::endl;
}

// Hash collision demo
void HashCollisionDemo() {
  std::cout << "\n=== Hash Collision Demo ===" << std::endl;

  // Custom hash function that creates collisions
  struct BadHash {
    std::size_t operator()(int key) const {
      return key % 4;  // Force collisions
    }
  };

  FlatHashMap<int, std::string, 16> map;

  std::cout << "Using bad hash function (key % 4) to force collisions"
            << std::endl;

  // Insert elements that will collide
  std::vector<int> keys = {1, 5, 9, 13, 2, 6, 10, 14};

  for (int key : keys) {
    map[key] = "Value_" + std::to_string(key);
    std::cout << "Inserted key " << key << " (hash: " << BadHash{}(key) << ")"
              << std::endl;
  }

  std::cout << "\nFinal map contents:" << std::endl;
  for (const auto& [key, value] : map) {
    std::cout << "Key " << key << " (hash: " << BadHash{}(key) << ") -> \""
              << value << "\"" << std::endl;
  }

  std::cout << "\nTesting lookups after collisions:" << std::endl;
  for (int key : keys) {
    auto it = map.find(key);
    if (it != map.end()) {
      std::cout << "Found key " << key << ": \"" << it->second << "\""
                << std::endl;
    } else {
      std::cout << "Key " << key << " not found!" << std::endl;
    }
  }
}

// Performance comparison
void PerformanceComparison() {
  std::cout << "\n=== Performance Comparison ===" << std::endl;

  constexpr size_t kIterations = 10000;
  constexpr size_t kCapacity = 16384;

  // Test FlatHashMap
  {
    std::cout << "\nTesting FlatHashMap:" << std::endl;
    FlatHashMap<int, int, kCapacity>
        flat_map;

    auto start = std::chrono::high_resolution_clock::now();

    // Insert
    for (size_t i = 0; i < kIterations; ++i) {
      flat_map[i] = i * 2;
    }

    auto mid = std::chrono::high_resolution_clock::now();

    // Lookup
    int sum = 0;
    for (size_t i = 0; i < kIterations; ++i) {
      sum += flat_map[i];
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto insert_time =
        std::chrono::duration_cast<std::chrono::microseconds>(mid - start);
    auto lookup_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - mid);

    std::cout << "  Insert time: " << insert_time.count() << " μs" << std::endl;
    std::cout << "  Lookup time: " << lookup_time.count() << " μs" << std::endl;
    std::cout << "  Load factor: " << std::fixed << std::setprecision(2)
              << flat_map.load_factor() << std::endl;
    std::cout << "  Sum: " << sum << std::endl;
  }

  // Test std::unordered_map
  {
    std::cout << "\nTesting std::unordered_map:" << std::endl;
    std::unordered_map<int, int> std_map;
    std_map.reserve(kIterations);

    auto start = std::chrono::high_resolution_clock::now();

    // Insert
    for (size_t i = 0; i < kIterations; ++i) {
      std_map[i] = i * 2;
    }

    auto mid = std::chrono::high_resolution_clock::now();

    // Lookup
    int sum = 0;
    for (size_t i = 0; i < kIterations; ++i) {
      sum += std_map[i];
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto insert_time =
        std::chrono::duration_cast<std::chrono::microseconds>(mid - start);
    auto lookup_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - mid);

    std::cout << "  Insert time: " << insert_time.count() << " μs" << std::endl;
    std::cout << "  Lookup time: " << lookup_time.count() << " μs" << std::endl;
    std::cout << "  Load factor: " << std::fixed << std::setprecision(2)
              << std_map.load_factor() << std::endl;
    std::cout << "  Sum: " << sum << std::endl;
  }
}

// Capacity limit demo
void CapacityLimitDemo() {
  std::cout << "\n=== Capacity Limit Demo ===" << std::endl;

  FlatHashMap<int, std::string, 8>
      small_map;

  std::cout << "Created map with capacity: " << small_map.max_size()
            << std::endl;

  try {
    // Fill the map
    for (int i = 0; i < 8; ++i) {
      small_map[i] = "Value_" + std::to_string(i);
      std::cout << "Inserted key " << i << ", size: " << small_map.size()
                << ", load factor: " << std::fixed << std::setprecision(2)
                << small_map.load_factor() << std::endl;
    }

    std::cout << "Map is full: " << small_map.full() << std::endl;

    // Try to insert one more (should throw)
    std::cout << "Trying to insert beyond capacity..." << std::endl;
    small_map[8] = "Should_Fail";

  } catch (const std::runtime_error& e) {
    std::cout << "Caught expected exception: " << e.what() << std::endl;
  }
}

int main() {
  std::cout << "FlatHashMap Demo" << std::endl;
  std::cout << "================" << std::endl;

  try {
    BasicFlatHashMapDemo();
    IteratorDemo();
    ComplexObjectDemo();
    EraseDemo();
    HashCollisionDemo();
    PerformanceComparison();
    CapacityLimitDemo();

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  using Key = std::array<char, 20>;

  std::cout << std::is_trivial_v<FlatHashMap<Key, int>> << std::endl;
  std::cout << std::is_standard_layout_v<FlatHashMap<Key, int>> << std::endl;

  std::cout << "\nDemo completed successfully!" << std::endl;
  return 0;
}