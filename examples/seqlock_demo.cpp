//
// Created by liuxiang on 2025/4/10.
//

#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <fmt/core.h>
#include <fmt/format.h>

#include "nova/concurrency/seqlock.h"

struct Order {
  uint16_t strategy_id;
  uint16_t order_id;
  double price;
  uint32_t quantity;
  uint8_t side;
  uint8_t order_type;
  uint64_t create_time;
  uint64_t update_time;
  uint64_t expire_time;
  uint64_t match_time;
  uint64_t cancel_time;
  uint64_t finish_time;
  uint64_t reject_time;
};

void Reader(const nova::MRSWSeqLock<Order>& order, int id) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1, 100);

  for (int i = 0; i < 5; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));

    order.Visit([id](const Order& order) noexcept {
      fmt::println("Reader {} sees order: strategy_id={}, order_id={}", id,
                   order.strategy_id, order.order_id);
    });
  }
}

void Writer(nova::MRSWSeqLock<Order>& order, int id) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1, 100);

  for (int i = 0; i < 3; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));

    Order new_order{};
    new_order.strategy_id = id;
    new_order.order_id = i + 1;

    order.Update([&new_order](Order& order) noexcept { order = new_order; });

    fmt::println("Writer {} updated order: strategy_id={}, order_id={}", id,
                 new_order.strategy_id, new_order.order_id);
  }
}

template <template <typename> class SeqLockType>
void SeqLockDemo(const std::string& lock_type_name) {
  fmt::println("\n=== {} Demo ===", lock_type_name);

  SeqLockType<Order> order;

  Order initial_order{};
  initial_order.strategy_id = 100;
  initial_order.order_id = 1000;
  initial_order.price = 123.45;
  initial_order.quantity = 500;
  order.Store(initial_order);

  double price = order.Visit(
      [](const Order& order) noexcept -> double { return order.price; });
  fmt::println("Current order price: {}", price);

  struct OrderSummary {
    uint16_t strategy_id;
    uint16_t order_id;
    double price;
    uint32_t quantity;
  };

  OrderSummary summary =
      order.Visit([](const Order& order) noexcept -> OrderSummary {
        return OrderSummary{order.strategy_id, order.order_id, order.price,
                            order.quantity};
      });

  fmt::println("Order info: ID={}.{}, Price={}, Quantity={}",
               summary.strategy_id, summary.order_id, summary.price,
               summary.quantity);

  double old_price = order.Update([](Order& order) noexcept -> double {
    double old_price = order.price;
    order.price = 135.79;
    return old_price;
  });
  fmt::println("Previous price: {}", old_price);

  bool update_success = order.Update([](Order& order) noexcept -> bool {
    if (order.quantity < 1000) {
      order.quantity = 1000;
      return true;
    }
    return false;
  });
  fmt::println("Quantity update successful: {}", update_success ? "yes" : "no");

  Order current_order = order.Load();
  fmt::println("Final order: ID={}.{}, Price={}, Quantity={}",
               current_order.strategy_id, current_order.order_id,
               current_order.price, current_order.quantity);

  fmt::println("\n=== Multi-threaded Test for {} ===", lock_type_name);

  SeqLockType<Order> shared_order;

  initial_order = Order{};
  initial_order.strategy_id = 200;
  initial_order.order_id = 2000;
  initial_order.price = 99.99;
  initial_order.quantity = 100;
  shared_order.Store(initial_order);

  auto reader_func = [](const SeqLockType<Order>& order, int id) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10, 50);

    for (int i = 0; i < 3; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));

      double price = order.Visit(
          [](const Order& o) noexcept -> double { return o.price; });

      fmt::println("Reader {} reads price: {:.2f}", id, price);

      double total_value = order.Visit([](const Order& o) noexcept -> double {
        return o.price * o.quantity;
      });

      fmt::println("Reader {} calculates total value: {:.2f}", id, total_value);
    }
  };

  auto writer_func = [](SeqLockType<Order>& order, int id) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100, 200);
    std::uniform_real_distribution<> price_dis(100.0, 200.0);

    for (int i = 0; i < 3; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));

      double old_price =
          order.Update([&price_dis, &gen](Order& o) noexcept -> double {
            double prev = o.price;
            o.price = price_dis(gen);
            return prev;
          });

      double new_price = order.Visit(
          [](const Order& o) noexcept -> double { return o.price; });

      fmt::println("Writer {} updates price: {:.2f} -> {:.2f}", id, old_price,
                   new_price);

      uint32_t old_qty =
          order.Update([&dis, &gen](Order& o) noexcept -> uint32_t {
            uint32_t prev = o.quantity;
            o.quantity = dis(gen);
            return prev;
          });

      fmt::println("Writer {} updates quantity: {} -> {}", id, old_qty,
                   order.Visit([](const Order& o) noexcept -> uint32_t {
                     return o.quantity;
                   }));
    }
  };

  std::vector<std::thread> mt_threads;
  mt_threads.reserve(4);

  for (int i = 0; i < 3; ++i) {
    mt_threads.emplace_back(reader_func, std::ref(shared_order), i);
  }

  mt_threads.emplace_back(writer_func, std::ref(shared_order), 0);

  for (auto& t : mt_threads) {
    t.join();
  }

  current_order = shared_order.Load();
  fmt::println("\nFinal shared order: ID={}.{}, Price={:.2f}, Quantity={}",
               current_order.strategy_id, current_order.order_id,
               current_order.price, current_order.quantity);
}

