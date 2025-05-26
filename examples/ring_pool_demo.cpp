#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
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

// Different types for mixed type demo
struct Point2D {
  float x, y;

  void Print() const {
    std::cout << "Point2D{x=" << x << ", y=" << y << "}" << std::endl;
  }
};

struct Rectangle {
  Point2D top_left;
  Point2D bottom_right;
  uint32_t color;

  void Print() const {
    std::cout << "Rectangle{top_left=(" << top_left.x << "," << top_left.y
              << "), bottom_right=(" << bottom_right.x << "," << bottom_right.y
              << "), color=0x" << std::hex << color << std::dec << "}"
              << std::endl;
  }
};

struct Circle {
  Point2D center;
  float radius;
  uint32_t color;

  void Print() const {
    std::cout << "Circle{center=(" << center.x << "," << center.y
              << "), radius=" << radius << ", color=0x" << std::hex << color
              << std::dec << "}" << std::endl;
  }
};

struct TextLabel {
  Point2D position;
  char text[64];
  uint16_t font_size;
  uint32_t color;

  void Print() const {
    std::cout << "TextLabel{position=(" << position.x << "," << position.y
              << "), text=\"" << text << "\", font_size=" << font_size
              << ", color=0x" << std::hex << color << std::dec << "}"
              << std::endl;
  }
};

// Enum to track object types
enum class ObjectType : uint8_t {
  POINT = 1,
  RECTANGLE = 2,
  CIRCLE = 3,
  TEXT_LABEL = 4
};

// Header structure to identify object types in the pool
struct ObjectHeader {
  ObjectType type;
  uint32_t size;
  uint64_t timestamp;
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

  constexpr size_t kPoolSize = 128 * 1024;  // 1MB
  constexpr size_t kIterations = 10000;     // 10K operations

  std::cout << "cmp size: " << kPoolSize << ", "
            << kIterations * sizeof(PerformanceData) << std::endl;

  // Test dynamic RingPool
  {
    std::cout << "\nTesting Dynamic RingPool:" << std::endl;
    RingPool pool(kPoolSize);
    std::vector<PerformanceData> results;
    results.reserve(kIterations);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < kIterations; ++i) {
      auto& data = pool.Emplace<PerformanceData>();
      data.timestamp =
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch())
              .count();
      data.value = static_cast<double>(i);
      std::snprintf(data.description, sizeof(data.description),
                    "Performance test data %zu", i);
      results.push_back(data);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double total_time = duration.count() / 1000000.0;
    double ops_per_second = kIterations / total_time;
    double avg_latency = (total_time * 1000000.0) / kIterations;

    std::cout << "Total time: " << std::fixed << std::setprecision(3)
              << total_time << " seconds" << std::endl;
    std::cout << "Operations per second: " << std::fixed << std::setprecision(0)
              << ops_per_second << " ops/s" << std::endl;
    std::cout << "Average latency: " << std::fixed << std::setprecision(3)
              << avg_latency << " us" << std::endl;

    auto sum = 0.;
    for (auto data : results) {
      sum += data.value;
    }
    std::cout << "Sum: " << sum << std::endl;
  }

  // Test static RingPool

  std::cout << "\nTesting Static RingPool:" << std::endl;
  static_impl::RingPool<kPoolSize> pool;
  std::vector<PerformanceData> results;
  results.reserve(kIterations);

  auto start = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < kIterations; ++i) {
    auto& data = pool.Emplace<PerformanceData>();
    data.timestamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    data.value = static_cast<double>(i);
    std::snprintf(data.description, sizeof(data.description),
                  "Performance test data %zu", i);
    results.push_back(data);
    std::cout << "Iteration " << i << ": write_count=" << pool.write_count()
              << ", latest_pos=" << pool.latest_pos()
              << ", write_pos=" << pool.write_pos() << std::endl;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  double total_time = duration.count() / 1000000.0;
  double ops_per_second = kIterations / total_time;
  double avg_latency = (total_time * 1000000.0) / kIterations;

  std::cout << "Total time: " << std::fixed << std::setprecision(3)
            << total_time << " seconds" << std::endl;
  std::cout << "Operations per second: " << std::fixed << std::setprecision(0)
            << ops_per_second << " ops/s" << std::endl;
  std::cout << "Average latency: " << std::fixed << std::setprecision(3)
            << avg_latency << " us" << std::endl;

  auto sum = 0.;
  for (auto data : results) {
    sum += data.value;
  }
  std::cout << "Sum: " << sum << std::endl;
}

