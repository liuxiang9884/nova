#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#include "nova/base/ring_pool.h"

using namespace nova;

// Test data structure
struct TestData {
  int32_t id;
  double value;
  char name[32];

  void Print() const {
    std::cout << "TestData{id=" << id << ", value=" << value
              << ", name=" << name << "}" << std::endl;
  }
};

// Performance test data structure
struct PerformanceData {
  int64_t timestamp;
  double value;
  char description[64];
};

// Basic functionality demo for dynamic RingPool
void DynamicRingPoolDemo() {
  std::cout << "\n=== Dynamic RingPool Demo ===" << std::endl;

  // Create a 1KB pool
  RingPool pool(1024);
  std::cout << "Pool capacity: " << pool.capacity() << " bytes" << std::endl;

  // Use Emplace to construct objects
  auto& data1 = pool.Emplace<TestData>();
  size_t data1_pos = pool.latest_pos();
  data1.id = 1;
  data1.value = 3.14;
  std::strcpy(data1.name, "test1");
  std::cout << "Emplaced data1 at position " << data1_pos << ": ";
  data1.Print();

  // Use Allocate to allocate memory
  auto& data2 = pool.Allocate<TestData>();
  size_t data2_pos = pool.latest_pos();
  data2.id = 2;
  data2.value = 2.718;
  std::strcpy(data2.name, "test2");
  std::cout << "Allocated data2 at position " << data2_pos << ": ";
  data2.Print();

  // Use Push to copy data
  TestData data3{3, 1.414, "test3"};
  auto* ptr = pool.Push(&data3, sizeof(TestData));
  size_t data3_pos = pool.latest_pos();
  auto& data3_ref = *reinterpret_cast<TestData*>(ptr);
  std::cout << "Pushed data3 at position " << data3_pos << ": ";
  data3_ref.Print();

  // Use Read to read data
  auto& data1_read = pool.Read<TestData>(data1_pos);
  std::cout << "Read data1 from position " << data1_pos << ": ";
  data1_read.Print();

  auto& data2_read = pool.Read<TestData>(data2_pos);
  std::cout << "Read data2 from position " << data2_pos << ": ";
  data2_read.Print();

  auto& data3_read = pool.Read<TestData>(data3_pos);
  std::cout << "Read data3 from position " << data3_pos << ": ";
  data3_read.Print();

  // Display statistics
  std::cout << "Write position: " << pool.write_pos() << std::endl;
  std::cout << "Write count: " << pool.write_count() << std::endl;
  std::cout << "Available space: " << pool.available_space() << std::endl;
}

// Basic functionality demo for static RingPool
void StaticRingPoolDemo() {
  std::cout << "\n=== Static RingPool Demo ===" << std::endl;

  // Create a 1KB pool
  static_impl::RingPool<1024> pool;
  std::cout << "Pool capacity: " << pool.capacity() << " bytes" << std::endl;

  // Use Emplace to construct objects
  auto& data1 = pool.Emplace<TestData>();
  size_t data1_pos = pool.latest_pos();
  data1.id = 1;
  data1.value = 3.14;
  std::strcpy(data1.name, "test1");
  std::cout << "Emplaced data1 at position " << data1_pos << ": ";
  data1.Print();

  // Use Allocate to allocate memory
  auto& data2 = pool.Allocate<TestData>();
  size_t data2_pos = pool.latest_pos();
  data2.id = 2;
  data2.value = 2.718;
  std::strcpy(data2.name, "test2");
  std::cout << "Allocated data2 at position " << data2_pos << ": ";
  data2.Print();

  // Use Push to copy data
  TestData data3{3, 1.414, "test3"};
  auto* ptr = pool.Push(&data3, sizeof(TestData));
  size_t data3_pos = pool.latest_pos();
  auto& data3_ref = *reinterpret_cast<TestData*>(ptr);
  std::cout << "Pushed data3 at position " << data3_pos << ": ";
  data3_ref.Print();

  // Use Read to read data
  auto& data1_read = pool.Read<TestData>(data1_pos);
  std::cout << "Read data1 from position " << data1_pos << ": ";
  data1_read.Print();

  auto& data2_read = pool.Read<TestData>(data2_pos);
  std::cout << "Read data2 from position " << data2_pos << ": ";
  data2_read.Print();

  auto& data3_read = pool.Read<TestData>(data3_pos);
  std::cout << "Read data3 from position " << data3_pos << ": ";
  data3_read.Print();

  // Display statistics
  std::cout << "Write position: " << pool.write_pos() << std::endl;
  std::cout << "Write count: " << pool.write_count() << std::endl;
  std::cout << "Available space: " << pool.available_space() << std::endl;
}

