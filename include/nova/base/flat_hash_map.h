#pragma once

#include <array>
#include <bit>
#include <functional>
#include <stdexcept>
#include <tuple>

namespace nova::static_impl {

template <class Key, class Value, std::size_t N = 1024,
          class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
class FlatHashMap {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<Key, Value>;
  using size_type = std::size_t;
  using hasher = Hash;
  using key_equal = KeyEqual;

  // Load factor and capacity calculation
  static constexpr double kLoadFactor = 0.618;
  static constexpr size_type Capacity =
      std::bit_ceil(static_cast<size_type>(N / kLoadFactor));
  static constexpr size_type kCapacityMask = Capacity - 1;

  // Slot structure using value_type + bool
  struct Slot {
    value_type data;
    bool occupied;
  };

  // Container type
  using Container = std::array<Slot, Capacity>;

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
    return N;
  }
  [[nodiscard]] constexpr size_type capacity() const noexcept {
    return Capacity;
  }
  [[nodiscard]] constexpr bool empty() const noexcept {
    return size_ == 0;
  }
  [[nodiscard]] constexpr bool full() const noexcept {
    return size_ == N;
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

  // Helper functions - using bit operations for power-of-2 capacity
  size_type hash_key(const key_type& key) const {
    return hash_(key) & kCapacityMask;  // Optimized % for power of 2
  }

  size_type find_slot(const key_type& key) const;
  size_type find_empty_slot(const key_type& key) const;

  template <typename K, typename... Args>
  std::pair<iterator, bool> emplace_impl(K&& key, Args&&... args);

  template <typename FirstArg, typename... RestArgs>
  std::pair<iterator, bool> emplace_from_args(FirstArg&& first,
                                              RestArgs&&... rest);
};

// Iterator implementation
template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
class FlatHashMap<Key, Value, N, Hash, KeyEqual>::iterator {
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
    return container_->at(index_).data;
  }
  pointer operator->() const {
    return &container_->at(index_).data;
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
    while (index_ < Capacity && !container_->at(index_).occupied) {
      ++index_;
    }
  }

  friend class FlatHashMap;
};

// Const iterator implementation
template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
class FlatHashMap<Key, Value, N, Hash, KeyEqual>::const_iterator {
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
    return container_->at(index_).data;
  }
  pointer operator->() const {
    return &container_->at(index_).data;
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
    while (index_ < Capacity && !container_->at(index_).occupied) {
      ++index_;
    }
  }

  friend class FlatHashMap;
};

