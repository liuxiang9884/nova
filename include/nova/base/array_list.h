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

  constexpr size_type capacity() const {
    return N;
  }

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
      nodes_[new_node].next = 0;
      nodes_[new_node].prev = 0;
    } else {
      nodes_[tail_].next = new_node;
      nodes_[new_node].prev = tail_;
      nodes_[new_node].next = 0;
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
      nodes_[new_node].next = 0;
      nodes_[new_node].prev = 0;
    } else {
      nodes_[tail_].next = new_node;
      nodes_[new_node].prev = tail_;
      nodes_[new_node].next = 0;
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
      head_ = 0;
      tail_ = 0;
    } else {
      tail_ = nodes_[old_tail].prev;
      nodes_[tail_].next = 0;
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
    head_ = 0;
    tail_ = 0;

    // 初始化空闲链表，使用0表示无效索引
    for (size_type i = 0; i < N; ++i) {
      nodes_[i].next = (i + 1) % N;               // 循环链接
      nodes_[i].prev = (i == 0) ? N - 1 : i - 1;  // 处理边界情况
    }

    // 设置空闲链表的头
    free_head_ = 0;
  }

  size_type allocate_node() {
    if (free_head_ == 0) {
      throw std::runtime_error("No free nodes available");
    }

    size_type node = free_head_;
    free_head_ = nodes_[free_head_].next;
    return node;
  }

  void deallocate_node(size_type node) {
    nodes_[node].next = free_head_;
    nodes_[node].prev = 0;
    free_head_ = node;
  }

  Node* get_node_at_index(size_type index) {
    if (index >= size_) return nullptr;

    size_type current = head_;
    for (size_type i = 0; i < index; ++i) {
      current = nodes_[current].next;
    }
    return &nodes_[current];
  }

  const Node* get_node_at_index(size_type index) const {
    if (index >= size_) return nullptr;

    size_type current = head_;
    for (size_type i = 0; i < index; ++i) {
      current = nodes_[current].next;
    }
    return &nodes_[current];
  }

 private:
  std::array<Node, N> nodes_;
  size_type size_{0};
  size_type head_{0};
  size_type tail_{0};
  size_type free_head_{0};
};

}  // namespace static_impl

}  // namespace nova