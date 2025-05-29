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
/// @tparam N Maximum length of the string
template <std::size_t N>
class FixedString {
 public:
  // Type definitions for STL compatibility
  using value_type = char;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = char&;
  using const_reference = const char&;
  using pointer = char*;
  using const_pointer = const char*;
  using iterator = char*;
  using const_iterator = const char*;

  /// @brief Default constructor is now trivial (implicitly defined)
  FixedString() = default;

  /// @brief Constructor from C-style string
  /// @param str C-style string to copy
  explicit FixedString(const char* str) {
    if (str == nullptr) {
      size_ = 0;
      return;
    }

    std::size_t len = std::strlen(str);
    size_ = CheckAndAdjustLength(len);
    std::memcpy(data_.data(), str, size_);
  }

  /// @brief Constructor from std::string
  /// @param str String to copy
  explicit FixedString(const std::string& str) {
    std::size_t len = str.size();
    size_ = CheckAndAdjustLength(len);
    std::memcpy(data_.data(), str.data(), size_);
  }

  /// @brief Constructor from std::string_view
  /// @param str String view to copy
  explicit FixedString(std::string_view str) {
    std::size_t len = str.size();
    size_ = CheckAndAdjustLength(len);
    std::memcpy(data_.data(), str.data(), size_);
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
      return *this;
    }

