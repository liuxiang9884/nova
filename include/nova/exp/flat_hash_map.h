#pragma once

#include <array>
#include <bit>
#include <functional>
#include <memory>
#include <stdexcept>
#include <tuple>

namespace nova::exp {

// CRTP base class containing common FlatHashMap implementation
template <typename Derived, class Key, class Value, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>>
class FlatHashMapBase {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<Key, Value>;
  using size_type = std::size_t;
  using hasher = Hash;
  using key_equal = KeyEqual;

  // Load factor
  static constexpr double kLoadFactor = 0.618;

  // Slot structure
  struct Slot {
    value_type data;
    bool occupied = false;
  };

  // Iterator class
  class iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = FlatHashMapBase::value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    iterator() = default;
    iterator(Slot* container, size_type index, size_type capacity)
        : container_(container), index_(index), capacity_(capacity) {
      skip_empty();
    }

    reference operator*() const {
      return container_[index_].data;
    }
    pointer operator->() const {
      return &container_[index_].data;
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

    [[nodiscard]] size_type index() const {
      return index_;
    }

   private:
    Slot* container_ = nullptr;
    size_type index_ = 0;
    size_type capacity_ = 0;

    void skip_empty() {
      while (index_ < capacity_ && !container_[index_].occupied) {
        ++index_;
      }
    }

    friend class FlatHashMapBase;
  };

  class const_iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = FlatHashMapBase::value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;

    const_iterator() = default;
    const_iterator(const Slot* container, size_type index, size_type capacity)
        : container_(container), index_(index), capacity_(capacity) {
      skip_empty();
    }

    // Convert from iterator
    explicit const_iterator(const iterator& it)
        : container_(it.container_),
          index_(it.index_),
          capacity_(it.capacity_) {}

    reference operator*() const {
      return container_[index_].data;
    }
    pointer operator->() const {
      return &container_[index_].data;
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

    [[nodiscard]] size_type index() const {
      return index_;
    }

   private:
    const Slot* container_ = nullptr;
    size_type index_ = 0;
    size_type capacity_ = 0;

    void skip_empty() {
      while (index_ < capacity_ && !container_[index_].occupied) {
        ++index_;
      }
    }

    friend class FlatHashMapBase;
  };

  // Capacity operations
  [[nodiscard]] size_type size() const noexcept {
    return size_;
  }

  [[nodiscard]] bool empty() const noexcept {
    return size_ == 0;
  }

  [[nodiscard]] double load_factor() const noexcept {
    return static_cast<double>(size_) / derived().capacity();
  }

  // Element access
  mapped_type& operator[](const key_type& key) {
    auto result = try_emplace(key);
    return result.first->second;
  }

  mapped_type& operator[](key_type&& key) {
    auto result = try_emplace(std::move(key));
    return result.first->second;
  }

  mapped_type& at(const key_type& key) {
    size_type index = find_slot(key);
    if (index == derived().capacity()) {
      throw std::runtime_error("Key not found");
    }
    return derived().data()[index].data.second;
  }

  const mapped_type& at(const key_type& key) const {
    size_type index = find_slot(key);
    if (index == derived().capacity()) {
      throw std::runtime_error("Key not found");
    }
    return derived().data()[index].data.second;
  }

  // Modifiers
  std::pair<iterator, bool> insert(const value_type& value) {
    return emplace_impl(value.first, value.second);
  }

  std::pair<iterator, bool> insert(value_type&& value) {
    return emplace_impl(std::move(value.first), std::move(value.second));
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
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

  template <typename K, typename... Args>
  std::pair<iterator, bool> emplace(K&& key, Args&&... args) {
    return emplace_impl(std::forward<K>(key), std::forward<Args>(args)...);
  }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
    return emplace_impl(key, std::forward<Args>(args)...);
  }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
    return emplace_impl(std::move(key), std::forward<Args>(args)...);
  }

  size_type erase(const key_type& key) {
    size_type index = find_slot(key);
    if (index == derived().capacity()) {
      return 0;
    }

    derived().data()[index].occupied = false;
    --size_;

    // Rehash elements that might have been displaced by linear probing
    size_type next_index = (index + 1) & (derived().capacity() - 1);
    while (derived().data()[next_index].occupied) {
      value_type temp = std::move(derived().data()[next_index].data);
      derived().data()[next_index].occupied = false;
      --size_;

      // Reinsert the element
      emplace_impl(std::move(temp.first), std::move(temp.second));

      next_index = (next_index + 1) & (derived().capacity() - 1);
    }

    return 1;
  }

  iterator erase(const_iterator pos) {
    if (pos.index_ >= derived().capacity() ||
        !derived().data()[pos.index_].occupied) {
      return end();
    }

    size_type index = pos.index_;
    derived().data()[index].occupied = false;
    --size_;

    // Rehash elements that might have been displaced by linear probing
    size_type next_index = (index + 1) & (derived().capacity() - 1);
    while (derived().data()[next_index].occupied) {
      value_type temp = std::move(derived().data()[next_index].data);
      derived().data()[next_index].occupied = false;
      --size_;

      // Reinsert the element
      emplace_impl(std::move(temp.first), std::move(temp.second));

      next_index = (next_index + 1) & (derived().capacity() - 1);
    }

    return iterator(derived().data(), index, derived().capacity());
  }

