//
// Created by liuxiang on 2025/5/28.
//

#pragma once

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

#include "nova/base/fixed_string.h"
#include "nova/base/flat_hash_map.h"
#include "nova/common/traits.h"

namespace nova {

static constexpr std::size_t kShmNameSize = 32;

/// @brief Shared memory allocator exception class
class ShmAllocatorError final : public std::runtime_error {
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

  // Shared memory instance name type
  using ShmName = FixedString<kShmNameSize>;

  // Hash function for ShmName
  using ShmNameHash = FixedStringHash<kShmNameSize>;

  // Comparison function for ShmName
  struct ShmNameEqual {
    bool operator()(const ShmName& lhs, const ShmName& rhs) const {
      return lhs == rhs;
    }

    // Support for heterogeneous lookup with std::string_view
    bool operator()(const ShmName& lhs, std::string_view rhs) const {
      return lhs.view() == rhs;
    }

    bool operator()(std::string_view lhs, const ShmName& rhs) const {
      return lhs == rhs.view();
    }

    bool operator()(std::string_view lhs, std::string_view rhs) const {
      return lhs == rhs;
    }
  };

  // Metadata for instances stored in shared memory
  struct ShmInstanceMeta {
    ShmInstanceMeta() = default;
    ShmInstanceMeta(size_type off, size_type sz, size_type align,
                    bool ctor = false)
        : offset(off), size(sz), alignment(align), constructed(ctor) {}

    // Offset in storage area
    size_type offset;
    // Size in bytes occupied by instance
    size_type size;
    // Alignment requirement
    size_type alignment;
    // Whether constructed
    bool constructed;
  };

  // Shared memory header information
  struct ShmHeader {
    ShmHeader() = default;
    ShmHeader(const char* shm_name, size_type total_sz, size_type storage_off,
              size_type storage_sz)
        : name{},
          total_size(total_sz),
          storage_offset(storage_off),
          storage_size(storage_sz),
          current_storage_used(0),
          initialized(true) {
      std::strncpy(name, shm_name, sizeof(name) - 1);
      name[sizeof(name) - 1] = '\0';
    }
    // Shared memory name
    char name[64];
    // Total size
    size_type total_size;
    // Storage area start offset
    size_type storage_offset;
    // Storage area size
    size_type storage_size;
    // Currently used storage size
    size_type current_storage_used;
    // Whether initialized
    bool initialized;
  };

  static constexpr size_type kMaxInstances = 1024;

  // Index type, using FlatHashMap to store instance metadata
  using IndexType =
      nova::static_impl::ShmFlatHashMap<ShmName, ShmInstanceMeta, kMaxInstances,
                                        ShmNameHash, ShmNameEqual>;

  /// @brief Constructor, create or open shared memory
  /// @param name Shared memory name
  /// @param storage_size Storage area size in bytes
  /// @param create_if_not_exists Whether to create if not exists
  ShmAllocator(const char* name, size_type storage_size,
               bool create_if_not_exists = true);

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
  template <typename T>
  void Destruct(std::string_view name);

  /// @brief Get pointer to object with specified name
  /// @tparam T Object type
  /// @param name Instance name
  /// @return Pointer, returns nullptr if not found
  template <typename T>
  T* Find(std::string_view name) const;

  /// @brief Get memory block with specified name
  /// @param name Instance name
  /// @return Memory block, returns nullptr if not found
  [[nodiscard]] void* GetBlock(std::string_view name) const;

  /// @brief Get typed pointer to object with specified name
  /// @tparam T Object type
  /// @param name Instance name
  /// @return Typed pointer, returns nullptr if not found
  template <typename T>
  [[nodiscard]] T* Get(std::string_view name) const;

  /// @brief Check if instance with specified name exists
  /// @param name Instance name
  /// @return Whether exists
  [[nodiscard]] bool Contains(std::string_view name) const;

  /// @brief Check if instance with specified name is constructed
  /// @param name Instance name
  /// @return Whether constructed
  [[nodiscard]] bool IsConstructed(std::string_view name) const;

  /// @brief Get all instance names
  /// @return List of instance names as string views to shared memory
  [[nodiscard]] std::vector<std::string_view> GetInstanceNames() const;

  /// @brief Get current instance count
  /// @return Current instance count
  [[nodiscard]] size_type instance_count() const;

  /// @brief Get maximum instance count
  /// @return Maximum instance count
  [[nodiscard]] size_type max_instances() const;

  /// @brief Get total size
  /// @return Total size in bytes
  [[nodiscard]] size_type total_size() const;

  /// @brief Get used storage size
  /// @return Used size in bytes
  [[nodiscard]] size_type used_storage_size() const;

  /// @brief Get available storage size
  /// @return Available size in bytes
  [[nodiscard]] size_type available_storage_size() const;

  /// @brief Get storage area total size
  /// @return Storage area size in bytes
  [[nodiscard]] size_type storage_size() const;

  /// @brief Get shared memory name
  /// @return Shared memory name
  [[nodiscard]] std::string_view shm_name() const;

  /// @brief Release all shared memory and delete shared memory object
  /// @note This object becomes unusable after calling this function
  void DeallocateAll();

  /// @brief Check if shared memory is valid
  /// @return Whether valid
  [[nodiscard]] bool Valid() const;

  /// @brief Check if a shared memory segment exists
  /// @param name Shared memory name
  /// @return True if the shared memory exists, false otherwise
  [[nodiscard]] static bool ShmExists(const char* shm_name);

  /// @brief Map shared memory with platform-specific optimizations
  /// @param size Memory size to map
  /// @return Mapped memory pointer
  [[nodiscard]] void* MapMemory(size_type size) const;

