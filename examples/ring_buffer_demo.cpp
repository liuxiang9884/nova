#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

#include "nova/base/ring_buffer.h"

using namespace nova;

// Types that satisfy shared memory constraints for testing
struct SharedMemoryPoint {
  float x, y, z;
  int32_t timestamp;
};

struct SharedMemoryData {
  int id;
  double value;
  char name[32];  // Use fixed-size array instead of std::string
  bool active;
};

// Fixed-size string type for testing
struct FixedString {
  char data[32];

  FixedString() {
    data[0] = '\0';
  }

  FixedString(const char* str) {
    strncpy(data, str, sizeof(data) - 1);
    data[sizeof(data) - 1] = '\0';
  }

  const char* c_str() const {
    return data;
  }
};

// Test basic operations
void TestBasicOperations() {
  std::cout << "\n=== Testing Basic Operations ===" << std::endl;
  RingBuffer<int> buffer(4);  // Actual capacity is 4

  // Test writing
  buffer.Push(1);
  buffer.Push(2);
  buffer.Push(3);
  buffer.Push(4);
  buffer.Push(5);  // Overwrite first element

  // Test access
  std::cout << "Buffer contents: ";
  for (size_t i = 0; i < buffer.write_count(); ++i) {
    std::cout << buffer[i] << " ";
  }
  std::cout << std::endl;

  // Test latest element
  std::cout << "Latest element: " << buffer.latest() << std::endl;
  std::cout << "Write position: " << buffer.write_position() << std::endl;
  std::cout << "Write count: " << buffer.write_count() << std::endl;
  std::cout << "Capacity: " << buffer.capacity() << std::endl;
}

// Test move semantics
void TestMoveSemantics() {
  std::cout << "\n=== Testing Move Semantics ===" << std::endl;
  RingBuffer<std::string> buffer1(4);
  buffer1.Push("Hello");
  buffer1.Push("World");

  // Move construction
  RingBuffer<std::string> buffer2(std::move(buffer1));
  std::cout << "After move construction:" << std::endl;
  std::cout << "Buffer2 contents: ";
  for (size_t i = 0; i < buffer2.write_count(); ++i) {
    std::cout << buffer2[i] << " ";
  }
  std::cout << std::endl;

  // Move assignment
  RingBuffer<std::string> buffer3(4);
  buffer3 = std::move(buffer2);
  std::cout << "After move assignment:" << std::endl;
  std::cout << "Buffer3 contents: ";
  for (size_t i = 0; i < buffer3.write_count(); ++i) {
    std::cout << buffer3[i] << " ";
  }
  std::cout << std::endl;
}

// Test emplace
void TestEmplace() {
  std::cout << "\n=== Testing Emplace ===" << std::endl;
  RingBuffer<std::string> buffer(4);

  // Use emplace for direct construction
  buffer.Emplace("Hello");
  buffer.Emplace(3, 'x');  // Construct "xxx"
  buffer.Emplace("World");

  std::cout << "Buffer contents: ";
  for (size_t i = 0; i < buffer.write_count(); ++i) {
    std::cout << buffer[i] << " ";
  }
  std::cout << std::endl;
}

// Test StaticRingBuffer
void TestStaticRingBuffer() {
  std::cout << "\n=== Testing StaticRingBuffer ===" << std::endl;
  StaticRingBuffer<int, 4> buffer;  // Fixed capacity of 4

  // Test writing
  buffer.Push(1);
  buffer.Push(2);
  buffer.Push(3);
  buffer.Push(4);
  buffer.Push(5);  // Overwrite first element

  // Test access
  std::cout << "Buffer contents: ";
  for (size_t i = 0; i < buffer.write_count(); ++i) {
    std::cout << buffer[i] << " ";
  }
  std::cout << std::endl;

  // Test latest element
  std::cout << "Latest element: " << buffer.latest() << std::endl;
  std::cout << "Write position: " << buffer.write_position() << std::endl;
  std::cout << "Write count: " << buffer.write_count() << std::endl;
  std::cout << "Capacity: " << buffer.capacity() << std::endl;
  std::cout << "Is empty: " << (buffer.IsEmpty() ? "true" : "false")
            << std::endl;
  std::cout << "Is full: " << (buffer.IsFull() ? "true" : "false") << std::endl;
}

int main() {
  TestBasicOperations();
  TestMoveSemantics();
  TestEmplace();
  TestStaticRingBuffer();
  return 0;
}