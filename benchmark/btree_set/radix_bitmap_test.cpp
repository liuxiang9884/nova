#include <algorithm>
#include <bit>
#include <cstdint>
#include <random>
#include <set>
#include <vector>

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

TEST(RadixBitmap, PageSlotsReuseAndAccountForRetainedStorage) {
  RadixBitmapSet set;
  const auto empty = set.StorageBytes();
  auto key = [](uint32_t prefix) {
    return std::bit_cast<int32_t>((prefix << 16) ^ 0x80000000u);
  };
  for (uint32_t p = 0; p < 600; ++p) ASSERT_TRUE(set.Insert(key(p)));
  const auto peak = set.StorageBytes();
  for (uint32_t p = 0; p < 500; ++p) ASSERT_TRUE(set.Erase(key(p)));
  EXPECT_EQ(set.StorageBytes(), peak);  // Unused slab slots are counted.
  for (uint32_t p = 600; p < 1100; ++p) ASSERT_TRUE(set.Insert(key(p)));
  EXPECT_EQ(set.StorageBytes(), peak);  // Released slots need no new slabs.
  EXPECT_EQ(set.Size(), 600u);
  for (uint32_t p = 0; p < 500; ++p) EXPECT_FALSE(set.Contains(key(p)));
  for (uint32_t p = 500; p < 1100; ++p) {
    ASSERT_EQ(set.LowerBound(key(p)), key(p));
    ASSERT_TRUE(set.Erase(key(p)));
  }
  EXPECT_EQ(set.StorageBytes(), empty);
  ASSERT_TRUE(set.Insert(INT32_MAX));  // Pool can restart after full release.
  EXPECT_EQ(set.LowerBound(INT32_MIN), INT32_MAX);
  // Destruction must also destroy a nonempty pool's live Page objects.
}

TEST(RadixBitmap, DestroyNonemptyPoolWithSparseDenseAndFreeSlots) {
  // Sanitizers verify that live heap leaves are freed once and dead slots are
  // not destroyed again when a partially filled pool goes out of scope.
  RadixBitmapSet set;
  for (int32_t page = 0; page < 600; ++page)
    ASSERT_TRUE(set.Insert(page * 65536));
  for (int32_t word = 1; word < 20; ++word)
    ASSERT_TRUE(set.Insert(word * 64));
  for (int32_t word = 1; word < 70; ++word)
    ASSERT_TRUE(set.Insert(65536 + word * 64));
  for (int32_t page = 100; page < 600; ++page)
    ASSERT_TRUE(set.Erase(page * 65536));
  EXPECT_EQ(set.Size(), 188u);
  EXPECT_TRUE(set.Contains(19 * 64));
  EXPECT_TRUE(set.Contains(65536 + 69 * 64));
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

TEST(RadixBitmap, CompactLeafRankGrowthAndMiddleErase) {
  RadixBitmapSet set;
  const auto empty_bytes = set.StorageBytes();
  std::set<int32_t> reference;
  std::vector<int32_t> words;
  for (int32_t word = 0; word < 1024; ++word) words.push_back(word);
  std::mt19937 rng(19);
  std::shuffle(words.begin(), words.end(), rng);
  for (auto word : words) {
    for (int bit : {0, 1, 63}) {
      const int32_t key = -65536 + word * 64 + bit;
      ASSERT_TRUE(set.Insert(key));
      reference.insert(key);
      CheckLowerBound(set, reference, key - 1);
      CheckLowerBound(set, reference, key + 1);
    }
  }
  std::shuffle(words.begin(), words.end(), rng);
  for (auto word : words) {
    for (int bit : {1, 0, 63}) {
      const int32_t key = -65536 + word * 64 + bit;
      ASSERT_TRUE(set.Erase(key));
      reference.erase(key);
      ASSERT_EQ(set.Size(), reference.size());
      ASSERT_FALSE(set.Contains(key));
      CheckLowerBound(set, reference, key);
      CheckLowerBound(set, reference, -65536);
    }
  }
  EXPECT_EQ(set.StorageBytes(), empty_bytes);
}

TEST(RadixBitmap, DensePromotionPreservesKeysAndOrder) {
  RadixBitmapSet set;
  const auto empty_bytes = set.StorageBytes();
  std::set<int32_t> reference;
  // 64 distinct words then another word: cross the promotion threshold while
  // preserving multiple bits per word, signed order, and duplicate semantics.
  for (int32_t word = 1023; word >= 0; word -= 13) {
    for (int bit : {0, 31, 63}) {
      const int32_t key = -65536 + word * 64 + bit;
      ASSERT_TRUE(set.Insert(key));
      ASSERT_FALSE(set.Insert(key));
      reference.insert(key);
      ASSERT_EQ(set.Size(), reference.size());
      for (auto saved : reference) ASSERT_TRUE(set.Contains(saved));
      CheckLowerBound(set, reference, key - 1);
      CheckLowerBound(set, reference, key + 1);
    }
  }
  for (int32_t key = -65536; key < 0; ++key) {
    ASSERT_EQ(set.Contains(key), reference.contains(key));
    CheckLowerBound(set, reference, key);
  }
  const int32_t anchor = *reference.rbegin();
  for (auto key : reference) {
    if (key == anchor) continue;
    ASSERT_TRUE(set.Erase(key));
    ASSERT_FALSE(set.Erase(key));
  }
  ASSERT_EQ(set.Size(), 1u);
  // A promoted page stays dense after shrinking. Reinsert into every emptied
  // leaf: stale compact rank prefixes must never be consulted in dense mode.
  for (int32_t word = 0; word < 1023; ++word) {
    const int32_t key = -65536 + word * 64;
    ASSERT_TRUE(set.Insert(key));
    ASSERT_TRUE(set.Contains(key));
    ASSERT_EQ(set.LowerBound(key), key);
    ASSERT_TRUE(set.Erase(key));
    ASSERT_EQ(set.LowerBound(key), anchor);
  }
  ASSERT_TRUE(set.Erase(anchor));
  EXPECT_EQ(set.Size(), 0u);
  EXPECT_EQ(set.StorageBytes(), empty_bytes);
}

TEST(RadixBitmap, LocalGroupsGrowShrinkAndReuse) {
  RadixBitmapSet set;
  const auto empty_bytes = set.StorageBytes();
  std::mt19937 rng(721);
  for (int group = 0; group < 16; ++group) {
    std::vector<int32_t> keys;
    for (int word : {0, 1, 2, 7, 8, 15, 16, 31, 32, 47, 62, 63}) {
      keys.push_back(-65536 + group * 4096 + word * 64);
      keys.push_back(keys.back() + 63);
    }
    std::shuffle(keys.begin(), keys.end(), rng);
    std::set<int32_t> reference;
    for (int cycle = 0; cycle < 3; ++cycle) {
      for (auto key : keys) {
        ASSERT_TRUE(set.Insert(key));
        reference.insert(key);
        ASSERT_EQ(set.Size(), reference.size());
        CheckLowerBound(set, reference, key - 1);
      }
      std::shuffle(keys.begin(), keys.end(), rng);
      for (auto key : keys) {
        ASSERT_TRUE(set.Erase(key));
        reference.erase(key);
        ASSERT_EQ(set.Size(), reference.size());
        for (auto probe : keys)
          ASSERT_EQ(set.Contains(probe), reference.contains(probe));
        CheckLowerBound(set, reference, key);
      }
      EXPECT_EQ(set.StorageBytes(), empty_bytes);
    }
  }
}

}  // namespace nova_bench
