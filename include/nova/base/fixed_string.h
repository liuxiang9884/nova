//
// Created by liuxiang on 2025/5/28.
//

#pragma once

#include <array>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "nova/common/macros.h"

namespace nova {

/// @brief Fixed-size string with compile-time maximum length
/// @tparam N Maximum length of the string (excluding null terminator)
template <std::size_t N>
class FixedString {
 public:
  static constexpr std::size_t max_size = N;

  /// @brief Default constructor, creates empty string
  FixedString() : size_(0) {
    data_[0] = '\0';
  }

  /// @brief Constructor from C-style string
  /// @param str C-style string to copy
  FixedString(const char* str) {
    if (str == nullptr) {
      size_ = 0;
      data_[0] = '\0';
      return;
    }

    std::size_t len = std::strlen(str);
    size_ = CheckAndAdjustLength(len);
    std::memcpy(data_.data(), str, size_);
    data_[size_] = '\0';
  }

  /// @brief Constructor from std::string
  /// @param str String to copy
  FixedString(const std::string& str) {
    std::size_t len = str.size();
    size_ = CheckAndAdjustLength(len);
    std::memcpy(data_.data(), str.data(), size_);
    data_[size_] = '\0';
  }

  /// @brief Constructor from std::string_view
  /// @param str String view to copy
  FixedString(std::string_view str) {
    std::size_t len = str.size();
    size_ = CheckAndAdjustLength(len);
    std::memcpy(data_.data(), str.data(), size_);
    data_[size_] = '\0';
  }

  /// @brief Copy constructor
  FixedString(const FixedString& other) = default;

  /// @brief Move constructor
  FixedString(FixedString&& other) noexcept = default;

  /// @brief Copy assignment operator
  FixedString& operator=(const FixedString& other) = default;

  /// @brief Move assignment operator
  FixedString& operator=(FixedString&& other) noexcept = default;

  /// @brief Assignment from C-style string
  /// @param str C-style string to assign
  FixedString& operator=(const char* str) {
    if (str == nullptr) {
      size_ = 0;
      data_[0] = '\0';
      return *this;
    }

    std::size_t len = std::strlen(str);
    size_ = CheckAndAdjustLength(len);
    std::memcpy(data_.data(), str, size_);
    data_[size_] = '\0';
    return *this;
  }

  /// @brief Assignment from std::string
  /// @param str String to assign
  FixedString& operator=(const std::string& str) {
    std::size_t len = str.size();
    size_ = CheckAndAdjustLength(len);
    std::memcpy(data_.data(), str.data(), size_);
    data_[size_] = '\0';
    return *this;
  }

  /// @brief Assignment from std::string_view
  /// @param str String view to assign
  FixedString& operator=(std::string_view str) {
    std::size_t len = str.size();
    size_ = CheckAndAdjustLength(len);
    std::memcpy(data_.data(), str.data(), size_);
    data_[size_] = '\0';
    return *this;
  }

  /// @brief Get C-style string
  /// @return Null-terminated C-style string
  const char* c_str() const {
    return data_.data();
  }

  /// @brief Get string data pointer
  /// @return Pointer to string data
  const char* data() const {
    return data_.data();
  }

  /// @brief Get string view
  /// @return String view of the content
  std::string_view view() const {
    return std::string_view(data_.data(), size_);
  }

  /// @brief Convert to std::string
  /// @return Copy as std::string
  std::string string() const {
    return std::string(data_.data(), size_);
  }

  /// @brief Get current size
  /// @return Current string length
  std::size_t size() const {
    return size_;
  }

  /// @brief Check if string is empty
  /// @return True if empty, false otherwise
  bool empty() const {
    return size_ == 0;
  }

  /// @brief Get maximum capacity
  /// @return Maximum string length
  static constexpr std::size_t capacity() {
    return N;
  }

  /// @brief Clear the string
  void Clear() {
    size_ = 0;
    data_[0] = '\0';
  }

  /// @brief Equality comparison
  /// @param other Other FixedString to compare with
  /// @return True if equal, false otherwise
  bool operator==(const FixedString& other) const {
    return size_ == other.size_ &&
           std::memcmp(data_.data(), other.data_.data(), size_) == 0;
  }

  /// @brief Inequality comparison
  /// @param other Other FixedString to compare with
  /// @return True if not equal, false otherwise
  bool operator!=(const FixedString& other) const {
    return !(*this == other);
  }

  /// @brief Equality comparison with C-style string
  /// @param str C-style string to compare with
  /// @return True if equal, false otherwise
  bool operator==(const char* str) const {
    if (str == nullptr) {
      return size_ == 0;
    }
    return std::strcmp(data_.data(), str) == 0;
  }

  /// @brief Inequality comparison with C-style string
  /// @param str C-style string to compare with
  /// @return True if not equal, false otherwise
  bool operator!=(const char* str) const {
    return !(*this == str);
  }

  /// @brief Equality comparison with std::string_view
  /// @param str String view to compare with
  /// @return True if equal, false otherwise
  bool operator==(std::string_view str) const {
    return view() == str;
  }

  /// @brief Inequality comparison with std::string_view
  /// @param str String view to compare with
  /// @return True if not equal, false otherwise
  bool operator!=(std::string_view str) const {
    return !(*this == str);
  }

 private:
  // String data storage (including space for null terminator)
  std::array<char, N + 1> data_;
  // Current string length
  std::size_t size_;

  /// @brief Check and adjust length for string operations
  /// @param len Input length
  /// @return Adjusted length (truncated if necessary)
  /// @throws std::length_error in debug mode if length exceeds capacity
  std::size_t CheckAndAdjustLength(std::size_t len) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (len > N) {
        throw std::length_error(fmt::format(
            "String too long for FixedString: {} > {} (capacity)", len, N));
      }
      return len;
    } else {
      return len > N ? N : len;  // Truncate to fit in release mode
    }
  }
};

/// @brief Hash function for FixedString
template <std::size_t N>
struct FixedStringHash {
  std::size_t operator()(const FixedString<N>& str) const {
    return std::hash<std::string_view>{}(str.view());
  }
};

}  // namespace nova
