#pragma once

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>
#include <string>

#include "nova/common/macros.h"
#include <fcntl.h>

// Define MAP_POPULATE for non-Linux systems
#if !defined(MAP_POPULATE)
#define MAP_POPULATE 0
#endif

namespace nova {

class MappedBinaryFile {
 public:
  // File open modes
  enum class OpenMode {
    kReadOnly,   // Read-only mode
    kReadWrite,  // Read-write mode
    kWriteOnly   // Write-only mode
  };

  // Memory mapping modes
  enum class MapMode {
    kLazy,    // Default lazy loading
    kPreload  // Preload all pages
  };

  // File permission constants
  static constexpr mode_t kDefaultFileMode = 0644;  // rw-r--r--

  explicit MappedBinaryFile() = default;

  explicit MappedBinaryFile(const std::string& file_path,
                            OpenMode mode = OpenMode::kReadOnly,
                            std::size_t initial_size = 0,
                            MapMode map_mode = MapMode::kLazy) {
    Open(file_path, mode, initial_size, map_mode);
  }

  ~MappedBinaryFile() {
    Close();
  }

  // Disable copy operations
  MappedBinaryFile(const MappedBinaryFile&) = delete;
  MappedBinaryFile& operator=(const MappedBinaryFile&) = delete;

  // Enable move operations
  MappedBinaryFile(MappedBinaryFile&& other) noexcept
      : fd_(other.fd_),
        data_(other.data_),
        size_(other.size_),
        current_pos_(other.current_pos_) {
    other.fd_ = -1;
    other.data_ = nullptr;
    other.size_ = 0;
    other.current_pos_ = 0;
  }

  MappedBinaryFile& operator=(MappedBinaryFile&& other) noexcept {
    if (this != &other) {
      Close();
      fd_ = other.fd_;
      data_ = other.data_;
      size_ = other.size_;
      current_pos_ = other.current_pos_;
      other.fd_ = -1;
      other.data_ = nullptr;
      other.size_ = 0;
      other.current_pos_ = 0;
    }
    return *this;
  }

  void Open(const std::string& file_path, OpenMode mode = OpenMode::kReadOnly,
            std::size_t initial_size = 0, MapMode map_mode = MapMode::kLazy) {
    auto [flags, prot] = ProcessOpenMode(mode);
    fd_ = open(file_path.c_str(), flags, kDefaultFileMode);
    if (fd_ == -1) {
      throw std::runtime_error("Failed to open file: " + file_path);
    }

    // Get or set file size
    struct stat st{};
    if (fstat(fd_, &st) == -1) {
      Close();
      throw std::runtime_error("Failed to get file stats");
    }

    size_ = st.st_size;
    if (mode != OpenMode::kReadOnly && initial_size > size_) {
      if (ftruncate(fd_, static_cast<off_t>(initial_size)) == -1) {
        Close();
        throw std::runtime_error("Failed to resize file");
      }
      size_ = initial_size;
    }

    // Set up mapping flags
    int map_flags = MAP_SHARED;

    if constexpr (NOVA_OS == NOVA_OS_LINUX) {
      if (map_mode == MapMode::kPreload) {
        map_flags |= MAP_POPULATE;
      }
    }

    // Map file to memory
    data_ = mmap(nullptr, size_, prot, map_flags, fd_, 0);
    if (data_ == MAP_FAILED) {
      Close();
      throw std::runtime_error("Failed to map file to memory");
    }
  }

  void Close() {
    if (data_ != nullptr) {
      munmap(data_, size_);
      data_ = nullptr;
    }
    if (fd_ != -1) {
      close(fd_);
      fd_ = -1;
    }
    size_ = 0;
    current_pos_ = 0;
  }

  [[nodiscard]] bool IsOpen() const {
    return fd_ != -1 && data_ != nullptr;
  }

  // Basic read/write operations
  void ReadBuffer(void* buffer, std::size_t size) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (current_pos_ + size > size_) {
        throw std::runtime_error("Read operation exceeds file size");
      }
    }
    std::memcpy(buffer, static_cast<char*>(data_) + current_pos_, size);
    current_pos_ += size;
  }

  void WriteBuffer(const void* buffer, std::size_t size) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (current_pos_ + size > size_) {
        throw std::runtime_error("Write operation exceeds file size");
      }
    }
    std::memcpy(static_cast<char*>(data_) + current_pos_, buffer, size);
    current_pos_ += size;
  }

  // Template-based read/write operations
  template <typename T>
  T ReadAs() {
    T value;
    ReadBuffer(&value, sizeof(T));
    return value;
  }

  template <typename T>
  void BatchRead(T* buffer, std::size_t count = 1) {
    ReadBuffer(buffer, sizeof(T) * count);
  }

  template <typename T>
  void Read(T& value) {
    ReadBuffer(&value, sizeof(T));
  }

  template <typename First, typename... Rest>
  void Read(First& first, Rest&... rest) {
    Read(first);
    if constexpr (sizeof...(rest) > 0) {
      Read(rest...);
    }
  }

  template <typename T>
  void BatchWrite(const T* data, std::size_t count = 1) {
    WriteBuffer(data, sizeof(T) * count);
  }

  template <typename T>
  void Write(const T& value) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Only trivially copyable types can be written");
    WriteBuffer(&value, sizeof(T));
  }

  template <typename First, typename... Rest>
  void Write(const First& first, const Rest&... rest) {
    Write(first);
    if constexpr (sizeof...(rest) > 0) {
      Write(rest...);
    }
  }

  template <typename Container>
  void WriteArray(const Container& data) {
    using value_type = typename Container::value_type;
    static_assert(std::is_trivially_copyable_v<value_type>,
                  "Only trivially copyable types can be written");

    if (!data.empty()) {
      WriteBuffer(data.data(), data.size() * sizeof(value_type));
    }
  }

  // File positioning operations
  void Seek(std::size_t pos) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (pos > size_) {
        throw std::runtime_error("Seek position exceeds file size");
      }
    }
    current_pos_ = pos;
  }

  [[nodiscard]] std::size_t Tell() const {
    return current_pos_;
  }

  [[nodiscard]] bool Eof() const {
    return current_pos_ >= size_;
  }

  [[nodiscard]] std::size_t size() const {
    return size_;
  }

  void* data() {
    return data_;
  }

  [[nodiscard]] const void* data() const {
    return data_;
  }

 private:
  static std::pair<int, int> ProcessOpenMode(OpenMode mode) {
    int flags = 0;
    int prot = 0;

    switch (mode) {
      case OpenMode::kReadOnly:
        flags = O_RDONLY;
        prot = PROT_READ;
        break;
      case OpenMode::kReadWrite:
        flags = O_RDWR | O_CREAT;
        prot = PROT_READ | PROT_WRITE;
        break;
      case OpenMode::kWriteOnly:
        flags = O_WRONLY | O_CREAT;
        prot = PROT_WRITE;
        break;
    }

    return {flags, prot};
  }

  int fd_{-1};                  // File descriptor
  void* data_{nullptr};         // Memory mapped address
  std::size_t size_{0};         // File size
  std::size_t current_pos_{0};  // Current position
};

}  // namespace nova