#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <iostream>

#include "nova/base/flat_hash_map.h"
#include <fcntl.h>

using namespace nova::static_impl;

// Test data structures for shared memory
struct Point {
  int x, y;

  bool operator==(const Point& other) const {
    return x == other.x && y == other.y;
  }
};

struct PersonData {
  int id;
  char name[32];
  double salary;
};

// Custom hash for Point
struct PointHash {
  std::size_t operator()(const Point& p) const noexcept {
    return std::hash<int>{}(p.x) ^ (std::hash<int>{}(p.y) << 1);
  }
};

// Custom equality for Point
struct PointEqual {
  bool operator()(const Point& lhs, const Point& rhs) const noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y;
  }
};

// Shared memory name
const char* SHM_NAME = "/nova_flat_hash_map_demo";

// Map type for shared memory
using SharedMapType = ShmFlatHashMap<int, PersonData, 32>;

void process1_writer() {
  std::cout << "Process 1 (Writer) PID: " << getpid() << std::endl;

  // Create shared memory
  const int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (shm_fd == -1) {
    perror("shm_open failed in process 1");
    exit(1);
  }

  // Set the size of shared memory
  const size_t shm_size = sizeof(SharedMapType);
  if (ftruncate(shm_fd, shm_size) == -1) {
    perror("ftruncate failed");
    close(shm_fd);
    exit(1);
  }

  // Map shared memory
  void* shm_ptr =
      mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (shm_ptr == MAP_FAILED) {
    perror("mmap failed in process 1");
    close(shm_fd);
    exit(1);
  }

  // Initialize FlatHashMap in shared memory using placement new
  SharedMapType* shared_map = new (shm_ptr) SharedMapType();

  std::cout << "Process 1: Created shared memory FlatHashMap" << std::endl;
  std::cout << "Process 1: Map size: " << sizeof(SharedMapType) << " bytes"
            << std::endl;
  std::cout << "Process 1: Map capacity: " << shared_map->capacity()
            << std::endl;

  // Insert test data
  PersonData employees[] = {{1001, "Alice Johnson", 75000.0},
                            {1002, "Bob Smith", 68000.0},
                            {1003, "Charlie Brown", 82000.0},
                            {1004, "Diana Prince", 95000.0},
                            {1005, "Eve Wilson", 71000.0}};

  std::cout << "\nProcess 1: Inserting employee data..." << std::endl;
  for (const auto& emp : employees) {
    (*shared_map)[emp.id] = emp;
    std::cout << "Process 1: Inserted ID " << emp.id << " -> " << emp.name
              << " (Salary: $" << emp.salary << ")" << std::endl;
  }

  std::cout << "\nProcess 1: Final map size: " << shared_map->size()
            << std::endl;
  std::cout << "Process 1: Load factor: " << shared_map->load_factor()
            << std::endl;

  std::cout
      << "\nProcess 1: Data written to shared memory. Waiting for process 2..."
      << std::endl;

  // Wait a bit for process 2 to read
  sleep(3);

  // Cleanup (don't call destructor as process 2 might still be using it)
  munmap(shm_ptr, shm_size);
  close(shm_fd);

  std::cout << "Process 1: Finished writing, exiting." << std::endl;
}

void process2_reader() {
  std::cout << "Process 2 (Reader) PID: " << getpid() << std::endl;

  // Wait a moment for process 1 to set up
  sleep(1);

  // Open existing shared memory
  int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
  if (shm_fd == -1) {
    perror("shm_open failed in process 2");
    exit(1);
  }

  // Map shared memory
  size_t shm_size = sizeof(SharedMapType);
  void* shm_ptr =
      mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (shm_ptr == MAP_FAILED) {
    perror("mmap failed in process 2");
    close(shm_fd);
    exit(1);
  }

  // Access the existing FlatHashMap in shared memory
  SharedMapType* shared_map = static_cast<SharedMapType*>(shm_ptr);

  std::cout << "Process 2: Accessed shared memory FlatHashMap" << std::endl;
  std::cout << "Process 2: Map size: " << shared_map->size() << std::endl;
  std::cout << "Process 2: Map capacity: " << shared_map->capacity()
            << std::endl;
  std::cout << "Process 2: Load factor: " << shared_map->load_factor()
            << std::endl;

  // Test lookups
  int test_ids[] = {1001, 1003, 1005, 1999};  // 1999 doesn't exist

  std::cout << "\nProcess 2: Looking up employee data..." << std::endl;
  for (int id : test_ids) {
    const auto it = shared_map->find(id);
    if (it != shared_map->end()) {
      const auto& emp = it->second;
      std::cout << "Process 2: Found ID " << id << " -> " << emp.name
                << " (Salary: $" << emp.salary << ")" << std::endl;
    } else {
      std::cout << "Process 2: ID " << id << " not found" << std::endl;
    }
  }

  // Iterate through all data
  std::cout << "\nProcess 2: All employees in shared memory:" << std::endl;
  for (const auto& [id, emp] : *shared_map) {
    std::cout << "Process 2: ID " << id << " -> " << emp.name << " (Salary: $"
              << emp.salary << ")" << std::endl;
  }

  // Test contains
  std::cout << "\nProcess 2: Testing contains()..." << std::endl;
  std::cout << "Process 2: Contains ID 1002: " << shared_map->contains(1002)
            << std::endl;
  std::cout << "Process 2: Contains ID 9999: " << shared_map->contains(9999)
            << std::endl;

  // Cleanup
  munmap(shm_ptr, shm_size);
  close(shm_fd);

  std::cout << "Process 2: Finished reading, exiting." << std::endl;
}

void cleanup_shared_memory() {
  // Clean up shared memory
  if (shm_unlink(SHM_NAME) == -1) {
    perror("shm_unlink failed");
  } else {
    std::cout << "Shared memory cleaned up successfully." << std::endl;
  }
}

int main() {
  std::cout << "Shared Memory FlatHashMap Cross-Process Demo" << std::endl;
  std::cout << "============================================" << std::endl;

  // Clean up any existing shared memory
  shm_unlink(SHM_NAME);

  // Fork to create two processes
  pid_t pid = fork();

  if (pid == -1) {
    perror("fork failed");
    return 1;
  } else if (pid == 0) {
    // Child process - reader
    process2_reader();
  } else {
    // Parent process - writer
    process1_writer();

    // Wait for child process to complete
    int status;
    wait(&status);

    // Clean up shared memory
    cleanup_shared_memory();

    std::cout << "\nDemo completed successfully!" << std::endl;
  }

  return 0;
}