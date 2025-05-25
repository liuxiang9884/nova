#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "nova/base/ring_pool.h"

// Fixed-size string struct (POD, trivial)
struct FixedString {
  char data[32];
};

// Test struct (POD, trivial)
struct TestStruct {
  int id;
  FixedString name;
  double value;
};

static_assert(std::is_standard_layout_v<FixedString>);
static_assert(std::is_trivial_v<FixedString>);
static_assert(std::is_trivially_copyable_v<FixedString>);
static_assert(std::is_default_constructible_v<FixedString>);
static_assert(std::is_copy_constructible_v<FixedString>);
static_assert(std::is_move_constructible_v<FixedString>);

static_assert(std::is_standard_layout_v<TestStruct>);
static_assert(std::is_trivial_v<TestStruct>);
static_assert(std::is_trivially_copyable_v<TestStruct>);
static_assert(std::is_default_constructible_v<TestStruct>);
static_assert(std::is_copy_constructible_v<TestStruct>);
static_assert(std::is_move_constructible_v<TestStruct>);

// Test basic operations
void TestBasicOperations() {
  std::cout << "\n=== Testing Basic Operations ===\n" << std::endl;
  nova::static_impl::RingPool<1024> pool;

  // Emplace object
  auto& obj1 = pool.Emplace<TestStruct>(1, FixedString{"test1"}, 1.1);
  auto& obj2 = pool.Emplace<TestStruct>(2, FixedString{"test2"}, 2.2);
  auto& obj3 = pool.Emplace<TestStruct>(3, FixedString{"test3"}, 3.3);

  std::cout << "Object 1: id=" << obj1.id << ", name=" << obj1.name.data
            << ", value=" << obj1.value << std::endl;
  std::cout << "Object 2: id=" << obj2.id << ", name=" << obj2.name.data
            << ", value=" << obj2.value << std::endl;
  std::cout << "Object 3: id=" << obj3.id << ", name=" << obj3.name.data
            << ", value=" << obj3.value << std::endl;

  // Allocate raw memory
  auto* raw_mem = pool.Allocate(100);
  std::cout << "Allocated 100 bytes at position: " << pool.latest_pos()
            << (raw_mem != nullptr ? " (success)" : " (fail)") << std::endl;

  // Allocate object without initialization
  auto& obj4 = pool.Allocate<TestStruct>();
  obj4.id = 4;
  std::strncpy(obj4.name.data, "test4", sizeof(obj4.name.data) - 1);
  obj4.value = 4.4;
  std::cout << "Object 4: id=" << obj4.id << ", name=" << obj4.name.data
            << ", value=" << obj4.value << std::endl;

  // Push raw data
  std::string data = "Test Data";
  auto* pushed_data = pool.Push(data.data(), data.size());
  std::cout << "Pushed data at position: " << pool.latest_pos() << std::endl;
  std::cout << "Data content: "
            << std::string_view(reinterpret_cast<char*>(pushed_data),
                                data.size())
            << std::endl;

  // Read back object
  auto& read_obj =
      pool.Read<TestStruct>(pool.latest_pos() - sizeof(TestStruct));
  std::cout << "Read object: id=" << read_obj.id
            << ", name=" << read_obj.name.data << ", value=" << read_obj.value
            << std::endl;

  std::cout << "Latest position: " << pool.latest_pos() << std::endl;
}

// Test ring buffer wrap-around
void TestRingWrap() {
  std::cout << "\n=== Testing Ring Wrap ===\n" << std::endl;
  nova::static_impl::RingPool<64> pool;  // Small pool to test wrap-around

  // Fill the pool
  for (int i = 0; i < 5; ++i) {
    auto* mem = pool.Allocate(10);
    std::cout << "Allocation " << i << " at position: " << pool.latest_pos()
              << (mem != nullptr ? " (success)" : " (fail)") << std::endl;
  }

  std::cout << "Write position after wrap: " << pool.write_pos() << std::endl;
  std::cout << "Write count: " << pool.write_count() << std::endl;
  std::cout << "Latest position: " << pool.latest_pos() << std::endl;
}

// Test multiple types
void TestMultipleTypes() {
  std::cout << "\n=== Testing Multiple Types ===\n" << std::endl;
  nova::static_impl::RingPool<128> pool;

  // Allocate int
  auto& int_val = pool.Allocate<int>();
  int_val = 42;
  std::cout << "Integer at position " << pool.latest_pos() << ": " << int_val
            << std::endl;

  // Allocate double
  auto& double_val = pool.Allocate<double>();
  double_val = 3.14159;
  std::cout << "Double at position " << pool.latest_pos() << ": " << double_val
            << std::endl;

  // Allocate FixedString
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

  std::cout << "Latest position: " << pool.latest_pos() << std::endl;
}

int main() {
  TestBasicOperations();
  TestRingWrap();
  TestMultipleTypes();
  return 0;
}