    std::size_t len = std::strlen(str);
    size_ = CheckAndAdjustLength(len);
    std::memcpy(data_.data(), str, size_);
    return *this;
  }

  /// @brief Assignment from std::string
  /// @param str String to assign
  FixedString& operator=(const std::string& str) {
    std::size_t len = str.size();
    size_ = CheckAndAdjustLength(len);
    std::memcpy(data_.data(), str.data(), size_);
    return *this;
  }

  /// @brief Assignment from std::string_view
  /// @param str String view to assign
  FixedString& operator=(std::string_view str) {
    std::size_t len = str.size();
    size_ = CheckAndAdjustLength(len);
    std::memcpy(data_.data(), str.data(), size_);
    return *this;
  }

  /// @brief Get string data pointer
  /// @return Pointer to string data
  [[nodiscard]] const char* data() const {
    return data_.data();
  }

  /// @brief Get string view
  /// @return String view of the content
  [[nodiscard]] std::string_view view() const {
    return std::string_view(data_.data(), size_);
  }

  /// @brief Convert to std::string
  /// @return Copy as std::string
  [[nodiscard]] std::string string() const {
    return std::string(data_.data(), size_);
  }

  /// @brief Get current size
  /// @return Current string length
  [[nodiscard]] std::size_t size() const {
    return size_;
  }

  /// @brief Check if string is empty
  /// @return True if empty, false otherwise
  [[nodiscard]] bool empty() const {
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
  }

  // Iterator support
  /// @brief Get iterator to beginning
  /// @return Iterator to the first character
  iterator begin() {
    return data_.data();
  }

  /// @brief Get const iterator to beginning
  /// @return Const iterator to the first character
  [[nodiscard]] const_iterator begin() const {
    return data_.data();
  }

  /// @brief Get const iterator to beginning
  /// @return Const iterator to the first character
  [[nodiscard]] const_iterator cbegin() const {
    return data_.data();
  }

  /// @brief Get iterator to end
  /// @return Iterator to one past the last character
  iterator end() {
    return data_.data() + size_;
  }

  /// @brief Get const iterator to end
  /// @return Const iterator to one past the last character
  [[nodiscard]] const_iterator end() const {
    return data_.data() + size_;
  }

  /// @brief Get const iterator to end
  /// @return Const iterator to one past the last character
  [[nodiscard]] const_iterator cend() const {
    return data_.data() + size_;
  }

  // Comparison operators
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

  /// @brief Less than comparison
  /// @param other Other FixedString to compare with
  /// @return True if this string is lexicographically less than other
  bool operator<(const FixedString& other) const {
    return view() < other.view();
  }

  /// @brief Greater than comparison
  /// @param other Other FixedString to compare with
  /// @return True if this string is lexicographically greater than other
  bool operator>(const FixedString& other) const {
    return view() > other.view();
  }

  /// @brief Less than or equal comparison
  /// @param other Other FixedString to compare with
  /// @return True if this string is lexicographically less than or equal to
  /// other
  bool operator<=(const FixedString& other) const {
    return view() <= other.view();
  }

  /// @brief Greater than or equal comparison
  /// @param other Other FixedString to compare with
  /// @return True if this string is lexicographically greater than or equal to
  /// other
  bool operator>=(const FixedString& other) const {
    return view() >= other.view();
  }

  /// @brief Equality comparison with C-style string
  /// @param str C-style string to compare with
  /// @return True if equal, false otherwise
  bool operator==(const char* str) const {
    if (str == nullptr) {
      return size_ == 0;
    }
    std::size_t str_len = std::strlen(str);
    return size_ == str_len && std::memcmp(data_.data(), str, size_) == 0;
  }

  /// @brief Inequality comparison with C-style string
  /// @param str C-style string to compare with
  /// @return True if not equal, false otherwise
  bool operator!=(const char* str) const {
    return !(*this == str);
  }

  /// @brief Less than comparison with C-style string
  /// @param str C-style string to compare with
  /// @return True if this string is lexicographically less than str
  bool operator<(const char* str) const {
    if (str == nullptr) {
      return false;  // Non-empty string is not less than null
    }
    return view() < std::string_view(str);
  }

  /// @brief Greater than comparison with C-style string
  /// @param str C-style string to compare with
  /// @return True if this string is lexicographically greater than str
  bool operator>(const char* str) const {
    if (str == nullptr) {
      return size_ > 0;  // Non-empty string is greater than null
    }
    return view() > std::string_view(str);
  }

  /// @brief Less than or equal comparison with C-style string
  /// @param str C-style string to compare with
  /// @return True if this string is lexicographically less than or equal to str
  bool operator<=(const char* str) const {
    if (str == nullptr) {
      return size_ == 0;  // Only empty string is <= null
    }
    return view() <= std::string_view(str);
  }

  /// @brief Greater than or equal comparison with C-style string
  /// @param str C-style string to compare with
  /// @return True if this string is lexicographically greater than or equal to
  /// str
  bool operator>=(const char* str) const {
    if (str == nullptr) {
      return true;  // Any string is >= null
    }
    return view() >= std::string_view(str);
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

  /// @brief Less than comparison with std::string_view
  /// @param str String view to compare with
  /// @return True if this string is lexicographically less than str
  bool operator<(std::string_view str) const {
    return view() < str;
  }

  /// @brief Greater than comparison with std::string_view
  /// @param str String view to compare with
  /// @return True if this string is lexicographically greater than str
  bool operator>(std::string_view str) const {
    return view() > str;
  }

  /// @brief Less than or equal comparison with std::string_view
  /// @param str String view to compare with
  /// @return True if this string is lexicographically less than or equal to str
  bool operator<=(std::string_view str) const {
    return view() <= str;
  }

  /// @brief Greater than or equal comparison with std::string_view
  /// @param str String view to compare with
  /// @return True if this string is lexicographically greater than or equal to
  /// str
  bool operator>=(std::string_view str) const {
    return view() >= str;
  }

 private:
  // String data storage (no null terminator needed)
  std::array<char, N> data_;
  // Current string length
  std::size_t size_{0};

  /// @brief Check and adjust length for string operations
  /// @param len Input length
  /// @return Adjusted length (truncated if necessary)
  /// @throws std::length_error in debug mode if length exceeds capacity
  [[nodiscard]] std::size_t CheckAndAdjustLength(std::size_t len) const {
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

  // Support for heterogeneous lookup with std::string_view
  std::size_t operator()(std::string_view str) const {
    return std::hash<std::string_view>{}(str);
  }
};

}  // namespace nova