  void clear() noexcept {
    for (size_type i = 0; i < derived().capacity(); ++i) {
      derived().data()[i].occupied = false;
    }
    size_ = 0;
  }

  // Lookup
  iterator find(const key_type& key) {
    return find<key_type>(key);
  }

  const_iterator find(const key_type& key) const {
    return find<key_type>(key);
  }

  template <typename K>
  iterator find(const K& key) {
    size_type index = find_slot(key);
    if (index == derived().capacity()) {
      return end();
    }
    return iterator(derived().data(), index, derived().capacity());
  }

  template <typename K>
  const_iterator find(const K& key) const {
    size_type index = find_slot(key);
    if (index == derived().capacity()) {
      return end();
    }
    return const_iterator(derived().data(), index, derived().capacity());
  }

  bool contains(const key_type& key) const {
    return contains<key_type>(key);
  }

  template <typename K>
  bool contains(const K& key) const {
    return find_slot(key) != derived().capacity();
  }

  size_type count(const key_type& key) const {
    return contains(key) ? 1 : 0;
  }

  // Iterators
  iterator begin() {
    return iterator(derived().data(), 0, derived().capacity());
  }

  const_iterator begin() const {
    return const_iterator(derived().data(), 0, derived().capacity());
  }

  const_iterator cbegin() const {
    return const_iterator(derived().data(), 0, derived().capacity());
  }

  iterator end() {
    return iterator(derived().data(), derived().capacity(),
                    derived().capacity());
  }

  const_iterator end() const {
    return const_iterator(derived().data(), derived().capacity(),
                          derived().capacity());
  }

  const_iterator cend() const {
    return const_iterator(derived().data(), derived().capacity(),
                          derived().capacity());
  }

 protected:
  size_type size_ = 0;
  hasher hash_;
  key_equal equal_;

  // CRTP access
  Derived& derived() {
    return static_cast<Derived&>(*this);
  }
  const Derived& derived() const {
    return static_cast<const Derived&>(*this);
  }

  // Common capacity calculation logic
  static constexpr size_type CalculateCapacityForSize(size_type max_size) {
    if (max_size == 0) return 1;

    // Find the smallest power of 2 greater than max_size
    const size_type n = std::bit_ceil(max_size + 1);

    // If max_size/n < load_factor, use n as capacity
    if (static_cast<double>(max_size) / static_cast<double>(n) < kLoadFactor) {
      return n;
    } else {
      // Otherwise use the original calculation
      return std::bit_ceil(
          static_cast<size_type>(static_cast<double>(max_size) / kLoadFactor));
    }
  }

  // Helper functions
  size_type hash_key(const key_type& key) const {
    return hash_(key) & (derived().capacity() - 1);
  }

  template <typename K>
  size_type hash_key(const K& key) const {
    return hash_(key) & (derived().capacity() - 1);
  }

  size_type find_slot(const key_type& key) const {
    return find_slot<key_type>(key);
  }

  template <typename K>
  size_type find_slot(const K& key) const {
    size_type index = hash_key(key);
    size_type capacity = derived().capacity();

    // Linear probing
    for (size_type i = 0; i < capacity; ++i) {
      size_type current_index = (index + i) & (capacity - 1);

      if (!derived().data()[current_index].occupied) {
        return capacity;  // Not found
      }

      if (equal_(derived().data()[current_index].data.first, key)) {
        return current_index;
      }
    }

    return capacity;  // Not found
  }

  size_type find_empty_slot(const key_type& key) const {
    return find_empty_slot<key_type>(key);
  }

  template <typename K>
  size_type find_empty_slot(const K& key) const {
    size_type index = hash_key(key);
    size_type capacity = derived().capacity();

    // Linear probing to find empty slot
    for (size_type i = 0; i < capacity; ++i) {
      size_type current_index = (index + i) & (capacity - 1);

      if (!derived().data()[current_index].occupied) {
        return current_index;
      }

      // Check if key already exists
      if (equal_(derived().data()[current_index].data.first, key)) {
        return current_index;
      }
    }

    return capacity;  // No empty slot found
  }

  template <typename K, typename... Args>
  std::pair<iterator, bool> emplace_impl(K&& key, Args&&... args) {
    size_type index = find_empty_slot(key);

    if (index == derived().capacity()) {
      throw std::runtime_error("HashMap is full");
    }

    // Check if key already exists
    if (derived().data()[index].occupied &&
        equal_(derived().data()[index].data.first, key)) {
      return {iterator(derived().data(), index, derived().capacity()), false};
    }

    // Insert new element using placement new for zero-copy construction
    new (&derived().data()[index].data) value_type(
        std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)),
        std::forward_as_tuple(std::forward<Args>(args)...));
    derived().data()[index].occupied = true;
    ++size_;

    return {iterator(derived().data(), index, derived().capacity()), true};
  }

  template <typename FirstArg, typename... RestArgs>
  std::pair<iterator, bool> emplace_from_args(FirstArg&& first,
                                              RestArgs&&... rest) {
    return emplace_impl(std::forward<FirstArg>(first),
                        std::forward<RestArgs>(rest)...);
  }
};

