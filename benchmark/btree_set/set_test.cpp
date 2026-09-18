#include <algorithm>
#include <limits>
#include <set>

#include <gtest/gtest.h>

#include "dataset.hpp"
#include "set_adapters.hpp"

namespace nova_bench {

TEST(Dataset, DistinctDisjointAndSigned) {
  const Dataset data(100'000);
  const std::set<int32_t> keys(data.insert.begin(), data.insert.end());
  ASSERT_EQ(keys.size(), data.insert.size());
  EXPECT_LT(*keys.begin(), 0);
  EXPECT_GT(*keys.rbegin(), 0);
  for (auto key : data.miss) ASSERT_FALSE(keys.contains(key));
  size_t found = 0;
  for (auto key : data.mixed) found += keys.contains(key);
  EXPECT_EQ(found, data.insert.size() / 2);
  EXPECT_EQ(Dataset(100'000).insert, data.insert);
}

template <class Set>
void CheckBoundaries() {
  const std::vector<int32_t> keys{INT32_MIN, INT32_MAX, -1, 0, 1, -2, 2};
  Set set(128);
  for (auto key : keys) EXPECT_FALSE(set.Contains(key));
  for (int pass = 0; pass < 3; ++pass) {
    for (auto key : keys) set.Insert(key);
  }
  for (auto key : keys) EXPECT_TRUE(set.Contains(key));
  EXPECT_FALSE(set.Erase(100));
  for (auto key : keys) {
    EXPECT_TRUE(set.Erase(key));
    EXPECT_FALSE(set.Contains(key));
    EXPECT_FALSE(set.Erase(key));
  }
  set.Insert(INT32_MIN);
  EXPECT_TRUE(set.Contains(INT32_MIN));
  EXPECT_TRUE(set.Erase(INT32_MIN));
}

template <class Set>
void CheckBatches() {
  for (size_t n : {size_t{63}, size_t{64}, size_t{4096}, size_t{100'000}}) {
    const Dataset data(n);
    Set set(n);
    for (auto key : data.insert) set.Insert(key);
    for (auto key : data.hit) ASSERT_TRUE(set.Contains(key)) << n << ' ' << key;
    for (auto key : data.miss)
      ASSERT_FALSE(set.Contains(key)) << n << ' ' << key;
    for (auto key : data.erase) {
      ASSERT_TRUE(set.Erase(key)) << n << ' ' << key;
      ASSERT_FALSE(set.Contains(key)) << n << ' ' << key;
    }
    for (auto key : data.hit)
      ASSERT_FALSE(set.Contains(key)) << n << ' ' << key;
  }
}

template <class Set>
void CheckRebalancing() {
  constexpr int32_t count = 2048;
  Set set(10'000);
  std::set<int32_t> reference;
  // Ordered insertions followed by alternating missing/present erases exercise
  // boundary children, borrowing, merging, and internal separator replacement.
  for (int32_t key = -count; key < count; key += 2) {
    set.Insert(key);
    reference.insert(key);
  }
  for (auto key : {INT32_MIN, INT32_MAX}) {
    set.Insert(key);
    set.Insert(key);
    reference.insert(key);
  }
  std::vector<int32_t> erase;
  for (int32_t key = -count; key < count; ++key) erase.push_back(key);
  erase.push_back(INT32_MIN);
  erase.push_back(INT32_MAX);
  Shuffle(erase, 917);
  for (auto key : erase) {
    ASSERT_EQ(set.Erase(key), reference.erase(key) != 0) << key;
    for (auto probe :
         {INT32_MIN, -2048, -512, -2, 0, 2, 512, 2046, INT32_MAX}) {
      ASSERT_EQ(set.Contains(probe), reference.contains(probe))
          << key << ' ' << probe;
    }
  }
}

#define NOVA_SET_TESTS(Name, Type)         \
  TEST(Name, BoundariesAndDuplicates) {    \
    CheckBoundaries<Type>();               \
  }                                        \
  TEST(Name, RandomBatches) {              \
    CheckBatches<Type>();                  \
  }                                        \
  TEST(Name, RebalancingAndMissingErase) { \
    CheckRebalancing<Type>();              \
  }

NOVA_SET_TESTS(AbslBtree, AbslSet)
NOVA_SET_TESTS(StdSet, StdSet)
#if NOVA_SET_HAS_AVX2
NOVA_SET_TESTS(AuthorBtree, AuthorSet)
#endif
#undef NOVA_SET_TESTS

}  // namespace nova_bench