void CheckSeqLockAlignas() {
  static_assert(sizeof(nova::MRSWSeqLock<int>) % nova::kCacheLineSize == 0,
                "MRSWSeqLock<int> size must be a multiple of cache line size");
  static_assert(
      sizeof(nova::MRSWSeqLock<int64_t>) % nova::kCacheLineSize == 0,
      "MRSWSeqLock<int64_t> size must be a multiple of cache line size");
  static_assert(
      sizeof(nova::MRSWSeqLock<double>) % nova::kCacheLineSize == 0,
      "MRSWSeqLock<double> size must be a multiple of cache line size");

  static_assert(
      sizeof(nova::DoubleBufferMRSWSeqLock<int>) % nova::kCacheLineSize == 0,
      "DoubleBufferMRSWSeqLock<int> size must be a multiple of cache line "
      "size");
  static_assert(
      sizeof(nova::DoubleBufferMRSWSeqLock<int64_t>) % nova::kCacheLineSize ==
          0,
      "DoubleBufferMRSWSeqLock<int64_t> size must be a multiple of cache line "
      "size");
  static_assert(
      sizeof(nova::DoubleBufferMRSWSeqLock<double>) % nova::kCacheLineSize == 0,
      "DoubleBufferMRSWSeqLock<double> size must be a multiple of cache line "
      "size");

  fmt::println("order_size: {}", sizeof(Order));
  fmt::println("MRSWSeqLock<Order> size: {}", sizeof(nova::MRSWSeqLock<Order>));
  fmt::println("DoubleBufferMRSWSeqLock<Order> size: {}",
               sizeof(nova::DoubleBufferMRSWSeqLock<Order>));

  const std::array<nova::DoubleBufferMRSWSeqLock<Order>, 8> orders{};
  const auto ptr1 = reinterpret_cast<const uint8_t*>(orders[3].buffers());
  const auto ptr2 = reinterpret_cast<const uint8_t*>(&orders[6].seq());
  fmt::println("orders size: {}", sizeof(orders));
  fmt::println(
      "address distance between order[6].seq() and order[3].buffers() : {}",
      ptr2 - ptr1);
}

int main() {
  CheckSeqLockAlignas();
  // Run demo with both SeqLock implementations
  SeqLockDemo<nova::MRSWSeqLock>("MRSWSeqLock");
  SeqLockDemo<nova::DoubleBufferMRSWSeqLock>("DoubleBufferMRSWSeqLock");

  return 0;
}