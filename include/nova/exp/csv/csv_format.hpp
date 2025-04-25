#ifndef CSV_FORMAT_HPP
#define CSV_FORMAT_HPP

#include <vector>
#include <string>
#include <stdexcept>


/** Determines how to handle rows that are shorter or longer than the majority
 */
enum class VariableColumnPolicy { THROW = -1, IGNORE_ROW = 0, KEEP = 1 };

class CSVFormat {
    public:
     CSVFormat() = default;
     CSVFormat& Delimiter(char delim);
     CSVFormat& Delimiter(const std::vector<char>& delims);
     CSVFormat& Trim(const std::vector<char>& ws);
     CSVFormat& Quote(char quote);
     CSVFormat& ColNames(const std::vector<std::string>& names);
     CSVFormat& HeaderRow(int row);
   
     bool IsQuotingEnabled() const {
       return !this->no_quote_;
     }
   
     char GetQuoteChar() const {
       return this->quote_char_;
     }
     int GetHeader() const {
       return this->header_;
     }
   
     std::vector<char> GetPossibleDelims() const {
       return this->delimiters_;
     }
     std::vector<char> GetTrimChars() const {
       return this->trim_chars_;
     }
   
     bool GuessDelim() {
       return this->possible_delimiters_.size() > 1;
     }
   
     char GetDelim() const {
       if (this->delimiters_.size() > 1) {
         return this->delimiters_[0];
       }
       return this->possible_delimiters_[0];
     }
   
     void AssertNoCharOverlap();
     static CSVFormat GuessCSV();
   
     std::vector<char> delimiters_ = {','};
     std::vector<char> trim_chars_ = {};
     int header_ = 0;
     bool no_quote_ = false;
     char quote_char_ = '"';
     std::vector<std::string> col_names_ = {};
     std::vector<char> possible_delimiters_ = {','};
     /**< Allow variable length columns? */
     VariableColumnPolicy variable_column_policy_ = VariableColumnPolicy::IGNORE_ROW;
};

inline void CSVFormat::AssertNoCharOverlap() {
    auto delims = std::set<char>(this->possible_delimiters_.begin(),
                                 this->possible_delimiters_.end()),
         trims =
             std::set<char>(this->trim_chars_.begin(), this->trim_chars_.end());
  
    // Stores intersection of possible delimiters and trim characters
    std::vector<char> intersection = {};
  
    // Find which characters overlap, if any
    std::set_intersection(delims.begin(), delims.end(), trims.begin(),
                          trims.end(), std::back_inserter(intersection));
  
    // Make sure quote character is not contained in possible delimiters
    // or whitespace characters
    if (delims.find(this->quote_char_) != delims.end() ||
        trims.find(this->quote_char_) != trims.end()) {
      intersection.push_back(this->quote_char_);
    }
  
    if (!intersection.empty()) {
      std::string err_msg =
          "There should be no overlap between the quote character, "
          "the set of possible delimiters "
          "and the set of whitespace characters. Offending characters: ";
  
      // Create a pretty error message with the list of overlapping
      // characters
      for (size_t i = 0; i < intersection.size(); i++) {
        err_msg += "'";
        err_msg += intersection[i];
        err_msg += "'";
  
        if (i + 1 < intersection.size()) err_msg += ", ";
      }
  
      throw std::runtime_error(err_msg + '.');
    }
}
  
inline CSVFormat& CSVFormat::Delimiter(char delim) {
    this->possible_delimiters_ = {delim};
    this->AssertNoCharOverlap();
    return *this;
}
  
inline CSVFormat& CSVFormat::Delimiter(const std::vector<char>& delim) {
    this->possible_delimiters_ = delim;
    this->AssertNoCharOverlap();
    return *this;
}

inline CSVFormat& CSVFormat::Quote(char quote) {
    this->no_quote_ = false;
    this->quote_char_ = quote;
    this->AssertNoCharOverlap();
    return *this;
}

inline CSVFormat& CSVFormat::Trim(const std::vector<char>& chars) {
    this->trim_chars_ = chars;
    this->AssertNoCharOverlap();
    return *this;
}

inline CSVFormat& CSVFormat::ColNames(const std::vector<std::string>& names) {
    this->col_names_ = names;
    this->header_ = -1;
    return *this;
}

inline CSVFormat& CSVFormat::HeaderRow(int row) {
    if (row < 0) {
        this->variable_column_policy_ = VariableColumnPolicy::KEEP;
    }
    this->header_ = row;
    this->col_names_ = {};
    return *this;
}

/** CSVFormat for guessing the delimiter */
inline CSVFormat CSVFormat::GuessCSV() {
    CSVFormat format;
    format.Delimiter({',', '|', '\t', ';', '^'}).Quote('"').HeaderRow(0);
    return format;
}

// CSVFormat::CSVFormat(const CSVFormat& format) {
//     this->possible_delimiters_ = {',', '|', '\t', ';', '^'};
//     this->quote_char_ = '"';
//     this->header_ = 0;
// }
// CSVFormat& CSVFormat::operator=(const CSVFormat& format) {
//     this->possible_delimiters_ = {',', '|', '\t', ';', '^'};
//     this->quote_char_ = '"';
//     this->header_ = 0;
//     this->no_quote_ = format.no_quote_;
//     this->delimiters_ = format.delimiters_;
//     this->trim_chars_ = format.trim_chars_;
//     this->col_names_ = format.col_names_;
//     this->header_ = format.header_;
//     return *this;
// }

#endif // CSV_FORMAT_HPP