// Performance comparison between dynamic and static RingPool
void PerformanceComparisonDemo() {
  std::cout << "\n=== Performance Comparison Demo ===" << std::endl;

  constexpr size_t kPoolSize = 1024 * 1024;  // 1MB
  constexpr size_t kIterations = 10000;      // 10K operations

  std::cout << "cmp size: " << kPoolSize << ", "
            << kIterations * sizeof(PerformanceData) << std::endl;
  //
  // // Test dynamic RingPool
  // {
  //   std::cout << "\nTesting Dynamic RingPool:" << std::endl;
  //   RingPool pool(kPoolSize);
  //   std::vector<PerformanceData> results;
  //   results.reserve(kIterations);
  //
  //   auto start = std::chrono::high_resolution_clock::now();
  //
  //   for (size_t i = 0; i < kIterations; ++i) {
  //     auto& data = pool.Emplace<PerformanceData>();
  //     data.timestamp =
  //         std::chrono::duration_cast<std::chrono::nanoseconds>(
  //             std::chrono::high_resolution_clock::now().time_since_epoch())
  //             .count();
  //     data.value = static_cast<double>(i);
  //     std::snprintf(data.description, sizeof(data.description),
  //                   "Performance test data %zu", i);
  //     results.push_back(data);
  //   }
  //
  //   auto end = std::chrono::high_resolution_clock::now();
  //   auto duration =
  //       std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  //
  //   double total_time = duration.count() / 1000000.0;
  //   double ops_per_second = kIterations / total_time;
  //   double avg_latency = (total_time * 1000000.0) / kIterations;
  //
  //   std::cout << "Total time: " << std::fixed << std::setprecision(3)
  //             << total_time << " seconds" << std::endl;
  //   std::cout << "Operations per second: " << std::fixed <<
  //   std::setprecision(0)
  //             << ops_per_second << " ops/s" << std::endl;
  //   std::cout << "Average latency: " << std::fixed << std::setprecision(3)
  //             << avg_latency << " us" << std::endl;
  //
  //   auto sum = 0.;
  //   for (auto data : results) {
  //     sum += data.value;
  //   }
  //   std::cout << "Sum: " << sum << std::endl;
  // }

  // Test static RingPool

  std::cout << "\nTesting Static RingPool:" << std::endl;
  static_impl::RingPool<kPoolSize> pool;
  // std::vector<PerformanceData> results;
  // results.reserve(kIterations);

  // auto start = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < kIterations; ++i) {
    auto& data = pool.Emplace<PerformanceData>();
    data.timestamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    data.value = static_cast<double>(i);
    // std::snprintf(data.description, sizeof(data.description),
    //               "Performance test data %zu", i);
    // results.push_back(data);
  }

  // auto end = std::chrono::high_resolution_clock::now();
  // auto duration =
  //     std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  //
  // double total_time = duration.count() / 1000000.0;
  // double ops_per_second = kIterations / total_time;
  // double avg_latency = (total_time * 1000000.0) / kIterations;
  //
  // std::cout << "Total time: " << std::fixed << std::setprecision(3)
  //           << total_time << " seconds" << std::endl;
  // std::cout << "Operations per second: " << std::fixed <<
  // std::setprecision(0)
  //           << ops_per_second << " ops/s" << std::endl;
  // std::cout << "Average latency: " << std::fixed << std::setprecision(3)
  //           << avg_latency << " us" << std::endl;
  //
  // auto sum = 0.;
  // for (auto data : results) {
  //   sum += data.value;
  // }
  // std::cout << "Sum: " << sum << std::endl;

  std::cout << "haha" << std::endl;
}

