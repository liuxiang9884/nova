#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

struct RawCSVField {
    size_t start;
    size_t length;
};

class CSVFieldList {
public:
    CSVFieldList(size_t initial_capacity = 1024)
        : fields_(initial_capacity), size_(0), is_complete_(false) {
    }

    void EmplaceBack(size_t start, size_t length) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (size_ >= fields_.size()) {
            // Double the capacity when needed
            fields_.resize(fields_.size() * 2);
        }
        fields_[size_] = {start, length};
        size_++;
        
        // Notify waiting consumers that new data is available
        cv_.notify_all();
    }

    const RawCSVField& operator[](size_t index) const {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait if the requested index isn't available yet
        cv_.wait(lock, [this, index]() {
            return index < size_ || is_complete_;
        });
        
        if (index >= size_) {
            throw std::out_of_range("Index out of range in CSVFieldList");
        }
        
        return fields_[index];
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }
    
    // Mark parsing as complete - useful for signaling consumers
    void MarkComplete() {
        std::lock_guard<std::mutex> lock(mutex_);
        is_complete_ = true;
        cv_.notify_all();
    }
    
    // Wait until the list has at least 'count' elements
    void WaitForSize(size_t count) const {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this, count]() {
            return size_ >= count || is_complete_;
        });
    }

private:
    std::vector<RawCSVField> fields_;
    size_t size_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    bool is_complete_;
};
