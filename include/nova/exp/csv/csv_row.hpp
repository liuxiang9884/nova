#ifndef CSV_ROW_HPP
#define CSV_ROW_HPP

#include "csv_names.h"

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cassert>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "csv_datatype.hpp"
#include "csv_objects.hpp"
// #include "csv_utility.hpp"

constexpr size_t PAGE_SIZE = 4096;

// Forward declaration
class CSVField;


/** A barebones class used for describing CSV fields */
struct RawCSVField {
  RawCSVField() = default;
  RawCSVField(size_t start, size_t length, bool double_quote = false)
      : start(start), length(length), has_double_quote(double_quote) {}
  /** The start of the field, relative to the beginning of the row */
  size_t start;

  /** The length of the row, ignoring quote escape characters */
  size_t length;

  /** Whether or not the field contains an escaped quote */
  bool has_double_quote;
};



class CSVFieldList {
 public:
  /** Construct a CSVFieldList which allocates blocks of a certain size */
  CSVFieldList(size_t single_buffer_capacity = (size_t)(PAGE_SIZE /
                                                        sizeof(RawCSVField)))
      : single_buffer_capacity_(single_buffer_capacity) {
    this->Allocate();
  }

  // No copy constructor
  CSVFieldList(const CSVFieldList& other) = delete;

  // CSVFieldArrays may be moved
  CSVFieldList(CSVFieldList&& other)
      : single_buffer_capacity_(other.single_buffer_capacity_),
        current_buffer_size_(other.current_buffer_size_),
        back_(other.back_) {
    for (auto&& buffer : other.buffers_) {
      this->buffers_.emplace_back(std::move(buffer));
    }
  }

  template <class... Args>
  void EmplaceBack(Args&&... args) {
    if (this->current_buffer_size_ == this->single_buffer_capacity_) {
      this->Allocate();
    }

    *(back_++) = RawCSVField(std::forward<Args>(args)...);
    current_buffer_size_++;
  }

  size_t Size() const noexcept {
    return (current_buffer_size_ +
            ((buffers_.size() - 1) * single_buffer_capacity_));
  }

  RawCSVField& operator[](size_t n) const{
    const size_t page_on = n / single_buffer_capacity_;
    const size_t buffer_idx = (page_on < 1) ? n : n % single_buffer_capacity_;
    return buffers_[page_on][buffer_idx];
  }

 private:
  const size_t single_buffer_capacity_;

  /**
   * Prefer std::deque over std::vector because it does not
   * reallocate upon expansion, allowing pointers to its members
   * to remain valid & avoiding potential race conditions when
   * CSVFieldList is accesssed simulatenously by a reading thread and
   * a writing thread
   */
  std::deque<std::unique_ptr<RawCSVField[]>> buffers_ = {};

  /** Number of items in the current buffer */
  size_t current_buffer_size_ = 0;

  /** Pointer to the current empty field */
  RawCSVField* back_ = nullptr;

  /** Allocate a new page of memory */
  void Allocate(){
    buffers_.push_back(std::make_unique<RawCSVField[]>(single_buffer_capacity_));
    current_buffer_size_ = 0;
    back_ = buffers_.back().get();
  }
};

struct RawCSVData {
  CSVFieldList fields;
  std::shared_ptr<void> data_ptr = nullptr;
  std::string_view data = "";

  std::unordered_set<size_t> has_double_quotes = {};

  // TODO: Consider replacing with a more thread-safe structure
  std::unordered_map<size_t, std::string> double_quote_fields = {};

  ColNamesPtr col_names = nullptr;
  ParseFlagMap parse_flags;
  WhitespaceMap ws_flags;
};
using RawCSVDataPtr = std::shared_ptr<RawCSVData>;

class CSVRow {
 public:
  CSVRow() = default;

  /** Construct a CSVRow from a RawCSVDataPtr */
  CSVRow(RawCSVDataPtr data) : data_(data) {}
  CSVRow(RawCSVDataPtr data, size_t data_start, size_t field_bounds)
      : data_(data), data_start_(data_start), fields_start_(field_bounds) {};

  /** Indicates whether row is empty or not */
  bool Empty() const noexcept {
    return Size() == 0;
  }

  /** Return the number of fields in this row */
  size_t Size() const noexcept {
    return row_length_;
  }

