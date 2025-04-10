//
// Created by liuxiang on 2025/4/10.
//

#include <iostream>

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

int main() {

  static_assert(sizeof(nova::SeqLock<int>) % nova::kCacheLineSize == 0,
              "SeqLock<int> size must be a multiple of cache line size");
  static_assert(sizeof(nova::SeqLock<int64_t>) % nova::kCacheLineSize == 0,
                "SeqLock<int64_t> size must be a multiple of cache line size");
  static_assert(sizeof(nova::SeqLock<double>) % nova::kCacheLineSize == 0,
                "SeqLock<double> size must be a multiple of cache line size");

  std::cout << "order_size: " << sizeof(Order) << std::endl;
  std::cout << "seq_order_size: " << sizeof(nova::SeqLock<Order>) << std::endl;
  return 0;
}