// Simple test with fewer iterations
void SimpleStaticRingPoolTest() {
  std::cout << "\n=== Simple Static RingPool Test ===" << std::endl;

  constexpr size_t kPoolSize = 1024;  // 1KB
  constexpr size_t kIterations = 10;  // Only 10 operations

  static_impl::RingPool<kPoolSize> pool;

  for (size_t i = 0; i < kIterations; ++i) {
    auto& data = pool.Emplace<PerformanceData>();
    data.timestamp = i;
    data.value = static_cast<double>(i);
    std::cout << "Iteration " << i << ": write_count=" << pool.write_count()
              << ", latest_pos=" << pool.latest_pos()
              << ", write_pos=" << pool.write_pos() << std::endl;
  }

  std::cout << "Simple test completed successfully" << std::endl;
}

// Progressive test to find the issue
void ProgressiveStaticRingPoolTest() {
  std::cout << "\n=== Progressive Static RingPool Test ===" << std::endl;

  constexpr size_t kPoolSize = 128 * 1024;  // 128KB
  static_impl::RingPool<kPoolSize> pool;

  // Test different iteration counts
  {
    std::vector<size_t> test_counts = {100,  500,   1000,  2000,
                                       5000, 10000, 15000, 20000};

    for (auto count : test_counts) {
      std::cout << "Testing with " << count << " iterations..." << std::endl;

      pool.Reset();  // Reset the pool for each test

      for (size_t i = 0; i < count; ++i) {
        auto& data = pool.Emplace<PerformanceData>();  // Back to using Emplace
        data.timestamp = i;
        data.value = static_cast<double>(i);
        // Don't initialize description to avoid potential issues

        // Only print every 1000th iteration to reduce output
        if (i % 1000 == 0 || i == count - 1) {
          std::cout << "  " << pool.write_count() << ", " << pool.latest_pos()
                    << ", " << pool.write_pos() << std::endl;
        }
      }

      std::cout << "  Final state: write_count=" << pool.write_count()
                << ", latest_pos=" << pool.latest_pos()
                << ", write_pos=" << pool.write_pos() << std::endl;
      std::cout << "  Test with " << count
                << " iterations completed successfully" << std::endl;
    }

    std::cout << "Vector going out of scope..." << std::endl;
  }

  std::cout << "All progressive tests completed" << std::endl;
  std::cout << "Pool going out of scope..." << std::endl;
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
  std::cout << "Alignment: " << alignof(PerformanceData) << std::endl;
  std::cout << "Is POD: " << std::is_pod_v<PerformanceData> << std::endl;
  std::cout << "Is standard layout: "
            << std::is_standard_layout_v<PerformanceData> << std::endl;
  std::cout << "Is trivial: "
            << std::is_trivial_v<PerformanceData> << std::endl;

  std::cout << "haha" << std::endl;
}

