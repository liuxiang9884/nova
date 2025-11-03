//
// Created by liuxiang on 2025/6/15.
//

#pragma once

#include <array>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <utility>

#include <fmt/format.h>

#include "nova/common/macros.h"

namespace nova {

template <typename T, std::size_t N>
class FixedArray {
 public:
  // Type definitions
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = T&;
  using const_reference = const T&;
  using pointer = T*;
  using const_pointer = const T*;
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // Constructors
  constexpr FixedArray() = default;
  constexpr FixedArray(const FixedArray&) = default;
  constexpr FixedArray(FixedArray&&) = default;
  constexpr FixedArray& operator=(const FixedArray&) = default;
  constexpr FixedArray& operator=(FixedArray&&) = default;

  // Element access
  constexpr reference at(size_type pos) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (pos >= size_) {
        throw std::out_of_range("FixedArray::at");
      }
    }
    return data_[pos];
  }

  constexpr const_reference at(size_type pos) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (pos >= size_) {
        throw std::out_of_range("FixedArray::at");
      }
    }
    return data_[pos];
  }

  constexpr reference operator[](size_type pos) {
    return data_[pos];
  }
  constexpr const_reference operator[](size_type pos) const {
    return data_[pos];
  }
  constexpr reference front() {
    return data_[0];
  }
  constexpr const_reference front() const {
    return data_[0];
  }
  constexpr reference back() {
    return data_[size_ - 1];
  }
  constexpr const_reference back() const {
    return data_[size_ - 1];
  }
  constexpr pointer data() noexcept {
    return data_.data();
  }
  constexpr const_pointer data() const noexcept {
    return data_.data();
  }

  // Iterators
  constexpr iterator begin() noexcept {
    return data_.begin();
  }
  constexpr const_iterator begin() const noexcept {
    return data_.begin();
  }
  constexpr const_iterator cbegin() const noexcept {
    return data_.cbegin();
  }
  constexpr iterator end() noexcept {
    return data_.begin() + size_;
  }
  constexpr const_iterator end() const noexcept {
    return data_.begin() + size_;
  }
  constexpr const_iterator cend() const noexcept {
    return data_.cbegin() + size_;
  }
  constexpr reverse_iterator rbegin() noexcept {
    return reverse_iterator(end());
  }
  constexpr const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }
  constexpr const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(cend());
  }
  constexpr reverse_iterator rend() noexcept {
    return reverse_iterator(begin());
  }
  constexpr const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }
  constexpr const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator(cbegin());
  }

  // Capacity
  [[nodiscard]] constexpr bool empty() const noexcept {
    return size_ == 0;
  }
  [[nodiscard]] constexpr size_type size() const noexcept {
    return size_;
  }
  [[nodiscard]] constexpr size_type max_size() const noexcept {
    return N;
  }
  [[nodiscard]] constexpr size_type capacity() const noexcept {
    return N;
  }

  // Modifiers
  constexpr void push_back(const T& value) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (size_ >= N) {
        throw std::length_error("FixedArray::push_back");
      }
    }
    data_[size_++] = value;
  }

  constexpr void push_back(T&& value) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (size_ >= N) {
        throw std::length_error("FixedArray::push_back");
      }
    }
    data_[size_++] = std::move(value);
  }

  template <typename... Args>
  constexpr reference emplace_back(Args&&... args) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (size_ >= N) {
        throw std::length_error("FixedArray::emplace_back");
      }
    }

    // data_[size_] = T(std::forward<Args>(args)...);
    new (&data_[size_]) T(std::forward<Args>(args)...);
    return data_[size_++];
  }

  constexpr void pop_back() {
    if constexpr (NOVA_DEBUG_MODE) {
      if (empty()) {
        throw std::out_of_range("FixedArray::pop_back");
      }
    }
    --size_;
  }

  constexpr void resize(size_type new_size) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (new_size > N) {
        throw std::length_error("FixedArray::resize");
      }
    }
    if (new_size > size_) {
      for (size_type i = size_; i < new_size; ++i) {
        new (&data_[i]) T();
      }
    }
    size_ = new_size;
  }

  constexpr void resize(size_type new_size, const T& value) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (new_size > N) {
        throw std::length_error("FixedArray::resize");
      }
    }
    if (new_size > size_) {
      for (size_type i = size_; i < new_size; ++i) {
        data_[i] = value;
      }
    }
    size_ = new_size;
  }

  constexpr void clear() noexcept {
    size_ = 0;
  }

  // Comparison operators
  friend constexpr bool operator==(const FixedArray& lhs,
                                   const FixedArray& rhs) {
    if (lhs.size_ != rhs.size_) return false;
    for (size_type i = 0; i < lhs.size_; ++i) {
      if (lhs.data_[i] != rhs.data_[i]) return false;
    }
    return true;
  }

  friend constexpr bool operator!=(const FixedArray& lhs,
                                   const FixedArray& rhs) {
    return !(lhs == rhs);
  }

 private:
  std::array<T, N> data_;
  size_type size_{0};
};

}  // namespace nova
