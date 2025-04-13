//
// Created by liuxiang on 2025/4/11.
//

#pragma once

#include <atomic>
#include <cassert>
#include <type_traits>

#include "nova/common/hardware.h"

namespace nova {

template <typename T, std::size_t Capacity>
class SPSCQueue {
 public:
  using ValueType = T;

  static_assert(Capacity >= 2, "Capacity must more than 2");
  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be a power of 2");

  explicit SPSCQueue() : head_(0), tail_(0) {
    assert(alignof(SPSCQueue<T, Capacity>) >= kCacheLineSize);
    assert(reinterpret_cast<char*>(&tail_) - reinterpret_cast<char*>(&head_) >=
           static_cast<size_t>(kCacheLineSize));
  };

  SPSCQueue(SPSCQueue&) = default;

  SPSCQueue(SPSCQueue&&) noexcept : head_(0), tail_(0) {};

  SPSCQueue& operator=(const SPSCQueue&) = default;

  ~SPSCQueue() {
    while (Front()) {
      Pop();
    }
  }

  template <typename... Args>
  void Emplace(Args&&... args) noexcept(
      std::is_nothrow_constructible<T, Args&&...>::value) {
    static_assert(std::is_constructible<T, Args&&...>::value,
                  "T must be constructable with Args&&...");
    const auto current = head_.load(std::memory_order_relaxed);
    auto next = ((current + 1) & (Capacity - 1));
    while (next == tail_.load(std::memory_order_acquire)) {
    };

    new (&slots_[current]) T(std::forward<Args>(args)...);
    head_.store(next, std::memory_order_release);
  }

  template <typename... Args>
  bool TryEmplace(Args&&... args) noexcept(
      std::is_nothrow_constructible<T, Args&&...>::value) {
    static_assert(std::is_constructible<T, Args&&...>::value,
                  "T must be constructable with Args&&...");

    const auto current = head_.load(std::memory_order_relaxed);
    auto next = ((current + 1) & (Capacity - 1));

    if (next != tail_.load(std::memory_order_acquire)) {
      new (&slots_[current]) T(std::forward<Args>(args)...);
      head_.store(next, std::memory_order_release);
      return true;
    }

    return false;
  }

  void Push(const T& val) noexcept(
      std::is_nothrow_copy_constructible<T>::value) {
    static_assert(std::is_copy_constructible<T>::value,
                  "T must be copy constructable");
    Emplace(val);
  }

  template <typename P, typename = typename std::enable_if<
                            std::is_constructible<T, P&&>::value>::type>
  void Push(P&& val) noexcept(std::is_nothrow_constructible<T, P&&>::value) {
    Emplace(std::forward<P>(val));
  }

  bool TryPush(const T& val) noexcept(
      std::is_nothrow_copy_constructible<T>::value) {
    static_assert(std::is_copy_constructible<T>::value,
                  "T must be copy constructable");
    return TryEmplace(val);
  }

  template <typename P, typename = typename std::enable_if<
                            std::is_constructible<T, P&&>::value>::type>
  bool TryPush(P&& val) noexcept(std::is_nothrow_constructible<T, P&&>::value) {
    return TryEmplace(std::forward<P>(val));
  }

  T* Front() noexcept {
    const auto tail = tail_.load(std::memory_order_relaxed);
    if (head_.load(std::memory_order_acquire) == tail) {
      return nullptr;
    }
    return reinterpret_cast<T*>(&slots_[tail]);
  }

  void Pop() noexcept {
    static_assert(std::is_nothrow_destructible<T>::value,
                  "T must be nothrow destructible");
    const auto tail = tail_.load(std::memory_order_relaxed);
    (*reinterpret_cast<T*>(&slots_[tail])).~T();
    auto next = ((tail + 1) & (Capacity - 1));
    tail_.store(next, std::memory_order_release);
  }

  [[nodiscard]] std::size_t size() const noexcept {
    int ret = static_cast<int>(head_.load(std::memory_order_acquire) -
                               tail_.load(std::memory_order_acquire));
    if (ret < 0) {
      ret += Capacity;
    }
    return ret;
  }

