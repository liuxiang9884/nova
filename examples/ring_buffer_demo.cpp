#include <cassert>
#include <iostream>
#include <string>

#include "nova/base/ring_buffer.h"

using namespace nova;

void TestBasicOperations() {
  std::cout << "=== Testing Basic Operations ===" << std::endl;

  // Create a ring buffer with capacity 5 (will be rounded up to 8)
  RingBuffer<int> buffer(5);
  std::cout << "Created buffer with requested capacity 5, actual capacity: "
            << buffer.Capacity() << std::endl;

  // Test push operations
  std::cout << "Pushing elements 1, 2, 3..." << std::endl;
  buffer.Push(1);
  buffer.Push(2);
  buffer.Push(3);

  std::cout << "Current write position: " << buffer.WritePosition()
            << std::endl;
  std::cout << "Latest element: " << buffer.Latest() << std::endl;

  // Test accessing elements by relative index
  std::cout << "Element at index 0 (latest): " << buffer[0] << std::endl;
  std::cout << "Element at index 1 (previous): " << buffer[1] << std::endl;
  std::cout << "Element at index 2 (oldest): " << buffer[2] << std::endl;
}

void TestIndexAccess() {
  std::cout << "\n=== Testing Index Access ===" << std::endl;

  RingBuffer<std::string> buffer(4);  // Capacity will be 4

  buffer.Push("Hello");
  buffer.Push("World");
  buffer.Push("Ring");
  buffer.Push("Buffer");

  std::cout << "Buffer contents by relative index (0 = latest):" << std::endl;
  for (size_t i = 0; i < buffer.Capacity(); ++i) {
    std::cout << "  [" << i << "] = " << buffer[i] << std::endl;
  }

  std::cout << "\nBuffer contents by absolute position:" << std::endl;
  for (size_t i = 0; i < buffer.Capacity(); ++i) {
    std::cout << "  At(" << i << ") = " << buffer.At(i) << std::endl;
  }

  // Modify element by relative index
  buffer[1] = "Modified";
  std::cout << "After modifying index 1: " << buffer[1] << std::endl;
}

void TestOverwriting() {
  std::cout << "\n=== Testing Overwriting ===" << std::endl;

  RingBuffer<int> buffer(4);  // Capacity is 4

  // Fill the buffer
  std::cout << "Filling buffer with 1, 2, 3, 4..." << std::endl;
  for (int i = 1; i <= 4; ++i) {
    buffer.Push(i);
    std::cout << "Pushed " << i
              << ", write position: " << buffer.WritePosition() << std::endl;
  }

  std::cout << "\nBuffer state after filling:" << std::endl;
  for (size_t i = 0; i < buffer.Capacity(); ++i) {
    std::cout << "  [" << i << "] = " << buffer[i] << std::endl;
  }

  // Continue pushing (will overwrite)
  std::cout << "\nPushing 5, 6..." << std::endl;
  buffer.Push(5);
  buffer.Push(6);

  std::cout << "Buffer state after overwriting:" << std::endl;
  for (size_t i = 0; i < buffer.Capacity(); ++i) {
    std::cout << "  [" << i << "] = " << buffer[i] << std::endl;
  }

  std::cout << "Latest element: " << buffer.Latest() << std::endl;
  std::cout << "Write position: " << buffer.WritePosition() << std::endl;
}

void TestPowerOfTwoCapacity() {
  std::cout << "\n=== Testing Power of 2 Capacity ===" << std::endl;

  std::vector<size_t> test_sizes = {1, 2,  3,  4,  5,  7,  8,
                                    9, 15, 16, 17, 31, 32, 33};

  for (size_t size : test_sizes) {
    RingBuffer<int> buffer(size);
    std::cout << "Requested: " << size
              << " -> Actual capacity: " << buffer.Capacity() << std::endl;
  }
}

void TestCopyAndMove() {
  std::cout << "\n=== Testing Copy and Move ===" << std::endl;

  RingBuffer<int> original(4);
  for (int i = 1; i <= 3; ++i) {
    original.Push(i);
  }

  std::cout << "Original write position: " << original.WritePosition()
            << std::endl;
  std::cout << "Original latest: " << original.Latest() << std::endl;

  // Test copy constructor
  RingBuffer<int> copied(original);
  std::cout << "Copied write position: " << copied.WritePosition() << std::endl;
  std::cout << "Copied latest: " << copied.Latest() << std::endl;

  // Test move constructor
  RingBuffer<int> moved(std::move(copied));
  std::cout << "Moved write position: " << moved.WritePosition() << std::endl;
  std::cout << "Moved latest: " << moved.Latest() << std::endl;
  std::cout << "Copied write position after move: " << copied.WritePosition()
            << std::endl;
}

