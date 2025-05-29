//
// Created by liuxiang on 2025/5/28.
//

#include "nova/interprocess/shm_allocator.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>

// MAP_POPULATE may not be available on all platforms
#ifndef MAP_POPULATE
#define MAP_POPULATE 0
#endif

namespace nova {

void* ShmAllocator::MapMemory(size_type size, int fd,
                              const std::string& error_msg,
                              bool cleanup_shm_on_failure) const {
  // Set up mapping flags
  int map_flags = MAP_SHARED;

  // Add platform-specific optimizations
  if constexpr (NOVA_OS == NOVA_OS_LINUX) {
    map_flags |= MAP_POPULATE;  // Preload pages on Linux for better performance
  }

  void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, map_flags, fd, 0);
  if (ptr == MAP_FAILED) {
    // Always cleanup fd and potentially shm on failure
    close(fd);
    if (cleanup_shm_on_failure) {
      shm_unlink(shm_name_.c_str());
    }
    throw ShmAllocatorError(error_msg);
  }

  return ptr;
}

void ShmAllocator::CleanupNewShmOnFailure() {
  if (shm_fd_ != -1) {
    close(shm_fd_);
    shm_fd_ = -1;
  }
  shm_unlink(shm_name_.c_str());
}

void ShmAllocator::CleanupExistingShmOnFailure() {
  if (shm_fd_ != -1) {
    close(shm_fd_);
    shm_fd_ = -1;
  }
}

void ShmAllocator::CleanupMappedResources() {
  if (shm_ptr_ != nullptr && shm_ptr_ != MAP_FAILED) {
    munmap(shm_ptr_, shm_size_);
    shm_ptr_ = nullptr;
  }
  if (shm_fd_ != -1) {
    close(shm_fd_);
    shm_fd_ = -1;
  }
}

ShmAllocator::ShmAllocator(std::string_view name, size_type storage_size,
                           bool create_if_not_exists)
    : shm_name_(name),
      shm_fd_(-1),
      shm_ptr_(nullptr),
      shm_size_(0),  // Will be set based on layout calculation
      header_(nullptr),
      index_(nullptr),
      storage_(nullptr) {
  // Calculate layout sizes based on desired storage size
  const auto layout = CalculateLayoutSizes(storage_size);
  shm_size_ = layout.total_size;

  // Try to open existing shared memory
  shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0666);

  if (shm_fd_ == -1) {
    if (!create_if_not_exists) {
      throw ShmAllocatorError("Shared memory does not exist: " + shm_name_);
    }

    // Create new shared memory
    shm_fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_RDWR | O_EXCL, 0666);
    if (shm_fd_ == -1) {
      throw ShmAllocatorError("Failed to create shared memory: " + shm_name_ +
                              ", error: " + std::strerror(errno));
    }

    // Set size to calculated total size
    if (ftruncate(shm_fd_, static_cast<off_t>(layout.total_size)) == -1) {
      CleanupNewShmOnFailure();
      throw ShmAllocatorError("Failed to set shared memory size");
    }

    // Map memory using common function
    shm_ptr_ = MapMemory(layout.total_size, shm_fd_,
                         "Failed to map shared memory", true);

    // Initialize layout
    InitializeLayout(storage_size);
  } else {
    // Get existing shared memory size
    struct stat shm_stat{};
    if (fstat(shm_fd_, &shm_stat) == -1) {
      CleanupExistingShmOnFailure();
      throw ShmAllocatorError("Failed to get shared memory size");
    }

    shm_size_ = shm_stat.st_size;

    // Map memory using common function
    shm_ptr_ = MapMemory(shm_size_, shm_fd_,
                         "Failed to map existing shared memory", false);

    // Validate layout
    ValidateLayout();
  }
}

ShmAllocator::~ShmAllocator() {
  CleanupMappedResources();
}

void ShmAllocator::InitializeLayout(size_type storage_size) {
  const auto layout = CalculateLayoutSizes(storage_size);

  // Initialize header
  header_ = static_cast<ShmHeader*>(shm_ptr_);
  new (header_) ShmHeader(shm_name_.c_str(), layout.total_size,
                          layout.storage_offset, layout.storage_size);

  // Initialize index (using placement new and default construction)
  index_ = reinterpret_cast<IndexType*>(static_cast<char*>(shm_ptr_) +
                                        layout.header_size);
  // Use placement new to construct the object properly
  new (index_) IndexType();

  // Set storage area pointer
  storage_ = static_cast<char*>(shm_ptr_) + layout.storage_offset;
}

void ShmAllocator::ValidateLayout() {
  if (shm_size_ < sizeof(ShmHeader)) {
    throw ShmAllocatorError("Invalid shared memory: too small for header");
  }

  header_ = static_cast<ShmHeader*>(shm_ptr_);
  if (!header_->initialized) {
    throw ShmAllocatorError("Shared memory not properly initialized");
  }

  if (header_->total_size != shm_size_) {
    throw ShmAllocatorError("Shared memory size mismatch");
  }

  auto layout = CalculateLayoutSizes(header_->storage_size);

  // Set pointers
  index_ = reinterpret_cast<IndexType*>(static_cast<char*>(shm_ptr_) +
                                        layout.header_size);
  storage_ = static_cast<char*>(shm_ptr_) + layout.storage_offset;
}

