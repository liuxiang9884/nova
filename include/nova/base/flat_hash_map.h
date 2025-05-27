#pragma once

#include <array>
#include <functional>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace nova::static_impl {

// Exception for FlatHashMap operations
class FlatHashMapError : public std::runtime_error {
 public:
  explicit FlatHashMapError(const std::string& msg) : std::runtime_error(msg) {}
};

template <class Key, class Value, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>, std::size_t Capacity = 1024>
class FlatHashMap {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<Key, Value>;
  using size_type = std::size_t;
  using hasher = Hash;
  using key_equal = KeyEqual;

  // Container type - stores actual key-value pairs
  using Container = std::array<std::optional<value_type>, Capacity>;

  // Iterator types
  class iterator;
  class const_iterator;

  // Constructors
  FlatHashMap() = default;

  // Copy constructor
  FlatHashMap(const FlatHashMap& other) = default;

  // Move constructor
  FlatHashMap(FlatHashMap&& other) noexcept = default;

  // Assignment operators
  FlatHashMap& operator=(const FlatHashMap& other) = default;
  FlatHashMap& operator=(FlatHashMap&& other) noexcept = default;

  // Destructor
  ~FlatHashMap() = default;

  // Capacity
  [[nodiscard]] constexpr size_type size() const noexcept {
    return size_;
  }
  [[nodiscard]] constexpr size_type max_size() const noexcept {
    return Capacity;
  }
  [[nodiscard]] constexpr bool empty() const noexcept {
    return size_ == 0;
  }
  [[nodiscard]] constexpr bool full() const noexcept {
    return size_ == Capacity;
  }
  [[nodiscard]] constexpr double load_factor() const noexcept {
    return static_cast<double>(size_) / Capacity;
  }

  // Element access
  mapped_type& operator[](const key_type& key);
  mapped_type& operator[](key_type&& key);
  mapped_type& at(const key_type& key);
  const mapped_type& at(const key_type& key) const;

  // Modifiers
  std::pair<iterator, bool> insert(const value_type& value);
  std::pair<iterator, bool> insert(value_type&& value);

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args);

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args);

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args);

  size_type erase(const key_type& key);
  iterator erase(const_iterator pos);

  void clear() noexcept;

  // Lookup
  iterator find(const key_type& key);
  const_iterator find(const key_type& key) const;

  bool contains(const key_type& key) const;
  size_type count(const key_type& key) const;

  // Iterators
  iterator begin();
  const_iterator begin() const;
  const_iterator cbegin() const;

  iterator end();
  const_iterator end() const;
  const_iterator cend() const;

  // Direct container access
  Container& container() {
    return container_;
  }
  const Container& container() const {
    return container_;
  }

 private:
  Container container_;
  size_type size_ = 0;
  hasher hash_;
  key_equal equal_;

  // Helper functions
  size_type hash_key(const key_type& key) const;
  size_type find_slot(const key_type& key) const;
  size_type find_empty_slot(const key_type& key) const;

  template <typename K, typename... Args>
  std::pair<iterator, bool> emplace_impl(K&& key, Args&&... args);

  template <typename FirstArg, typename... RestArgs>
  std::pair<iterator, bool> emplace_from_args(FirstArg&& first,
                                              RestArgs&&... rest);
};

// Iterator implementation
template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
class FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::iterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = typename FlatHashMap::value_type;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;

  iterator() = default;
  iterator(Container* container, size_type index)
      : container_(container), index_(index) {
    skip_empty();
  }

  reference operator*() const {
    return container_->at(index_).value();
  }
  pointer operator->() const {
    return &container_->at(index_).value();
  }

  iterator& operator++() {
    ++index_;
    skip_empty();
    return *this;
  }

  iterator operator++(int) {
    iterator tmp = *this;
    ++(*this);
    return tmp;
  }

  bool operator==(const iterator& other) const {
    return container_ == other.container_ && index_ == other.index_;
  }

  bool operator!=(const iterator& other) const {
    return !(*this == other);
  }

  size_type index() const {
    return index_;
  }

 private:
  Container* container_ = nullptr;
  size_type index_ = Capacity;

  void skip_empty() {
    while (index_ < Capacity && !container_->at(index_).has_value()) {
      ++index_;
    }
  }

  friend class FlatHashMap;
};