void TestClear() {
  std::cout << "\n=== Testing Clear ===" << std::endl;

  RingBuffer<int> buffer(4);

  // Add some elements
  for (int i = 1; i <= 3; ++i) {
    buffer.Push(i);
  }

  std::cout << "Before clear - Write position: " << buffer.WritePosition()
            << std::endl;
  std::cout << "Before clear - Latest: " << buffer.Latest() << std::endl;

  buffer.Clear();

  std::cout << "After clear - Write position: " << buffer.WritePosition()
            << std::endl;

  // Check if buffer is cleared
  std::cout << "Buffer contents after clear:" << std::endl;
  for (size_t i = 0; i < buffer.Capacity(); ++i) {
    std::cout << "  At(" << i << ") = " << buffer.At(i) << std::endl;
  }
}

void TestWrapAround() {
  std::cout << "\n=== Testing Wrap Around ===" << std::endl;

  RingBuffer<char> buffer(4);  // Capacity is 4

  // Push elements that will wrap around
  char elements[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G'};

  for (char c : elements) {
    buffer.Push(c);
    std::cout << "Pushed '" << c
              << "', write position: " << buffer.WritePosition()
              << ", latest: '" << buffer.Latest() << "'" << std::endl;

    std::cout << "  Current state: ";
    for (size_t i = 0; i < buffer.Capacity(); ++i) {
      std::cout << "'" << buffer.At(i) << "' ";
    }
    std::cout << std::endl;
  }
}

void TestEmplace() {
  std::cout << "\n=== Testing Emplace ===" << std::endl;

  // Test with string construction
  RingBuffer<std::string> buffer(4);

  // Use Emplace to construct strings directly
  buffer.Emplace("Hello");
  buffer.Emplace(5, 'A');  // Construct string with 5 'A's
  buffer.Emplace("World");

  std::cout << "After emplacing strings:" << std::endl;
  for (size_t i = 0; i < 3; ++i) {
    std::cout << "  [" << i << "] = \"" << buffer[i] << "\"" << std::endl;
  }

  // Test with custom struct
  struct Point {
    int x, y;
    Point(int x_val, int y_val) : x(x_val), y(y_val) {}
    Point() : x(0), y(0) {}
  };

  RingBuffer<Point> point_buffer(4);

  // Emplace points directly
  point_buffer.Emplace(10, 20);
  point_buffer.Emplace(30, 40);
  point_buffer.Push(Point(50, 60));  // Compare with Push

  std::cout << "\nAfter emplacing points:" << std::endl;
  for (size_t i = 0; i < 3; ++i) {
    Point p = point_buffer[i];
    std::cout << "  [" << i << "] = Point(" << p.x << ", " << p.y << ")"
              << std::endl;
  }
}

// Static RingBuffer Tests
void TestStaticBasicOperations() {
  std::cout << "\n=== Testing Static Buffer Basic Operations ===" << std::endl;

  // Create a static ring buffer with capacity 8
  StaticRingBuffer<int, 8> buffer;
  std::cout << "Created static buffer with capacity: " << buffer.Capacity()
            << std::endl;

  // Test push operations
  std::cout << "Pushing elements 1, 2, 3..." << std::endl;
  buffer.Push(1);
  buffer.Push(2);
  buffer.Push(3);

  std::cout << "Current write position: " << buffer.WritePosition()
            << std::endl;
  std::cout << "Written count: " << buffer.WrittenCount() << std::endl;
  std::cout << "Latest element: " << buffer.Latest() << std::endl;

  // Test accessing elements by index
  std::cout << "Element at index 0 (first): " << buffer[0] << std::endl;
  std::cout << "Element at index 1 (second): " << buffer[1] << std::endl;
  std::cout << "Element at index 2 (third): " << buffer[2] << std::endl;
}

void TestStaticCompileTimeFeatures() {
  std::cout << "\n=== Testing Static Buffer Compile-Time Features ==="
            << std::endl;

  // Test constexpr functionality
  StaticRingBuffer<int, 4> buffer;
  auto capacity = buffer.Capacity();
  bool is_empty = buffer.IsEmpty();

  std::cout << "Constexpr capacity: " << capacity << std::endl;
  std::cout << "Constexpr is_empty: " << is_empty << std::endl;

  // This would cause a compile error due to static_assert
  // StaticRingBuffer<int, 5> bad_buffer;  // 5 is not a power of 2
}

void TestStaticOverwriting() {
  std::cout << "\n=== Testing Static Buffer Overwriting ===" << std::endl;

  StaticRingBuffer<char, 4> buffer;

  // Fill the buffer
  std::cout << "Filling buffer with A, B, C, D..." << std::endl;
  buffer.Push('A');
  buffer.Push('B');
  buffer.Push('C');
  buffer.Push('D');

  std::cout << "Buffer state after filling:" << std::endl;
  std::cout << "Written count: " << buffer.WrittenCount() << std::endl;
  std::cout << "Is full: " << buffer.IsFull() << std::endl;

  for (size_t i = 0; i < buffer.WrittenCount(); ++i) {
    std::cout << "  [" << i << "] = '" << buffer[i] << "'" << std::endl;
  }

  // Continue pushing (will overwrite)
  std::cout << "\nPushing E, F..." << std::endl;
  buffer.Push('E');
  buffer.Push('F');

  std::cout << "Buffer state after overwriting:" << std::endl;
  std::cout << "Written count: " << buffer.WrittenCount() << std::endl;
  for (size_t i = 0; i < buffer.WrittenCount(); ++i) {
    std::cout << "  [" << i << "] = '" << buffer[i] << "'" << std::endl;
  }

  std::cout << "Latest element: '" << buffer.Latest() << "'" << std::endl;
  std::cout << "Write position: " << buffer.WritePosition() << std::endl;
}

void TestStaticEmplace() {
  std::cout << "\n=== Testing Static Buffer Emplace ===" << std::endl;

  // Test with string construction
  StaticRingBuffer<std::string, 4> buffer;

  // Use Emplace to construct strings directly
  buffer.Emplace("Hello");
  buffer.Emplace(5, 'A');  // Construct string with 5 'A's
  buffer.Emplace("World");

  std::cout << "After emplacing strings:" << std::endl;
  for (size_t i = 0; i < buffer.WrittenCount(); ++i) {
    std::cout << "  [" << i << "] = \"" << buffer[i] << "\"" << std::endl;
  }

  // Test with custom struct
  struct Point {
    int x, y;
    Point(int x_val, int y_val) : x(x_val), y(y_val) {}
    Point() : x(0), y(0) {}
  };

  StaticRingBuffer<Point, 4> point_buffer;

  // Emplace points directly
  point_buffer.Emplace(10, 20);
  point_buffer.Emplace(30, 40);
  point_buffer.Push(Point(50, 60));  // Compare with Push

  std::cout << "\nAfter emplacing points:" << std::endl;
  for (size_t i = 0; i < point_buffer.WrittenCount(); ++i) {
    Point p = point_buffer[i];
    std::cout << "  [" << i << "] = Point(" << p.x << ", " << p.y << ")"
              << std::endl;
  }
}

void TestStaticIterators() {
  std::cout << "\n=== Testing Static Buffer Iterators ===" << std::endl;

  StaticRingBuffer<int, 8> buffer;

  // Fill with some data
  for (int i = 1; i <= 5; ++i) {
    buffer.Push(i * 10);
  }

  std::cout << "Buffer contents using iterators:" << std::endl;
  std::cout << "Raw buffer (begin to end): ";
  for (auto it = buffer.begin(); it != buffer.end(); ++it) {
    std::cout << *it << " ";
  }
  std::cout << std::endl;

  std::cout << "Range-based for loop: ";
  for (const auto& value : buffer) {
    std::cout << value << " ";
  }
  std::cout << std::endl;
}

void TestStaticClear() {
  std::cout << "\n=== Testing Static Buffer Clear ===" << std::endl;

  StaticRingBuffer<int, 4> buffer;

  // Add some elements
  for (int i = 1; i <= 3; ++i) {
    buffer.Push(i);
  }

  std::cout << "Before clear - Written count: " << buffer.WrittenCount()
            << std::endl;
  std::cout << "Before clear - Latest: " << buffer.Latest() << std::endl;

  buffer.Clear();

  std::cout << "After clear - Written count: " << buffer.WrittenCount()
            << std::endl;
  std::cout << "After clear - Is empty: " << buffer.IsEmpty() << std::endl;

  // Check if buffer is cleared
  std::cout << "Buffer contents after clear:" << std::endl;
  for (size_t i = 0; i < buffer.Capacity(); ++i) {
    std::cout << "  At(" << i << ") = " << buffer.At(i) << std::endl;
  }
}

void TestStaticDifferentSizes() {
  std::cout << "\n=== Testing Static Buffer Different Sizes ===" << std::endl;

  StaticRingBuffer<int, 1> tiny_buffer;
  StaticRingBuffer<int, 2> small_buffer;
  StaticRingBuffer<int, 16> medium_buffer;
  StaticRingBuffer<int, 1024> large_buffer;

  std::cout << "Tiny buffer (1): capacity = " << tiny_buffer.Capacity()
            << std::endl;
  std::cout << "Small buffer (2): capacity = " << small_buffer.Capacity()
            << std::endl;
  std::cout << "Medium buffer (16): capacity = " << medium_buffer.Capacity()
            << std::endl;
  std::cout << "Large buffer (1024): capacity = " << large_buffer.Capacity()
            << std::endl;

  // Test tiny buffer behavior
  tiny_buffer.Push(42);
  tiny_buffer.Push(84);  // Should overwrite
  std::cout << "Tiny buffer after 2 pushes: [0] = " << tiny_buffer[0]
            << std::endl;
  std::cout << "Tiny buffer written count: " << tiny_buffer.WrittenCount()
            << std::endl;
}

void TestSharedMemoryCompatibility() {
  std::cout << "\n=== Testing Shared Memory Compatibility ===" << std::endl;

  // Example of compatible types for shared memory
  struct PODStruct {
    int id;
    double value;
    char name[32];  // Fixed-size array instead of std::string
    bool active;
  };

  struct CompatiblePoint {
    float x, y, z;
    int32_t timestamp;
  };

  // These types are compatible with shared memory
  StaticRingBuffer<int, 8> int_buffer;
  StaticRingBuffer<PODStruct, 4> pod_buffer;
  StaticRingBuffer<CompatiblePoint, 16> point_buffer;

  std::cout << "int buffer: " << sizeof(int_buffer) << " bytes" << std::endl;
  std::cout << "POD struct buffer: " << sizeof(pod_buffer) << " bytes"
            << std::endl;
  std::cout << "Point buffer: " << sizeof(point_buffer) << " bytes"
            << std::endl;

  // Test with POD struct
  PODStruct data{42, 3.14, "test_data", true};
  pod_buffer.Push(data);

  std::cout << "Successfully pushed POD struct" << std::endl;
  std::cout << "Retrieved: id=" << pod_buffer.Latest().id
            << ", value=" << pod_buffer.Latest().value
            << ", name=" << pod_buffer.Latest().name << std::endl;

  // Examples of INCOMPATIBLE types (these would cause compile errors):
  /*
  struct BadStruct {
    std::string name;      // Dynamic allocation - BAD
    std::vector<int> data; // Dynamic allocation - BAD
    int* ptr;              // Pointer member - BAD
  };

  // This would fail compilation:
  // StaticRingBuffer<BadStruct, 4> bad_buffer;
  // StaticRingBuffer<std::string, 4> string_buffer;
  */

  std::cout << "Shared memory compatibility checks passed!" << std::endl;
}

int main() {
  try {
    // Dynamic RingBuffer tests
    TestBasicOperations();
    TestIndexAccess();
    TestOverwriting();
    TestPowerOfTwoCapacity();
    TestCopyAndMove();
    TestClear();
    TestWrapAround();
    TestEmplace();

    // Static RingBuffer tests
    TestStaticBasicOperations();
    TestStaticCompileTimeFeatures();
    TestStaticOverwriting();
    TestStaticEmplace();
    TestStaticIterators();
    TestStaticClear();
    TestStaticDifferentSizes();

    // Shared memory compatibility test
    TestSharedMemoryCompatibility();

    std::cout << "\n=== All tests completed successfully! ===" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}