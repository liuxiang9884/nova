#ifndef CSV_READER_H
#define CSV_READER_H

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "csv_parser.hpp"
#include "csv_objects.hpp"
#include "csv_format.hpp"


class CSVRow;

std::string FormatRow(const std::vector<std::string>& row,
                      std::string_view delim = ", ");

template <bool B, class T = void>
using enable_if_t = typename std::enable_if<B, T>::type;



/** Stores the inferred format of a CSV file. */
struct CSVGuessResult {
  char delim;
  int header_row;
};

CSVGuessResult GuessFormat(std::string_view filename,
                           const std::vector<char>& delims = {',', '|', '\t',
                                                              ';', '^', '~'});



class CSVReader {
 public:
  class iterator {
   public:
    using value_type = CSVRow;
    using difference_type = std::ptrdiff_t;
    using pointer = CSVRow*;
    using reference = CSVRow&;
    using iterator_category = std::input_iterator_tag;

    iterator() = default;
    iterator(CSVReader* reader) : parent_(reader) {};
    iterator(CSVReader*, CSVRow&&);

    /** Access the CSVRow held by the iterator */
    reference operator*() {
      return this->row_;
    }

    /** Return a pointer to the CSVRow the iterator has stopped at */
    pointer operator->() {
      return &(this->row_);
    }

    iterator& operator++();   /**< Pre-increment iterator */
    iterator operator++(int); /**< Post-increment iterator */

    /** Returns true if iterators were constructed from the same CSVReader
     *  and point to the same row
     */
    bool operator==(const iterator& other) const noexcept {
      return (this->parent_ == other.parent_) && (this->i == other.i);
    }

    bool operator!=(const iterator& other) const noexcept {
      return !operator==(other);
    }

   private:
    CSVReader* parent_ = nullptr;  // Pointer to parent
    CSVRow row_;                   // Current row
    size_t i = 0;                  // Index of current row
  };

  // explicit CSVReader(const std::string& delimiter = ",");

  CSVReader(std::string_view filename, CSVFormat format = CSVFormat::GuessCSV());

  template <typename TStream,
            enable_if_t<std::is_base_of<std::istream, TStream>::value, int> = 0>
  CSVReader(TStream& source, CSVFormat format) : format_(format) {
    using Parser = StreamParser<TStream>;

    if (!format_.col_names_.empty()) SetColNames(format_.col_names_);
    
    parser_= std::make_unique<Parser>(source, format_, col_names_); 

    this->InitialRead();
  }

  ~CSVReader() {
    if (this->read_csv_worker_.joinable()) {
      this->read_csv_worker_.join();
    }
  }

  std::vector<std::string> GetColNames() const;

  CSVFormat GetFormat() const;

  void TrimHeader();
  int IndexOf(std::string_view col_name) const;
  bool Empty() const noexcept {
    return n_rows_ == 0;
  }

  bool Eof() const noexcept {
    return this->parser_->Eof();
  };
  bool ReadRow(CSVRow& row);

  iterator Begin();
  iterator End() const noexcept;

  bool ReadCSV(size_t bytes = 1024*1024);

  void SetColNames(const std::vector<std::string>&);

  CSVFormat format_;

  std::unique_ptr<IBasicCSVParser> parser_ = nullptr;

  std::unique_ptr<RowCollection> records_ = std::make_unique<RowCollection>();
  
  ColNamesPtr col_names_ = std::make_shared<ColNames>();

  size_t n_cols_ = 0; /**< The number of columns in this CSV */
  size_t n_rows_ = 0; /**< How many rows (minus header) have been read so far */

 private:
  /** Whether or not rows before header were trimmed */
  bool header_trimmed_ = false;

  std::string delimiter_;
  std::vector<std::vector<std::string>> data_;

  std::thread read_csv_worker_;

  void InitialRead() {
    read_csv_worker_ = std::thread(&CSVReader::ReadCSV, this, 1024*1024);
    read_csv_worker_.join();
  }
};

#endif  // CSV_READER_H