// Const iterator implementation
template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
class FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::const_iterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = typename FlatHashMap::value_type;
  using difference_type = std::ptrdiff_t;
  using pointer = const value_type*;
  using reference = const value_type&;

  const_iterator() = default;
  const_iterator(const Container* container, size_type index)
      : container_(container), index_(index) {
    skip_empty();
  }

  // Convert from iterator
  const_iterator(const iterator& it)
      : container_(it.container_), index_(it.index_) {}

  reference operator*() const {
    return container_->at(index_).value();
  }
  pointer operator->() const {
    return &container_->at(index_).value();
  }

  const_iterator& operator++() {
    ++index_;
    skip_empty();
    return *this;
  }

  const_iterator operator++(int) {
    const_iterator tmp = *this;
    ++(*this);
    return tmp;
  }

  bool operator==(const const_iterator& other) const {
    return container_ == other.container_ && index_ == other.index_;
  }

  bool operator!=(const const_iterator& other) const {
    return !(*this == other);
  }

  size_type index() const {
    return index_;
  }

 private:
  const Container* container_ = nullptr;
  size_type index_ = Capacity;

  void skip_empty() {
    while (index_ < Capacity && !container_->at(index_).has_value()) {
      ++index_;
    }
  }

  friend class FlatHashMap;
};

