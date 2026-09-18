#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace nova_bench {

// Experimental four-level radix bitmap: 8+8+10+6 ordered key bits.
// Each page packs only occupied 64-bit leaves, addressed by bitmap rank.
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
    unsigned Rank(unsigned bit) const noexcept {
      unsigned count = 0;
      for (unsigned i = 0; i < (bit >> 6); ++i)
        count += std::popcount(words[i]);
      return count +
             std::popcount(words[bit >> 6] & ((uint64_t{1} << (bit & 63)) - 1));
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

  struct Page {
    Bitmap<1024> occupied;
    std::vector<uint64_t> leaves;

    bool Insert(unsigned low) {
      const unsigned block = low >> 6;
      const auto mask = uint64_t{1} << (low & 63);
      const unsigned rank = occupied.Rank(block);
      if (!occupied.Contains(block)) {
        // vector<uint64_t> insertion has the strong exception guarantee.
        // Publish the validity bit only after allocation/movement succeeds.
        leaves.insert(leaves.begin() + rank, mask);
        occupied.Insert(block);
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
             (leaves[occupied.Rank(block)] & (uint64_t{1} << (low & 63))) != 0;
    }
    bool Erase(unsigned low) noexcept {
      const unsigned block = low >> 6;
      if (!occupied.Contains(block)) return false;
      const unsigned rank = occupied.Rank(block);
      auto& word = leaves[rank];
      const auto mask = uint64_t{1} << (low & 63);
      if ((word & mask) == 0) return false;
      word &= ~mask;
      if (word == 0) {
        leaves.erase(leaves.begin() + rank);
        occupied.Erase(block);
      }
      return true;
    }
    std::optional<unsigned> LowerBound(unsigned low) const noexcept {
      const unsigned block = low >> 6;
      if (occupied.Contains(block)) {
        const auto word =
            leaves[occupied.Rank(block)] & (~uint64_t{0} << (low & 63));
        if (word != 0) return (block << 6) | std::countr_zero(word);
      }
      const unsigned next = occupied.Next(block + 1);
      if (next == 1024) return std::nullopt;
      return (next << 6) | std::countr_zero(leaves[occupied.Rank(next)]);
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
    if (page->leaves.empty()) {
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
      if (page)
        bytes += sizeof(Page) + page->leaves.capacity() * sizeof(uint64_t);
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
