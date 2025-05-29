//
// Created by liuxiang on 2025/5/28.
//

#pragma once

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "nova/base/fixed_string.h"
#include "nova/base/flat_hash_map.h"
#include "nova/common/traits.h"
#include <fcntl.h>

namespace nova {

/// @brief Shared memory allocator exception class
class ShmAllocatorError : public std::runtime_error {
 public:
  explicit ShmAllocatorError(const std::string& message)
      : std::runtime_error("ShmAllocator: " + message) {}
};

/// @brief Shared memory allocator
///
/// This class manages a shared memory region, supporting allocation and
/// construction of multiple named instances. Memory layout:
/// [Header][Index(FlatHashMap)][Storage]
///
/// @note This allocator does not support deallocating individual instances,
/// only supports releasing all memory at once
class ShmAllocator {
 public:
  // Size and offset types
  using size_type = std::size_t;
  using ShmOffset = size_type;
  using ShmSize = size_type;

  // Shared memory instance name type
  using ShmName = FixedString<32>;

  // Hash function for ShmName
  using ShmNameHash = FixedStringHash<32>;

  // Comparison function for ShmName
  struct ShmNameEqual {
    bool operator()(const ShmName& lhs, const ShmName& rhs) const {
      return lhs == rhs;
    }
  };

  // Metadata for instances stored in shared memory
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

  // Shared memory header information
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

  // Shared memory block representing an allocated memory region
  class ShmBlock {
   public:
    // Default constructor
    ShmBlock() : ptr_(nullptr) {}

    // Constructor
    explicit ShmBlock(void* ptr) : ptr_(ptr) {}

    // Get memory pointer
    void* data() const {
      return ptr_;
    }

    // Get typed pointer
    template <typename T>
    T* as() const {
      static_assert(is_shm_compatible_v<T>,
                    "Type must be shared memory compatible");
      return static_cast<T*>(ptr_);
    }

    // Check if valid
    bool valid() const {
      return ptr_ != nullptr;
    }

    // Check if empty
    bool empty() const {
      return ptr_ == nullptr;
    }

   private:
    void* ptr_;
  };

  // Index type, using FlatHashMap to store instance metadata
  using IndexType =
      nova::static_impl::ShmFlatHashMap<ShmName, ShmInstanceMeta, 1024,
                                        ShmNameHash, ShmNameEqual>;

  /// @brief Constructor, create or open shared memory
  /// @param name Shared memory name
  /// @param storage_size Storage area size in bytes
  /// @param max_instances Maximum instance count
  /// @param create_if_not_exists Whether to create if not exists
  ShmAllocator(std::string_view name, ShmSize storage_size,
               ShmSize max_instances, bool create_if_not_exists = true);

  /// @brief Destructor, automatically clean up resources
  ~ShmAllocator();

  // Disable copy and move
  ShmAllocator(const ShmAllocator&) = delete;
  ShmAllocator& operator=(const ShmAllocator&) = delete;
  ShmAllocator(ShmAllocator&&) = delete;
  ShmAllocator& operator=(ShmAllocator&&) = delete;

  /// @brief Allocate memory block for specified type
  /// @tparam T Type to allocate
  /// @param name Instance name
  /// @return Pointer to allocated memory, returns existing pointer if already
  /// exists
  template <typename T>
  T* Allocate(std::string_view name);

  /// @brief Construct object with specified name
  /// @tparam T Type to construct
  /// @tparam Args Constructor argument types
  /// @param name Instance name
  /// @param args Constructor arguments
  /// @return Pointer to constructed object, returns existing pointer if already
  /// constructed
  template <typename T, typename... Args>
  T* Construct(std::string_view name, Args&&... args);

  /// @brief Destruct object with specified name
  /// @tparam T Object type
  /// @param name Instance name
  /// @return Whether destruction was successful
  template <typename T>
  bool Destruct(std::string_view name);

  /// @brief Get pointer to object with specified name
  /// @tparam T Object type
  /// @param name Instance name
  /// @return Pointer, returns nullptr if not found
  template <typename T>
  T* Find(std::string_view name) const;

  /// @brief Get memory block with specified name
  /// @param name Instance name
  /// @return Memory block, returns empty block if not found
  ShmBlock GetBlock(std::string_view name) const;

  /// @brief Check if instance with specified name exists
  /// @param name Instance name
  /// @return Whether exists
  bool Exists(std::string_view name) const;

  /// @brief Check if instance with specified name is constructed
  /// @param name Instance name
  /// @return Whether constructed
  bool IsConstructed(std::string_view name) const;

  /// @brief Get all instance names
  /// @return List of instance names
  std::vector<std::string> GetInstanceNames() const;

  /// @brief Get current instance count
  /// @return Current instance count
  ShmSize instance_count() const;

  /// @brief Get maximum instance count
  /// @return Maximum instance count
  ShmSize max_instances() const;

  /// @brief Get total size
  /// @return Total size in bytes
  ShmSize total_size() const;

  /// @brief Get used storage size
  /// @return Used size in bytes
  ShmSize used_storage_size() const;

  /// @brief Get available storage size
  /// @return Available size in bytes
  ShmSize available_storage_size() const;

  /// @brief Get storage area total size
  /// @return Storage area size in bytes
  ShmSize storage_size() const;

  /// @brief Get shared memory name
  /// @return Shared memory name
  std::string_view shm_name() const;

