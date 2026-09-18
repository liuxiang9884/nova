#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

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

  // Most 64K-key pages hold about 15 occupied words at N=1M. Keep the first
  // sixteen in the page, then grow geometrically. No global pool or constructor
  // reserve moves allocation out of the timed insertion path.
  class Leaves {
   public:
    void Insert(unsigned rank, uint64_t value) {
      if (size_ == capacity_) {
        const unsigned next_capacity = capacity_ * 2;
        auto fresh = std::make_unique_for_overwrite<uint64_t[]>(next_capacity);
        NOVA_RADIX_OBSERVE(leaf_allocations, 1);
        NOVA_RADIX_OBSERVE(leaf_bytes, next_capacity * sizeof(uint64_t));
        NOVA_RADIX_OBSERVE(copied_bytes, size_ * sizeof(uint64_t));
        NOVA_RADIX_OBSERVE(leaf_releases, heap_ ? 1 : 0);
        std::copy_n(Data(), size_, fresh.get());
        heap_ = std::move(fresh);
        capacity_ = static_cast<uint16_t>(next_capacity);
      }
      auto* data = Data();
      NOVA_RADIX_OBSERVE(moved_bytes, (size_ - rank) * sizeof(uint64_t));
      std::memmove(data + rank + 1, data + rank,
                   (size_ - rank) * sizeof(uint64_t));
      data[rank] = value;
      ++size_;
    }
    void Erase(unsigned rank) noexcept {
      auto* data = Data();
      NOVA_RADIX_OBSERVE(moved_bytes, (size_ - rank - 1) * sizeof(uint64_t));
      std::memmove(data + rank, data + rank + 1,
                   (size_ - rank - 1) * sizeof(uint64_t));
      --size_;
    }
    uint64_t& operator[](unsigned rank) noexcept {
      return Data()[rank];
    }
    uint64_t operator[](unsigned rank) const noexcept {
      return Data()[rank];
    }
    unsigned Size() const noexcept {
      return size_;
    }
    void Reset() noexcept {
      NOVA_RADIX_OBSERVE(leaf_releases, heap_ ? 1 : 0);
      heap_.reset();
      size_ = 0;
      capacity_ = 16;
    }
    bool empty() const noexcept {
      return size_ == 0;
    }
    size_t HeapBytes() const noexcept {
      return heap_ ? capacity_ * sizeof(uint64_t) : 0;
    }

   private:
    uint64_t* Data() noexcept {
      return heap_ ? heap_.get() : inline_.data();
    }
    const uint64_t* Data() const noexcept {
      return heap_ ? heap_.get() : inline_.data();
    }
    std::unique_ptr<uint64_t[]> heap_;
    uint16_t size_ = 0;
    uint16_t capacity_ = 16;
    std::array<uint64_t, 16> inline_{};
  };

  struct Page {
    Bitmap<1024> occupied;
    Leaves leaves;
    // Number of occupied leaves preceding each 64-bit summary word.
    std::array<uint16_t, 16> prefix{};
    std::unique_ptr<uint64_t[]> dense;

    uint64_t Word(unsigned block) const noexcept {
      return dense ? dense[block] : leaves[Rank(block)];
    }
    bool Empty() const noexcept {
      return dense ? occupied.Empty() : leaves.empty();
    }
    size_t HeapBytes() const noexcept {
      return dense ? 1024 * sizeof(uint64_t) : leaves.HeapBytes();
    }

    unsigned Rank(unsigned block) const noexcept {
      return prefix[block >> 6] +
             std::popcount(occupied.words[block >> 6] &
                           ((uint64_t{1} << (block & 63)) - 1));
    }
    template <bool Add>
    void UpdatePrefix(unsigned block) noexcept {
      NOVA_RADIX_OBSERVE(prefix_updates, 1);
#if defined(__AVX2__)
      // Compare lane numbers with the changed summary word. True lanes contain
      // -1: subtracting this mask increments those prefixes, adding decrements.
      const auto lanes = _mm256_setr_epi16(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                                           12, 13, 14, 15);
      const auto mask = _mm256_cmpgt_epi16(
          lanes, _mm256_set1_epi16(static_cast<short>(block >> 6)));
      const auto values =
          _mm256_loadu_si256(reinterpret_cast<const __m256i*>(prefix.data()));
      const auto updated =
          Add ? _mm256_sub_epi16(values, mask) : _mm256_add_epi16(values, mask);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(prefix.data()), updated);
#else
      for (unsigned i = (block >> 6) + 1; i < prefix.size(); ++i) {
        if constexpr (Add)
          ++prefix[i];
        else
          --prefix[i];
      }
#endif
    }

    bool Insert(unsigned low) {
      const unsigned block = low >> 6;
      const auto mask = uint64_t{1} << (low & 63);
      if (dense) {
        auto& word = dense[block];
        if ((word & mask) != 0) return false;
        if (word == 0) occupied.Insert(block);
        word |= mask;
        return true;
      }
      const unsigned rank = Rank(block);
      if (!occupied.Contains(block)) {
        if (leaves.Size() == 64) {
          // Conversion stays in Insert's timed path. Allocate and populate
          // before publishing; a failed allocation leaves the page unchanged.
          auto fresh = std::make_unique<uint64_t[]>(1024);
          NOVA_RADIX_OBSERVE(dense_promotions, 1);
          NOVA_RADIX_OBSERVE(dense_bytes, 1024 * sizeof(uint64_t));
          unsigned index = 0;
          for (unsigned bit = occupied.Next(0); bit < 1024;
               bit = occupied.Next(bit + 1)) {
            fresh[bit] = leaves[index++];
          }
          fresh[block] = mask;
          dense = std::move(fresh);
          leaves.Reset();
          occupied.Insert(block);
          // Prefix counts are no longer read for this page in dense mode.
          return true;
        }
        // Leaf growth allocates/copies before publishing its new storage.
        // Publish the validity bit only after allocation/movement succeeds.
        leaves.Insert(rank, mask);
        occupied.Insert(block);
        UpdatePrefix<true>(block);
        return true;
      }
      auto& word = leaves[rank];
      const bool changed = (word & mask) == 0;
      word |= mask;
      return changed;
    }
    bool Contains(unsigned low) const noexcept {
      const unsigned block = low >> 6;
      return occupied.Contains(block) &&
             (Word(block) & (uint64_t{1} << (low & 63))) != 0;
    }
    bool Erase(unsigned low) noexcept {
      const unsigned block = low >> 6;
      if (!occupied.Contains(block)) return false;
      const unsigned rank = dense ? block : Rank(block);
      auto& word = dense ? dense[rank] : leaves[rank];
      const auto mask = uint64_t{1} << (low & 63);
      if ((word & mask) == 0) return false;
      word &= ~mask;
      if (word == 0) {
        occupied.Erase(block);
        if (!dense) {
          leaves.Erase(rank);
          UpdatePrefix<false>(block);
        }
      }
      return true;
    }
    std::optional<unsigned> LowerBound(unsigned low) const noexcept {
      const unsigned block = low >> 6;
      if (occupied.Contains(block)) {
        const auto word = Word(block) & (~uint64_t{0} << (low & 63));
        if (word != 0) return (block << 6) | std::countr_zero(word);
      }
      const unsigned next = occupied.Next(block + 1);
      if (next == 1024) return std::nullopt;
      return (next << 6) | std::countr_zero(Word(next));
    }
  };

  // Slots are initialized only when a page is inserted. Released pages return
  // to this set's free list; their leaf allocations are still freed immediately.
  union Slot {
    Page page;
    Slot* next;
    Slot() noexcept {}  // Leave unused payload untouched.
    ~Slot() {}         // Live Pages are destroyed explicitly by the set.
  };
  static constexpr size_t kSlabPages = 256;

  Page* NewPage() {
    Slot* slot;
    if (free_slot_) {
      slot = free_slot_;
      free_slot_ = slot->next;
    } else {
      if (slabs_.empty() || used_in_last_slab_ == kSlabPages) {
        auto fresh = std::make_unique<Slot[]>(kSlabPages);
        slabs_.push_back(std::move(fresh));
        used_in_last_slab_ = 0;
        NOVA_RADIX_OBSERVE(pool_allocations, 1);
        NOVA_RADIX_OBSERVE(pool_bytes, kSlabPages * sizeof(Slot));
      }
      slot = &slabs_.back()[used_in_last_slab_++];
    }
    return std::construct_at(&slot->page);
  }
  void ReleasePage(Page* page) noexcept {
    std::destroy_at(page);
    auto* slot = reinterpret_cast<Slot*>(page);
    slot->next = free_slot_;
    free_slot_ = slot;
  }
  void ReleaseEmptyPool() noexcept {
    NOVA_RADIX_OBSERVE(pool_releases, slabs_.size());
    std::vector<std::unique_ptr<Slot[]>>{}.swap(slabs_);
    free_slot_ = nullptr;
    used_in_last_slab_ = 0;
  }

 public:
  // No size-hint preallocation: all page/leaf allocation is timed in Insert.
  explicit RadixBitmapSet(size_t = 0) noexcept {}
  ~RadixBitmapSet() {
    for (auto* page : pages_) if (page) std::destroy_at(page);
  }
  RadixBitmapSet(const RadixBitmapSet&) = delete;
  RadixBitmapSet& operator=(const RadixBitmapSet&) = delete;
  RadixBitmapSet(RadixBitmapSet&&) = delete;
  RadixBitmapSet& operator=(RadixBitmapSet&&) = delete;

  bool Insert(int32_t key) {
    const uint32_t ordered = Encode(key);
    const unsigned prefix = ordered >> 16;
    auto& page = pages_[prefix];
    if (!page) {
      auto* fresh = NewPage();
      NOVA_RADIX_OBSERVE(page_allocations, 1);
      NOVA_RADIX_OBSERVE(page_bytes, sizeof(Page));
      fresh->Insert(ordered & 65535);
      page = fresh;
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
      NOVA_RADIX_OBSERVE(leaf_releases,
                         !page->dense && page->leaves.HeapBytes() ? 1 : 0);
      NOVA_RADIX_OBSERVE(dense_releases, page->dense ? 1 : 0);
      ReleasePage(page);
      page = nullptr;
      if (size_ == 0) ReleaseEmptyPool();
      auto& group = groups_[prefix >> 8];
      group.Erase(prefix & 255);
      if (group.Empty()) root_.Erase(prefix >> 8);
    }
    return true;
  }

  size_t Size() const noexcept {
    return size_;
  }

  // Includes all slab slots (live and free), directory capacity and live leaves.
  // Excludes allocator bookkeeping; empty sets release every slab.
  size_t StorageBytes() const noexcept {
    size_t bytes = sizeof(*this) + slabs_.size() * kSlabPages * sizeof(Slot) +
                   slabs_.capacity() * sizeof(std::unique_ptr<Slot[]>);
    for (const auto& page : pages_) {
      if (page) bytes += page->HeapBytes();
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
  std::array<Page*, 65536> pages_{};
  std::vector<std::unique_ptr<Slot[]>> slabs_;
  Slot* free_slot_ = nullptr;
  size_t used_in_last_slab_ = 0;
  size_t size_ = 0;
};

}  // namespace nova_bench

#ifdef NOVA_RADIX_DEFAULT_OBSERVER
#undef NOVA_RADIX_OBSERVE
#undef NOVA_RADIX_DEFAULT_OBSERVER
#endif
