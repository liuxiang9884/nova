#ifndef CSV_PARSER_HPP
#define CSV_PARSER_HPP

#include "csv_names.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "csv_datatype.hpp"
#include "csv_objects.hpp"
#include "csv_row.hpp"
#include "csv_format.hpp"

constexpr const int UNINITIALIZED_FIELD = -1;

struct GuessScore {
  double score;
  size_t header;
};

std::string GetCSVHead(std::string_view filename);
size_t GetFileSize(std::string_view filename);

class IBasicCSVParser {
 public:
  IBasicCSVParser() = default;
  IBasicCSVParser(const CSVFormat&, const ColNamesPtr&);
  IBasicCSVParser(const ParseFlagMap& parse_flags,
                  const WhitespaceMap& ws_flags)
      : parse_flags_(parse_flags), ws_flags_(ws_flags) {};

  virtual ~IBasicCSVParser() {}

  void SetOutput(RowCollection& rows) {
    this->records_ = &rows;
  }

  /** Whether or not we have reached the end of source */
  bool Eof() {
    return this->eof_;
  }

  /** Parse the next block of data */
  virtual void Next(size_t bytes) = 0;

  /** Indicate the last block of data has been parsed */
  void EndFeed();

  ParseFlags ParseFlag(const char ch) const noexcept {
    return parse_flags_.data()[ch + 128];
  }

  ParseFlags CompoundParseFlag(const char ch) const noexcept {
    return QuoteEscapeFlag(ParseFlag(ch), this->quote_escape_);
  }

  /** Whether or not this CSV has a UTF-8 byte order mark */
  bool Utf8Bom() const {
    return this->utf8_bom_;
  }

 protected:
  /** @name Current Parser State */
  ///@{
  CSVRow current_row_;
  RawCSVDataPtr data_ptr_ = nullptr;
  ColNamesPtr col_names_ = nullptr;
  CSVFieldList* fields_ = nullptr;
  int field_start_ = UNINITIALIZED_FIELD;
  size_t field_length_ = 0;

  /** An array where the (i + 128)th slot gives the ParseFlags for ASCII
   * character i */
  ParseFlagMap parse_flags_;
  ///@}

  /** @name Current Stream/File State */
  ///@{
  bool eof_ = false;

  /** The size of the incoming CSV */
  size_t source_size_ = 0;
  ///@}

  /** Whether or not source needs to be read in chunks */
  bool NoChunk() const {
    return this->source_size_ < 1024 * 1024;
  }

  /** Parse the current chunk of data *
   *
   *  @returns How many character were read that are part of complete rows
   */
  size_t Parse();

  /** Create a new RawCSVDataPtr for a new chunk of data */
  void ResetDataPtr();

 private:
  /** An array where the (i + 128)th slot determines whether ASCII character i
   * should be trimmed
   */
  WhitespaceMap ws_flags_;
  bool quote_escape_ = false;
  bool field_has_double_quote_ = false;

  /** Where we are in the current data block */
  size_t data_pos_ = 0;

  /** Whether or not an attempt to find Unicode BOM has been made */
  bool unicode_bom_scan_ = false;
  bool utf8_bom_ = false;

  /** Where complete rows should be pushed to */
  RowCollection* records_ = nullptr;

  bool WsFlag(const char ch) const noexcept {
    return ws_flags_.data()[ch + 128];
  }

  size_t& CurrentRowStart() {
    return this->current_row_.data_start_;
  }

  void ParseField() noexcept;

  /** Finish parsing the current field */
  void PushField();

  /** Finish parsing the current row */
  void PushRow();

  /** Handle possible Unicode byte order mark */
  void TrimUtf8Bom();
};

template <typename TStream>
class StreamParser : public IBasicCSVParser {
 public:
  StreamParser(TStream& source, const CSVFormat& format,
               const ColNamesPtr& col_names = nullptr)
      : IBasicCSVParser(format, col_names), source_(std::move(source)) {};
  StreamParser(TStream& source, ParseFlagMap parse_flags,
               WhitespaceMap ws_flags)
      : IBasicCSVParser(parse_flags, ws_flags), source_(std::move(source)) {};

  ~StreamParser() = default;

  void Next(size_t bytes) {
    if (Eof()) { return; }
    ResetDataPtr();
    this -> data_ptr_->data_ptr = std::make_shared<std::string>();
    if (source_size_ == 0) {
      auto start = source_.tellg();
      source_.seekg(0, std::ios::end);
      auto end = source_.tellg();
      source_.seekg(0, std::ios::beg);
      source_size_ = end - start;
    }

    size_t length = std::min(source_size_ - stream_pos_, bytes);
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(length);
    source_.seekg(stream_pos_, std::ios::beg);
    source_.read(buffer.get(), length);
    stream_pos_ = source_.tellg();
    ((std::string*)(data_ptr_->data_ptr.get()))->assign(buffer.get(), length);

    this ->data_ptr_->data = *((std::string*)(data_ptr_->data_ptr.get()));

    this->current_row_ = CSVRow(this->data_ptr_);
    size_t remainder = Parse();
    if (stream_pos_ == source_size_ || NoChunk()) {
      eof_ = true;
      EndFeed();
    } else {
      this->stream_pos_ -= (length - remainder);
    }
  }

 private:
  TStream source_;
  size_t stream_pos_ = 0;
};

class MmapParser : public IBasicCSVParser {
 public:
  MmapParser(std::string_view filename, const CSVFormat& format,
             const ColNamesPtr& col_names = nullptr)
      : IBasicCSVParser(format, col_names) {
    this->filename_ = filename.data();
    this->source_size_ = GetFileSize(filename);
  };

  ~MmapParser() {}

  void Next(size_t bytes) override;

 private:
  std::string filename_;
  size_t mmap_pos_ = 0;
};

#endif  // CSV_PARSER_HPP