  /// @brief Release all shared memory and delete shared memory object
  /// @note This object becomes unusable after calling this function
  void DeallocateAll();

  /// @brief Check if shared memory is valid
  /// @return Whether valid
  bool Valid() const;

 private:
  // Shared memory name
  std::string shm_name_;
  // Shared memory file descriptor
  int shm_fd_;
  // Shared memory pointer
  void* shm_ptr_;
  // Shared memory size
  ShmSize shm_size_;
  // Header pointer
  ShmHeader* header_;
  // Index pointer
  IndexType* index_;
  // Storage area pointer
  void* storage_;

  /// @brief Initialize shared memory layout
  /// @param storage_size Storage area size
  /// @param max_instances Maximum instance count
  void InitializeLayout(ShmSize storage_size, ShmSize max_instances);

  /// @brief Validate shared memory layout
  void ValidateLayout();

  /// @brief Calculate layout sizes
  /// @param storage_size Desired storage area size
  /// @param max_instances Maximum instance count
  /// @return Size information for each part
  struct LayoutSizes {
    ShmSize header_size;
    ShmSize index_size;
    ShmSize storage_offset;
    ShmSize storage_size;
    ShmSize total_size;  // Total shared memory size needed
  };
  static LayoutSizes CalculateLayoutSizes(ShmSize storage_size,
                                          ShmSize max_instances);

  /// @brief Internal implementation of memory allocation
  /// @param name Instance name
  /// @param size Size
  /// @param alignment Alignment requirement
  /// @return Allocated memory pointer
  void* AllocateImpl(std::string_view name, ShmSize size, ShmSize alignment);

  /// @brief Convert string_view to ShmName
  /// @param name String view
  /// @param shm_name Output ShmName
  static void ToShmName(std::string_view name, ShmName& shm_name);

  /// @brief Convert ShmName to string
  /// @param shm_name ShmName
  /// @return String
  static std::string FromShmName(const ShmName& shm_name);
};

// Template function implementations

template <typename T>
T* ShmAllocator::Allocate(std::string_view name) {
  static_assert(is_shm_compatible_v<T>,
                "Type must be shared memory compatible");

  void* ptr = AllocateImpl(name, sizeof(T), alignof(T));
  return static_cast<T*>(ptr);
}

template <typename T, typename... Args>
T* ShmAllocator::Construct(std::string_view name, Args&&... args) {
  static_assert(is_shm_compatible_v<T>,
                "Type must be shared memory compatible");

  ShmName shm_name;
  ToShmName(name, shm_name);

  // Check if already exists
  auto it = index_->find(shm_name);
  if (it != index_->end()) {
    void* ptr = static_cast<char*>(storage_) + it->second.offset;
    if (it->second.constructed) {
      // Already constructed, return directly
      return static_cast<T*>(ptr);
    } else {
      // Already allocated but not constructed, perform construction
      T* obj_ptr = static_cast<T*>(ptr);
      new (obj_ptr) T(std::forward<Args>(args)...);

      // Update metadata
      ShmInstanceMeta meta = it->second;
      meta.constructed = true;
      index_->erase(shm_name);
      index_->emplace(shm_name, meta);

      return obj_ptr;
    }
  }

  // Allocate new memory and construct
  void* ptr = AllocateImpl(name, sizeof(T), alignof(T));
  T* obj_ptr = static_cast<T*>(ptr);
  new (obj_ptr) T(std::forward<Args>(args)...);

  // Update construction status
  auto find_it = index_->find(shm_name);
  if (find_it != index_->end()) {
    ShmInstanceMeta meta = find_it->second;
    meta.constructed = true;
    index_->erase(shm_name);
    index_->emplace(shm_name, meta);
  }

  return obj_ptr;
}

template <typename T>
bool ShmAllocator::Destruct(std::string_view name) {
  static_assert(is_shm_compatible_v<T>,
                "Type must be shared memory compatible");

  ShmName shm_name;
  ToShmName(name, shm_name);

  auto it = index_->find(shm_name);
  if (it == index_->end() || !it->second.constructed) {
    return false;
  }

  // Call destructor
  void* ptr = static_cast<char*>(storage_) + it->second.offset;
  T* obj_ptr = static_cast<T*>(ptr);
  obj_ptr->~T();

  // Update metadata
  ShmInstanceMeta meta = it->second;
  meta.constructed = false;
  index_->erase(shm_name);
  index_->emplace(shm_name, meta);

  return true;
}

template <typename T>
T* ShmAllocator::Find(std::string_view name) const {
  static_assert(is_shm_compatible_v<T>,
                "Type must be shared memory compatible");

  ShmName shm_name;
  ToShmName(name, shm_name);

  auto it = index_->find(shm_name);
  if (it == index_->end()) {
    return nullptr;
  }

  void* ptr = static_cast<char*>(storage_) + it->second.offset;
  return static_cast<T*>(ptr);
}

// Helper functions

/// @brief Memory alignment helper function
inline ShmAllocator::ShmSize align_up(ShmAllocator::ShmSize size,
                                      ShmAllocator::ShmSize alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

/// @brief Get alignment requirement of type
template <typename T>
constexpr ShmAllocator::ShmSize alignment_of() {
  return alignof(T);
}

/// @brief Get size of type
template <typename T>
constexpr ShmAllocator::ShmSize size_of() {
  return sizeof(T);
}

}  // namespace nova