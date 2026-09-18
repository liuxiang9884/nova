#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nova_bench {

// Each step is invertible modulo 2^32: distinct inputs stay distinct.
inline uint32_t Permute(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  return x ^ (x >> 16);
}

inline void Shuffle(std::vector<int32_t>& values, uint32_t seed) {
  std::mt19937 rng(seed);
  for (size_t i = values.size(); i > 1; --i) {
    std::swap(values[i - 1], values[static_cast<size_t>(rng()) % i]);
  }
}

struct Dataset {
  std::vector<int32_t> insert;
  std::vector<int32_t> hit;
  std::vector<int32_t> miss;
  std::vector<int32_t> mixed;
  std::vector<int32_t> erase;

  explicit Dataset(size_t n) {
    if (n == 0 || n > 10'000'000) {
      throw std::invalid_argument("dataset size must be in [1, 10000000]");
    }
    insert.reserve(n);
    miss.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      insert.push_back(
          std::bit_cast<int32_t>(Permute(static_cast<uint32_t>(2 * i))));
      miss.push_back(
          std::bit_cast<int32_t>(Permute(static_cast<uint32_t>(2 * i + 1))));
    }
    Shuffle(insert, 0x18a001u);
    hit = insert;
    Shuffle(hit, 0x18a002u);
    Shuffle(miss, 0x18a003u);
    mixed.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      mixed.push_back(i % 2 == 0 ? hit[i] : miss[i]);
    }
    Shuffle(mixed, 0x18a004u);
    erase = insert;
    Shuffle(erase, 0x18a005u);
  }
};

}  // namespace nova_bench
