#include <cstdint>
#include <cstring>
#include <filesystem>
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

  namespace fs = std::filesystem;

  try {
    // Basic file operations with different modes
    {
      std::cout << "1. File open modes:" << std::endl;

      // Clean up existing test files
      const char* test_files[] = {"test_ro.bin",    "test_wo.bin",
                                  "test_rw.bin",    "test_append.bin",
                                  "test_trunc.bin", "test_ate.bin"};
      for (const auto& file : test_files) {
        if (fs::exists(file)) {
          fs::remove(file);
        }
      }

      // 1. Read-only mode (will fail if file doesn't exist)
      std::cout << "  - ReadOnly mode: ";
      try {
        nova::BinaryFile file("test_ro.bin",
                              nova::BinaryFile::OpenMode::kReadOnly);
        std::cout << "Opened successfully (unexpected)" << std::endl;
      } catch (const std::exception&) {
        std::cout << "Failed as expected (file doesn't exist)" << std::endl;
      }

      // 2. Write-only mode (creates file if doesn't exist)
      {
        std::cout << "  - WriteOnly mode: ";
        try {
          nova::BinaryFile file("test_wo.bin",
                                nova::BinaryFile::OpenMode::kWriteOnly);
          file.WriteBuffer("Test data", 9);
          std::cout << "Created and wrote to file successfully" << std::endl;
        } catch (const std::exception& e) {
          std::cout << "Failed: " << e.what() << std::endl;
        }
      }

      // 3. Read-Write mode (only opens existing files)
      {
        std::cout << "  - ReadWrite mode on existing file: ";
        try {
          nova::BinaryFile file("test_wo.bin",
                                nova::BinaryFile::OpenMode::kReadWrite);
          std::cout << "Opened successfully" << std::endl;

          char buffer[10];
          file.ReadBuffer(buffer, 9);
          buffer[9] = '\0';
          std::cout << "    Read data: " << buffer << std::endl;

          file.SeekWriteCursor(0);
          file.WriteBuffer("New data!", 9);
          std::cout << "    Wrote new data" << std::endl;
        } catch (const std::exception& e) {
          std::cout << "Failed: " << e.what() << std::endl;
        }
      }

      // 4. Append mode (creates file if needed, always writes at end)
      {
        std::cout << "  - Append mode: ";
        try {
          nova::BinaryFile file("test_append.bin",
                                nova::BinaryFile::OpenMode::kAppend);
          file.WriteBuffer("First append", 12);
          file.WriteBuffer(" - Second append", 16);
          std::cout << "Appended data successfully" << std::endl;
        } catch (const std::exception& e) {
          std::cout << "Failed: " << e.what() << std::endl;
        }

        // Read the appended data
        try {
          nova::BinaryFile file("test_append.bin",
                                nova::BinaryFile::OpenMode::kReadOnly);
          char buffer[50];
          file.ReadBuffer(buffer, 28);
          buffer[28] = '\0';
          std::cout << "    Read appended data: " << buffer << std::endl;
        } catch (const std::exception& e) {
          std::cout << "    Failed to read: " << e.what() << std::endl;
        }
      }

      // 5. ReadAppend mode (read and append to existing file)
      {
        std::cout << "  - ReadAppend mode: ";
        try {
          nova::BinaryFile file("test_append.bin",
                                nova::BinaryFile::OpenMode::kReadAppend);
          char buffer[50];
          file.ReadBuffer(buffer, 28);
          buffer[28] = '\0';
          std::cout << "Read existing data" << std::endl;
          std::cout << "    Content: " << buffer << std::endl;

          // Append more data (should go to the end regardless of current
          // position)
          file.WriteBuffer(" - Third append", 15);
          std::cout << "    Appended more data" << std::endl;
        } catch (const std::exception& e) {
          std::cout << "Failed: " << e.what() << std::endl;
        }
      }

      // 6. Truncate mode (creates or empties existing file)
      {
        std::cout << "  - Truncate mode: ";
        try {
          // First put some data
          {
            nova::BinaryFile file("test_trunc.bin",
                                  nova::BinaryFile::OpenMode::kWriteOnly);
            file.WriteBuffer("Original data", 13);
          }

          // Now truncate and write new data
          nova::BinaryFile file("test_trunc.bin",
                                nova::BinaryFile::OpenMode::kTruncate);
          file.WriteBuffer("Truncated data", 14);
          std::cout << "Truncated and wrote new data" << std::endl;

          // Read back to verify
          file.SeekReadCursor(0);
          char buffer[20];
          file.ReadBuffer(buffer, 14);
          buffer[14] = '\0';
          std::cout << "    Content after truncate: " << buffer << std::endl;
        } catch (const std::exception& e) {
          std::cout << "Failed: " << e.what() << std::endl;
        }
      }

      // 7. ReadWriteTruncate mode (read/write with truncation)
      {
        std::cout << "  - ReadWriteTruncate mode: ";
        try {
          nova::BinaryFile file("test_rw.bin",
                                nova::BinaryFile::OpenMode::kReadWriteTruncate);
          file.WriteBuffer("ReadWriteTruncate test", 21);
          std::cout << "Created and wrote to file" << std::endl;

          // Read back the data
          file.SeekReadCursor(0);
          char buffer[30];
          file.ReadBuffer(buffer, 21);
          buffer[21] = '\0';
          std::cout << "    Content: " << buffer << std::endl;
        } catch (const std::exception& e) {
          std::cout << "Failed: " << e.what() << std::endl;
        }
      }

      // 8. AtEnd mode (open and position at end)
      {
        std::cout << "  - AtEnd mode: ";
        try {
          // First create a file with data
          {
            nova::BinaryFile file("test_ate.bin",
                                  nova::BinaryFile::OpenMode::kWriteOnly);
            file.WriteBuffer("Initial data for AtEnd mode", 26);
          }

          // Open with AtEnd
          nova::BinaryFile file("test_ate.bin",
                                nova::BinaryFile::OpenMode::kAtEnd);
          std::cout << "Opened with cursor at end" << std::endl;
          std::cout << "    Current position: " << file.CurrentReadCursor()
                    << std::endl;

          // We're at the end, so add more data
          file.WriteBuffer(" - Added at end", 15);

          // Read the whole file from beginning
          file.SeekReadCursor(0);
          char buffer[50];
          file.ReadBuffer(buffer, 41);
          buffer[41] = '\0';
          std::cout << "    Full content: " << buffer << std::endl;
        } catch (const std::exception& e) {
          std::cout << "Failed: " << e.what() << std::endl;
        }
      }

      std::cout << std::endl;
    }

    // Writing basic types
    {
      std::cout << "2. Writing basic types:" << std::endl;

      nova::BinaryFile file("test_data.bin",
                            nova::BinaryFile::OpenMode::kTruncate);

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

      nova::BinaryFile file("test_data.bin",
                            nova::BinaryFile::OpenMode::kReadOnly);

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

      nova::BinaryFile file("test_array.bin",
                            nova::BinaryFile::OpenMode::kTruncate);

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

    // Using generic buffer read/write
    {
      std::cout << "5. Generic buffer read/write:" << std::endl;

      nova::BinaryFile file("test_buffer.bin",
                            nova::BinaryFile::OpenMode::kReadWriteTruncate);

      // Create a generic buffer
      const char* text = "This is a test of generic buffer operations";
      size_t textLength = strlen(text) + 1;  // Include null terminator

      // Write the buffer
      file.WriteBuffer(text, textLength);
      std::cout << "  - Wrote text buffer: \"" << text << "\"" << std::endl;

      // Reset position
      file.SeekReadCursor(0);

      // Read the buffer
      char buffer[100];
      file.ReadBuffer(buffer, textLength);

      std::cout << "  - Read text buffer: \"" << buffer << "\"" << std::endl;

      // Using generic buffer for binary data
      uint8_t binaryData[10] = {0x01, 0x02, 0x03, 0x04, 0x05,
                                0x06, 0x07, 0x08, 0x09, 0x0A};
      file.SeekWriteCursor(0);
      file.WriteBuffer(binaryData, sizeof(binaryData));

      std::cout << "  - Wrote binary buffer of " << sizeof(binaryData)
                << " bytes" << std::endl;

      // Reset position and read
      file.SeekReadCursor(0);
      uint8_t readBuffer[10];
      file.ReadBuffer(readBuffer, sizeof(readBuffer));

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
      std::cout << "6. Seeking and cursor positions:" << std::endl;

      nova::BinaryFile file("test_data.bin",
                            nova::BinaryFile::OpenMode::kReadWrite);

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
      std::cout << "7. Using custom structures:" << std::endl;

      nova::BinaryFile file("test_struct.bin",
                            nova::BinaryFile::OpenMode::kReadWriteTruncate);

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
      std::cout << "8. Multiple value read/write:" << std::endl;

      nova::BinaryFile file("test_multi.bin",
                            nova::BinaryFile::OpenMode::kReadWriteTruncate);

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
      std::cout << "9. File state checks:" << std::endl;

      nova::BinaryFile file("test_data.bin",
                            nova::BinaryFile::OpenMode::kReadOnly);

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