// Performance comparison between dynamic and static RingPool
void TestStaticRingPool() {
  std::cout << "\n=== Test static ring pool ===" << std::endl;

  constexpr size_t kPoolSize = 128 * 1024;  // 128KB
  constexpr size_t kIterations = 20000;     // 20K operations

  std::cout << "cmp size: " << kPoolSize << ", "
            << kIterations * sizeof(PerformanceData) << std::endl;

  std::cout << "\nTesting Static RingPool:" << std::endl;
  static_impl::RingPool<kPoolSize> pool;

  for (size_t i = 0; i < kIterations; ++i) {
    auto& data = pool.Emplace<PerformanceData>();
    data.timestamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    data.value = static_cast<double>(i);
    std::cout << pool.write_count() << ", " << pool.latest_pos() << ","
              << pool.write_pos() << std::endl;
  }

  // Print final state
  std::cout << "Final state:" << std::endl;
  std::cout << "Write count: " << pool.write_count() << std::endl;
  std::cout << "Latest pos: " << pool.latest_pos() << std::endl;
  std::cout << "Write pos: " << pool.write_pos() << std::endl;
  std::cout << "Pool size: " << kPoolSize << std::endl;
  std::cout << "Data size: " << sizeof(PerformanceData) << std::endl;

  // Explicitly call destructors for all objects
  size_t pos = 0;
  while (pos < pool.write_pos()) {
    if (pos + sizeof(PerformanceData) <= pool.write_pos()) {
      auto* ptr = reinterpret_cast<PerformanceData*>(pool.At(pos));
      ptr->~PerformanceData();
    }
    pos += sizeof(PerformanceData);
  }

  std::cout << "haha" << std::endl;
}
// Memory alignment demo
void AlignmentDemo() {
  std::cout << "\n=== Alignment Demo ===" << std::endl;

  // Test with dynamic RingPool
  {
    std::cout << "\nTesting Dynamic RingPool:" << std::endl;
    RingPool pool(1024);
    std::cout << "Pool alignment: " << pool.kAlignment << " bytes" << std::endl;

    struct alignas(1) SmallData {
      char data[1];
    };
    struct alignas(4) MediumData {
      int32_t data[2];
    };
    struct alignas(8) LargeData {
      double data[4];
    };

    auto& small = pool.Emplace<SmallData>();
    size_t small_pos = pool.latest_pos();
    std::cout << "Small data position: " << small_pos << std::endl;

    auto& medium = pool.Emplace<MediumData>();
    size_t medium_pos = pool.latest_pos();
    std::cout << "Medium data position: " << medium_pos << std::endl;

    auto& large = pool.Emplace<LargeData>();
    size_t large_pos = pool.latest_pos();
    std::cout << "Large data position: " << large_pos << std::endl;

    std::cout << "Alignment check:" << std::endl;
    std::cout << "Small data aligned: "
              << (reinterpret_cast<uintptr_t>(&small) % alignof(SmallData) == 0)
              << std::endl;
    std::cout << "Medium data aligned: "
              << (reinterpret_cast<uintptr_t>(&medium) % alignof(MediumData) ==
                  0)
              << std::endl;
    std::cout << "Large data aligned: "
              << (reinterpret_cast<uintptr_t>(&large) % alignof(LargeData) == 0)
              << std::endl;
  }

  // Test with static RingPool
  {
    std::cout << "\nTesting Static RingPool:" << std::endl;
    static_impl::RingPool<1024> pool;
    std::cout << "Pool alignment: " << pool.kAlignment << " bytes" << std::endl;

    struct alignas(1) SmallData {
      char data[1];
    };
    struct alignas(4) MediumData {
      int32_t data[2];
    };
    struct alignas(8) LargeData {
      double data[4];
    };

    auto& small = pool.Emplace<SmallData>();
    size_t small_pos = pool.latest_pos();
    std::cout << "Small data position: " << small_pos << std::endl;

    auto& medium = pool.Emplace<MediumData>();
    size_t medium_pos = pool.latest_pos();
    std::cout << "Medium data position: " << medium_pos << std::endl;

    auto& large = pool.Emplace<LargeData>();
    size_t large_pos = pool.latest_pos();
    std::cout << "Large data position: " << large_pos << std::endl;

    std::cout << "Alignment check:" << std::endl;
    std::cout << "Small data aligned: "
              << (reinterpret_cast<uintptr_t>(&small) % alignof(SmallData) == 0)
              << std::endl;
    std::cout << "Medium data aligned: "
              << (reinterpret_cast<uintptr_t>(&medium) % alignof(MediumData) ==
                  0)
              << std::endl;
    std::cout << "Large data aligned: "
              << (reinterpret_cast<uintptr_t>(&large) % alignof(LargeData) == 0)
              << std::endl;
  }
}

int main() {
  std::cout << "RingPool Demo" << std::endl;
  std::cout << "============" << std::endl;

  try {
    // DynamicRingPoolDemo();
    // StaticRingPoolDemo();
    // PerformanceComparisonDemo();
    TestStaticRingPool();
    // AlignmentDemo();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}