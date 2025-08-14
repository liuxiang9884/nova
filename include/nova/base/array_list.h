#include <array>
#include <cstddef>
#include <stdexcept>

namespace nova {

namespace static_impl {

template <typename T, size_t N>
class ArrayList {
 public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = std::ptrdiff_t;
  using reference = T&;
  using const_reference = const T&;
  using pointer = T*;
  using const_pointer = const T*;

  static constexpr size_type capacity() noexcept {
    return N;
  }

  static constexpr size_type npos = capacity();

  struct Node {
    T data;
    size_type next;
    size_type prev;

    Node() : data{}, next{0}, prev{0} {}
    Node(const T& val) : data{val}, next{0}, prev{0} {}
    Node(T&& val) : data{std::move(val)}, next{0}, prev{0} {}
  };

  using node_type = Node;

  ArrayList() {
    initialize_free_list();
  }

  ArrayList(const ArrayList& other) {
    initialize_free_list();
    for (size_type i = 0; i < other.size_; ++i) {
      push_back(other.at(i));
    }
  }

  ArrayList(ArrayList&& other) noexcept {
    initialize_free_list();
    for (size_type i = 0; i < other.size_; ++i) {
      push_back(std::move(other.at(i)));
    }
    other.clear();
  }

  ArrayList(std::initializer_list<T> init) {
    initialize_free_list();
    for (const auto& item : init) {
      push_back(item);
    }
  }

  ~ArrayList() = default;

  ArrayList& operator=(const ArrayList& other) {
    if (this != &other) {
      clear();
      for (size_type i = 0; i < other.size_; ++i) {
        push_back(other.at(i));
      }
    }
    return *this;
  }

  ArrayList& operator=(ArrayList&& other) noexcept {
    if (this != &other) {
      clear();
      for (size_type i = 0; i < other.size_; ++i) {
        push_back(std::move(other.at(i)));
      }
      other.clear();
    }
    return *this;
  }

  void push_back(const T& value) {
    if (size_ >= N) {
      throw std::runtime_error("ArrayList is full");
    }

    size_type new_node = allocate_node();
    nodes_[new_node].data = value;

    if (size_ == 0) {
      head_ = new_node;
      tail_ = new_node;
      nodes_[new_node].next = npos;
      nodes_[new_node].prev = npos;
    } else {
      nodes_[tail_].next = new_node;
      nodes_[new_node].prev = tail_;
      nodes_[new_node].next = npos;
      tail_ = new_node;
    }
    size_++;
  }

  void push_back(T&& value) {
    if (size_ >= N) {
      throw std::runtime_error("ArrayList is full");
    }

    size_type new_node = allocate_node();
    nodes_[new_node].data = std::move(value);

    if (size_ == 0) {
      head_ = new_node;
      tail_ = new_node;
      nodes_[new_node].next = npos;
      nodes_[new_node].prev = npos;
    } else {
      nodes_[tail_].next = new_node;
      nodes_[new_node].prev = tail_;
      nodes_[new_node].next = npos;
      tail_ = new_node;
    }
    size_++;
  }

  void pop_back() {
    if (size_ == 0) {
      throw std::runtime_error("ArrayList is empty");
    }

    size_type old_tail = tail_;
    if (size_ == 1) {
      head_ = npos;
      tail_ = npos;
    } else {
      tail_ = nodes_[old_tail].prev;
      nodes_[tail_].next = npos;
    }

    deallocate_node(old_tail);
    size_--;
  }

  void clear() {
    while (size_ > 0) {
      pop_back();
    }
    initialize_free_list();
  }

  size_type size() const {
    return size_;
  }
  bool empty() const {
    return size_ == 0;
  }
  bool full() const {
    return size_ == N;
  }

  reference at(size_type index) {
    if (index >= size_) {
      throw std::out_of_range("Index out of range");
    }
    return get_node_at_index(index)->data;
  }

  const_reference at(size_type index) const {
    if (index >= size_) {
      throw std::out_of_range("Index out of range");
    }
    return get_node_at_index(index)->data;
  }

  reference operator[](size_type index) {
    return at(index);
  }
  const_reference operator[](size_type index) const {
    return at(index);
  }

  reference front() {
    if (empty()) throw std::runtime_error("ArrayList is empty");
    return nodes_[head_].data;
  }
  const_reference front() const {
    if (empty()) throw std::runtime_error("ArrayList is empty");
    return nodes_[head_].data;
  }

  reference back() {
    if (empty()) throw std::runtime_error("ArrayList is empty");
    return nodes_[tail_].data;
  }
  const_reference back() const {
    if (empty()) throw std::runtime_error("ArrayList is empty");
    return nodes_[tail_].data;
  }

 private:
  void initialize_free_list() {
    size_ = 0;
    head_ = npos;
    tail_ = npos;

    nodes_[0].next = 1;
    nodes_[0].prev = npos;

    for (size_type i = 1; i < N - 1; ++i) {
      nodes_[i].next = i + 1;
      nodes_[i].prev = i - 1;
    }

    nodes_[N - 1].next = npos;
    nodes_[N - 1].prev = N - 2;

    free_head_ = 0;
  }

  size_type allocate_node() {
    if (free_head_ == npos) {
      throw std::runtime_error("No free nodes available");
    }

    size_type node = free_head_;
    free_head_ = nodes_[free_head_].next;
    return node;
  }

  void deallocate_node(size_type node) {
    nodes_[node].next = free_head_;
    nodes_[node].prev = npos;
    free_head_ = node;
  }

  Node* get_node_at_index(size_type index) {
    if (index >= size_) return nullptr;

    size_type current = head_;
    for (size_type i = 0; i < index && current != npos; ++i) {
      current = nodes_[current].next;
    }
    return current != npos ? &nodes_[current] : nullptr;
  }

  const Node* get_node_at_index(size_type index) const {
    if (index >= size_) return nullptr;

    size_type current = head_;
    for (size_type i = 0; i < index && current != npos; ++i) {
      current = nodes_[current].next;
    }
    return current != npos ? &nodes_[current] : nullptr;
  }

 private:
  std::array<Node, N> nodes_;
  size_type size_{0};
  size_type head_{npos};
  size_type tail_{npos};
  size_type free_head_{npos};
};

}  // namespace static_impl

}  // namespace nova