  [[nodiscard]] bool IsEmpty() const {
    return size() == 0;
  }

  static constexpr std::size_t capacity() {
    return Capacity;
  }

 private:
  using AtomicIndexType = std::atomic<uint64_t>;
  using StorageType =
      typename std::aligned_storage<sizeof(T), alignof(T)>::type;
  char pad0_[kCacheLineSize] = {0};
  alignas(kCacheLineSize) StorageType slots_[Capacity];
  alignas(kCacheLineSize) AtomicIndexType head_;
  alignas(kCacheLineSize) AtomicIndexType tail_;
  char pad1_[kCacheLineSize - sizeof(AtomicIndexType)] = {0};
};

// a single producer, unblocking bounded broadcast queue
template <typename T, std::size_t Capacity>
class SPBroadcastQueue {
public:
  static_assert(std::is_nothrow_copy_assignable<T>::value ||
                std::is_nothrow_move_assignable<T>::value,
                "T must be nothrow copy or move assignable");

  static_assert(std::is_nothrow_destructible<T>::value,
                "T must be nothrow destructible.");

  static_assert((Capacity >= 2) && ((Capacity & (Capacity - 1)) == 0),
                "Capacity must be a power of 2.");

  SPBroadcastQueue() : current_(0) {}

  template <typename... Args>
  void Emplace(Args&& ... args) noexcept {
    static_assert(std::is_nothrow_constructible<T, Args&& ...>::value,
                  "T must be nothrow constructible with Args&& ...");
    const auto current = current_.load(std::memory_order_acquire);
    auto next = current + 1;
    auto& slot =slots_[Idx(current)];
    new(&slot) T(std::forward<Args>(args)...);
    current_.store(next, std::memory_order_release);
  }

  void Push(const T& val) noexcept {
    static_assert(std::is_nothrow_copy_constructible<T>::value,
                  "T must be nothrow copy constructible");
    Emplace(val);
  }

  [[nodiscard]] uint64_t current() const {
    return current_.load(std::memory_order_acquire);
  }

  T Value(uint64_t i) {
    auto& slot = slots_[Idx(i)];
    return *reinterpret_cast<T*>(&slots_[Idx(i)]);
  }

  T& Ref(uint64_t i) {
    auto& slot = slots_[Idx(i)];
    return *reinterpret_cast<T*>(&slots_[Idx(i)]);
  }

  T Pop(uint64_t& i) {
    auto& slot = slots_[Idx(i)];
    ++i;
    return *reinterpret_cast<T*>(&slot);
  }
  T& PopRef(uint64_t& i) {
    auto& slot = slots_[Idx(i)];
    ++i;
    return *reinterpret_cast<T*>(&slot);
  }

  bool TryPop(uint64_t& i, T& val) {
    if (current() <= i) {
      return false;
    } else {
      val = *reinterpret_cast<T*>(&slots_[Idx(i)]);
      ++i;
      return true;
    }
  }

  bool TryPop(uint64_t& i, T** ptr) {
    if (current() <= i) {
      return false;
    } else {
      *ptr = reinterpret_cast<T*>(&(slots_[Idx(i)]));
      ++i;
      return true;
    }
  }

  [[nodiscard]] constexpr std::size_t Idx(std::size_t i) const noexcept {
    return i & mask_;
  }

  [[nodiscard]] constexpr std::size_t Turn(std::size_t i) const noexcept {
    return i / Capacity;
  }

private:
  using StorageType = typename std::aligned_storage<sizeof(T), alignof(T)>::type;
  std::size_t mask_ = Capacity - 1;
  using AtomicIndexType = std::atomic<uint64_t>;
  char pad0_[kCacheLineSize] = {0};
  alignas(kCacheLineSize) StorageType slots_[Capacity];
  // current: count of enqueue element.
  alignas(kCacheLineSize) AtomicIndexType current_;
  char pad1_[kCacheLineSize - sizeof(AtomicIndexType)] = {0};
};


}  // namespace nova