// Implementation of member functions

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::size_type
FlatHashMap<Key, Value, N, Hash, KeyEqual>::find_slot(
    const key_type& key) const {
  size_type index = hash_key(key);

  // Linear probing
  for (size_type i = 0; i < Capacity; ++i) {
    size_type current_index = (index + i) % Capacity;

    if (!container_[current_index].occupied) {
      return Capacity;  // Not found
    }

    if (equal_(container_[current_index].data.first, key)) {
      return current_index;
    }
  }

  return Capacity;  // Not found
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::size_type
FlatHashMap<Key, Value, N, Hash, KeyEqual>::find_empty_slot(
    const key_type& key) const {
  size_type index = hash_key(key);

  // Linear probing to find empty slot
  for (size_type i = 0; i < Capacity; ++i) {
    size_type current_index = (index + i) % Capacity;

    if (!container_[current_index].occupied) {
      return current_index;
    }

    // Check if key already exists
    if (equal_(container_[current_index].data.first, key)) {
      return current_index;
    }
  }

  return Capacity;  // No empty slot found
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::mapped_type&
FlatHashMap<Key, Value, N, Hash, KeyEqual>::operator[](const key_type& key) {
  auto result = try_emplace(key);
  return result.first->second;
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::mapped_type&
FlatHashMap<Key, Value, N, Hash, KeyEqual>::operator[](key_type&& key) {
  auto result = try_emplace(std::move(key));
  return result.first->second;
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::mapped_type&
FlatHashMap<Key, Value, N, Hash, KeyEqual>::at(const key_type& key) {
  size_type index = find_slot(key);
  if (index == Capacity) {
    throw std::runtime_error("Key not found");
  }
  return container_[index].data.second;
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
const typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::mapped_type&
FlatHashMap<Key, Value, N, Hash, KeyEqual>::at(const key_type& key) const {
  size_type index = find_slot(key);
  if (index == Capacity) {
    throw std::runtime_error("Key not found");
  }
  return container_[index].data.second;
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
std::pair<typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::iterator, bool>
FlatHashMap<Key, Value, N, Hash, KeyEqual>::insert(const value_type& value) {
  return emplace(value);
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
std::pair<typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::iterator, bool>
FlatHashMap<Key, Value, N, Hash, KeyEqual>::insert(value_type&& value) {
  return emplace(std::move(value));
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
template <typename... Args>
std::pair<typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::iterator, bool>
FlatHashMap<Key, Value, N, Hash, KeyEqual>::emplace(Args&&... args) {
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

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
template <typename FirstArg, typename... RestArgs>
std::pair<typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::iterator, bool>
FlatHashMap<Key, Value, N, Hash, KeyEqual>::emplace_from_args(
    FirstArg&& first, RestArgs&&... rest) {
  return emplace_impl(std::forward<FirstArg>(first),
                      std::forward<RestArgs>(rest)...);
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
template <typename... Args>
std::pair<typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::iterator, bool>
FlatHashMap<Key, Value, N, Hash, KeyEqual>::try_emplace(const key_type& key,
                                                        Args&&... args) {
  return emplace_impl(key, std::forward<Args>(args)...);
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
template <typename... Args>
std::pair<typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::iterator, bool>
FlatHashMap<Key, Value, N, Hash, KeyEqual>::try_emplace(key_type&& key,
                                                        Args&&... args) {
  return emplace_impl(std::move(key), std::forward<Args>(args)...);
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
template <typename K, typename... Args>
std::pair<typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::iterator, bool>
FlatHashMap<Key, Value, N, Hash, KeyEqual>::emplace_impl(K&& key,
                                                         Args&&... args) {
  size_type index = find_empty_slot(key);

  if (index == Capacity) {
    throw std::runtime_error("HashMap is full");
  }

  // Check if key already exists
  if (container_[index].occupied && equal_(container_[index].data.first, key)) {
    return {iterator(&container_, index), false};
  }

  // Insert new element using placement new for zero-copy construction
  new (&container_[index].data) value_type(
      std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)),
      std::forward_as_tuple(std::forward<Args>(args)...));
  container_[index].occupied = true;
  ++size_;

  return {iterator(&container_, index), true};
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::size_type
FlatHashMap<Key, Value, N, Hash, KeyEqual>::erase(const key_type& key) {
  size_type index = find_slot(key);
  if (index == Capacity) {
    return 0;
  }

  container_[index].occupied = false;
  --size_;

  // Rehash elements that might have been displaced by linear probing
  size_type next_index = (index + 1) % Capacity;
  while (container_[next_index].occupied) {
    value_type temp = std::move(container_[next_index].data);
    container_[next_index].occupied = false;
    --size_;

    // Reinsert the element
    emplace_impl(std::move(temp.first), std::move(temp.second));

    next_index = (next_index + 1) % Capacity;
  }

  return 1;
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::iterator
FlatHashMap<Key, Value, N, Hash, KeyEqual>::erase(const_iterator pos) {
  if (pos.index_ >= Capacity || !container_[pos.index_].occupied) {
    return end();
  }

  size_type index = pos.index_;
  container_[index].occupied = false;
  --size_;

  // Rehash elements that might have been displaced by linear probing
  size_type next_index = (index + 1) % Capacity;
  while (container_[next_index].occupied) {
    value_type temp = std::move(container_[next_index].data);
    container_[next_index].occupied = false;
    --size_;

    // Reinsert the element
    emplace_impl(std::move(temp.first), std::move(temp.second));

    next_index = (next_index + 1) % Capacity;
  }

  return iterator(&container_, index);
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
void FlatHashMap<Key, Value, N, Hash, KeyEqual>::clear() noexcept {
  for (auto& slot : container_) {
    slot.occupied = false;
  }
  size_ = 0;
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::iterator
FlatHashMap<Key, Value, N, Hash, KeyEqual>::find(const key_type& key) {
  size_type index = find_slot(key);
  if (index == Capacity) {
    return end();
  }
  return iterator(&container_, index);
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::const_iterator
FlatHashMap<Key, Value, N, Hash, KeyEqual>::find(const key_type& key) const {
  size_type index = find_slot(key);
  if (index == Capacity) {
    return end();
  }
  return const_iterator(&container_, index);
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
bool FlatHashMap<Key, Value, N, Hash, KeyEqual>::contains(
    const key_type& key) const {
  return find_slot(key) != Capacity;
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::size_type
FlatHashMap<Key, Value, N, Hash, KeyEqual>::count(const key_type& key) const {
  return contains(key) ? 1 : 0;
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::iterator
FlatHashMap<Key, Value, N, Hash, KeyEqual>::begin() {
  return iterator(&container_, 0);
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::const_iterator
FlatHashMap<Key, Value, N, Hash, KeyEqual>::begin() const {
  return const_iterator(&container_, 0);
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::const_iterator
FlatHashMap<Key, Value, N, Hash, KeyEqual>::cbegin() const {
  return const_iterator(&container_, 0);
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::iterator
FlatHashMap<Key, Value, N, Hash, KeyEqual>::end() {
  return iterator(&container_, Capacity);
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::const_iterator
FlatHashMap<Key, Value, N, Hash, KeyEqual>::end() const {
  return const_iterator(&container_, Capacity);
}

template <class Key, class Value, std::size_t N, class Hash, class KeyEqual>
typename FlatHashMap<Key, Value, N, Hash, KeyEqual>::const_iterator
FlatHashMap<Key, Value, N, Hash, KeyEqual>::cend() const {
  return const_iterator(&container_, Capacity);
}

// Shared memory compatible type alias with constraints
template <class Key, class Value, std::size_t N = 1024,
          class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using ShmFlatHashMap = FlatHashMap<Key, Value, N, Hash, KeyEqual>;

// Type trait to check if types are shared memory compatible
template <typename T>
constexpr bool is_shm_compatible_v =
    std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T> &&
    !std::is_pointer_v<T>;

// Static assertions for shared memory compatibility
template <class Key, class Value, std::size_t N>
struct ShmFlatHashMapTraits {
  static_assert(is_shm_compatible_v<Key>,
                "Key type must be shared memory compatible (standard layout, "
                "trivially copyable, non-pointer)");
  static_assert(is_shm_compatible_v<Value>,
                "Value type must be shared memory compatible (standard layout, "
                "trivially copyable, non-pointer)");
  static_assert(sizeof(Key) <= 1024,
                "Key type should be reasonably sized for shared memory usage");
  static_assert(
      sizeof(Value) <= 1024,
      "Value type should be reasonably sized for shared memory usage");

  using type = ShmFlatHashMap<Key, Value, N>;

  // Helper to validate at runtime
  static constexpr bool validate() {
    return is_shm_compatible_v<Key> && is_shm_compatible_v<Value>;
  }

  // Memory size calculation for mmap
  static constexpr std::size_t memory_size() {
    return sizeof(ShmFlatHashMap<Key, Value, N>);
  }
};

// Convenience macro for creating shared memory compatible FlatHashMap
#define NOVA_SHM_FLAT_HASH_MAP(Key, Value, N) \
  typename ::nova::static_impl::ShmFlatHashMapTraits<Key, Value, N>::type

}  // namespace nova::static_impl