#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>

#ifndef NOVA_RADIX_OBSERVE
#define NOVA_RADIX_OBSERVE(name, amount) ((void)0)
#define NOVA_RADIX_DEFAULT_OBSERVER
#endif

namespace nova_bench {

// Experimental four-level radix bitmap: 8+8+10+6 ordered key bits.
// Sparse pages pack occupied 64-bit leaves by rank; dense pages index directly.
// Independently implemented; the author's radix implementation is unpublished.
class RadixBitmapSet {
 private:
  template <unsigned Bits>
  struct Bitmap {
    std::array<uint64_t, Bits / 64> words{};

    bool Contains(unsigned bit) const noexcept {
      return (words[bit >> 6] & (uint64_t{1} << (bit & 63))) != 0;
    }
    void Insert(unsigned bit) noexcept {
      words[bit >> 6] |= uint64_t{1} << (bit & 63);
    }
    void Erase(unsigned bit) noexcept {
      words[bit >> 6] &= ~(uint64_t{1} << (bit & 63));
    }
    bool Empty() const noexcept {
      for (auto word : words)
        if (word != 0) return false;
      return true;
    }
    unsigned Next(unsigned start) const noexcept {
      if (start >= Bits) return Bits;
      unsigned index = start >> 6;
      uint64_t word = words[index] & (~uint64_t{0} << (start & 63));
      for (;;) {
        if (word != 0) return index * 64 + std::countr_zero(word);
        if (++index == words.size()) return Bits;
        word = words[index];
      }
    }
  };

  // Each 64-word group ranks and moves only its own occupied leaves. Two
  // leaves live inline; larger groups own a geometrically grown allocation.
  struct Bucket {
    struct Heap { uint64_t* data; size_t capacity; };
    union Values {
      uint64_t small[2];
      Heap heap;
      Values() : small{} {}
    } values;
    uint64_t occupied = 0;

    unsigned Size() const noexcept { return std::popcount(occupied); }
    unsigned Rank(unsigned bit) const noexcept {
      return std::popcount(occupied & ((uint64_t{1} << bit) - 1));
    }
    uint64_t* Data() noexcept {
      return Size() > 2 ? values.heap.data : values.small;
    }
    const uint64_t* Data() const noexcept {
      return Size() > 2 ? values.heap.data : values.small;
    }
    size_t HeapBytes() const noexcept {
      return Size() > 2 ? values.heap.capacity * sizeof(uint64_t) : 0;
    }
    // Called by the owning sparse Page, or once during dense conversion.
    void Release() noexcept {
      if (Size() > 2) {
        NOVA_RADIX_OBSERVE(leaf_releases, 1);
        delete[] values.heap.data;
      }
    }
    void Insert(unsigned bit, uint64_t word) {
      const unsigned size = Size(), rank = Rank(bit);
      const auto capacity = size > 2 ? values.heap.capacity : 2;
      if (size == capacity) {
        auto fresh = std::make_unique_for_overwrite<uint64_t[]>(capacity * 2);
        NOVA_RADIX_OBSERVE(leaf_allocations, 1);
        NOVA_RADIX_OBSERVE(leaf_bytes, capacity * 2 * sizeof(uint64_t));
        NOVA_RADIX_OBSERVE(copied_bytes, size * sizeof(uint64_t));
        std::copy_n(Data(), rank, fresh.get());
        fresh[rank] = word;
        std::copy_n(Data() + rank, size - rank, fresh.get() + rank + 1);
        Release();
        values.heap = {fresh.release(), capacity * 2};
      } else {
        auto* data = Data();
        NOVA_RADIX_OBSERVE(moved_bytes, (size - rank) * sizeof(uint64_t));
        std::memmove(data + rank + 1, data + rank,
                     (size - rank) * sizeof(uint64_t));
        data[rank] = word;
      }
      occupied |= uint64_t{1} << bit;
    }
    void Erase(unsigned bit) noexcept {
      const unsigned size = Size(), rank = Rank(bit);
      auto* data = Data();
      if (size == 3) {
        // Save both survivors before switching the active union member.
        const auto first = data[rank == 0 ? 1 : 0];
        const auto second = data[rank == 2 ? 1 : 2];
        NOVA_RADIX_OBSERVE(copied_bytes, 2 * sizeof(uint64_t));
        Release();
        values.small[0] = first;
        values.small[1] = second;
      } else {
        NOVA_RADIX_OBSERVE(moved_bytes, (size - rank - 1) * sizeof(uint64_t));
        std::memmove(data + rank, data + rank + 1,
                     (size - rank - 1) * sizeof(uint64_t));
      }
      occupied &= ~(uint64_t{1} << bit);
    }
  };