ShmAllocator::LayoutSizes ShmAllocator::CalculateLayoutSizes(
    size_type storage_size) {
  LayoutSizes layout{};

  // Header size (aligned to 8 bytes)
  layout.header_size = AlignUp(sizeof(ShmHeader), 8);

  // Index size (aligned to 8 bytes)
  layout.index_size = AlignUp(sizeof(IndexType), 8);

  // Storage area offset
  layout.storage_offset = layout.header_size + layout.index_size;

  // Storage area size is the requested size
  layout.storage_size = storage_size;

  // Total size is the sum of all components
  layout.total_size = layout.storage_offset + layout.storage_size;

  return layout;
}

void* ShmAllocator::AllocateImpl(std::string_view name, size_type size,
                                 size_type alignment) {
  // Check if already exists using heterogeneous lookup
  auto it = index_->find(name);
  if (it != index_->end()) {
    // Already exists, return existing pointer
    return static_cast<char*>(storage_) + it->second.offset;
  }

  // Calculate aligned offset
  const size_type aligned_offset =
      AlignUp(header_->current_storage_used, alignment);
  const size_type aligned_size = AlignUp(size, alignment);

  // Check if enough space available
  if (aligned_offset + aligned_size > header_->storage_size) {
    throw ShmAllocatorError("Not enough storage space");
  }

  // Create metadata
  ShmInstanceMeta meta(aligned_offset, aligned_size, alignment, false);

  // Insert into index using heterogeneous emplace with string_view
  auto result = index_->emplace(name, meta);
  if (!result.second) {
    throw ShmAllocatorError("Failed to insert instance metadata");
  }

  // Update used size
  header_->current_storage_used = aligned_offset + aligned_size;

  return static_cast<char*>(storage_) + aligned_offset;
}

void ShmAllocator::ToShmName(std::string_view name, ShmName& shm_name) {
  if (name.size() > ShmName::capacity()) {
    throw ShmAllocatorError("Instance name too long (max " +
                            std::to_string(ShmName::capacity()) +
                            " characters)");
  }

  shm_name = ShmName(name);
}

std::string ShmAllocator::FromShmName(const ShmName& shm_name) {
  return shm_name.string();
}

void* ShmAllocator::GetBlock(std::string_view name) const {
  // Use heterogeneous lookup directly with string_view
  auto it = index_->find(name);
  if (it == index_->end()) {
    return nullptr;
  }

  void* ptr = static_cast<char*>(storage_) + it->second.offset;
  return ptr;
}

bool ShmAllocator::Exists(std::string_view name) const {
  // Use heterogeneous lookup directly with string_view
  return index_->contains(name);
}

bool ShmAllocator::IsConstructed(std::string_view name) const {
  // Use heterogeneous lookup directly with string_view
  auto it = index_->find(name);
  return it != index_->end() && it->second.constructed;
}

std::vector<std::string_view> ShmAllocator::GetInstanceNames() const {
  std::vector<std::string_view> names;
  names.reserve(index_->size());
  for (const auto& pair : *index_) {
    names.push_back(pair.first.view());
  }

  return names;
}

ShmAllocator::size_type ShmAllocator::instance_count() const {
  return index_->size();
}

ShmAllocator::size_type ShmAllocator::max_instances() const {
  return 1024;  // This matches the N template parameter in IndexType definition
}

ShmAllocator::size_type ShmAllocator::total_size() const {
  return header_->total_size;
}

ShmAllocator::size_type ShmAllocator::used_storage_size() const {
  return header_->current_storage_used;
}

ShmAllocator::size_type ShmAllocator::available_storage_size() const {
  return header_->storage_size - header_->current_storage_used;
}

ShmAllocator::size_type ShmAllocator::storage_size() const {
  return header_->storage_size;
}

std::string_view ShmAllocator::shm_name() const {
  return shm_name_;
}

void ShmAllocator::DeallocateAll() {
  // Note: This function can only be called once. After calling this function,
  // the ShmAllocator object becomes invalid and should not be used again.
  // All constructed objects should be destructed manually before calling this.

  // Cleanup mapped resources and file descriptor
  CleanupMappedResources();

  // Delete shared memory object
  shm_unlink(shm_name_.c_str());

  // Reset pointers
  header_ = nullptr;
  index_ = nullptr;
  storage_ = nullptr;
}

bool ShmAllocator::Valid() const {
  return shm_ptr_ != nullptr && shm_ptr_ != MAP_FAILED && shm_fd_ != -1 &&
         header_ != nullptr && header_->initialized && index_ != nullptr &&
         storage_ != nullptr;
}

bool ShmAllocator::ShmExists(std::string_view name) {
  // Convert to c string for shm_open
  std::string shm_name_str(name);

  // Try to open existing shared memory
  int fd = shm_open(shm_name_str.c_str(), O_RDWR, 0666);

  if (fd == -1) {
    return false;  // Shared memory does not exist
  }

  // Close the file descriptor since we only wanted to check existence
  close(fd);
  return true;  // Shared memory exists
}

}  // namespace nova