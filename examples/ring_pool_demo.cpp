#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "nova/base/ring_pool.h"

// Fixed-size string for testing
struct FixedString {
  char data[32];
};

static_assert(std::is_standard_layout_v<FixedString>);
static_assert(std::is_trivial_v<FixedString>);
static_assert(std::is_trivially_copyable_v<FixedString>);
static_assert(std::is_default_constructible_v<FixedString>);
static_assert(std::is_copy_constructible_v<FixedString>);
static_assert(std::is_move_constructible_v<FixedString>);

// Test struct for demonstration
struct TestStruct {
  int a;
  double b;
  FixedString c;
};

static_assert(std::is_standard_layout_v<TestStruct>);
static_assert(std::is_trivial_v<TestStruct>);
static_assert(std::is_trivially_copyable_v<TestStruct>);
static_assert(std::is_default_constructible_v<TestStruct>);
static_assert(std::is_copy_constructible_v<TestStruct>);
static_assert(std::is_move_constructible_v<TestStruct>);

void TestBasicOperations() {
  std::cout << "=== Testing Basic Operations ===" << std::endl;
  nova::StaticRingPool<1024> pool;

  // Test Emplace
  auto& obj1 = pool.Emplace<TestStruct>();
  obj1.a = 1;
  obj1.b = 3.14;
  std::strncpy(obj1.c.data, "Hello", sizeof(obj1.c.data) - 1);
  obj1.c.data[sizeof(obj1.c.data) - 1] = '\0';
  std::cout << "Emplaced object: a=" << obj1.a << ", b=" << obj1.b
            << ", c=" << obj1.c.data << std::endl;
  std::cout << "Position: " << pool.latest_pos() << std::endl;

  // Test Allocate with size
  auto* raw_mem = pool.Allocate(100);
  std::cout << "Allocated 100 bytes at position: " << pool.latest_pos()
            << std::endl;

  // Test template Allocate
  auto& obj2 = pool.Allocate<TestStruct>();
  obj2.a = 2;
  obj2.b = 2.718;
  std::strncpy(obj2.c.data, "World", sizeof(obj2.c.data) - 1);
  std::cout << "Allocated object: a=" << obj2.a << ", b=" << obj2.b
            << ", c=" << obj2.c.data << std::endl;
  std::cout << "Position: " << pool.latest_pos() << std::endl;

  // Test Push
  std::string data = "Test Data";
  auto* pushed_data = pool.Push(data.data(), data.size());
  std::cout << "Pushed data at position: " << pool.latest_pos() << std::endl;
  std::cout << "Data content: "
            << std::string_view(reinterpret_cast<char*>(pushed_data),
                                data.size())
            << std::endl;

  // Test Read
  auto& read_obj =
      pool.Read<TestStruct>(pool.latest_pos() - sizeof(TestStruct));
  std::cout << "Read object: a=" << read_obj.a << ", b=" << read_obj.b
            << ", c=" << read_obj.c.data << std::endl;

  // Test latest
  std::cout << "Latest position: " << pool.latest_pos() << std::endl;
}

void TestRingWrap() {
  std::cout << "\n=== Testing Ring Wrap ===" << std::endl;
  nova::StaticRingPool<64> pool;  // Small pool to test wrap-around

  // Fill the pool
  std::vector<size_t> positions;
  for (int i = 0; i < 5; ++i) {
    auto* mem = pool.Allocate(10);
    positions.push_back(pool.latest_pos());
    std::cout << "Allocation " << i << " at position: " << pool.latest_pos()
              << (mem != nullptr ) << std::endl;
  }

  // Verify wrap-around
  std::cout << "Write position after wrap: " << pool.write_pos() << std::endl;
  std::cout << "Write count: " << pool.write_count() << std::endl;
  std::cout << "Latest position: " << pool.latest_pos() << std::endl;
}

void TestMultipleTypes() {
  std::cout << "\n=== Testing Multiple Types ===" << std::endl;
  nova::StaticRingPool<1024> pool;

  // Allocate different types
  auto& int_val = pool.Allocate<int>();
  int_val = 42;
  std::cout << "Integer at position " << pool.latest_pos() << ": " << int_val
            << std::endl;

  auto& double_val = pool.Allocate<double>();
  double_val = 3.14159;
  std::cout << "Double at position " << pool.latest_pos() << ": " << double_val
            << std::endl;

  auto& str_val = pool.Allocate<FixedString>();
  std::strncpy(str_val.data, "Hello, StaticRingPool!",
               sizeof(str_val.data) - 1);
  std::cout << "String at position " << pool.latest_pos() << ": "
            << str_val.data << std::endl;

  // Read back using Read
  std::cout << "Read back integer: "
            << pool.Read<int>(pool.latest_pos() - sizeof(int) - sizeof(double) -
                              sizeof(FixedString))
            << std::endl;
  std::cout << "Read back double: "
            << pool.Read<double>(pool.latest_pos() - sizeof(double) -
                                 sizeof(FixedString))
            << std::endl;
  std::cout
      << "Read back string: "
      << pool.Read<FixedString>(pool.latest_pos() - sizeof(FixedString)).data
      << std::endl;

  // Test latest
  std::cout << "Latest position: " << pool.latest_pos() << std::endl;
}

int main() {
  TestBasicOperations();
  TestRingWrap();
  TestMultipleTypes();
  return 0;
}