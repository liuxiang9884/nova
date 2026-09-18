#include <bit>
#include <cstdint>
#include <random>
#include <set>
#include <type_traits>

#include <gtest/gtest.h>

#include "radix_bitmap_set.hpp"

namespace nova_bench {

void CheckLowerBound(const RadixBitmapSet& actual,
                     const std::set<int32_t>& expected, int32_t key) {
  const auto found = expected.lower_bound(key);
  const auto result = actual.LowerBound(key);
  ASSERT_EQ(result.has_value(), found != expected.end()) << key;
  if (result) EXPECT_EQ(*result, *found) << key;
}

TEST(RadixBitmap, BoundariesAndPageReclamation) {
  RadixBitmapSet set(0);
  const auto empty_bytes = set.StorageBytes();
  std::set<int32_t> reference;
  for (uint32_t base : {0u, 0x7fff0000u, 0x80000000u, 0xffff0000u}) {
    for (uint32_t low : {0u, 1u, 63u, 64u, 127u, 128u, 255u, 256u, 16383u,
                         16384u, 32767u, 32768u, 65534u, 65535u}) {
      const auto key = std::bit_cast<int32_t>(base + low);
      EXPECT_TRUE(set.Insert(key));
      EXPECT_FALSE(set.Insert(key));
      reference.insert(key);
    }
  }
  EXPECT_EQ(set.Size(), reference.size());
  EXPECT_GT(set.StorageBytes(), empty_bytes);
  for (const auto key : reference) {
    EXPECT_TRUE(set.Contains(key));
    CheckLowerBound(set, reference, key);
    if (key != INT32_MIN) CheckLowerBound(set, reference, key - 1);
    if (key != INT32_MAX) CheckLowerBound(set, reference, key + 1);
  }
  for (auto key : reference) EXPECT_TRUE(set.Erase(key));
  EXPECT_EQ(set.Size(), 0u);
  EXPECT_EQ(set.StorageBytes(), empty_bytes);
  EXPECT_FALSE(set.LowerBound(INT32_MIN));
  EXPECT_TRUE(set.Insert(INT32_MAX));
  EXPECT_EQ(set.LowerBound(INT32_MIN), INT32_MAX);
  EXPECT_TRUE(set.Erase(INT32_MAX));
}

TEST(RadixBitmap, DensePageAndEverySummaryBit) {
  RadixBitmapSet set(0);
  const auto empty_bytes = set.StorageBytes();
  // A whole page exercises every leaf bit and every third-level summary bit.
  for (int32_t key = -65536; key < 0; ++key) ASSERT_TRUE(set.Insert(key));
  for (int32_t key = -65536; key < 0; ++key) {
    ASSERT_EQ(set.LowerBound(key), key);
    ASSERT_TRUE(set.Erase(key));
    ASSERT_FALSE(set.Contains(key));
  }
  EXPECT_EQ(set.StorageBytes(), empty_bytes);
  // Exercise every bit at both upper summary levels, with bounded live memory.
  for (uint32_t high = 0; high < 65536; ++high) {
    const auto key = std::bit_cast<int32_t>((high << 16) ^ 0x80000000u);
    ASSERT_TRUE(set.Insert(key));
    ASSERT_EQ(set.LowerBound(INT32_MIN), key);
    ASSERT_TRUE(set.Erase(key));
  }
  EXPECT_EQ(set.Size(), 0u);
  EXPECT_EQ(set.StorageBytes(), empty_bytes);
}

TEST(RadixBitmap, RandomInterleavingAgainstStdSet) {
  RadixBitmapSet set(0);
  const auto empty_bytes = set.StorageBytes();
  std::set<int32_t> reference;
  std::mt19937 rng(20260918);
  // Alternate dense local keys with full-domain sparse keys. Reset between
  // rounds to exercise repeated reclamation without requiring sanitizer GBs.
  for (int round = 0; round < 8; ++round) {
    for (int step = 0; step < 10000; ++step) {
      int32_t key = (rng() & 1) ? static_cast<int32_t>(rng() % 4096) - 2048
                                : std::bit_cast<int32_t>(uint32_t(rng()));
      switch (rng() % 3) {
        case 0:
          EXPECT_EQ(set.Insert(key), reference.insert(key).second);
          break;
        case 1:
          EXPECT_EQ(set.Erase(key), reference.erase(key) != 0);
          break;
        default:
          EXPECT_EQ(set.Contains(key), reference.contains(key));
          break;
      }
      ASSERT_EQ(set.Size(), reference.size());
      CheckLowerBound(set, reference, key);
      CheckLowerBound(set, reference, INT32_MIN);
      CheckLowerBound(set, reference, INT32_MAX);
    }
    for (auto key : reference) ASSERT_TRUE(set.Erase(key));
    reference.clear();
    EXPECT_EQ(set.StorageBytes(), empty_bytes);
  }
}

TEST(RadixBitmap, ReusedPagesAndEmptyLeaves) {
  RadixBitmapSet set;
  const auto empty_bytes = set.StorageBytes();
  for (int cycle = 0; cycle < 4; ++cycle) {
    const int32_t base = (cycle - 2) * 65536;
    // Leave exactly one occupied leaf; probe all other leaves after allocator
    // reuse. This catches stale or uninitialized leaf data becoming visible.
    ASSERT_TRUE(set.Insert(base + 65535));
    for (int32_t low = 0; low < 65535; ++low) {
      ASSERT_FALSE(set.Contains(base + low));
      ASSERT_FALSE(set.Erase(base + low));
      ASSERT_EQ(set.LowerBound(base + low), base + 65535);
    }
    ASSERT_TRUE(set.Erase(base + 65535));
    EXPECT_EQ(set.StorageBytes(), empty_bytes);
    for (int32_t low = 0; low < 65536; low += 64) {
      ASSERT_TRUE(set.Insert(base + low));
    }
    for (int32_t low = 65535; low >= 0; --low) {
      ASSERT_EQ(set.Erase(base + low), (low % 64) == 0);
    }
    EXPECT_EQ(set.Size(), 0u);
    EXPECT_EQ(set.StorageBytes(), empty_bytes);
  }
}

}  // namespace nova_bench
