//
// Created by liuxiang on 2025/5/28.
//

#include "nova/interprocess/shm_allocator.h"

#include <sys/mman.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace nova;

// Helper function to cleanup existing shared memory
void CleanupShm(const std::string& shm_name) {
  shm_unlink(shm_name.c_str());
}

// Test data structures
struct TestPoint {
  double x, y, z;
  int id;

  TestPoint() = default;
  TestPoint(double x_, double y_, double z_, int id_)
      : x(x_), y(y_), z(z_), id(id_) {}

  bool operator==(const TestPoint& other) const {
    return x == other.x && y == other.y && z == other.z && id == other.id;
  }
};

struct TestEmployee {
  char name[32];
  int id;
  double salary;
  bool active;

  TestEmployee() = default;
  TestEmployee(const char* name_, int id_, double salary_, bool active_ = true)
      : id(id_), salary(salary_), active(active_) {
    std::strncpy(name, name_, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
  }

  bool operator==(const TestEmployee& other) const {
    return std::strcmp(name, other.name) == 0 && id == other.id &&
           salary == other.salary && active == other.active;
  }
};

// Test result tracking
struct TestResult {
  std::string test_name;
  bool passed;
  std::string error_message;
};

std::vector<TestResult> test_results;

void RunTest(const std::string& test_name, bool condition,
             const std::string& error_msg = "") {
  test_results.push_back({test_name, condition, error_msg});
  if (condition) {
    std::cout << test_name << " + pass" << std::endl;
  } else {
    std::cout << test_name << " + failed";
    if (!error_msg.empty()) {
      std::cout << " (" << error_msg << ")";
    }
    std::cout << std::endl;
  }
}

// Test basic allocation
void TestBasicAllocation() {
  const std::string shm_name = "/test_basic_alloc";
  CleanupShm(shm_name);

  try {
    ShmAllocator allocator(shm_name.c_str(), 1024 * 1024);

    // Test int allocation
    auto int_ptr = allocator.Allocate<int>("test_int");
    *int_ptr = 42;
    RunTest("TestBasicAllocation::int_allocation",
            int_ptr != nullptr && *int_ptr == 42);

    // Test double allocation
    auto double_ptr = allocator.Allocate<double>("test_double");
    *double_ptr = 3.14159;
    RunTest("TestBasicAllocation::double_allocation",
            double_ptr != nullptr && *double_ptr == 3.14159);

    // Test duplicate allocation returns same pointer
    auto int_ptr2 = allocator.Allocate<int>("test_int");
    RunTest("TestBasicAllocation::duplicate_allocation",
            int_ptr == int_ptr2 && *int_ptr2 == 42);

    allocator.DeallocateAll();
  } catch (const std::exception& e) {
    RunTest("TestBasicAllocation", false, e.what());
  }
}

// Test construction and destruction
void TestConstruction() {
  const std::string shm_name = "/test_construction";
  CleanupShm(shm_name);

  try {
    ShmAllocator allocator(shm_name.c_str(), 1024 * 1024);

    // Test point construction
    auto point_ptr =
        allocator.Construct<TestPoint>("test_point", 1.0, 2.0, 3.0, 123);
    TestPoint expected_point(1.0, 2.0, 3.0, 123);
    RunTest("TestConstruction::point_construction",
            point_ptr != nullptr && *point_ptr == expected_point);

    // Test employee construction
    auto emp_ptr =
        allocator.Construct<TestEmployee>("test_emp", "Alice", 1001, 75000.0);
    TestEmployee expected_emp("Alice", 1001, 75000.0);
    RunTest("TestConstruction::employee_construction",
            emp_ptr != nullptr && *emp_ptr == expected_emp);

    // Test duplicate construction returns same object
    auto point_ptr2 =
        allocator.Construct<TestPoint>("test_point", 999.0, 999.0, 999.0, 999);
    RunTest("TestConstruction::duplicate_construction",
            point_ptr == point_ptr2 && *point_ptr2 == expected_point);

    // Test IsConstructed
    RunTest("TestConstruction::is_constructed_true",
            allocator.IsConstructed("test_point") &&
                allocator.IsConstructed("test_emp"));

    // Test Contains
    RunTest("TestConstruction::contains",
            allocator.Contains("test_point") && allocator.Contains("test_emp"));

    // Test destructor
    allocator.Destruct<TestPoint>("test_point");
    RunTest("TestConstruction::after_destruction",
            !allocator.IsConstructed("test_point") &&
                allocator.Contains("test_point"));

    allocator.DeallocateAll();
  } catch (const std::exception& e) {
    RunTest("TestConstruction", false, e.what());
  }
}

// Test Find and Get operations
void TestFindAndGet() {
  const std::string shm_name = "/test_find_get";
  CleanupShm(shm_name);

  try {
    ShmAllocator allocator(shm_name.c_str(), 1024 * 1024);

    // Create test objects
    auto point_ptr =
        allocator.Construct<TestPoint>("find_point", 10.0, 20.0, 30.0, 456);
    auto emp_ptr =
        allocator.Construct<TestEmployee>("find_emp", "Bob", 2002, 85000.0);

    // Test Find
    auto found_point = allocator.Find<TestPoint>("find_point");
    auto found_emp = allocator.Find<TestEmployee>("find_emp");
    RunTest("TestFindAndGet::find_existing",
            found_point == point_ptr && found_emp == emp_ptr);

    // Test Find non-existent
    auto not_found = allocator.Find<TestPoint>("non_existent");
    RunTest("TestFindAndGet::find_non_existent", not_found == nullptr);

    // Test Get
    auto got_point = allocator.Get<TestPoint>("find_point");
    auto got_emp = allocator.Get<TestEmployee>("find_emp");
    RunTest("TestFindAndGet::get_existing",
            got_point == point_ptr && got_emp == emp_ptr);

    // Test GetBlock
    auto block_ptr = allocator.GetBlock("find_point");
    RunTest("TestFindAndGet::get_block",
            block_ptr == static_cast<void*>(point_ptr));

    allocator.DeallocateAll();
  } catch (const std::exception& e) {
    RunTest("TestFindAndGet", false, e.what());
  }
}

// Test string_view heterogeneous lookup
void TestHeterogeneousLookup() {
  const std::string shm_name = "/test_heterogeneous";
  CleanupShm(shm_name);

  try {
    ShmAllocator allocator(shm_name.c_str(), 1024 * 1024);

    // Create test object
    auto point_ptr =
        allocator.Construct<TestPoint>("hetero_test", 5.0, 6.0, 7.0, 789);

    // Test with string_view
    std::string_view name_view = "hetero_test";
    auto found_by_view = allocator.Find<TestPoint>(name_view);
    RunTest("TestHeterogeneousLookup::find_by_string_view",
            found_by_view == point_ptr);

    // Test Contains with string_view
    RunTest("TestHeterogeneousLookup::contains_by_string_view",
            allocator.Contains(name_view));

    // Test IsConstructed with string_view
    RunTest("TestHeterogeneousLookup::is_constructed_by_string_view",
            allocator.IsConstructed(name_view));

    // Test GetBlock with string_view
    auto block_by_view = allocator.GetBlock(name_view);
    RunTest("TestHeterogeneousLookup::get_block_by_string_view",
            block_by_view == static_cast<void*>(point_ptr));

    allocator.DeallocateAll();
  } catch (const std::exception& e) {
    RunTest("TestHeterogeneousLookup", false, e.what());
  }
}

// Test instance enumeration
void TestInstanceEnumeration() {
  const std::string shm_name = "/test_enumeration";
  CleanupShm(shm_name);

  try {
    ShmAllocator allocator(shm_name.c_str(), 1024 * 1024);

    // Create multiple instances
    allocator.Construct<TestPoint>("point1", 1.0, 1.0, 1.0, 1);
    allocator.Construct<TestPoint>("point2", 2.0, 2.0, 2.0, 2);
    allocator.Construct<TestEmployee>("emp1", "Alice", 1001, 50000.0);
    allocator.Construct<TestEmployee>("emp2", "Bob", 1002, 60000.0);

    // Test instance count
    RunTest("TestInstanceEnumeration::instance_count",
            allocator.instance_count() == 4);

    // Test max instances
    RunTest("TestInstanceEnumeration::max_instances",
            allocator.max_instances() == 1024);

    // Test GetInstanceNames
    auto names = allocator.GetInstanceNames();
    RunTest("TestInstanceEnumeration::names_count", names.size() == 4);

    // Check if all names are present
    bool all_names_found = true;
    std::vector<std::string> expected_names = {"point1", "point2", "emp1",
                                               "emp2"};
    for (const auto& expected : expected_names) {
      bool found = false;
      for (const auto& actual : names) {
        if (actual == expected) {
          found = true;
          break;
        }
      }
      if (!found) {
        all_names_found = false;
        break;
      }
    }
    RunTest("TestInstanceEnumeration::all_names_found", all_names_found);

    allocator.DeallocateAll();
  } catch (const std::exception& e) {
    RunTest("TestInstanceEnumeration", false, e.what());
  }
}

// Test memory usage statistics
void TestMemoryStatistics() {
  const std::string shm_name = "/test_memory_stats";
  CleanupShm(shm_name);

  const size_t storage_size = 1024 * 1024;

  try {
    ShmAllocator allocator(shm_name.c_str(), storage_size);

    // Test initial state
    RunTest("TestMemoryStatistics::initial_used_storage",
            allocator.used_storage_size() == 0);
    RunTest("TestMemoryStatistics::initial_available_storage",
            allocator.available_storage_size() == storage_size);
    RunTest("TestMemoryStatistics::storage_size",
            allocator.storage_size() == storage_size);

    // Allocate some objects
    allocator.Construct<TestPoint>("stat_point", 1.0, 2.0, 3.0, 1);
    allocator.Construct<TestEmployee>("stat_emp", "Charlie", 3003, 70000.0);

    // Test after allocation
    size_t used_after = allocator.used_storage_size();
    size_t available_after = allocator.available_storage_size();

    RunTest("TestMemoryStatistics::used_after_allocation", used_after > 0);
    RunTest("TestMemoryStatistics::available_after_allocation",
            available_after < storage_size);
    RunTest("TestMemoryStatistics::total_consistency",
            used_after + available_after == storage_size);

    allocator.DeallocateAll();
  } catch (const std::exception& e) {
    RunTest("TestMemoryStatistics", false, e.what());
  }
}

// Test persistence across allocator instances
void TestPersistence() {
  const std::string shm_name = "/test_persistence";
  CleanupShm(shm_name);

  try {
    // Phase 1: Create and populate
    {
      ShmAllocator allocator(shm_name.c_str(), 1024 * 1024);
      allocator.Construct<TestPoint>("persist_point", 100.0, 200.0, 300.0,
                                     12345);
      allocator.Construct<TestEmployee>("persist_emp", "David", 4004, 80000.0);

      RunTest("TestPersistence::phase1_creation",
              allocator.instance_count() == 2);
    }

    // Phase 2: Reconnect and verify
    {
      ShmAllocator allocator(shm_name.c_str(), 0, false);  // Don't create

      RunTest("TestPersistence::phase2_instance_count",
              allocator.instance_count() == 2);

      auto point_ptr = allocator.Find<TestPoint>("persist_point");
      auto emp_ptr = allocator.Find<TestEmployee>("persist_emp");

      TestPoint expected_point(100.0, 200.0, 300.0, 12345);
      TestEmployee expected_emp("David", 4004, 80000.0);

      RunTest("TestPersistence::phase2_point_data",
              point_ptr != nullptr && *point_ptr == expected_point);
      RunTest("TestPersistence::phase2_emp_data",
              emp_ptr != nullptr && *emp_ptr == expected_emp);

      allocator.DeallocateAll();
    }
  } catch (const std::exception& e) {
    RunTest("TestPersistence", false, e.what());
  }
}

// Test error conditions
void TestErrorHandling() {
  const std::string shm_name = "/test_error_handling";
  CleanupShm(shm_name);

  try {
    // Test opening non-existent shared memory
    bool caught_exception = false;
    try {
      ShmAllocator allocator("/non_existent_shm", 0, false);
    } catch (const ShmAllocatorError&) {
      caught_exception = true;
    }
    RunTest("TestErrorHandling::non_existent_shm", caught_exception);

    // Test memory exhaustion
    {
      ShmAllocator allocator(shm_name.c_str(), 1024);  // Small size

      struct LargeObject {
        char data[2048];  // Larger than available space
      };

      bool caught_exhaustion = false;
      try {
        allocator.Construct<LargeObject>("large_obj");
      } catch (const ShmAllocatorError&) {
        caught_exhaustion = true;
      }
      RunTest("TestErrorHandling::memory_exhaustion", caught_exhaustion);

      allocator.DeallocateAll();
    }

    // Test long name handling
    {
      ShmAllocator allocator(shm_name.c_str(), 1024 * 1024);

      // Name longer than FixedString capacity (32 chars)
      std::string long_name(50, 'x');
      bool caught_long_name = false;
      try {
        allocator.Allocate<int>(long_name);
      } catch (const std::exception&) {
        caught_long_name = true;
      }
      RunTest("TestErrorHandling::long_name", caught_long_name);

      allocator.DeallocateAll();
    }

  } catch (const std::exception& e) {
    RunTest("TestErrorHandling", false, e.what());
  }
}

// Test alignment
void TestAlignment() {
  const std::string shm_name = "/test_alignment";
  CleanupShm(shm_name);

  try {
    ShmAllocator allocator(shm_name.c_str(), 1024 * 1024);

    // Test different types with different alignment requirements
    auto char_ptr = allocator.Allocate<char>("test_char");
    auto int_ptr = allocator.Allocate<int>("test_int");
    auto double_ptr = allocator.Allocate<double>("test_double");

    // Check alignment
    RunTest("TestAlignment::char_alignment",
            reinterpret_cast<uintptr_t>(char_ptr) % alignof(char) == 0);
    RunTest("TestAlignment::int_alignment",
            reinterpret_cast<uintptr_t>(int_ptr) % alignof(int) == 0);
    RunTest("TestAlignment::double_alignment",
            reinterpret_cast<uintptr_t>(double_ptr) % alignof(double) == 0);

    allocator.DeallocateAll();
  } catch (const std::exception& e) {
    RunTest("TestAlignment", false, e.what());
  }
}

// Test ShmExists utility function
void TestShmExists() {
  const std::string shm_name = "/test_shm_exists";
  CleanupShm(shm_name);

  try {
    // Test non-existent
    RunTest("TestShmExists::non_existent",
            !ShmAllocator::ShmExists(shm_name.c_str()));

    // Create and test existent
    {
      ShmAllocator allocator(shm_name.c_str(), 1024);
      RunTest("TestShmExists::existent",
              ShmAllocator::ShmExists(shm_name.c_str()));
      allocator.DeallocateAll();
    }

    // Test after cleanup
    RunTest("TestShmExists::after_cleanup",
            !ShmAllocator::ShmExists(shm_name.c_str()));

  } catch (const std::exception& e) {
    RunTest("TestShmExists", false, e.what());
  }
}

int main() {
  std::cout << "ShmAllocator Comprehensive Test Suite" << std::endl;
  std::cout << "======================================" << std::endl;

  TestBasicAllocation();
  TestConstruction();
  TestFindAndGet();
  TestHeterogeneousLookup();
  TestInstanceEnumeration();
  TestMemoryStatistics();
  TestPersistence();
  TestErrorHandling();
  TestAlignment();
  TestShmExists();

  // Summary
  std::cout << "\nTest Summary:" << std::endl;
  int passed = 0, failed = 0;
  for (const auto& result : test_results) {
    if (result.passed) {
      passed++;
    } else {
      failed++;
    }
  }

  std::cout << "Total tests: " << test_results.size() << std::endl;
  std::cout << "Passed: " << passed << std::endl;
  std::cout << "Failed: " << failed << std::endl;

  if (failed == 0) {
    std::cout << "All tests passed! ✅" << std::endl;
  } else {
    std::cout << "Some tests failed! ❌" << std::endl;
  }

  return failed > 0 ? 1 : 0;
}