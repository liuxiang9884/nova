#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "nova/utils/binary_file.h"

// Simple struct for demo
struct Point {
  float x;
  float y;
  float z;
};

// Print helpers
void PrintPoint(const Point& p) {
  std::cout << "Point: (" << p.x << ", " << p.y << ", " << p.z << ")"
            << std::endl;
}

int main() {
  std::cout << "BinaryFile Demo" << std::endl;
  std::cout << "==============" << std::endl << std::endl;

  try {
    // Basic file operations
    {
      std::cout << "1. Basic file operations:" << std::endl;

      // Create a binary file
      nova::BinaryFile file("test_data.bin", std::ios::binary | std::ios::in |
                                                 std::ios::out |
                                                 std::ios::trunc);

      std::cout << "  - File created successfully" << std::endl;
      std::cout << "  - IsOpen(): " << (file.IsOpen() ? "true" : "false")
                << std::endl;

      // Close the file
      file.Close();
      std::cout << "  - File closed" << std::endl;
      std::cout << "  - IsOpen() after Close(): "
                << (file.IsOpen() ? "true" : "false") << std::endl;
      std::cout << std::endl;
    }

    // Writing basic types
    {
      std::cout << "2. Writing basic types:" << std::endl;

      nova::BinaryFile file("test_data.bin", std::ios::binary | std::ios::in |
                                                 std::ios::out |
                                                 std::ios::trunc);

      // Write basic types
      int32_t intVal = 42;
      float floatVal = 3.14f;
      double doubleVal = 2.71828;
      char charVal = 'A';

      file.Write(intVal);
      file.Write(floatVal);
      file.Write(doubleVal);
      file.Write(charVal);

      std::cout << "  - Wrote: int=" << intVal << ", float=" << floatVal
                << ", double=" << doubleVal << ", char=" << charVal
                << std::endl;

      // Flush to ensure data is written
      file.Flush();
      std::cout << "  - Data flushed to disk" << std::endl;
      std::cout << std::endl;
    }

    // Reading basic types
    {
      std::cout << "3. Reading basic types:" << std::endl;

      nova::BinaryFile file("test_data.bin", std::ios::binary | std::ios::in);

      // Read individual values with different methods
      int32_t intVal = file.ReadAs<int32_t>();
      float floatVal;
      file.Read(floatVal);
      double doubleVal;
      char charVal;

      // Read multiple values at once
      file.Read(doubleVal, charVal);

      std::cout << "  - Read: int=" << intVal << ", float=" << floatVal
                << ", double=" << doubleVal << ", char=" << charVal
                << std::endl;
      std::cout << std::endl;
    }

    // Writing arrays
    {
      std::cout << "4. Writing arrays:" << std::endl;

      nova::BinaryFile file("test_array.bin", std::ios::binary | std::ios::in |
                                                  std::ios::out |
                                                  std::ios::trunc);

      // Create an array of floats
      float values[5] = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};

      // Write the array using BatchWrite
      file.BatchWrite(values, 5);
      std::cout << "  - Wrote 5 float values using BatchWrite" << std::endl;

      // Create a vector of integers
      std::vector<int> intVector = {10, 20, 30, 40, 50};

      // Write the vector using WriteArray
      file.WriteArray(intVector);
      std::cout << "  - Wrote vector of " << intVector.size()
                << " integers using WriteArray" << std::endl;

      file.Flush();
      std::cout << std::endl;
    }

    // Reading arrays
    {
      std::cout << "5. Reading arrays:" << std::endl;

      nova::BinaryFile file("test_array.bin", std::ios::binary | std::ios::in);

      // Read float array
      float values[5];
      file.BatchRead(values, 5);

      std::cout << "  - Read float array: ";
      for (int i = 0; i < 5; i++) {
        std::cout << values[i];
        if (i < 4) std::cout << ", ";
      }
      std::cout << std::endl;

      // Read integer vector individual values
      std::vector<int> intVector(5);
      for (int i = 0; i < 5; i++) {
        intVector[i] = file.ReadAs<int>();
      }

      std::cout << "  - Read int vector: ";
      for (size_t i = 0; i < intVector.size(); i++) {
        std::cout << intVector[i];
        if (i < intVector.size() - 1) std::cout << ", ";
      }
      std::cout << std::endl;
      std::cout << std::endl;
    }

    // Using generic buffer read/write
    {
      std::cout << "6. Generic buffer read/write:" << std::endl;

      nova::BinaryFile file("test_buffer.bin", std::ios::binary | std::ios::in |
                                                   std::ios::out |
                                                   std::ios::trunc);

      // Create a generic buffer
      const char* text = "This is a test of generic buffer operations";
      size_t textLength = strlen(text) + 1;  // Include null terminator

      // Write the buffer
      file.Write(text, textLength);
      std::cout << "  - Wrote text buffer: \"" << text << "\"" << std::endl;

      // Reset position
      file.SeekReadCursor(0);

      // Read the buffer
      char buffer[100];
      file.Read(buffer, textLength);

      std::cout << "  - Read text buffer: \"" << buffer << "\"" << std::endl;

      // Using generic buffer for binary data
      uint8_t binaryData[10] = {0x01, 0x02, 0x03, 0x04, 0x05,
                                0x06, 0x07, 0x08, 0x09, 0x0A};
      file.SeekWriteCursor(0);
      file.Write(binaryData, sizeof(binaryData));

      std::cout << "  - Wrote binary buffer of " << sizeof(binaryData)
                << " bytes" << std::endl;

      // Reset position and read
      file.SeekReadCursor(0);
      uint8_t readBuffer[10];
      file.Read(readBuffer, sizeof(readBuffer));

      std::cout << "  - Read binary buffer: ";
      for (size_t i = 0; i < sizeof(readBuffer); i++) {
        std::cout << "0x" << std::hex << static_cast<int>(readBuffer[i]);
        if (i < sizeof(readBuffer) - 1) std::cout << ", ";
      }
      std::cout << std::dec << std::endl;
      std::cout << std::endl;
    }

    // Seeking and cursor positions
    {
      std::cout << "7. Seeking and cursor positions:" << std::endl;

      nova::BinaryFile file("test_data.bin",
                            std::ios::binary | std::ios::in | std::ios::out);

      // Get initial position
      auto initialPos = file.CurrentReadCursor();
      std::cout << "  - Initial cursor position: " << initialPos << std::endl;

      // Seek to position of float value (after int32)
      file.SeekReadCursor(sizeof(int32_t));
      std::cout << "  - After seeking to float position: "
                << file.CurrentReadCursor() << std::endl;

      // Read the float
      float floatVal = file.ReadAs<float>();
      std::cout << "  - Read float value: " << floatVal << std::endl;

      // Seek relative to current position (skip double)
      file.SeekReadCursor(sizeof(double), std::ios::cur);
      std::cout << "  - After skipping double value: "
                << file.CurrentReadCursor() << std::endl;

      // Read the char
      char charVal = file.ReadAs<char>();
      std::cout << "  - Read char value: " << charVal << std::endl;

      // Seek from end
      file.SeekReadCursor(-sizeof(char), std::ios::end);
      std::cout << "  - After seeking from end: " << file.CurrentReadCursor()
                << std::endl;

      // Read the last char again
      charVal = file.ReadAs<char>();
      std::cout << "  - Read last char again: " << charVal << std::endl;
      std::cout << std::endl;
    }

    // Using custom structures
    {
      std::cout << "8. Using custom structures:" << std::endl;

      nova::BinaryFile file("test_struct.bin", std::ios::binary | std::ios::in |
                                                   std::ios::out |
                                                   std::ios::trunc);

      // Create points
      Point p1 = {1.0f, 2.0f, 3.0f};
      Point p2 = {4.0f, 5.0f, 6.0f};
      Point p3 = {7.0f, 8.0f, 9.0f};

      // Write points
      file.Write(p1);
      file.Write(p2);
      file.Write(p3);

      std::cout << "  - Wrote 3 Point structures" << std::endl;

      // Reset file position
      file.SeekReadCursor(0);

      // Read points
      Point readPoints[3];
      file.BatchRead(readPoints, 3);

      // Print read points
      std::cout << "  - Read Points:" << std::endl;
      for (int i = 0; i < 3; i++) {
        std::cout << "    ";
        PrintPoint(readPoints[i]);
      }
      std::cout << std::endl;
    }

    // Multiple value read/write
    {
      std::cout << "9. Multiple value read/write:" << std::endl;

      nova::BinaryFile file("test_multi.bin", std::ios::binary | std::ios::in |
                                                  std::ios::out |
                                                  std::ios::trunc);

      // Write multiple values at once
      int a = 123;
      float b = 45.67f;
      double c = 89.012;
      char d = 'X';

      file.Write(a, b, c, d);
      std::cout << "  - Wrote multiple values at once using variadic Write"
                << std::endl;

      // Reset position
      file.SeekReadCursor(0);

      // Read multiple values at once
      int a2;
      float b2;
      double c2;
      char d2;

      file.Read(a2, b2, c2, d2);

      std::cout << "  - Read multiple values at once: " << std::endl;
      std::cout << "    a=" << a2 << ", b=" << b2 << ", c=" << c2
                << ", d=" << d2 << std::endl;
      std::cout << std::endl;
    }

    // File state checks
    {
      std::cout << "10. File state checks:" << std::endl;

      nova::BinaryFile file("test_data.bin", std::ios::binary | std::ios::in);

      // Check initial states
      std::cout << "  - Initial state: Good()="
                << (file.Good() ? "true" : "false")
                << ", Eof()=" << (file.Eof() ? "true" : "false") << std::endl;

      // Seek to end
      file.SeekReadCursor(0, std::ios::end);

      // Try to read past end
      char c;
      file.Read(c);

      // Check states after reading past end
      std::cout << "  - After reading past end: Good()="
                << (file.Good() ? "true" : "false")
                << ", Eof()=" << (file.Eof() ? "true" : "false") << std::endl;
      std::cout << std::endl;
    }

    std::cout << "All demos completed successfully!" << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
