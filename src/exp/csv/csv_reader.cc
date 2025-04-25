#include "nova/exp/csv/csv_reader.hpp"


std::string FormatRow(const std::vector<std::string>& row,
                      std::string_view delim) {
  /** Print a CSV row */
  std::stringstream ret;
  for (size_t i = 0; i < row.size(); i++) {
    ret << row[i];
    if (i + 1 < row.size())
      ret << delim;
    else
      ret << '\n';
  }
  ret.flush();

  return ret.str();
}

GuessScore CalculateScore(std::string_view head, const CSVFormat& format) {
  // Frequency counter of row length
  std::unordered_map<size_t, size_t> row_tally = {{0, 0}};

  // Map row lengths to row num where they first occurred
  std::unordered_map<size_t, size_t> row_when = {{0, 0}};

  // Parse the CSV
  std::stringstream source(head.data());
  RowCollection rows;

  StreamParser<std::stringstream> parser(source, format);
  parser.SetOutput(rows);
  parser.Next(1024 * 1024);

  for (size_t i = 0; i < rows.size(); i++) {
    auto& row = rows[i];

    // Ignore zero-length rows
    if (row.Size() > 0) {
      if (row_tally.find(row.Size()) != row_tally.end()) {
        row_tally[row.Size()]++;
      } else {
        row_tally[row.Size()] = 1;
        row_when[row.Size()] = i;
      }
    }
  }

  double final_score = 0;
  size_t header_row = 0;

  // Final score is equal to the largest
  // row size times rows of that size
  for (auto& pair : row_tally) {
    auto row_size = pair.first;
    auto row_count = pair.second;
    double score = (double)(row_size * row_count);
    if (score > final_score) {
      final_score = score;
      header_row = row_when[row_size];
    }
  }

  return {final_score, header_row};
}

CSVGuessResult GuessFormat(std::string_view head,
                           const std::vector<char>& delims) {
  /** For each delimiter, find out which row length was most common.
   *  The delimiter with the longest mode row length wins.
   *  Then, the line number of the header row is the first row with
   *  the mode row length.
   */

  CSVFormat format;
  size_t max_score = 0, header = 0;
  char current_delim = delims[0];

  for (char cand_delim : delims) {
    auto result = CalculateScore(head, format.Delimiter(cand_delim));

    if ((size_t)result.score > max_score) {
      max_score = (size_t)result.score;
      current_delim = cand_delim;
      header = result.header;
    }
  }

  return {current_delim, (int)header};
}



CSVReader::CSVReader(std::string_view filename, CSVFormat format)
    : format_(format) {
  auto head = GetCSVHead(filename);
  using Parser = MmapParser;

  /** Guess delimiter and header row */
  if (format_.GuessDelim()) {
    auto guess_result = GuessFormat(head, format_.possible_delimiters_);
    format_.Delimiter(guess_result.delim);
    format_.header_ = guess_result.header_row;
    this->format_ = format;
  }

  if (!format_.col_names_.empty()) this->SetColNames(format_.col_names_);

  // this->parser = std::unique_ptr<Parser>(new Parser(filename, format,
  // this->col_names_)); // For C++11
  this->parser_ = std::make_unique<Parser>(filename, format, this->col_names_);
  this->InitialRead();
}

/** Return the format of the original raw CSV */
CSVFormat CSVReader::GetFormat() const {
  CSVFormat new_format = this->format_;

  // Since users are normally not allowed to set
  // column names and header row simulatenously,
  // we will set the backing variables directly here
  new_format.col_names_ = this->col_names_->GetColNames();
  new_format.header_ = this->format_.header_;

  return new_format;
}

/** Return the CSV's column names as a vector of strings. */
std::vector<std::string> CSVReader::GetColNames() const {
  if (this->col_names_) {
    return this->col_names_->GetColNames();
  }

  return std::vector<std::string>();
}

/** Return the index of the column name if found or
 *         csv::CSV_NOT_FOUND otherwise.
 */
int CSVReader::IndexOf(std::string_view col_name) const {
  auto _col_names = this->GetColNames();
  for (size_t i = 0; i < _col_names.size(); i++)
    if (_col_names[i] == col_name) return (int)i;

  return CSV_NOT_FOUND;
}

