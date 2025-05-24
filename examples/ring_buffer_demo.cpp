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

int main() {
  try {
    TestBasicOperations();
    TestIndexAccess();
    TestOverwriting();
    TestPowerOfTwoCapacity();
    TestCopyAndMove();
    TestClear();
    TestWrapAround();
    TestEmplace();

    std::cout << "\n=== All tests completed successfully! ===" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}