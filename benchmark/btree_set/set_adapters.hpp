#pragma once

#include <cstddef>
#include <cstdint>
#include <set>

#include <absl/container/btree_set.h>

#include "radix_bitmap_set.hpp"

#if NOVA_SET_HAS_AVX2
#include "author_btree.hpp"
#endif

namespace nova_bench {

template <class Container>
class LibrarySet {
 public:
  explicit LibrarySet(size_t) {}
  void Insert(int32_t key) {
    values_.insert(key);
  }
  bool Contains(int32_t key) const {
    return values_.find(key) != values_.end();
  }
  bool Erase(int32_t key) {
    return values_.erase(key) != 0;
  }

 private:
  Container values_;
};

using AbslSet = LibrarySet<absl::btree_set<int32_t>>;
using StdSet = LibrarySet<std::set<int32_t>>;

#if NOVA_SET_HAS_AVX2
class AuthorSet {
 public:
  explicit AuthorSet(size_t n) : values_(static_cast<int>(n)) {}
  void Insert(int32_t key) {
    values_.insert(key);
  }
  bool Contains(int32_t key) {
    return values_.find(key);
  }
  bool Erase(int32_t key) {
    return values_.remove(key);
  }

 private:
  author::BTree values_;
};
#endif

}  // namespace nova_bench
