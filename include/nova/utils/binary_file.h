//
// Created by liuxiang on 2025/4/29.
//

#pragma once

#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "nova/common/macros.h"

namespace nova {

// Default DEBUG_MODE
#if !defined(NDEBUG)
#define DEBUG_MODE 1
#else
#define DEBUG_MODE 0
#endif

class BinaryFile {
 public:
  enum class SeekOrigin { Begin, Current, End };

  constexpr static auto kDefaultOpenMode =
      std::ios::binary | std::ios::in | std::ios::out;

  explicit BinaryFile(const std::string& file_path,
                      std::ios_base::openmode mode = kDefaultOpenMode)
      : file_(file_path, mode) {
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

  template <typename T>
  T ReadAs() {
    T value;
    file_.read(reinterpret_cast<char*>(&value), sizeof(T));
    ProcessError("ReadAs operation failed");
    return value;
  }

  // Read array into buffer
  template <typename T>
  void BatchRead(T* buffer, size_t count = 1) {
    file_.read(reinterpret_cast<char*>(buffer), sizeof(T) * count);
    ProcessError("BatchRead operation failed");
  }

  // Read a single variable
  template <typename T>
  void Read(T& value) {
    file_.read(reinterpret_cast<char*>(&value), sizeof(T));
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
    file_.write(reinterpret_cast<const char*>(data), sizeof(T) * count);
    ProcessError("Write operation failed");
  }

  // Write a single variable
  template <typename T>
  void Write(const T& value) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Only trivially copyable types can be written");
    file_.write(reinterpret_cast<const char*>(&value), sizeof(T));
    ProcessError("Write operation failed");
  }

  // Write container (vector, array, etc.)
  template <typename Container>
  void WriteArray(const Container& data) {
    using value_type = typename Container::value_type;
    static_assert(std::is_trivially_copyable_v<value_type>,
                  "Only trivially copyable types can be written");

    if (!data.empty()) {
      file_.write(
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
    file_.write(reinterpret_cast<const char*>(&value), sizeof(T));
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
    if constexpr (DEBUG_MODE) {
      if (!file_) {
        throw std::runtime_error(message);
      }
    }
  }

 private:
  std::fstream file_;
};

}  // namespace nova
