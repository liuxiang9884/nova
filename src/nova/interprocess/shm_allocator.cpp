//
// Created by liuxiang on 2025/5/28.
//

#include "nova/interprocess/shm_allocator.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>

#include <fmt/format.h>

#include "nova/common/macros.h"

namespace nova {

void* ShmAllocator::MapMemory(size_type size) const {
  // Set up mapping flags
  int map_flags = MAP_SHARED;

  // Add platform-specific optimizations
  if constexpr (NOVA_OS == NOVA_OS_LINUX) {
    map_flags |= MAP_POPULATE;  // Preload pages on Linux for better performance
  }

  void* ptr =
      mmap(nullptr, size, PROT_READ | PROT_WRITE, map_flags, shm_fd_, 0);
  if (ptr == MAP_FAILED) {
    // Always cleanup fd and potentially shm on failure
    close(shm_fd_);
    throw ShmAllocatorError("Failed to map shared memory");
  }

  return ptr;
}

void ShmAllocator::CleanupNewShmOnFailure(const char* name) {
  if (shm_fd_ != -1) {
    close(shm_fd_);
    shm_fd_ = -1;
  }
  shm_unlink(name);
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

ShmAllocator::ShmAllocator(const char* name, size_type storage_size,
                           bool create_if_not_exists)
    : shm_name_(),  // Will be set after header is initialized
      shm_fd_(-1),
      shm_ptr_(nullptr),
      shm_size_(0),
      header_(nullptr),
      index_(nullptr),
      storage_(nullptr) {
  // Try to open existing shared memory
  shm_fd_ = shm_open(name, O_RDWR, 0666);

  if (shm_fd_ == -1) {
    if (!create_if_not_exists) {
      throw ShmAllocatorError("Shared memory does not exist: " +
                              std::string(name));
    }
    CreateNewShm(name, storage_size);
  } else {
    OpenExistingShm();
  }
}

ShmAllocator::~ShmAllocator() {
  CleanupMappedResources();
}

void ShmAllocator::InitializeLayout(const char* name, size_type storage_size) {
  const auto layout = CalculateLayoutSizes(storage_size);

  // Initialize header
  header_ = static_cast<ShmHeader*>(shm_ptr_);
  new (header_) ShmHeader(name, layout.total_size, layout.storage_offset,
                          layout.storage_size);

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

std::pair<void*, ShmAllocator::IndexType::iterator> ShmAllocator::AllocateImpl(
    std::string_view name, size_type size, size_type alignment) {
  // Calculate aligned offset
  const size_type aligned_offset =
      AlignUp(header_->current_storage_used, alignment);
  const size_type aligned_size = AlignUp(size, alignment);

  // Check if enough space available
  if (aligned_offset + aligned_size > header_->storage_size) {
    throw ShmAllocatorError("Not enough storage space");
  }

  // Insert into index using heterogeneous emplace with string_view
  auto result = index_->emplace(
      name, ShmInstanceMeta(aligned_offset, aligned_size, alignment, false));
  if (!result.second) {
    throw ShmAllocatorError("Failed to insert instance metadata");
  }

  // Update used size
  header_->current_storage_used = aligned_offset + aligned_size;

  void* ptr = static_cast<char*>(storage_) + aligned_offset;
  return std::make_pair(ptr, result.first);
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

bool ShmAllocator::Contains(std::string_view name) const {
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

  // Delete shared memory object - use the name from header
  if (header_ != nullptr) {
    shm_unlink(header_->name);
  }

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

bool ShmAllocator::ShmExists(const char* shm_name) {
  // Try to open existing shared memory
  int fd = shm_open(shm_name, O_RDWR, 0666);

  if (fd == -1) {
    return false;  // Shared memory does not exist
  }

  // Close the file descriptor since we only wanted to check existence
  close(fd);
  return true;  // Shared memory exists
}

void ShmAllocator::CreateNewShm(const char* name, size_type storage_size) {
  // Calculate layout sizes based on desired storage size
  const auto layout = CalculateLayoutSizes(storage_size);
  shm_size_ = layout.total_size;

  // Create new shared memory
  shm_fd_ = shm_open(name, O_CREAT | O_RDWR | O_EXCL, 0666);
  if (shm_fd_ == -1) {
    throw ShmAllocatorError(
        "Failed to create shared memory: " + std::string(name) +
        ", error: " + std::strerror(errno));
  }

  // Set size to calculated total size
  if (ftruncate(shm_fd_, static_cast<off_t>(layout.total_size)) == -1) {
    CleanupNewShmOnFailure(name);
    throw ShmAllocatorError("Failed to set shared memory size");
  }

  // Map memory using common function
  shm_ptr_ = MapMemory(layout.total_size);

  // Initialize layout
  InitializeLayout(name, storage_size);

  // Set shm_name_ to point to the name stored in header
  shm_name_ = std::string_view(header_->name);
}

void ShmAllocator::OpenExistingShm() {
  // Get existing shared memory size
  struct stat shm_stat{};
  if (fstat(shm_fd_, &shm_stat) == -1) {
    CleanupExistingShmOnFailure();
    throw ShmAllocatorError("Failed to get shared memory size");
  }

  shm_size_ = shm_stat.st_size;

  // Map memory using common function
  shm_ptr_ = MapMemory(shm_size_);

  // Validate layout
  ValidateLayout();

  // Set shm_name_ to point to the name stored in header
  shm_name_ = std::string_view(header_->name);
}

}  // namespace nova