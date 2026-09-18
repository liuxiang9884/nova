#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace nova_bench {

// Experimental full-domain int32 set, independently implemented for this
// benchmark. Four occupancy levels split the ordered key into 8+8+8+8 bits.
// The 64K pointer directory shortcuts the first two levels for exact lookup;
// ordered lookup uses all summaries. This is not the author's unpublished code.
class RadixBitmapSet {
 private:
  struct Bitmap256 {
    std::array<uint64_t, 4> words;

    bool Contains(unsigned bit) const noexcept {
      return (words[bit >> 6] & (uint64_t{1} << (bit & 63))) != 0;
    }
    bool Insert(unsigned bit) noexcept {
      auto& word = words[bit >> 6];
      const auto mask = uint64_t{1} << (bit & 63);
      const bool changed = (word & mask) == 0;
      word |= mask;
      return changed;
    }
    bool Erase(unsigned bit) noexcept {
      auto& word = words[bit >> 6];
      const auto mask = uint64_t{1} << (bit & 63);
      const bool changed = (word & mask) != 0;
      word &= ~mask;
      return changed;
    }
    bool Empty() const noexcept {
      return (words[0] | words[1] | words[2] | words[3]) == 0;
    }
    // Return 256 as a sentinel. Guard before shifting, including start == 256.
    unsigned Next(unsigned start) const noexcept {
      if (start >= 256) return 256;
      unsigned index = start >> 6;
      uint64_t word = words[index] & (~uint64_t{0} << (start & 63));
      for (;;) {
        if (word != 0) return index * 64 + std::countr_zero(word);
        if (++index == 4) return 256;
        word = words[index];
      }
    }
  };

  struct Page {
    Bitmap256 blocks{};
    std::array<Bitmap256, 256> leaves;
  };

 public:
  // The size hint is deliberately unused; page allocation/zeroing occurs on
  // insertion and remains inside the benchmark's timed region.
  explicit RadixBitmapSet(size_t = 0) noexcept {}
  RadixBitmapSet(const RadixBitmapSet&) = delete;
  RadixBitmapSet& operator=(const RadixBitmapSet&) = delete;
  RadixBitmapSet(RadixBitmapSet&&) = delete;
  RadixBitmapSet& operator=(RadixBitmapSet&&) = delete;

  bool Insert(int32_t key) {
    const uint32_t ordered = Encode(key);
    const unsigned prefix = ordered >> 16;
    const unsigned block = (ordered >> 8) & 255;
    auto& page = pages_[prefix];
    if (!page) {
      // Allocation is the only throwing operation; publish no summary or size
      // changes until it succeeds. Only the validity bitmap is initialized.
      page.reset(new Page);
      ++page_count_;
      groups_[prefix >> 8].Insert(prefix & 255);
      root_.Insert(prefix >> 8);
    }
    if (!page->blocks.Contains(block)) {
      page->leaves[block] = Bitmap256{};
      page->blocks.Insert(block);
    }
    if (!page->leaves[block].Insert(ordered & 255)) return false;
    ++size_;
    return true;
  }

  bool Contains(int32_t key) const noexcept {
    const uint32_t ordered = Encode(key);
    const auto& page = pages_[ordered >> 16];
    const unsigned block = (ordered >> 8) & 255;
    return page && page->blocks.Contains(block) &&
           page->leaves[block].Contains(ordered & 255);
  }

  bool Erase(int32_t key) noexcept {
    const uint32_t ordered = Encode(key);
    const unsigned prefix = ordered >> 16;
    const unsigned block = (ordered >> 8) & 255;
    auto& page = pages_[prefix];
    if (!page || !page->blocks.Contains(block) ||
        !page->leaves[block].Erase(ordered & 255))
      return false;
    --size_;
    if (page->leaves[block].Empty()) {
      page->blocks.Erase(block);
      if (page->blocks.Empty()) {
        page.reset();
        --page_count_;
        auto& group = groups_[prefix >> 8];
        group.Erase(prefix & 255);
        if (group.Empty()) root_.Erase(prefix >> 8);
      }
    }
    return true;
  }

  size_t Size() const noexcept {
    return size_;
  }

  // Object + live page payload; excludes allocator metadata/retained free
  // pages.
  size_t StorageBytes() const noexcept {
    return sizeof(*this) + page_count_ * sizeof(Page);
  }

  std::optional<int32_t> LowerBound(int32_t key) const noexcept {
    const uint32_t ordered = Encode(key);
    const unsigned prefix = ordered >> 16;
    const auto& page = pages_[prefix];
    if (page) {
      const unsigned block = (ordered >> 8) & 255;
      if (page->blocks.Contains(block)) {
        const unsigned bit = page->leaves[block].Next(ordered & 255);
        if (bit < 256) return Decode((ordered & 0xffffff00u) | bit);
      }
      const unsigned next_block = page->blocks.Next(block + 1);
      if (next_block < 256) {
        return Decode((prefix << 16) | (next_block << 8) |
                      page->leaves[next_block].Next(0));
      }
    }
    const unsigned next_page = NextPage(prefix + 1);
    if (next_page == 65536) return std::nullopt;
    const auto& next = *pages_[next_page];
    const unsigned block = next.blocks.Next(0);
    return Decode((next_page << 16) | (block << 8) |
                  next.leaves[block].Next(0));
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

  Bitmap256 root_{};
  std::array<Bitmap256, 256> groups_{};
  std::array<std::unique_ptr<Page>, 65536> pages_{};
  size_t size_ = 0;
  size_t page_count_ = 0;
};

}  // namespace nova_bench
