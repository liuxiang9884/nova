//
// Created by liuxiang on 2025/5/28.
//

#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "nova/interprocess/shm_allocator.h"

using namespace nova;

// Helper function to cleanup existing shared memory
void CleanupShm(const std::string& shm_name) {
  // Ignore errors as the shared memory might not exist
  shm_unlink(shm_name.c_str());
}

// Test POD structures
struct Point {
  double x, y, z;
  int id;

  Point() = default;
  Point(double x_, double y_, double z_, int id_)
      : x(x_), y(y_), z(z_), id(id_) {}

  void Print() const {
    std::cout << "Point{id=" << id << ", x=" << x << ", y=" << y << ", z=" << z
              << "}" << std::endl;
  }
};

struct Employee {
  char name[64];
  int id;
  double salary;
  bool active;

  Employee() = default;
  Employee(const char* name_, int id_, double salary_, bool active_ = true)
      : id(id_), salary(salary_), active(active_) {
    std::strncpy(name, name_, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
  }

  void Print() const {
    std::cout << "Employee{id=" << id << ", name='" << name
              << "', salary=" << salary
              << ", active=" << (active ? "true" : "false") << "}" << std::endl;
  }
};

void BasicFunctionalityDemo() {
  std::cout << "\n=== Basic Functionality Demo ===" << std::endl;

  const std::string shm_name = "/nova_shm_demo";
  // 1MB storage
  const ShmAllocator::size_type storage_size = 1024 * 1024;

  // Clean up any existing shared memory
  CleanupShm(shm_name);

  try {
    // Create allocator
    ShmAllocator allocator(shm_name.c_str(), storage_size);

    std::cout << "Created ShmAllocator:" << std::endl;
    std::cout << "  Name: " << allocator.shm_name() << std::endl;
    std::cout << "  Total size: " << allocator.total_size() << " bytes"
              << std::endl;
    std::cout << "  Max instances: " << allocator.max_instances() << std::endl;
    std::cout << "  Storage size: " << allocator.storage_size() << " bytes"
              << std::endl;

    // Allocate some basic types
    auto int_ptr = allocator.Allocate<int>("my_int");
    *int_ptr = 42;
    std::cout << "Allocated int: " << *int_ptr << std::endl;

    auto double_ptr = allocator.Allocate<double>("my_double");
    *double_ptr = 3.14159;
    std::cout << "Allocated double: " << *double_ptr << std::endl;

    // Construct complex objects
    auto point_ptr = allocator.Construct<Point>("origin", 0.0, 0.0, 0.0, 1);
    std::cout << "Constructed point: ";
    point_ptr->Print();

    auto employee_ptr =
        allocator.Construct<Employee>("Alice", "Alice", 1001, 75000.0);
    std::cout << "Constructed employee: ";
    employee_ptr->Print();

    // Check existence
    std::cout << "\nExistence checks:" << std::endl;
    std::cout << "  'my_int' exists: " << allocator.Contains("my_int")
              << std::endl;
    std::cout << "  'my_int' constructed: " << allocator.IsConstructed("my_int")
              << std::endl;
    std::cout << "  'origin' exists: " << allocator.Contains("origin")
              << std::endl;
    std::cout << "  'origin' constructed: " << allocator.IsConstructed("origin")
              << std::endl;

    // Get instance list
    auto names = allocator.GetInstanceNames();
    std::cout << "\nAll instances (" << names.size() << "):" << std::endl;
    for (const auto& name : names) {
      std::cout << "  - " << name << std::endl;
    }

    // Memory usage statistics
    std::cout << "\nMemory usage:" << std::endl;
    std::cout << "  Instance count: " << allocator.instance_count()
              << std::endl;
    std::cout << "  Used storage: " << allocator.used_storage_size() << " bytes"
              << std::endl;
    std::cout << "  Available storage: " << allocator.available_storage_size()
              << " bytes" << std::endl;

    // Cleanup
    allocator.DeallocateAll();
    std::cout << "\nCleaned up shared memory" << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}

void PersistenceDemo() {
  std::cout << "\n=== Persistence Demo ===" << std::endl;

  const std::string shm_name = "/nova_shm_persist";
  // 512KB storage
  const ShmAllocator::size_type storage_size = 512 * 1024;

  // Clean up any existing shared memory
  CleanupShm(shm_name);

  try {
    // Phase 1: Create and populate data
    {
      ShmAllocator allocator(shm_name.c_str(), storage_size);
      std::cout << "Phase 1: Creating and populating data" << std::endl;

      // Create some employee data
      for (int i = 0; i < 5; ++i) {
        std::string name = "employee_" + std::to_string(i);
        std::string emp_name = "Employee" + std::to_string(i);
        allocator.Construct<Employee>(name, emp_name.c_str(), 1000 + i,
                                      50000.0 + i * 5000);
      }

      std::cout << "Created " << allocator.instance_count() << " employees"
                << std::endl;
      // allocator destructor will not delete shared memory
    }
    std::cout << "\nStart Phase 2" << std::endl;
    // Phase 2: Reconnect and read data
    {
      ShmAllocator allocator(shm_name.c_str(), 100, false);
      // Don't create, only connect
      std::cout << "\nPhase 2: Reconnecting and reading data" << std::endl;

      auto names = allocator.GetInstanceNames();
      std::cout << "Found " << names.size() << " instances:" << std::endl;

      for (const auto& name : names) {
        if (name.find("employee_") == 0) {
          auto emp_ptr = allocator.Find<Employee>(name);
          if (emp_ptr) {
            std::cout << "  ";
            emp_ptr->Print();
          }
        }
      }

      // Cleanup
      allocator.DeallocateAll();
    }

  } catch (const std::exception& e) {
    std::cerr << "Phase2 Error: " << e.what() << std::endl;
  }
}

void MultiProcessDemo() {
  std::cout << "\n=== Multi-Process Demo ===" << std::endl;

  const std::string shm_name = "/nova_shm_multiproc";
  // 1MB storage
  const ShmAllocator::size_type storage_size = 1024 * 1024;

  // Clean up any existing shared memory
  CleanupShm(shm_name);

  pid_t pid = fork();

  if (pid == 0) {
    // Child process: writer
    try {
      std::cout << "[Writer Process " << getpid() << "] Starting..."
                << std::endl;

      ShmAllocator allocator(shm_name.c_str(), storage_size);

      // Write some data
      for (int i = 0; i < 10; ++i) {
        std::string name = "point_" + std::to_string(i);
        allocator.Construct<Point>(name, i * 1.0, i * 2.0, i * 3.0, i);
        std::cout << "[Writer] Created " << name << std::endl;
        // 100ms delay
        usleep(100000);
      }

      std::cout << "[Writer] Finished writing data" << std::endl;

    } catch (const std::exception& e) {
      std::cerr << "[Writer] Error: " << e.what() << std::endl;
    }

  } else if (pid > 0) {
    // Parent process: reader
    try {
      // Wait for child process to create shared memory
      sleep(1);

      std::cout << "[Reader Process " << getpid() << "] Starting..."
                << std::endl;

      ShmAllocator allocator(shm_name.c_str(), 0, false);

      // Monitor data changes
      for (int i = 0; i < 15; ++i) {
        auto names = allocator.GetInstanceNames();
        std::cout << "[Reader] Found " << names.size() << " instances"
                  << std::endl;

        for (const auto& name : names) {
          if (name.find("point_") == 0) {
            auto point_ptr = allocator.Find<Point>(name);
            if (point_ptr && allocator.IsConstructed(name)) {
              std::cout << "[Reader] " << name << ": ";
              point_ptr->Print();
            }
          }
        }

        // 200ms delay
        usleep(200000);
      }

      // Wait for child process to finish
      wait(nullptr);

      // Cleanup
      allocator.DeallocateAll();
      std::cout << "[Reader] Cleaned up shared memory" << std::endl;

    } catch (const std::exception& e) {
      std::cerr << "[Reader] Error: " << e.what() << std::endl;
    }

  } else {
    std::cerr << "Failed to fork process" << std::endl;
  }
}

void PerformanceTest() {
  std::cout << "\n=== Performance Test ===" << std::endl;

  const std::string shm_name = "/nova_shm_perf";
  // 10MB storage
  const ShmAllocator::size_type storage_size = 10 * 1024 * 1024;
  const int num_objects = 1000;

  // Clean up any existing shared memory
  CleanupShm(shm_name);

  try {
    ShmAllocator allocator(shm_name.c_str(), storage_size);

    // Allocation performance test
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_objects; ++i) {
      std::string name = "obj_" + std::to_string(i);
      allocator.Allocate<Point>(name);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Allocation performance:" << std::endl;
    std::cout << "  " << num_objects << " objects in " << duration.count()
              << " μs" << std::endl;
    std::cout << "  " << (num_objects * 1000000.0 / duration.count())
              << " objects/second" << std::endl;

    // Construction performance test
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_objects; ++i) {
      std::string name = "constructed_" + std::to_string(i);
      allocator.Construct<Point>(name, i * 1.0, i * 2.0, i * 3.0, i);
    }

    end = std::chrono::high_resolution_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Construction performance:" << std::endl;
    std::cout << "  " << num_objects << " objects in " << duration.count()
              << " μs" << std::endl;
    std::cout << "  " << (num_objects * 1000000.0 / duration.count())
              << " objects/second" << std::endl;

    // Lookup performance test
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_objects; ++i) {
      std::string name = "obj_" + std::to_string(i);
      auto ptr = allocator.Find<Point>(name);
      // Avoid unused variable warning
      (void)ptr;
    }

    end = std::chrono::high_resolution_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Lookup performance:" << std::endl;
    std::cout << "  " << num_objects << " lookups in " << duration.count()
              << " μs" << std::endl;
    std::cout << "  " << (num_objects * 1000000.0 / duration.count())
              << " lookups/second" << std::endl;

    // Memory usage statistics
    std::cout << "\nMemory usage:" << std::endl;
    std::cout << "  Total instances: " << allocator.instance_count()
              << std::endl;
    std::cout << "  Used storage: " << allocator.used_storage_size() << " bytes"
              << std::endl;
    std::cout << "  Storage efficiency: "
              << (100.0 * allocator.used_storage_size() /
                  allocator.storage_size())
              << "%" << std::endl;

    // Cleanup
    allocator.DeallocateAll();

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}

void ErrorHandlingDemo() {
  std::cout << "\n=== Error Handling Demo ===" << std::endl;

  const std::string shm_name = "/nova_shm_error";
  // 1MB storage, large enough
  const ShmAllocator::size_type storage_size = 1024 * 1024;

  // Clean up any existing shared memory
  CleanupShm(shm_name);

  try {
    ShmAllocator allocator(shm_name.c_str(), storage_size);

    std::cout << "Testing capacity limits..." << std::endl;

    // Test instance count limit
    try {
      for (ShmAllocator::size_type i = 0; i < 10; ++i) {
        std::string name = "test_" + std::to_string(i);
        allocator.Construct<Point>(name, i, i, i, static_cast<int>(i));
        std::cout << "  Created instance " << i << std::endl;
      }
    } catch (const ShmAllocatorError& e) {
      std::cout << "  Expected error: " << e.what() << std::endl;
    }

    // Test memory space limit
    try {
      // Try to allocate large objects
      struct LargeObject {
        // 512KB, should exceed remaining space
        char data[512 * 1024];
      };

      for (int i = 0; i < 10; ++i) {
        std::string name = "large_" + std::to_string(i);
        allocator.Construct<LargeObject>(name);
        std::cout << "  Created large object " << i << std::endl;
      }
    } catch (const ShmAllocatorError& e) {
      std::cout << "  Expected error: " << e.what() << std::endl;
    }

    // Test name length limit
    try {
      // 50 character name
      std::string long_name(50, 'x');
      allocator.Allocate<int>(long_name);
    } catch (const ShmAllocatorError& e) {
      std::cout << "  Expected error: " << e.what() << std::endl;
    }

    // Cleanup
    allocator.DeallocateAll();

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}

int main() {
  std::cout << "Nova ShmAllocator Demo" << std::endl;
  std::cout << "======================" << std::endl;

  // BasicFunctionalityDemo();
  PersistenceDemo();
  // MultiProcessDemo();
  // PerformanceTest();
  // ErrorHandlingDemo();

  std::cout << "\nDemo completed!" << std::endl;
  return 0;
}