//
// Created by liuxiang on 2025/5/28.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

#include "nova/base/fixed_string.h"
#include "nova/common/traits.h"

namespace nova {

/// @brief Shared memory instance name type
using ShmName = FixedString<32>;

/// @brief Hash function for ShmName
using ShmNameHash = FixedStringHash<32>;

/// @brief Comparison function for ShmName
struct ShmNameEqual {
  bool operator()(const ShmName& lhs, const ShmName& rhs) const {
    return lhs == rhs;
  }
};

/// @brief Shared memory offset type
using ShmOffset = std::size_t;

/// @brief Shared memory size type
using ShmSize = std::size_t;

/// @brief Metadata for instances stored in shared memory
struct ShmInstanceMeta {
  // Offset in storage area
  ShmOffset offset;
  // Size in bytes occupied by instance
  ShmSize size;
  // Alignment requirement
  ShmSize alignment;
  // Whether constructed
  bool constructed;

  ShmInstanceMeta() = default;
  ShmInstanceMeta(ShmOffset off, ShmSize sz, ShmSize align, bool ctor = false)
      : offset(off), size(sz), alignment(align), constructed(ctor) {}
};

/// @brief Shared memory header information
struct ShmHeader {
  // Shared memory name
  char name[64];
  // Total size
  ShmSize total_size;
  // Maximum instance count
  ShmSize max_instances;
  // Storage area start offset
  ShmSize storage_offset;
  // Storage area size
  ShmSize storage_size;
  // Currently used storage size
  ShmSize current_storage_used;
  // Version number
  std::uint32_t version;
  // Whether initialized
  bool initialized;

  ShmHeader() = default;
  ShmHeader(const char* shm_name, ShmSize total_sz, ShmSize max_inst,
            ShmSize storage_off, ShmSize storage_sz)
      : total_size(total_sz),
        max_instances(max_inst),
        storage_offset(storage_off),
        storage_size(storage_sz),
        current_storage_used(0),
        version(1),
        initialized(true) {
    std::strncpy(name, shm_name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
  }
};

/// @brief Shared memory block representing an allocated memory region
class ShmBlock {
 public:
  /// @brief Default constructor
  ShmBlock() : ptr_(nullptr) {}

  /// @brief Constructor
  explicit ShmBlock(void* ptr) : ptr_(ptr) {}

  /// @brief Get memory pointer
  void* data() const {
    return ptr_;
  }

  /// @brief Get typed pointer
  template <typename T>
  T* as() const {
    static_assert(is_shm_compatible_v<T>,
                  "Type must be shared memory compatible");
    return static_cast<T*>(ptr_);
  }

  /// @brief Check if valid
  bool valid() const {
    return ptr_ != nullptr;
  }

  /// @brief Check if empty
  bool empty() const {
    return ptr_ == nullptr;
  }

 private:
  void* ptr_;
};

/// @brief Memory alignment helper function
inline ShmSize align_up(ShmSize size, ShmSize alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

/// @brief Get alignment requirement of type
template <typename T>
constexpr ShmSize alignment_of() {
  return alignof(T);
}

/// @brief Get size of type
template <typename T>
constexpr ShmSize size_of() {
  return sizeof(T);
}

}  // namespace nova