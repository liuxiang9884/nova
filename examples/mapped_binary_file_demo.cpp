#include <array>
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
    nova::MappedBinaryFile file(
        "test.bin", nova::MappedBinaryFile::OpenMode::kReadWrite,
        1024,                                     // Pre-allocate 1024 bytes
        nova::MappedBinaryFile::MapMode::kLazy);  // Explicitly specify mapping
                                                  // mode

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

    // Using batch write
    int batch_array[3] = {10, 20, 30};
    file.BatchWrite(batch_array, 3);

    // Multiple parameter Write
    int a = 42;
    double b = 2.718;
    char c = 'X';
    file.Write(a, b, c);

    // Demonstrate using void* version of Write
    char raw_buffer[10] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'};
    file.Write(raw_buffer, 10);

    std::cout << "Write demonstration completed\n";
  } catch (const std::exception& e) {
    std::cerr << "Write error: " << e.what() << std::endl;
  }
}

void read_demo() {
  try {
    // Open file in read-only mode with preload
    nova::MappedBinaryFile file(
        "test.bin", nova::MappedBinaryFile::OpenMode::kReadOnly,
        0,                                           // No need to resize
        nova::MappedBinaryFile::MapMode::kPreload);  // Preload all data

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

    // Read batch data
    int batch_array[3];
    file.BatchRead(batch_array, 3);
    std::cout << "Read batch array: ";
    for (int i = 0; i < 3; i++) {
      std::cout << batch_array[i] << " ";
    }
    std::cout << std::endl;

    // Using multiple parameter Read
    int a;
    double b;
    char c;
    file.Read(a, b, c);
    std::cout << "Multiple read: " << a << ", " << b << ", '" << c << "'"
              << std::endl;

    // Demonstrate using void* version of Read
    char raw_buffer[11] = {0};
    file.Read(raw_buffer, 10);
    std::cout << "Raw buffer read: " << raw_buffer << std::endl;

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

    // Demonstrate Eof method
    file.Seek(file.size() - 4);  // Move near the end of file
    std::cout << "Near end of file. Eof? " << (file.Eof() ? "Yes" : "No")
              << std::endl;

    int last_int;
    file.Read(last_int);
    std::cout << "Read last integer: " << last_int << std::endl;
    std::cout << "Now at end of file. Eof? " << (file.Eof() ? "Yes" : "No")
              << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "Random access error: " << e.what() << std::endl;
  }
}

void sequential_read_demo() {
  try {
    nova::MappedBinaryFile file("test.bin",
                                nova::MappedBinaryFile::OpenMode::kReadOnly);

    // Use Eof() for loop reading
    file.Seek(0);  // Return to the beginning of the file

    std::cout << "Sequential read using Eof():\n";
    int count = 0;

    // Read the first 10 integers (or until end of file)
    while (!file.Eof() && count < 10) {
      if (file.Tell() + sizeof(int) > file.size()) {
        break;  // Prevent reading incomplete int
      }

      int val = file.ReadAs<int>();
      std::cout << "Read #" << count << ": " << val << std::endl;
      count++;
    }

  } catch (const std::exception& e) {
    std::cerr << "Sequential read error: " << e.what() << std::endl;
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

  std::cout << "\n4. Sequential Read with Eof Demonstration\n";
  sequential_read_demo();

  return 0;
}