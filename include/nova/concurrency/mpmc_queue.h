//
// Created by liuxiang on 2025/4/12.
//

#pragma once


#include <iostream>
#include <atomic>

#include <cassert>

#include "nova/common/hardware.h"

namespace nova {

template <typename T, std::size_t Capacity>
class MPMCQueue {
public:
  static_assert(std::is_nothrow_copy_assignable<T>::value ||
                std::is_nothrow_move_assignable<T>::value,
                "T must be nothrow copy or move assignable");

  static_assert(std::is_nothrow_destructible<T>::value,
                "T must be nothrow destructible.");

  static_assert((Capacity >= 2) && ((Capacity & (Capacity - 1)) == 0),
                "Capacity must be a power of 2.");

  explicit MPMCQueue() : head_(0), tail_(0) {
    static_assert(sizeof(MPMCQueue<T, Capacity>) % kCacheLineSize == 0,
                  "MPMCQueue<T> must be a multiple of cache line size");
    static_assert(sizeof(Slot) % kCacheLineSize == 0,
                  "Slot size must be a multiple of cache line size");
//    assert(reinterpret_cast<size_t>(&(slots_[0])) % kCacheLineSize == 0 &&
//           "slots_ array must be aligned to cache line size");
    assert(reinterpret_cast<char*>(&tail_) -
           reinterpret_cast<char*>(&head_) >=
           static_cast<size_t>(kCacheLineSize) &&
           "head and tail must be a cache line apart to prevent false sharing");
  }

  ~MPMCQueue() noexcept {
    for (std::size_t i = 0; i < Capacity; ++i) {
      slots_[i].~Slot();
    }
  }

  MPMCQueue(const MPMCQueue&) = delete;

  MPMCQueue& operator=(const MPMCQueue&) = delete;

  template <typename... Args>
  void Emplace(Args&& ... args) noexcept {
    static_assert(std::is_nothrow_constructible<T, Args&& ...>::value,
                  "T must be nothrow constructible with Args&& ...");
    const auto head = head_.fetch_add(1);
    auto& slot = slots_[Idx(head)];

    while (Turn(head) * 2 != slot.turn.load(std::memory_order_acquire)) {
    };
    slot.Construct(std::forward<Args>(args)...);
    slot.turn.store(Turn(head) * 2 + 1, std::memory_order_release);
  }

  template <typename... Args>
  bool TryEmplace(Args&& ...args) noexcept {
    static_assert(std::is_nothrow_constructible<T, Args&& ...>::value,
                  "T must be nothrow constructible with Arg&& ...");
    auto head = head_.load(std::memory_order_acquire);
    for (;;) {
      auto& slot = slots_[Idx(head)];
      if (Turn(head) * 2 == slot.turn.load(std::memory_order_acquire)) {
        if (head_.compare_exchange_strong(head, head + 1)) {
          slot.Construct(std::forward<Args>(args)...);
          slot.turn.store(Turn(head) * 2 + 1, std::memory_order_release);
          return true;
        }
      } else {
        const auto previous = head;
        head = head_.load(std::memory_order_acquire);
        if (head == previous) {
          return false;
        }
      }
    }
  }

  void Push(const T& val) noexcept {
    static_assert(std::is_nothrow_copy_constructible<T>::value,
                  "T must be nothrow copy constructible");
    Emplace(val);
  }

  template <typename P,
      typename = typename std::enable_if<
          std::is_nothrow_constructible<T, P&&>::value>::type>
  bool TryPush(P&& val) noexcept {
    return TryEmplace(std::forward<P>(val));
  }


  void Pop(T& val) {
    const auto tail = tail_.fetch_add(1);
    auto& slot = slots_[Idx(tail)];

    while (Turn(tail) * 2 + 1 != slot.turn.load(std::memory_order_acquire)) {
    };
    val = slot.Move();
    slot.Destroy();
    slot.turn.store(Turn(tail) * 2 + 2, std::memory_order_release);
  }

  bool TryPop(T& val) noexcept {
    auto tail = tail_.load(std::memory_order_acquire);
    for (;;) {
      auto& slot = slots_[Idx(tail)];
      if (Turn(tail) * 2 + 1 == slot.turn.load(std::memory_order_acquire)) {
        if (tail_.compare_exchange_strong(tail, tail + 1)) {
          val = slot.Move();
          slot.Destroy();
          slot.turn.store(Turn(tail) * 2 + 2, std::memory_order_release);
          return true;
        }
      } else {
        const auto previous = tail;
        tail = tail_.load(std::memory_order_acquire);
        if (tail == previous) {
          return false;
        }
      }
    }
  }

private:
  std::size_t mask_ = Capacity - 1;

  [[nodiscard]] constexpr std::size_t Idx(std::size_t i) const noexcept {
    return i & mask_;
  }

  [[nodiscard]] constexpr std::size_t Turn(std::size_t i) const noexcept {
    return i / Capacity;
  }

  struct Slot {
    ~Slot() noexcept {
      if (turn & 1) {
        Destroy();
      }
    }

    template <typename... Args>
    void Construct(Args&& ... args) noexcept {
      static_assert(std::is_nothrow_constructible<T, Args&& ...>::value,
                    "T must be nothrow constructible with Args&&...");
      new(&storage) T(std::forward<Args>(args)...);
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

  using AtomicIndexType = std::atomic<uint32_t>;
  char pad0_[kCacheLineSize] = {0};
  Slot slots_[Capacity];
  alignas(kCacheLineSize) AtomicIndexType head_;
  alignas(kCacheLineSize) AtomicIndexType tail_;
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
  void Emplace(Args&& ... args) noexcept {
    static_assert(std::is_nothrow_constructible<T, Args&& ...>::value,
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
    void Construct(Args&& ... args) noexcept {
      static_assert(std::is_nothrow_constructible<T, Args&& ...>::value,
                    "T must be nothrow constructible with Args&&...");
      new(&storage) T(std::forward<Args>(args)...);
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
} // namespace nova