  CSVField operator[](size_t n) const;
  CSVField operator[](const std::string&) const;
  std::string ToJson(const std::vector<std::string>& subset = {}) const;
  std::string ToJsonArray(const std::vector<std::string>& subset = {}) const;

  /** Retrieve this row's associated column names */
  std::vector<std::string> GetColNames() const {
    return data_->col_names->GetColNames();
  }

  std::string_view GetField(size_t n) const;

  operator std::vector<std::string>() const;

  // private:
  RawCSVDataPtr data_;
  size_t row_length_ = 0;
  size_t data_start_ = 0;
  size_t fields_start_ = 0;
  std::vector<std::string> col_names_;
};

class CSVField {
 public:
  /** Constructs a CSVField from a string_view */
  CSVField(std::string_view str) noexcept : sv_(str) {
    Type();
  }

  operator std::string() const {
    return std::string("<CSVField> ") + std::string(sv_);
  }
  DataType Type() const noexcept {
    if ((int)type_ < 0) {
      type_ = GetDataType(sv_, &value_);
    }
    return type_;
  }
  /** Retrieve the value of this field as a assigned type */
  template <typename T = std::string>
  T Get() {
    return static_cast<T>(value_);
  }

  std::string_view GetString() const noexcept {
    return sv_;
  }

  bool IsNull() const noexcept {
    return Type() == DataType::CSV_NULL;
  }
  bool IsStr() const noexcept {
    return Type() == DataType::CSV_STRING;
  }
  bool IsInt() const noexcept {
    return (Type() >= DataType::CSV_INT8) && (Type() <= DataType::CSV_INT64);
  }
  bool IsFloat() const noexcept {
    return Type() == DataType::CSV_DOUBLE;
  }

 private:
  std::string_view sv_ = "";
  mutable long double value_ = 0;
  mutable DataType type_ = DataType::UNKNOWN;
};

template <typename T>
class ThreadSafeDeque {
 public:
  ThreadSafeDeque(size_t notify_size = 100) : _notify_size(notify_size) {};
  ThreadSafeDeque(const ThreadSafeDeque& other) {
    this->data = other.data;
    this->_notify_size = other._notify_size;
  }

  ThreadSafeDeque(const std::deque<T>& source) : ThreadSafeDeque() {
    this->data = source;
  }

  void clear() noexcept {
    this->data.clear();
  }

  bool empty() const noexcept {
    return this->data.empty();
  }

  T& front() noexcept {
    return this->data.front();
  }

  T& operator[](size_t n) {
    return this->data[n];
  }

  void push_back(T&& item) {
    std::lock_guard<std::mutex> lock{this->_lock};
    this->data.push_back(std::move(item));

    if (this->size() >= _notify_size) {
      this->cond_.notify_all();
    }
  }

  T pop_front() noexcept {
    std::lock_guard<std::mutex> lock{this->_lock};
    T item = std::move(data.front());
    data.pop_front();
    return item;
  }

  size_t size() const noexcept {
    return this->data.size();
  }

  /** Returns true if a thread is actively pushing items to this deque */
  constexpr bool is_waitable() const noexcept {
    return this->_is_waitable;
  }

  /** Wait for an item to become available */
  void wait() {
    if (!is_waitable()) {
      return;
    }

    std::unique_lock<std::mutex> lock{this->_lock};
    this->cond_.wait(lock, [this] {
      return this->size() >= _notify_size || !this->is_waitable();
    });
    lock.unlock();
  }

  typename std::deque<T>::iterator begin() noexcept {
    return this->data.begin();
  }

  typename std::deque<T>::iterator end() noexcept {
    return this->data.end();
  }

  /** Tell listeners that this deque is actively being pushed to */
  void notify_all() {
    std::unique_lock<std::mutex> lock{this->_lock};
    this->_is_waitable = true;
    this->cond_.notify_all();
  }

  /** Tell all listeners to stop */
  void kill_all() {
    std::unique_lock<std::mutex> lock{this->_lock};
    this->_is_waitable = false;
    this->cond_.notify_all();
  }

 private:
  bool _is_waitable = false;
  size_t _notify_size;
  std::mutex _lock;
  std::condition_variable cond_;
  std::deque<T> data;
};

using RowCollection = ThreadSafeDeque<CSVRow>;

#endif  // CSV_ROW_HPP