  struct Page {
    std::array<Bucket, 16> buckets;
    std::unique_ptr<uint64_t[]> dense;
    uint16_t live_words = 0;

    ~Page() {
      if (!dense) for (auto& bucket : buckets) bucket.Release();
    }
    bool Occupied(unsigned block) const noexcept {
      return (buckets[block >> 6].occupied &
              (uint64_t{1} << (block & 63))) != 0;
    }
    uint64_t& Word(unsigned block) noexcept {
      auto& bucket = buckets[block >> 6];
      return dense ? dense[block] : bucket.Data()[bucket.Rank(block & 63)];
    }
    uint64_t Word(unsigned block) const noexcept {
      const auto& bucket = buckets[block >> 6];
      return dense ? dense[block] : bucket.Data()[bucket.Rank(block & 63)];
    }
    bool Empty() const noexcept { return live_words == 0; }
    size_t HeapBytes() const noexcept {
      if (dense) return 1024 * sizeof(uint64_t);
      size_t bytes = 0;
      for (const auto& bucket : buckets) bytes += bucket.HeapBytes();
      return bytes;
    }
    unsigned Next(unsigned start) const noexcept {
      if (start >= 1024) return 1024;
      unsigned group = start >> 6;
      auto bits = buckets[group].occupied & (~uint64_t{0} << (start & 63));
      for (;;) {
        if (bits) return group * 64 + std::countr_zero(bits);
        if (++group == buckets.size()) return 1024;
        bits = buckets[group].occupied;
      }
    }
    bool Insert(unsigned low) {
      const unsigned block = low >> 6;
      const auto mask = uint64_t{1} << (low & 63);
      auto& bucket = buckets[block >> 6];
      const auto leaf_bit = uint64_t{1} << (block & 63);
      if (Occupied(block)) {
        auto& word = Word(block);
        const bool changed = (word & mask) == 0;
        word |= mask;
        return changed;
      }
      if (dense) {
        dense[block] = mask;
        bucket.occupied |= leaf_bit;
      } else if (live_words == 64) {
        // Build before publishing: allocation failure leaves the page intact.
        auto fresh = std::make_unique<uint64_t[]>(1024);
        NOVA_RADIX_OBSERVE(dense_promotions, 1);
        NOVA_RADIX_OBSERVE(dense_bytes, 1024 * sizeof(uint64_t));
        for (unsigned group = 0; group < buckets.size(); ++group) {
          const auto& old = buckets[group];
          auto bits = old.occupied;
          unsigned rank = 0;
          while (bits) {
            fresh[group * 64 + std::countr_zero(bits)] = old.Data()[rank++];
            bits &= bits - 1;
          }
        }
        fresh[block] = mask;
        for (auto& old : buckets) old.Release();
        dense = std::move(fresh);
        bucket.occupied |= leaf_bit;
      } else {
        bucket.Insert(block & 63, mask);
      }
      ++live_words;
      return true;
    }
    bool Contains(unsigned low) const noexcept {
      const unsigned block = low >> 6;
      return Occupied(block) &&
             (Word(block) & (uint64_t{1} << (low & 63))) != 0;
    }
    bool Erase(unsigned low) noexcept {
      const unsigned block = low >> 6;
      if (!Occupied(block)) return false;
      auto& word = Word(block);
      const auto mask = uint64_t{1} << (low & 63);
      if ((word & mask) == 0) return false;
      word &= ~mask;
      if (word == 0) {
        auto& bucket = buckets[block >> 6];
        if (dense) bucket.occupied &= ~(uint64_t{1} << (block & 63));
        else bucket.Erase(block & 63);
        --live_words;
      }
      return true;
    }
    std::optional<unsigned> LowerBound(unsigned low) const noexcept {
      const unsigned block = low >> 6;
      if (Occupied(block)) {
        const auto word = Word(block) & (~uint64_t{0} << (low & 63));
        if (word != 0) return (block << 6) | std::countr_zero(word);
      }
      const unsigned next = Next(block + 1);
      if (next == 1024) return std::nullopt;
      return (next << 6) | std::countr_zero(Word(next));
    }
  };