// Static version - based on existing FlatHashMap
namespace static_impl {

template <class Key, class Value, std::size_t N = 1024,
          class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
class FlatHashMap
    : public FlatHashMapBase<FlatHashMap<Key, Value, N, Hash, KeyEqual>, Key,
                             Value, Hash, KeyEqual> {
 public:
  using Base = FlatHashMapBase<FlatHashMap, Key, Value, Hash, KeyEqual>;
  using typename Base::const_iterator;
  using typename Base::hasher;
  using typename Base::iterator;
  using typename Base::key_equal;
  using typename Base::key_type;
  using typename Base::mapped_type;
  using typename Base::size_type;
  using typename Base::Slot;
  using typename Base::value_type;

  // Calculate capacity using the same logic as original
  static constexpr size_type CalculateCapacity() {
    return Base::CalculateCapacityForSize(N);
  }

  static constexpr size_type Capacity = CalculateCapacity();

  using Container = std::array<Slot, Capacity>;

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

  // Capacity operations (required by CRTP base)
  [[nodiscard]] constexpr size_type capacity() const noexcept {
    return Capacity;
  }

  [[nodiscard]] constexpr size_type max_size() const noexcept {
    return N;
  }

  [[nodiscard]] constexpr bool full() const noexcept {
    return Base::size() >= N;
  }

  // Container access (required by CRTP base)
  Slot* data() {
    return container_.data();
  }

  const Slot* data() const {
    return container_.data();
  }

  // Direct container access for compatibility
  Container& container() {
    return container_;
  }

  const Container& container() const {
    return container_;
  }

 private:
  Container container_;
};

}  // namespace static_impl

// Dynamic version
template <class Key, class Value, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>>
class FlatHashMap
    : public FlatHashMapBase<FlatHashMap<Key, Value, Hash, KeyEqual>, Key,
                             Value, Hash, KeyEqual> {
 public:
  using Base = FlatHashMapBase<FlatHashMap, Key, Value, Hash, KeyEqual>;
  using typename Base::const_iterator;
  using typename Base::hasher;
  using typename Base::iterator;
  using typename Base::key_equal;
  using typename Base::key_type;
  using typename Base::mapped_type;
  using typename Base::size_type;
  using typename Base::Slot;
  using typename Base::value_type;

  // Constructors
  explicit FlatHashMap(size_type max_size = 1024)
      : max_size_(max_size), capacity_(CalculateCapacity(max_size)) {
    slots_.reset(new Slot[capacity_]);
    // Initialize all slots as unoccupied
    for (size_type i = 0; i < capacity_; ++i) {
      slots_[i].occupied = false;
    }
  }

  // Copy constructor
  FlatHashMap(const FlatHashMap& other)
      : max_size_(other.max_size_), capacity_(other.capacity_) {
    slots_.reset(new Slot[capacity_]);
    for (size_type i = 0; i < capacity_; ++i) {
      slots_[i] = other.slots_[i];
    }
    Base::size_ = other.size();
  }

  // Move constructor
  FlatHashMap(FlatHashMap&& other) noexcept
      : max_size_(other.max_size_),
        capacity_(other.capacity_),
        slots_(std::move(other.slots_)) {
    Base::size_ = other.size();
    other.max_size_ = 0;
    other.capacity_ = 0;
    other.Base::size_ = 0;
  }

  // Assignment operators
  FlatHashMap& operator=(const FlatHashMap& other) {
    if (this != &other) {
      max_size_ = other.max_size_;
      capacity_ = other.capacity_;
      slots_.reset(new Slot[capacity_]);
      for (size_type i = 0; i < capacity_; ++i) {
        slots_[i] = other.slots_[i];
      }
      Base::size_ = other.size();
    }
    return *this;
  }

  FlatHashMap& operator=(FlatHashMap&& other) noexcept {
    if (this != &other) {
      max_size_ = other.max_size_;
      capacity_ = other.capacity_;
      slots_ = std::move(other.slots_);
      Base::size_ = other.size();

      other.max_size_ = 0;
      other.capacity_ = 0;
      other.Base::size_ = 0;
    }
    return *this;
  }

  // Destructor
  ~FlatHashMap() = default;

  // Capacity operations (required by CRTP base)
  [[nodiscard]] size_type capacity() const noexcept {
    return capacity_;
  }

  [[nodiscard]] size_type max_size() const noexcept {
    return max_size_;
  }

  [[nodiscard]] bool full() const noexcept {
    return Base::size() >= max_size_;
  }

  // Container access (required by CRTP base)
  Slot* data() {
    return slots_.get();
  }

  const Slot* data() const {
    return slots_.get();
  }

 private:
  size_type max_size_{0};
  size_type capacity_{0};
  std::unique_ptr<Slot[]> slots_;

  // Calculate capacity using the same logic as static version
  static size_type CalculateCapacity(size_type max_size) {
    return Base::CalculateCapacityForSize(max_size);
  }
};

}  // namespace nova::exp