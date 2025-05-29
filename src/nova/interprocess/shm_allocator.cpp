//
// Created by liuxiang on 2025/5/28.
//

#include "nova/interprocess/shm_allocator.h"

#include <algorithm>
#include <cstring>

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace nova {

ShmAllocator::ShmAllocator(std::string_view name, size_type storage_size,
                           size_type max_instances, bool create_if_not_exists)
    : shm_name_(name),
      shm_fd_(-1),
      shm_ptr_(nullptr),
      shm_size_(0),  // Will be set based on layout calculation
      header_(nullptr),
      index_(nullptr),
      storage_(nullptr) {
  // Calculate layout sizes based on desired storage size
  auto layout = CalculateLayoutSizes(storage_size, max_instances);
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
    if (ftruncate(shm_fd_, layout.total_size) == -1) {
      close(shm_fd_);
      shm_unlink(shm_name_.c_str());
      throw ShmAllocatorError("Failed to set shared memory size");
    }

    // Map memory
    shm_ptr_ = mmap(nullptr, layout.total_size, PROT_READ | PROT_WRITE,
                    MAP_SHARED, shm_fd_, 0);
    if (shm_ptr_ == MAP_FAILED) {
      close(shm_fd_);
      shm_unlink(shm_name_.c_str());
      throw ShmAllocatorError("Failed to map shared memory");
    }

    // Initialize layout
    InitializeLayout(storage_size, max_instances);
  } else {
    // Get existing shared memory size
    struct stat shm_stat;
    if (fstat(shm_fd_, &shm_stat) == -1) {
      close(shm_fd_);
      throw ShmAllocatorError("Failed to get shared memory size");
    }

    shm_size_ = shm_stat.st_size;

    // Map memory
    shm_ptr_ = mmap(nullptr, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED,
                    shm_fd_, 0);
    if (shm_ptr_ == MAP_FAILED) {
      close(shm_fd_);
      throw ShmAllocatorError("Failed to map existing shared memory");
    }

    // Validate layout
    ValidateLayout();
  }
}

ShmAllocator::~ShmAllocator() {
  if (shm_ptr_ != nullptr && shm_ptr_ != MAP_FAILED) {
    munmap(shm_ptr_, shm_size_);
  }
  if (shm_fd_ != -1) {
    close(shm_fd_);
  }
}

void ShmAllocator::InitializeLayout(size_type storage_size,
                                    size_type max_instances) {
  auto layout = CalculateLayoutSizes(storage_size, max_instances);

  // Initialize header
  header_ = static_cast<ShmHeader*>(shm_ptr_);
  new (header_) ShmHeader(shm_name_.c_str(), layout.total_size, max_instances,
                          layout.storage_offset, layout.storage_size);

  // Initialize index (using placement new and default construction)
  index_ = reinterpret_cast<IndexType*>(static_cast<char*>(shm_ptr_) +
                                        layout.header_size);
  // Clear memory first, then use placement new
  std::memset(index_, 0, sizeof(IndexType));
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

  auto layout =
      CalculateLayoutSizes(header_->storage_size, header_->max_instances);

  // Set pointers
  index_ = reinterpret_cast<IndexType*>(static_cast<char*>(shm_ptr_) +
                                        layout.header_size);
  storage_ = static_cast<char*>(shm_ptr_) + layout.storage_offset;
}

ShmAllocator::LayoutSizes ShmAllocator::CalculateLayoutSizes(
    size_type storage_size, size_type /* max_instances */) {
  LayoutSizes layout;

  // Header size (aligned to 8 bytes)
  layout.header_size = align_up(sizeof(ShmHeader), 8);

  // Index size (aligned to 8 bytes)
  layout.index_size = align_up(sizeof(IndexType), 8);

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
  if (!Valid()) {
    throw ShmAllocatorError("Allocator is not valid");
  }

  ShmName shm_name;
  ToShmName(name, shm_name);

  // Check if already exists
  auto it = index_->find(shm_name);
  if (it != index_->end()) {
    // Already exists, return existing pointer
    return static_cast<char*>(storage_) + it->second.offset;
  }

  // Check if maximum instance count exceeded
  if (index_->size() >= header_->max_instances) {
    throw ShmAllocatorError("Maximum number of instances reached");
  }

  // Calculate aligned offset
  size_type aligned_offset = align_up(header_->current_storage_used, alignment);
  size_type aligned_size = align_up(size, alignment);

  // Check if enough space available
  if (aligned_offset + aligned_size > header_->storage_size) {
    throw ShmAllocatorError("Not enough storage space");
  }

  // Create metadata
  ShmInstanceMeta meta(aligned_offset, aligned_size, alignment, false);

  // Insert into index
  auto result = index_->emplace(shm_name, meta);
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
  if (!Valid()) {
    return nullptr;
  }

  ShmName shm_name;
  ToShmName(name, shm_name);

  auto it = index_->find(shm_name);
  if (it == index_->end()) {
    return nullptr;
  }

  void* ptr = static_cast<char*>(storage_) + it->second.offset;
  return ptr;
}

bool ShmAllocator::Exists(std::string_view name) const {
  if (!Valid()) {
    return false;
  }

  ShmName shm_name;
  ToShmName(name, shm_name);
  return index_->contains(shm_name);
}

bool ShmAllocator::IsConstructed(std::string_view name) const {
  if (!Valid()) {
    return false;
  }

  ShmName shm_name;
  ToShmName(name, shm_name);

  auto it = index_->find(shm_name);
  return it != index_->end() && it->second.constructed;
}

std::vector<std::string> ShmAllocator::GetInstanceNames() const {
  std::vector<std::string> names;
  if (!Valid()) {
    return names;
  }

  names.reserve(index_->size());
  for (const auto& pair : *index_) {
    names.push_back(FromShmName(pair.first));
  }

  return names;
}

ShmAllocator::size_type ShmAllocator::instance_count() const {
  return Valid() ? index_->size() : 0;
}

ShmAllocator::size_type ShmAllocator::max_instances() const {
  return Valid() ? header_->max_instances : 0;
}

ShmAllocator::size_type ShmAllocator::total_size() const {
  return Valid() ? header_->total_size : 0;
}

ShmAllocator::size_type ShmAllocator::used_storage_size() const {
  return Valid() ? header_->current_storage_used : 0;
}

ShmAllocator::size_type ShmAllocator::available_storage_size() const {
  return Valid() ? (header_->storage_size - header_->current_storage_used) : 0;
}

ShmAllocator::size_type ShmAllocator::storage_size() const {
  return Valid() ? header_->storage_size : 0;
}

std::string_view ShmAllocator::shm_name() const {
  return shm_name_;
}

void ShmAllocator::DeallocateAll() {
  if (!Valid()) {
    return;
  }

  // Destruct all constructed objects (user needs to call destruct manually)

  // Unmap memory
  if (shm_ptr_ != nullptr && shm_ptr_ != MAP_FAILED) {
    munmap(shm_ptr_, shm_size_);
    shm_ptr_ = nullptr;
  }

  // Close file descriptor
  if (shm_fd_ != -1) {
    close(shm_fd_);
    shm_fd_ = -1;
  }

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

}  // namespace nova