void CSVReader::TrimHeader() {
  if (!this->header_trimmed_) {
    for (int i = 0; i <= this->format_.header_ && !this->records_->empty();
         i++) {
      if (i == this->format_.header_ && this->col_names_->IsEmpty()) {
        this->SetColNames(this->records_->pop_front());
      } else {
        this->records_->pop_front();
      }
    }

    this->header_trimmed_ = true;
  }
}

/**
 *  @param[in] names Column names
 */
void CSVReader::SetColNames(const std::vector<std::string>& names) {
  this->col_names_->SetColNames(names);
  this->n_cols_ = names.size();
}

/**
 * Read a chunk of CSV data.
 *
 * @note This method is meant to be run on its own thread. Only one `read_csv()`
 * thread should be active at a time.
 *
 * @param[in] bytes Number of bytes to read.
 *
 * @see CSVReader::read_csv_worker
 * @see CSVReader::read_row()
 */
inline bool CSVReader::ReadCSV(size_t bytes) {
  // Tell read_row() to listen for CSV rows
  this->records_->notify_all();

  this->parser_->SetOutput(*this->records_);
  this->parser_->Next(bytes);

  if (!this->header_trimmed_) {
    this->TrimHeader();
  }

  // Tell read_row() to stop waiting
  this->records_->kill_all();

  return true;
}

/**
 * Retrieve rows as CSVRow objects, returning true if more rows are available.
 *
 * @par Performance Notes
 *  - Reads chunks of data that are csv::internals::ITERATION_CHUNK_SIZE bytes
 * large at a time
 *  - For performance details, read the documentation for CSVRow and CSVField.
 *
 * @param[out] row The variable where the parsed row will be stored
 * @see CSVRow, CSVField
 *
 * **Example:**
 * \snippet tests/test_read_csv.cpp CSVField Example
 *
 */
inline bool CSVReader::ReadRow(CSVRow& row) {
  while (true) {
    if (this->records_->empty()) {
      if (this->records_->is_waitable())
        // Reading thread is currently active => wait for it to populate records
        this->records_->wait();
      else if (this->parser_->Eof())
        // End of file and no more records
        return false;
      else {
        // Reading thread is not active => start another one
        if (this->read_csv_worker_.joinable()) this->read_csv_worker_.join();

        this->read_csv_worker_ = std::thread(&CSVReader::ReadCSV, this, 1024*1024);
      }
    } else if (this->records_->front().Size() != this->n_cols_ &&
               this->format_.variable_column_policy_ !=
                   VariableColumnPolicy::KEEP) {
      auto errored_row = this->records_->pop_front();

      if (this->format_.variable_column_policy_ ==
          VariableColumnPolicy::THROW) {
        if (errored_row.Size() < this->n_cols_)
          throw std::runtime_error("Line too short " + FormatRow(errored_row));

        throw std::runtime_error("Line too long " + FormatRow(errored_row));
      }
    } else {
      row = this->records_->pop_front();
      this->n_rows_++;
      return true;
    }
  }

  return false;
}

CSVReader::iterator CSVReader::Begin() {
  if (this->records_->empty()) {
    this->read_csv_worker_ =
        std::thread(&CSVReader::ReadCSV, this, 1024 * 1024);
    this->read_csv_worker_.join();

    // Still empty => return end iterator
    if (this->records_->empty()) return this->End();
  }

  this->n_rows_++;
  CSVReader::iterator ret(this, this->records_->pop_front());
  return ret;
}

/** A placeholder for the imaginary past the end row in a CSV.
 *  Attempting to deference this will lead to bad things.
 */
CSVReader::iterator CSVReader::End() const noexcept {
  return CSVReader::iterator();
}

CSVReader::iterator::iterator(CSVReader* parent, CSVRow&& row)
    : parent_(parent) {
  this->row_ = std::move(row);
}

/** Advance the iterator by one row. If this CSVReader has an
 *  associated file, then the iterator will lazily pull more data from
 *  that file until the end of file is reached.
 *
 *  @note This iterator does **not** block the thread responsible for parsing
 * CSV.
 *
 */
CSVReader::iterator& CSVReader::iterator::operator++() {
  if (!this->parent_->ReadRow(this->row_)) {
    this->parent_ = nullptr;  // this == end()
  }
  return *this;
}

/** Post-increment iterator */
CSVReader::iterator CSVReader::iterator::operator++(int) {
  auto temp = *this;
  if (!this->parent_->ReadRow(this->row_)) {
    this->parent_ = nullptr;  // this == end()
  }

  return temp;
}