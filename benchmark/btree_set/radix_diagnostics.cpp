// Counts actual structural work; this executable does NOT report timings.
#include <cstdint>
#include <iostream>
#include <string>

#include "dataset.hpp"

struct Counters {
  uint64_t pool_allocations = 0, pool_bytes = 0, pool_releases = 0;
  uint64_t page_allocations = 0, page_bytes = 0, page_releases = 0;
  uint64_t leaf_allocations = 0, leaf_bytes = 0, leaf_releases = 0;
  uint64_t moved_bytes = 0, copied_bytes = 0, prefix_updates = 0;
  uint64_t dense_promotions = 0, dense_bytes = 0, dense_releases = 0;
};
static Counters counters;
#define NOVA_RADIX_OBSERVE(name, amount) (counters.name += (amount))
#include "radix_bitmap_set.hpp"
#undef NOVA_RADIX_OBSERVE

void Print(size_t n, const char* operation, size_t storage) {
  std::cout << "{\"n\":" << n << ",\"operation\":\"" << operation
            << "\",\"storage_bytes\":" << storage;
#define FIELD(name) std::cout << ",\"" #name "\":" << counters.name
  FIELD(pool_allocations);
  FIELD(pool_bytes);
  FIELD(pool_releases);
  FIELD(page_allocations);
  FIELD(page_bytes);
  FIELD(page_releases);
  FIELD(leaf_allocations);
  FIELD(leaf_bytes);
  FIELD(leaf_releases);
  FIELD(moved_bytes);
  FIELD(copied_bytes);
  FIELD(prefix_updates);
  FIELD(dense_promotions);
  FIELD(dense_bytes);
  FIELD(dense_releases);
#undef FIELD
  std::cout << "}\n";
}

int main(int argc, char** argv) {
  try {
    if (argc != 2) {
      std::cerr << "Usage: nova_radix_diagnostics N (counts only, no timing)\n";
      return 1;
    }
    const size_t n = std::stoull(argv[1]);
    const nova_bench::Dataset data(n);
    nova_bench::RadixBitmapSet set(n);
    const auto empty_storage = set.StorageBytes();
    counters = {};
    for (auto key : data.insert) {
      if (!set.Insert(key)) return 2;
    }
    if (set.Size() != n) return 3;
    Print(n, "insert", set.StorageBytes());
    counters = {};
    for (auto key : data.erase) {
      if (!set.Erase(key)) return 4;
    }
    if (set.Size() != 0 || set.StorageBytes() != empty_storage) return 5;
    Print(n, "erase", set.StorageBytes());
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 6;
  }
}
