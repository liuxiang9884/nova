#include <string_view>
#include <iostream>
#include <fstream>

#include "nova/exp/csv/csv_parser.hpp"

ParseFlagMap MakeParseFlags(char delimiter) {
  std::array<ParseFlags, 256> ret = {};
  for (int i = -128; i < 128; i++) {
    const int arr_idx = i + 128;
    char ch = char(i);

    if (ch == delimiter)
      ret[arr_idx] = ParseFlags::DELIMITER;
    else if (ch == '\r' || ch == '\n')
      ret[arr_idx] = ParseFlags::NEWLINE;
    else
      ret[arr_idx] = ParseFlags::NOT_SPECIAL;
  }

  return ret;
}

ParseFlagMap MakeParseFlags(char delimiter, char quote_char) {
  std::array<ParseFlags, 256> ret = MakeParseFlags(delimiter);
  ret[(size_t)quote_char + 128] = ParseFlags::QUOTE;
  return ret;
}

WhitespaceMap MakeWsFlags(const char* ws_chars, size_t n_chars) {
  std::array<bool, 256> ret = {};
  for (int i = -128; i < 128; i++) {
    const int arr_idx = i + 128;
    char ch = char(i);
    ret[arr_idx] = false;

    for (size_t j = 0; j < n_chars; j++) {
      if (ws_chars[j] == ch) {
        ret[arr_idx] = true;
      }
    }
  }

  return ret;
}

WhitespaceMap MakeWsFlags(const std::vector<char>& flags) {
  return MakeWsFlags(flags.data(), flags.size());
}

size_t GetFileSize(std::string_view filename) {
  std::ifstream infile(std::string(filename), std::ios::binary);
  const auto start = infile.tellg();
  infile.seekg(0, std::ios::end);
  const auto end = infile.tellg();
  return end - start;
}

struct MmapSource {
  void* data;
  size_t size;
  int fd;

  const char* begin() const {
    return static_cast<const char*>(data);
  }
  const char* end() const {
    return static_cast<const char*>(data) + size;
  }

  ~MmapSource() {
    if (data && data != MAP_FAILED) {
      munmap(data, size);
    }
    if (fd != -1) {
      close(fd);
    }
  }
  std::string_view Str() const {
    auto str = std::string_view(static_cast<const char*>(data), size);
    // std::cout << "MmapSource::Str() data: " << str << std::endl;
    return str;
    // std::string str = std::string(begin(), end());
    // return str;
  }
};

MmapSource MakeMmapSoure(std::string_view filename, size_t start, size_t length) {
  MmapSource result = {nullptr, 0, -1};
  std::string path(filename);
  result.fd = open(path.c_str(), O_RDONLY);
  if (result.fd == -1) {
    std::cerr << "Error opening file\n";
    return result;
  }

  struct stat sb;
  // fstat(result.fd, &sb);
  if (fstat(result.fd, &sb) == -1) {  // 檢查 fstat 返回值
    std::cerr << "Error getting file stats\n";
    close(result.fd);
    result.fd = -1;
    return result;
  }

  if (start >= static_cast<size_t>(sb.st_size)) {
    std::cerr << "Start position beyond file size\n";
    close(result.fd);
    result.fd = -1;
    return result;
  }
  result.size = std::min(static_cast<size_t>(sb.st_size - start), length);
  if (result.size == 0) { 
    close(result.fd);
    result.fd = -1;
    return result;
  }

  result.data = mmap(nullptr, result.size, PROT_READ, MAP_PRIVATE, result.fd, start);
  if (result.data == MAP_FAILED) {
    std::cerr << "Error mapping file\n";
    close(result.fd);
    result.fd = -1;
    result.data = nullptr;
  }

  return result;
}

std::string GetCSVHead(std::string_view filename, size_t file_size) {
  const size_t bytes = 500000;

  std::error_code error;
  size_t length = std::min((size_t)file_size, bytes);
  // auto mmap = mio::make_mmap_source(std::string(filename), 0, length, error);
  auto mmap = MakeMmapSoure(filename, 0, length);

  if (error) {
    throw std::runtime_error("Cannot open file " + std::string(filename));
  }

  return std::string(mmap.begin(), mmap.end());
}

std::string GetCSVHead(std::string_view filename) {
  return GetCSVHead(filename, GetFileSize(filename));
}



IBasicCSVParser::IBasicCSVParser(const CSVFormat& format,
                                 const ColNamesPtr& col_names)
    : col_names_(col_names) {
  if (format.no_quote_) {
    this->parse_flags_ = MakeParseFlags(format.GetDelim());
  } else {
    this->parse_flags_ = MakeParseFlags(format.GetDelim(), format.quote_char_);
  }

  this->ws_flags_ =
      MakeWsFlags(format.trim_chars_.data(), format.trim_chars_.size());
}

inline void IBasicCSVParser::EndFeed() {
  // using ParseFlags;

  bool empty_last_field =
      this->data_ptr_ && !this->data_ptr_->data.empty() &&
      (ParseFlag(this->data_ptr_->data.back()) == ParseFlags::DELIMITER ||
       ParseFlag(this->data_ptr_->data.back()) == ParseFlags::QUOTE);

  // Push field
  if (this->field_length_ > 0 || empty_last_field) {
    this->PushField();
  }

  // Push row
  if (this->current_row_.Size() > 0) this->PushRow();
}

