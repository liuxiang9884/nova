#include <iostream>
#include <string>
#include <vector>

#include "nova/utils/mapped_binary_file.h"

// A simple struct for demonstration
struct Record {
  int id;
  double value;
  char name[32];
};

void write_demo() {
  try {
    // Create a new file for writing
    nova::MappedBinaryFile file("test.bin",
                                nova::MappedBinaryFile::OpenMode::kReadWrite,
                                1024);  // Pre-allocate 1024 bytes

    // Write basic types
    int number = 42;
    file.Write(number);

    double pi = 3.14159;
    file.Write(pi);

    // Write string
    std::string text = "Hello, Memory Mapping!";
    uint32_t len = text.length();
    file.Write(len);
    file.Write(text.c_str(), len);

    // Write struct
    Record record{1, 99.9, "Test Record"};
    file.Write(record);

    // Write array
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    file.WriteArray(numbers);

    std::cout << "Write demonstration completed\n";
  } catch (const std::exception& e) {
    std::cerr << "Write error: " << e.what() << std::endl;
  }
}

void read_demo() {
  try {
    // Open file in read-only mode
    nova::MappedBinaryFile file("test.bin",
                                nova::MappedBinaryFile::OpenMode::kReadOnly);

    // Read basic types
    int number = file.ReadAs<int>();
    std::cout << "Read integer: " << number << std::endl;

    double pi = file.ReadAs<double>();
    std::cout << "Read double: " << pi << std::endl;

    // Read string
    uint32_t len = file.ReadAs<uint32_t>();
    std::vector<char> buffer(len + 1, '\0');
    file.Read(buffer.data(), len);
    std::cout << "Read string: " << buffer.data() << std::endl;

    // Read struct
    Record record;
    file.Read(record);
    std::cout << "Read record: id=" << record.id << ", value=" << record.value
              << ", name=" << record.name << std::endl;

    // Read array
    std::vector<int> numbers(5);
    file.BatchRead(numbers.data(), 5);
    std::cout << "Read array: ";
    for (int n : numbers) {
      std::cout << n << " ";
    }
    std::cout << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "Read error: " << e.what() << std::endl;
  }
}

void random_access_demo() {
  try {
    nova::MappedBinaryFile file("test.bin",
                                nova::MappedBinaryFile::OpenMode::kReadOnly);

    // Jump to the second integer position (skip first int)
    file.Seek(sizeof(int));

    // Read double
    double value = file.ReadAs<double>();
    std::cout << "Random access read double: " << value << std::endl;

    // Get current position
    std::size_t pos = file.Tell();
    std::cout << "Current file position: " << pos << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "Random access error: " << e.what() << std::endl;
  }
}

int main() {
  std::cout << "=== MappedBinaryFile Demo Program ===\n\n";

  std::cout << "1. Write Demonstration\n";
  write_demo();

  std::cout << "\n2. Read Demonstration\n";
  read_demo();

  std::cout << "\n3. Random Access Demonstration\n";
  random_access_demo();

  return 0;
}