// Mixed types demo - demonstrates storing different types in the same pool
void MixedTypesDemo() {
  std::cout << "\n=== Mixed Types Demo ===" << std::endl;
  std::cout
      << "Demonstrating construction of different types in the same RingPool"
      << std::endl;

  // Create a 2KB pool to store various graphics objects
  RingPool pool(2048);
  std::cout << "Pool capacity: " << pool.capacity() << " bytes" << std::endl;

  // Store positions of different objects for later retrieval
  std::vector<std::pair<size_t, ObjectType>> object_positions;

  std::cout << "\n--- Creating different types of objects ---" << std::endl;

  // 1. Create some points
  auto& point1 = pool.Emplace<Point2D>();
  point1.x = 10.5f;
  point1.y = 20.3f;
  object_positions.emplace_back(pool.latest_pos(), ObjectType::POINT);
  std::cout << "Created Point2D at position " << pool.latest_pos() << ": ";
  point1.Print();

  auto& point2 = pool.Emplace<Point2D>();
  point2.x = 100.0f;
  point2.y = 200.0f;
  object_positions.emplace_back(pool.latest_pos(), ObjectType::POINT);
  std::cout << "Created Point2D at position " << pool.latest_pos() << ": ";
  point2.Print();

  // 2. Create a rectangle
  auto& rect = pool.Emplace<Rectangle>();
  rect.top_left = {50.0f, 50.0f};
  rect.bottom_right = {150.0f, 100.0f};
  rect.color = 0xFF0000;  // Red
  object_positions.emplace_back(pool.latest_pos(), ObjectType::RECTANGLE);
  std::cout << "Created Rectangle at position " << pool.latest_pos() << ": ";
  rect.Print();

  // 3. Create a circle
  auto& circle = pool.Emplace<Circle>();
  circle.center = {75.0f, 75.0f};
  circle.radius = 25.0f;
  circle.color = 0x00FF00;  // Green
  object_positions.emplace_back(pool.latest_pos(), ObjectType::CIRCLE);
  std::cout << "Created Circle at position " << pool.latest_pos() << ": ";
  circle.Print();

  // 4. Create a text label
  auto& label = pool.Emplace<TextLabel>();
  label.position = {200.0f, 300.0f};
  std::strcpy(label.text, "Hello, RingPool!");
  label.font_size = 16;
  label.color = 0x0000FF;  // Blue
  object_positions.emplace_back(pool.latest_pos(), ObjectType::TEXT_LABEL);
  std::cout << "Created TextLabel at position " << pool.latest_pos() << ": ";
  label.Print();

  // 5. Create more mixed objects
  auto& point3 = pool.Emplace<Point2D>();
  point3.x = 300.0f;
  point3.y = 400.0f;
  object_positions.emplace_back(pool.latest_pos(), ObjectType::POINT);
  std::cout << "Created Point2D at position " << pool.latest_pos() << ": ";
  point3.Print();

  auto& circle2 = pool.Emplace<Circle>();
  circle2.center = {250.0f, 350.0f};
  circle2.radius = 40.0f;
  circle2.color = 0xFFFF00;  // Yellow
  object_positions.emplace_back(pool.latest_pos(), ObjectType::CIRCLE);
  std::cout << "Created Circle at position " << pool.latest_pos() << ": ";
  circle2.Print();

  std::cout << "\n--- Pool Status Information ---" << std::endl;
  std::cout << "Current write position: " << pool.write_pos() << std::endl;
  std::cout << "Write count: " << pool.write_count() << std::endl;
  std::cout << "Used space: " << pool.write_pos() << " bytes" << std::endl;
  std::cout << "Remaining space: " << (pool.capacity() - pool.write_pos())
            << " bytes" << std::endl;

  std::cout << "\n--- Reading different types of objects from pool ---"
            << std::endl;
  for (size_t i = 0; i < object_positions.size(); ++i) {
    auto [pos, type] = object_positions[i];
    std::cout << "Position " << pos << " (type " << static_cast<int>(type)
              << "): ";

    switch (type) {
      case ObjectType::POINT: {
        auto& point = pool.Read<Point2D>(pos);
        point.Print();
        break;
      }
      case ObjectType::RECTANGLE: {
        auto& rectangle = pool.Read<Rectangle>(pos);
        rectangle.Print();
        break;
      }
      case ObjectType::CIRCLE: {
        auto& circle_obj = pool.Read<Circle>(pos);
        circle_obj.Print();
        break;
      }
      case ObjectType::TEXT_LABEL: {
        auto& text = pool.Read<TextLabel>(pos);
        text.Print();
        break;
      }
    }
  }

  std::cout << "\n--- Raw Memory Allocation Demo ---" << std::endl;

  // Demonstrate raw memory allocation for custom data
  struct CustomData {
    uint32_t magic_number;
    float values[4];
    char name[16];
  };

  auto* raw_ptr = pool.Allocate(sizeof(CustomData));
  auto* custom = new (raw_ptr) CustomData{.magic_number = 0xDEADBEEF,
                                          .values = {1.1f, 2.2f, 3.3f, 4.4f},
                                          .name = "Custom"};

  std::cout << "Allocated raw memory at position " << pool.latest_pos()
            << ", size: " << sizeof(CustomData) << " bytes" << std::endl;
  std::cout << "CustomData{magic=0x" << std::hex << custom->magic_number
            << std::dec << ", values=[" << custom->values[0] << ","
            << custom->values[1] << "," << custom->values[2] << ","
            << custom->values[3] << "], name=\"" << custom->name << "\"}"
            << std::endl;

  std::cout << "\n--- Final Pool Status ---" << std::endl;
  std::cout << "Final write position: " << pool.write_pos() << std::endl;
  std::cout << "Total write count: " << pool.write_count() << std::endl;
  std::cout << "Pool utilization: " << std::fixed << std::setprecision(1)
            << (static_cast<double>(pool.write_pos()) / pool.capacity() * 100.0)
            << "%" << std::endl;
}

