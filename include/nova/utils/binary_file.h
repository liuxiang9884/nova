//
// Created by liuxiang on 2025/4/29.
//

#pragma once

#include <fstream>
#include <stdexcept>
#include <string>

#include "nova/common/macros.h"

namespace nova {

class BinaryFile {
 public:
  // File open modes as an enum
  enum class OpenMode {
    // Basic modes
    // Open for reading
    kReadOnly = std::ios::binary | std::ios::in,
    // Open for writing
    kWriteOnly = std::ios::binary | std::ios::out,
    // Open for reading and writing (file must exist)
    kReadWrite = std::ios::binary | std::ios::in | std::ios::out,

    // Combined modes
    // Open for appending at the end
    kAppend = std::ios::binary | std::ios::out | std::ios::app,
    // Open for reading and appending
    kReadAppend =
        std::ios::binary | std::ios::in | std::ios::out | std::ios::app,
    // Create new or truncate existing file
    kTruncate = std::ios::binary | std::ios::out | std::ios::trunc,
    // Open for reading and writing, truncate if exists
    kReadWriteTruncate =
        std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc,

    // Special modes
    // Open and seek to end
    kAtEnd = std::ios::binary | std::ios::in | std::ios::out | std::ios::ate,
    // Open read-only and seek to end
    kReadAtEnd =
        std::ios::binary | std::ios::in | std::ios::out | std::ios::ate,
    // Create new file, fail if exists
    kExclusiveCreate = std::ios::binary | std::ios::out | std::ios::trunc
  };

  explicit BinaryFile(const std::string& file_path,
                      OpenMode mode = OpenMode::kReadOnly) {
    Open(file_path, mode);
  }

  explicit BinaryFile(const std::string& file_path,
                      std::ios_base::openmode mode) {
    Open(file_path, mode);
  }

  explicit BinaryFile() = default;

  void Open(const std::string& file_path,
            OpenMode mode = OpenMode::kReadWrite) {
    Open(file_path, static_cast<std::ios_base::openmode>(mode));
  }

  void Open(const std::string& file_path, std::ios_base::openmode mode) {
    file_.open(file_path, mode);
    if (!file_.is_open()) {
      throw std::runtime_error("Failed to open file: " + file_path);
    }
  }

  ~BinaryFile() {
    Close();
  }

  BinaryFile(const BinaryFile&) = delete;
  BinaryFile& operator=(const BinaryFile&) = delete;

  BinaryFile(BinaryFile&& other) noexcept {
    file_.swap(other.file_);
  }

  BinaryFile& operator=(BinaryFile&& other) noexcept {
    if (this != &other) {
      Close();
      file_.swap(other.file_);
    }
    return *this;
  }

  void Close() {
    if (file_.is_open()) {
      file_.close();
    }
  }

  bool IsOpen() const {
    return file_.is_open();
  }

  // Read data from file into a buffer of specific size
  void ReadBuffer(void* buffer, std::size_t size) {
    file_.read(static_cast<char*>(buffer), size);
    ProcessError("Read buffer operation failed");
  }

  // Write data from buffer to file
  void WriteBuffer(const void* buffer, std::size_t size) {
    file_.write(static_cast<const char*>(buffer), size);
    ProcessError("Write buffer operation failed");
  }

  template <typename T>
  T ReadAs() {
    T value;
    ReadBuffer(reinterpret_cast<char*>(&value), sizeof(T));
    ProcessError("ReadAs operation failed");
    return value;
  }

  // Read array into buffer
  template <typename T>
  void BatchRead(T* buffer, std::size_t count = 1) {
    ReadBuffer(reinterpret_cast<char*>(buffer), sizeof(T) * count);
    ProcessError("BatchRead operation failed");
  }

  // Read a single variable
  template <typename T>
  void Read(T& value) {
    ReadBuffer(reinterpret_cast<char*>(&value), sizeof(T));
    ProcessError("Read operation failed");
  }

  // Read multiple variables
  template <typename First, typename... Rest>
  void Read(First& first, Rest&... rest) {
    Read(first);
    if constexpr (sizeof...(rest) > 0) {
      Read(rest...);
    }
  }

  // Write array
  template <typename T>
  void BatchWrite(const T* data, std::size_t count = 1) {
    WriteBuffer(reinterpret_cast<const char*>(data), sizeof(T) * count);
    ProcessError("Write operation failed");
  }

  // Write a single variable
  template <typename T>
  void Write(const T& value) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Only trivially copyable types can be written");
    WriteBuffer(reinterpret_cast<const char*>(&value), sizeof(T));
    ProcessError("Write operation failed");
  }

  // Write container (vector, array, etc.)
  template <typename Container>
  void WriteArray(const Container& data) {
    using value_type = typename Container::value_type;
    static_assert(std::is_trivially_copyable_v<value_type>,
                  "Only trivially copyable types can be written");

    if (!data.empty()) {
      WriteBuffer(
          reinterpret_cast<const char*>(data.data()),
          static_cast<std::streamsize>(data.size() * sizeof(value_type)));
      ProcessError("Container write operation failed");
    }
  }

  // Write multiple variables
  template <typename First, typename... Rest>
  void Write(const First& first, const Rest&... rest) {
    Write(first);
    if constexpr (sizeof...(rest) > 0) {
      (Write(rest), ...);
    }
  }

  template <typename T>
  void WriteAs(const T& value) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Only trivially copyable types can be written");
    WriteBuffer(reinterpret_cast<const char*>(&value), sizeof(T));
    ProcessError("WriteAs operation failed");
  }

  void SeekReadCursor(int64_t offset, std::ios::seekdir dir = std::ios::beg) {
    file_.seekg(offset, dir);
    ProcessError("Seek read cursor operation failed");
  }

  void SeekWriteCursor(int64_t offset, std::ios::seekdir dir = std::ios::beg) {
    file_.seekp(offset, dir);
    ProcessError("Seek write cursor operation failed");
  }

  std::size_t CurrentReadCursor() {
    return file_.tellg();
  }

  std::size_t CurrentWriteCursor() {
    return file_.tellp();
  }

  void Flush() {
    file_.flush();
    ProcessError("Flush operation failed");
  }

  bool Good() const {
    return file_.good();
  }

  bool Eof() const {
    return file_.eof();
  }

  std::fstream& file() {
    return file_;
  }

 private:
  // Helper function for error processing, only throws in debug mode
  NOVA_FORCE_INLINE void ProcessError(const char* message) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (!file_) {
        throw std::runtime_error(message);
      }
    }
  }

 private:
  std::fstream file_;
};

}  // namespace nova
