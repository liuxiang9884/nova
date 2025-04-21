//
// Created by liuxiang on 2025/4/14.
//

#pragma once

#include <atomic>
#include <type_traits>

#include "nova/common/hardware.h"

namespace nova {

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
  void Emplace(Args&&... args) noexcept {
    static_assert(std::is_nothrow_constructible<T, Args&&...>::value,
                  "T must be nothrow constructible with Args&& ...");
    const auto current = current_.load(std::memory_order_acquire);
    auto next = current + 1;
    auto& slot = slots_[Idx(current)];
    new (&slot) T(std::forward<Args>(args)...);
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
  using StorageType =
      typename std::aligned_storage<sizeof(T), alignof(T)>::type;
  std::size_t mask_ = Capacity - 1;
  using AtomicIndexType = std::atomic<uint64_t>;
  char pad0_[kCacheLineSize] = {0};
  alignas(kCacheLineSize) StorageType slots_[Capacity];
  // current: count of enqueue element.
  alignas(kCacheLineSize) AtomicIndexType current_;
  char pad1_[kCacheLineSize - sizeof(AtomicIndexType)] = {0};
};

// a multiple producer, unblocking bounded broadcast queue
template <typename T, std::size_t Capacity>
class MPBroadcastQueue {
 public:
  static_assert(std::is_nothrow_copy_assignable<T>::value ||
                    std::is_nothrow_move_assignable<T>::value,
                "T must be nothrow copy or move assignable");

  static_assert(std::is_nothrow_destructible<T>::value,
                "T must be nothrow destructible.");

  static_assert((Capacity >= 2) && ((Capacity & (Capacity - 1)) == 0),
                "Capacity must be a power of 2.");

  MPBroadcastQueue() : current_(0) {}

  template <typename... Args>
  void Emplace(Args&&... args) noexcept {
    static_assert(std::is_nothrow_constructible<T, Args&&...>::value,
                  "T must be nothrow constructible with Args&& ...");
    const auto current = current_.fetch_add(1);
    auto& slot = slots_[Idx(current)];
    slot.Construct(std::forward<Args>(args)...);
    slot.turn.store(Turn(current) + 1, std::memory_order_release);
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
    while (slot.turn.load(std::memory_order_acquire) < Turn(i) + 1);
    return slot.Move();
  }

  T& Ref(uint64_t i) {
    auto& slot = slots_[Idx(i)];
    while (slot.turn.load(std::memory_order_acquire) < Turn(i) + 1);
    return slot.Move();
  }

  T Pop(uint64_t& i) {
    auto& slot = slots_[Idx(i)];
    while (slot.turn.load(std::memory_order_acquire) < Turn(i) + 1);
    ++i;
    return slot.Move();
  }

  T& PopRef(uint64_t& i) {
    auto& slot = slots_[Idx(i)];
    while (slot.turn.load(std::memory_order_acquire) < Turn(i) + 1);
    ++i;
    return slot.Move();
  }

  bool TryPop(uint64_t& i, T& val) {
    auto& slot = slots_[Idx(i)];
    if (slot.turn.load(std::memory_order_acquire) < Turn(i) + 1) {
      return false;
    } else {
      ++i;
      val = slot.Move();
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
  std::size_t mask_ = Capacity - 1;

  struct Slot {
    ~Slot() noexcept {
      Destroy();
    }

    template <typename... Args>
    void Construct(Args&&... args) noexcept {
      static_assert(std::is_nothrow_constructible<T, Args&&...>::value,
                    "T must be nothrow constructible with Args&&...");
      new (&storage) T(std::forward<Args>(args)...);
    }

    void Destroy() noexcept {
      static_assert(std::is_nothrow_destructible<T>::value,
                    "T must be nothrow destructible");
      reinterpret_cast<T*>(&storage)->~T();
    }

    T&& Move() noexcept {
      return reinterpret_cast<T&&>(storage);
    }

    alignas(kCacheLineSize) std::atomic<std::size_t> turn = {0};
    typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
  };

  using AtomicIndexType = std::atomic<uint64_t>;
  char pad0_[kCacheLineSize] = {0};
  Slot slots_[Capacity];
  alignas(kCacheLineSize) AtomicIndexType current_;
  char pad1_[kCacheLineSize - sizeof(AtomicIndexType)] = {0};
};

}  // namespace nova