// Implementation of member functions

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::size_type
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::hash_key(
    const key_type& key) const {
  return hash_(key) % Capacity;
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::size_type
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::find_slot(
    const key_type& key) const {
  size_type index = hash_key(key);

  // Linear probing
  for (size_type i = 0; i < Capacity; ++i) {
    size_type current_index = (index + i) % Capacity;

    if (!container_[current_index].has_value()) {
      return Capacity;  // Not found
    }

    if (equal_(container_[current_index]->first, key)) {
      return current_index;
    }
  }

  return Capacity;  // Not found
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::size_type
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::find_empty_slot(
    const key_type& key) const {
  size_type index = hash_key(key);

  // Linear probing to find empty slot
  for (size_type i = 0; i < Capacity; ++i) {
    size_type current_index = (index + i) % Capacity;

    if (!container_[current_index].has_value()) {
      return current_index;
    }

    // Check if key already exists
    if (equal_(container_[current_index]->first, key)) {
      return current_index;
    }
  }

  return Capacity;  // No empty slot found
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::mapped_type&
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::operator[](
    const key_type& key) {
  auto result = try_emplace(key);
  return result.first->second;
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::mapped_type&
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::operator[](key_type&& key) {
  auto result = try_emplace(std::move(key));
  return result.first->second;
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::mapped_type&
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::at(const key_type& key) {
  size_type index = find_slot(key);
  if (index == Capacity) {
    throw FlatHashMapError("Key not found");
  }
  return container_[index]->second;
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
const typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::mapped_type&
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::at(
    const key_type& key) const {
  size_type index = find_slot(key);
  if (index == Capacity) {
    throw FlatHashMapError("Key not found");
  }
  return container_[index]->second;
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
std::pair<typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::iterator,
          bool>
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::insert(
    const value_type& value) {
  return emplace(value);
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
std::pair<typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::iterator,
          bool>
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::insert(value_type&& value) {
  return emplace(std::move(value));
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
template <typename... Args>
std::pair<typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::iterator,
          bool>
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::emplace(Args&&... args) {
  // For emplace, we expect either:
  // 1. A pair<Key, Value>
  // 2. Key followed by Value constructor arguments
  if constexpr (sizeof...(args) == 1) {
    // Single argument case - should be a pair
    auto&& arg = std::get<0>(std::forward_as_tuple(args...));
    return emplace_impl(std::forward<decltype(arg.first)>(arg.first),
                        std::forward<decltype(arg.second)>(arg.second));
  } else {
    // Multiple arguments case - first is key, rest are value constructor args
    return emplace_from_args(std::forward<Args>(args)...);
  }
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
template <typename FirstArg, typename... RestArgs>
std::pair<typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::iterator,
          bool>
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::emplace_from_args(
    FirstArg&& first, RestArgs&&... rest) {
  return emplace_impl(std::forward<FirstArg>(first),
                      std::forward<RestArgs>(rest)...);
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
template <typename... Args>
std::pair<typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::iterator,
          bool>
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::try_emplace(
    const key_type& key, Args&&... args) {
  return emplace_impl(key, std::forward<Args>(args)...);
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
template <typename... Args>
std::pair<typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::iterator,
          bool>
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::try_emplace(key_type&& key,
                                                               Args&&... args) {
  return emplace_impl(std::move(key), std::forward<Args>(args)...);
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
template <typename K, typename... Args>
std::pair<typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::iterator,
          bool>
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::emplace_impl(
    K&& key, Args&&... args) {
  size_type index = find_empty_slot(key);

  if (index == Capacity) {
    throw FlatHashMapError("HashMap is full");
  }

  // Check if key already exists
  if (container_[index].has_value() && equal_(container_[index]->first, key)) {
    return {iterator(&container_, index), false};
  }

  // Insert new element
  container_[index] = value_type(std::forward<K>(key),
                                 mapped_type(std::forward<Args>(args)...));
  ++size_;

  return {iterator(&container_, index), true};
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::size_type
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::erase(const key_type& key) {
  size_type index = find_slot(key);
  if (index == Capacity) {
    return 0;
  }

  container_[index].reset();
  --size_;

  // Rehash elements that might have been displaced by linear probing
  size_type next_index = (index + 1) % Capacity;
  while (container_[next_index].has_value()) {
    value_type temp = std::move(*container_[next_index]);
    container_[next_index].reset();
    --size_;

    // Reinsert the element
    emplace_impl(std::move(temp.first), std::move(temp.second));

    next_index = (next_index + 1) % Capacity;
  }

  return 1;
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::iterator
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::erase(const_iterator pos) {
  if (pos.index_ >= Capacity || !container_[pos.index_].has_value()) {
    return end();
  }

  size_type index = pos.index_;
  container_[index].reset();
  --size_;

  // Rehash elements that might have been displaced by linear probing
  size_type next_index = (index + 1) % Capacity;
  while (container_[next_index].has_value()) {
    value_type temp = std::move(*container_[next_index]);
    container_[next_index].reset();
    --size_;

    // Reinsert the element
    emplace_impl(std::move(temp.first), std::move(temp.second));

    next_index = (next_index + 1) % Capacity;
  }

  return iterator(&container_, index);
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
void FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::clear() noexcept {
  for (auto& slot : container_) {
    slot.reset();
  }
  size_ = 0;
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::iterator
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::find(const key_type& key) {
  size_type index = find_slot(key);
  if (index == Capacity) {
    return end();
  }
  return iterator(&container_, index);
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::const_iterator
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::find(
    const key_type& key) const {
  size_type index = find_slot(key);
  if (index == Capacity) {
    return end();
  }
  return const_iterator(&container_, index);
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
bool FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::contains(
    const key_type& key) const {
  return find_slot(key) != Capacity;
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::size_type
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::count(
    const key_type& key) const {
  return contains(key) ? 1 : 0;
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::iterator
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::begin() {
  return iterator(&container_, 0);
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::const_iterator
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::begin() const {
  return const_iterator(&container_, 0);
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::const_iterator
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::cbegin() const {
  return const_iterator(&container_, 0);
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::iterator
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::end() {
  return iterator(&container_, Capacity);
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::const_iterator
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::end() const {
  return const_iterator(&container_, Capacity);
}

template <class Key, class Value, class Hash, class KeyEqual,
          std::size_t Capacity>
typename FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::const_iterator
FlatHashMap<Key, Value, Hash, KeyEqual, Capacity>::cend() const {
  return const_iterator(&container_, Capacity);
}

}  // namespace nova::static_impl