// Static RingPool mixed types demo
void StaticMixedTypesDemo() {
  std::cout << "\n=== Static RingPool Mixed Types Demo ===" << std::endl;
  std::cout
      << "Demonstrating construction of different types in static RingPool"
      << std::endl;

  // Create a 2KB static pool
  static_impl::RingPool<2048> pool;
  std::cout << "Static pool capacity: " << pool.capacity() << " bytes"
            << std::endl;

  // Create a graphics scene with mixed objects
  std::cout << "\n--- Creating Graphics Scene ---" << std::endl;

  // Background rectangle
  auto& background = pool.Emplace<Rectangle>();
  background.top_left = {0.0f, 0.0f};
  background.bottom_right = {800.0f, 600.0f};
  background.color = 0x000000;  // Black background
  size_t bg_pos = pool.latest_pos();
  std::cout << "Background rectangle at position " << bg_pos << ": ";
  background.Print();

  // Title text
  auto& title = pool.Emplace<TextLabel>();
  title.position = {400.0f, 50.0f};
  std::strcpy(title.text, "RingPool Graphics Demo");
  title.font_size = 24;
  title.color = 0xFFFFFF;  // White text
  size_t title_pos = pool.latest_pos();
  std::cout << "Title text at position " << title_pos << ": ";
  title.Print();

  // Create some geometric shapes
  std::vector<size_t> shape_positions;

  for (int i = 0; i < 3; ++i) {
    // Circle
    auto& circle = pool.Emplace<Circle>();
    circle.center = {100.0f + i * 150.0f, 200.0f};
    circle.radius = 30.0f + i * 10.0f;
    circle.color = 0xFF0000 + (i * 0x003300);  // Red to yellow gradient
    shape_positions.push_back(pool.latest_pos());
    std::cout << "Circle " << (i + 1) << " at position " << pool.latest_pos()
              << ": ";
    circle.Print();

    // Rectangle
    auto& rect = pool.Emplace<Rectangle>();
    rect.top_left = {80.0f + i * 150.0f, 300.0f};
    rect.bottom_right = {120.0f + i * 150.0f, 350.0f};
    rect.color = 0x0000FF + (i * 0x330000);  // Blue to magenta gradient
    shape_positions.push_back(pool.latest_pos());
    std::cout << "Rectangle " << (i + 1) << " at position " << pool.latest_pos()
              << ": ";
    rect.Print();

    // Label for each shape
    auto& label = pool.Emplace<TextLabel>();
    label.position = {85.0f + i * 150.0f, 370.0f};
    std::snprintf(label.text, sizeof(label.text), "Shape %d", i + 1);
    label.font_size = 12;
    label.color = 0xFFFFFF;
    shape_positions.push_back(pool.latest_pos());
    std::cout << "Label " << (i + 1) << " at position " << pool.latest_pos()
              << ": ";
    label.Print();
  }

  std::cout << "\n--- Memory Layout Analysis ---" << std::endl;
  std::cout << "Object type sizes:" << std::endl;
  std::cout << "  Point2D: " << sizeof(Point2D) << " bytes" << std::endl;
  std::cout << "  Rectangle: " << sizeof(Rectangle) << " bytes" << std::endl;
  std::cout << "  Circle: " << sizeof(Circle) << " bytes" << std::endl;
  std::cout << "  TextLabel: " << sizeof(TextLabel) << " bytes" << std::endl;

  std::cout << "\nObject alignment requirements:" << std::endl;
  std::cout << "  Point2D: " << alignof(Point2D) << " bytes" << std::endl;
  std::cout << "  Rectangle: " << alignof(Rectangle) << " bytes" << std::endl;
  std::cout << "  Circle: " << alignof(Circle) << " bytes" << std::endl;
  std::cout << "  TextLabel: " << alignof(TextLabel) << " bytes" << std::endl;

  std::cout << "\nPool usage statistics:" << std::endl;
  std::cout << "  Total objects: " << pool.write_count() << std::endl;
  std::cout << "  Used memory: " << pool.write_pos() << " bytes" << std::endl;
  std::cout << "  Memory utilization: " << std::fixed << std::setprecision(1)
            << (static_cast<double>(pool.write_pos()) / pool.capacity() * 100.0)
            << "%" << std::endl;

  // Demonstrate reading back the objects
  std::cout << "\n--- Object Integrity Verification ---" << std::endl;
  auto& bg_read = pool.Read<Rectangle>(bg_pos);
  std::cout << "Re-read background: ";
  bg_read.Print();

  auto& title_read = pool.Read<TextLabel>(title_pos);
  std::cout << "Re-read title: ";
  title_read.Print();

  std::cout << "\nMixed types demo completed!" << std::endl;
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

// Test with heap allocation
void HeapAllocatedRingPoolTest() {
  std::cout << "\n=== Heap Allocated RingPool Test ===" << std::endl;

  constexpr size_t kPoolSize = 128 * 1024;  // 128KB
  auto pool = std::make_unique<static_impl::RingPool<kPoolSize>>();

  // Calculate safe number of iterations
  constexpr size_t kMaxObjects = kPoolSize / sizeof(PerformanceData);
  constexpr size_t kIterations = kMaxObjects * 2;  // Test wrapping behavior

  std::cout << "Pool size: " << kPoolSize << " bytes" << std::endl;
  std::cout << "Object size: " << sizeof(PerformanceData) << " bytes"
            << std::endl;
  std::cout << "Max objects: " << kMaxObjects << std::endl;
  std::cout << "Test iterations: " << kIterations << std::endl;

  for (size_t i = 0; i < kIterations; ++i) {
    auto& data = pool->Emplace<PerformanceData>();
    data.timestamp = i;
    data.value = static_cast<double>(i);

    // Only print every 100th iteration to reduce output
    if (i % 100 == 0 || i == kIterations - 1) {
      std::cout << "  " << pool->write_count() << ", " << pool->latest_pos()
                << ", " << pool->write_pos() << std::endl;
    }
  }

  std::cout << "Heap allocated test completed successfully" << std::endl;
  std::cout << "Pool going out of scope..." << std::endl;
}

int main() {
  std::cout << "RingPool Demo" << std::endl;
  std::cout << "============" << std::endl;

  try {
    // Basic demos
    DynamicRingPoolDemo();
    StaticRingPoolDemo();

    // Mixed types demos - Demonstrate constructing different types in the same
    // pool
    MixedTypesDemo();
    StaticMixedTypesDemo();

    // Performance and advanced demos
    // PerformanceComparisonDemo();
    // SimpleStaticRingPoolTest();
    // std::cout << "Simple test function completed" << std::endl;

    // ProgressiveStaticRingPoolTest();
    // std::cout << "Progressive test function completed" << std::endl;
    //
    // HeapAllocatedRingPoolTest();
    // std::cout << "Heap allocated test function completed" << std::endl;

    // TestStaticRingPool();
    AlignmentDemo();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "Main function ending..." << std::endl;
  return 0;
}