inline void IBasicCSVParser::ParseField() noexcept {
  // using ParseFlags;
  auto& in = this->data_ptr_->data;

  // Trim off leading whitespace
  while (data_pos_ < in.size() && WsFlag(in[data_pos_])) data_pos_++;

  if (field_start_ == UNINITIALIZED_FIELD)
    field_start_ = (int)(data_pos_ - CurrentRowStart());

  // Optimization: Since NOT_SPECIAL characters tend to occur in contiguous
  // sequences, use the loop below to avoid having to go through the outer
  // switch statement as much as possible
  while (data_pos_ < in.size() &&
         CompoundParseFlag(in[data_pos_]) == ParseFlags::NOT_SPECIAL)
    data_pos_++;

  field_length_ = data_pos_ - (field_start_ + CurrentRowStart());

  // Trim off trailing whitespace, this->field_length constraint matters
  // when field is entirely whitespace
  for (size_t j = data_pos_ - 1; WsFlag(in[j]) && this->field_length_ > 0; j--)
    this->field_length_--;
}

inline void IBasicCSVParser::PushField() {
  // Update
  if (field_has_double_quote_) {
    fields_->EmplaceBack(field_start_ == UNINITIALIZED_FIELD
                             ? 0
                             : static_cast<unsigned int>(field_start_),
                         field_length_, true);
    field_has_double_quote_ = false;

  } else {
    fields_->EmplaceBack(field_start_ == UNINITIALIZED_FIELD
                             ? 0
                             : static_cast<unsigned int>(field_start_),
                         field_length_);
  }

  current_row_.row_length_++;

  // Reset field state
  field_start_ = UNINITIALIZED_FIELD;
  field_length_ = 0;
}

inline size_t IBasicCSVParser::Parse() {
  // using internals::ParseFlags;

  this->quote_escape_ = false;
  this->data_pos_ = 0;
  this->CurrentRowStart() = 0;
  this->TrimUtf8Bom();

  auto& in = this->data_ptr_->data;
  while (this->data_pos_ < in.size()) {
    switch (CompoundParseFlag(in[this->data_pos_])) {
      case ParseFlags::DELIMITER:
        this->PushField();
        this->data_pos_++;
        break;

      case ParseFlags::NEWLINE:
        this->data_pos_++;

        // Catches CRLF (or LFLF, CRCRLF, or any other non-sensical combination
        // of newlines)
        while (this->data_pos_ < in.size() &&
               ParseFlag(in[this->data_pos_]) == ParseFlags::NEWLINE)
          this->data_pos_++;

        // End of record -> Write record
        this->PushField();
        this->PushRow();

        // Reset
        this->current_row_ =
            CSVRow(data_ptr_, this->data_pos_, fields_->Size());
        break;

      case ParseFlags::NOT_SPECIAL:
        this->ParseField();
        break;

      case ParseFlags::QUOTE_ESCAPE_QUOTE:
        if (data_pos_ + 1 == in.size())
          return this->CurrentRowStart();
        else if (data_pos_ + 1 < in.size()) {
          auto next_ch = ParseFlag(in[data_pos_ + 1]);
          if (next_ch >= ParseFlags::DELIMITER) {
            quote_escape_ = false;
            data_pos_++;
            break;
          } else if (next_ch == ParseFlags::QUOTE) {
            // Case: Escaped quote
            data_pos_ += 2;
            this->field_length_ += 2;
            this->field_has_double_quote_ = true;
            break;
          }
        }

        // Case: Unescaped single quote => not strictly valid but we'll keep it
        this->field_length_++;
        data_pos_++;

        break;

      default:  // Quote (currently not quote escaped)
        if (this->field_length_ == 0) {
          quote_escape_ = true;
          data_pos_++;
          if (field_start_ == UNINITIALIZED_FIELD && data_pos_ < in.size() &&
              !WsFlag(in[data_pos_]))
            field_start_ = static_cast<int>(data_pos_ - CurrentRowStart());
          break;
        }

        // Case: Unescaped quote
        this->field_length_++;
        data_pos_++;

        break;
    }
  }

  return this->CurrentRowStart();
}

inline void IBasicCSVParser::PushRow() {
  current_row_.row_length_ = fields_->Size() - current_row_.fields_start_;
  this->records_->push_back(std::move(current_row_));
}

inline void IBasicCSVParser::ResetDataPtr() {
  this->data_ptr_ = std::make_shared<RawCSVData>();
  this->data_ptr_->parse_flags = this->parse_flags_;
  this->data_ptr_->col_names = this->col_names_;
  this->fields_ = &(this->data_ptr_->fields);
}

inline void IBasicCSVParser::TrimUtf8Bom() {
  auto& data = this->data_ptr_->data;

  if (!this->unicode_bom_scan_ && data.size() >= 3) {
    if (data[0] == '\xEF' && data[1] == '\xBB' && data[2] == '\xBF') {
      this->data_pos_ += 3;  // Remove BOM from input string
      this->utf8_bom_ = true;
    }

    this->unicode_bom_scan_ = true;
  }
}

inline void MmapParser::Next(size_t bytes = 1024 * 1024) {
  // Reset parser state
  this->field_start_ = UNINITIALIZED_FIELD;
  this->field_length_ = 0;
  this->ResetDataPtr();

  // Create memory map
  size_t length = std::min(this->source_size_ - this->mmap_pos_, bytes);
  std::error_code error;
  this->data_ptr_->data_ptr = std::make_shared<MmapSource>(
      MakeMmapSoure(this->filename_, this->mmap_pos_, length));
  this->mmap_pos_ += length;
  if (error) throw error;

  auto mmap_ptr = static_cast<MmapSource*>(this->data_ptr_->data_ptr.get());

  // Create string view
  this->data_ptr_->data = mmap_ptr->Str();
  

  // Parse
  this->current_row_ = CSVRow(this->data_ptr_);
  size_t remainder = this->Parse();

  if (this->mmap_pos_ == this->source_size_ || NoChunk()) {
    this->eof_ = true;
    this->EndFeed();
  }

  this->mmap_pos_ -= (length - remainder);
}