 public:
  // No size-hint preallocation: all page/leaf allocation is timed in Insert.
  explicit RadixBitmapSet(size_t = 0) noexcept {}
  RadixBitmapSet(const RadixBitmapSet&) = delete;
  RadixBitmapSet& operator=(const RadixBitmapSet&) = delete;
  RadixBitmapSet(RadixBitmapSet&&) = delete;
  RadixBitmapSet& operator=(RadixBitmapSet&&) = delete;

  bool Insert(int32_t key) {
    const uint32_t ordered = Encode(key);
    const unsigned prefix = ordered >> 16;
    auto& page = pages_[prefix];
    if (!page) {
      auto fresh = std::make_unique<Page>();
      NOVA_RADIX_OBSERVE(page_allocations, 1);
      NOVA_RADIX_OBSERVE(page_bytes, sizeof(Page));
      fresh->Insert(ordered & 65535);
      page = std::move(fresh);
      groups_[prefix >> 8].Insert(prefix & 255);
      root_.Insert(prefix >> 8);
    } else if (!page->Insert(ordered & 65535)) {
      return false;
    }
    ++size_;
    return true;
  }

  bool Contains(int32_t key) const noexcept {
    const uint32_t ordered = Encode(key);
    const auto& page = pages_[ordered >> 16];
    return page && page->Contains(ordered & 65535);
  }

  bool Erase(int32_t key) noexcept {
    const uint32_t ordered = Encode(key);
    const unsigned prefix = ordered >> 16;
    auto& page = pages_[prefix];
    if (!page || !page->Erase(ordered & 65535)) return false;
    --size_;
    if (page->Empty()) {
      NOVA_RADIX_OBSERVE(page_releases, 1);
      NOVA_RADIX_OBSERVE(dense_releases, page->dense ? 1 : 0);
      page.reset();
      auto& group = groups_[prefix >> 8];
      group.Erase(prefix & 255);
      if (group.Empty()) root_.Erase(prefix >> 8);
    }
    return true;
  }

  size_t Size() const noexcept {
    return size_;
  }

  // Object + live page payload + leaf capacity, excluding allocator overhead.
  size_t StorageBytes() const noexcept {
    size_t bytes = sizeof(*this);
    for (const auto& page : pages_) {
      if (page) bytes += sizeof(Page) + page->HeapBytes();
    }
    return bytes;
  }

  std::optional<int32_t> LowerBound(int32_t key) const noexcept {
    const uint32_t ordered = Encode(key);
    const unsigned prefix = ordered >> 16;
    const auto& page = pages_[prefix];
    if (page) {
      if (const auto low = page->LowerBound(ordered & 65535))
        return Decode((prefix << 16) | *low);
    }
    const unsigned next_page = NextPage(prefix + 1);
    if (next_page == 65536) return std::nullopt;
    return Decode((next_page << 16) | *pages_[next_page]->LowerBound(0));
  }

 private:
  static uint32_t Encode(int32_t key) noexcept {
    return static_cast<uint32_t>(key) ^ 0x80000000u;
  }
  static int32_t Decode(uint32_t key) noexcept {
    return std::bit_cast<int32_t>(key ^ 0x80000000u);
  }
  unsigned NextPage(unsigned start) const noexcept {
    if (start >= 65536) return 65536;
    const unsigned group = start >> 8;
    const unsigned bit = groups_[group].Next(start & 255);
    if (bit < 256) return (group << 8) | bit;
    const unsigned next_group = root_.Next(group + 1);
    if (next_group == 256) return 65536;
    return (next_group << 8) | groups_[next_group].Next(0);
  }

  Bitmap<256> root_{};
  std::array<Bitmap<256>, 256> groups_{};
  std::array<std::unique_ptr<Page>, 65536> pages_{};
  size_t size_ = 0;
};

}  // namespace nova_bench

#ifdef NOVA_RADIX_DEFAULT_OBSERVER
#undef NOVA_RADIX_OBSERVE
#undef NOVA_RADIX_DEFAULT_OBSERVER
#endif
