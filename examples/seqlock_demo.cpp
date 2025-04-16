//
// Created by liuxiang on 2025/4/10.
//

#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

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

    order.Visit([id](const Order& order) {
      std::cout << "Reader " << id
                << " sees order: strategy_id=" << order.strategy_id
                << ", order_id=" << order.order_id << std::endl;
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

    order.Update([&new_order](Order& order) { order = new_order; });

    std::cout << "Writer " << id
              << " updated order: strategy_id=" << new_order.strategy_id
              << ", order_id=" << new_order.order_id << std::endl;
  }
}

int main() {
  static_assert(sizeof(nova::SeqLock<int>) % nova::kCacheLineSize == 0,
                "SeqLock<int> size must be a multiple of cache line size");
  static_assert(sizeof(nova::SeqLock<int64_t>) % nova::kCacheLineSize == 0,
                "SeqLock<int64_t> size must be a multiple of cache line size");
  static_assert(sizeof(nova::SeqLock<double>) % nova::kCacheLineSize == 0,
                "SeqLock<double> size must be a multiple of cache line size");

  std::cout << "order_size: " << sizeof(Order) << std::endl;
  std::cout << "seq_order_size: " << sizeof(nova::SeqLock<Order>) << std::endl;
  std::cout << "seq_int_size: " << sizeof(nova::SeqLock<int32_t>) << std::endl;

  nova::MRSWSeqLock<Order> order;

  Order initial_order;
  initial_order.strategy_id = 0;
  initial_order.order_id = 0;
  order.Store(initial_order);

  std::vector<std::thread> threads;
  threads.reserve(3);

  for (int i = 0; i < 3; ++i) {
    threads.emplace_back(Reader, std::ref(order), i);
  }

  threads.emplace_back(Writer, std::ref(order), 1);

  for (auto& thread : threads) {
    thread.join();
  }

  return 0;
}