  /// @brief Cleanup when creating new shared memory fails
  void CleanupNewShmOnFailure(const char* name);

  /// @brief Cleanup when opening existing shared memory fails
  void CleanupExistingShmOnFailure();

  /// @brief Cleanup mapped memory and file descriptor
  void CleanupMappedResources();

 private:
  // Shared memory name (view of the name stored in shared memory header)
  std::string_view shm_name_;
  // Shared memory file descriptor
  int shm_fd_;
  // Shared memory pointer
  void* shm_ptr_;
  // Shared memory size
  size_type shm_size_;
  // Header pointer
  ShmHeader* header_;
  // Index pointer
  IndexType* index_;
  // Storage area pointer
  void* storage_;

  /// @brief Handle creating new shared memory
  /// @param name Shared memory name
  /// @param storage_size Storage area size
  void CreateNewShm(const char* name, size_type storage_size);

  /// @brief Handle opening existing shared memory
  /// @param name Shared memory name
  void OpenExistingShm();

  /// @brief Initialize shared memory layout
  /// @param name Shared memory name
  /// @param storage_size Storage area size
  void InitializeLayout(const char* name, size_type storage_size);

  /// @brief Validate shared memory layout
  void ValidateLayout();

  /// @brief Calculate layout sizes
  /// @param storage_size Desired storage area size
  /// @return Size information for each part
  struct LayoutSizes {
    size_type header_size;
    size_type index_size;
    size_type storage_offset;
    size_type storage_size;
    size_type total_size;  // Total shared memory size needed
  };
  static LayoutSizes CalculateLayoutSizes(size_type storage_size);

  /// @brief Internal implementation of memory allocation
  /// @param name Instance name
  /// @param size Size
  /// @param alignment Alignment requirement
  /// @return Pair of allocated memory pointer and iterator to the inserted
  /// element
  std::pair<void*, IndexType::iterator> AllocateImpl(std::string_view name,
                                                     size_type size,
                                                     size_type alignment);
};

// Template function implementations

template <typename T>
T* ShmAllocator::Allocate(std::string_view name) {
  static_assert(is_shm_compatible_v<T>,
                "Type must be shared memory compatible");

  // Check if already exists using heterogeneous lookup
  auto it = index_->find(name);
  if (it != index_->end()) {
    // Already exists, return existing pointer
    return static_cast<T*>(static_cast<char*>(storage_) + it->second.offset);
  }

  auto [ptr, meta_it] = AllocateImpl(name, sizeof(T), alignof(T));
  return static_cast<T*>(ptr);
}

template <typename T, typename... Args>
T* ShmAllocator::Construct(std::string_view name, Args&&... args) {
  static_assert(is_shm_compatible_v<T>,
                "Type must be shared memory compatible");

  // Check if already exists using heterogeneous lookup
  auto it = index_->find(name);
  if (it != index_->end()) {
    void* ptr = static_cast<char*>(storage_) + it->second.offset;
    if (it->second.constructed) {
      // Already constructed, return directly
      return static_cast<T*>(ptr);
    } else {
      // Already allocated but not constructed, perform construction
      T* obj_ptr = static_cast<T*>(ptr);
      new (obj_ptr) T(std::forward<Args>(args)...);

      // Update metadata directly
      it->second.constructed = true;

      return obj_ptr;
    }
  }

  // Allocate new memory and construct
  auto [ptr, meta_it] = AllocateImpl(name, sizeof(T), alignof(T));

  T* obj_ptr = static_cast<T*>(ptr);
  new (obj_ptr) T(std::forward<Args>(args)...);

  // Update construction status using the returned iterator
  meta_it->second.constructed = true;

  return obj_ptr;
}

template <typename T>
void ShmAllocator::Destruct(std::string_view name) {
  static_assert(is_shm_compatible_v<T>,
                "Type must be shared memory compatible");

  // Use heterogeneous lookup to check existence
  auto it = index_->find(name);
  if (it == index_->end() || !it->second.constructed) {
    return;
  }

  // Call destructor
  void* ptr = static_cast<char*>(storage_) + it->second.offset;
  T* obj_ptr = static_cast<T*>(ptr);
  obj_ptr->~T();

  // Update metadata directly
  it->second.constructed = false;
}

template <typename T>
T* ShmAllocator::Find(std::string_view name) const {
  static_assert(is_shm_compatible_v<T>,
                "Type must be shared memory compatible");

  // Use heterogeneous lookup directly
  auto it = index_->find(name);
  if (it == index_->end()) {
    return nullptr;
  }

  void* ptr = static_cast<char*>(storage_) + it->second.offset;
  return static_cast<T*>(ptr);
}

template <typename T>
T* ShmAllocator::Get(std::string_view name) const {
  static_assert(is_shm_compatible_v<T>,
                "Type must be shared memory compatible");

  void* ptr = GetBlock(name);
  return static_cast<T*>(ptr);
}

// Helper functions

/// @brief Align size up to the next multiple of alignment
/// @param size Size to align
/// @param alignment Alignment requirement (must be power of 2)
/// @return Aligned size
constexpr ShmAllocator::size_type AlignUp(ShmAllocator::size_type size,
                                          ShmAllocator::size_type alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

/// @brief Get the aligned size of type T
/// @tparam T Type to get aligned size for
/// @return Size of T aligned to its natural alignment
template <typename T>
constexpr ShmAllocator::size_type AlignUp() {
  return AlignUp(sizeof(T), alignof(T));
}

}  // namespace nova