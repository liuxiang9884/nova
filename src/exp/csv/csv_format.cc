#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>

#include "nova/exp/csv/csv_format.hpp"


// void CSVFormat::AssertNoCharOverlap() {
//     auto delims = std::set<char>(this->possible_delimiters_.begin(),
//                                  this->possible_delimiters_.end()),
//          trims =
//              std::set<char>(this->trim_chars_.begin(), this->trim_chars_.end());
  
//     // Stores intersection of possible delimiters and trim characters
//     std::vector<char> intersection = {};
  
//     // Find which characters overlap, if any
//     std::set_intersection(delims.begin(), delims.end(), trims.begin(),
//                           trims.end(), std::back_inserter(intersection));
  
//     // Make sure quote character is not contained in possible delimiters
//     // or whitespace characters
//     if (delims.find(this->quote_char_) != delims.end() ||
//         trims.find(this->quote_char_) != trims.end()) {
//       intersection.push_back(this->quote_char_);
//     }
  
//     if (!intersection.empty()) {
//       std::string err_msg =
//           "There should be no overlap between the quote character, "
//           "the set of possible delimiters "
//           "and the set of whitespace characters. Offending characters: ";
  
//       // Create a pretty error message with the list of overlapping
//       // characters
//       for (size_t i = 0; i < intersection.size(); i++) {
//         err_msg += "'";
//         err_msg += intersection[i];
//         err_msg += "'";
  
//         if (i + 1 < intersection.size()) err_msg += ", ";
//       }
  
//       throw std::runtime_error(err_msg + '.');
//     }
// }
  
// CSVFormat& CSVFormat::Delimiter(char delim) {
//     this->possible_delimiters_ = {delim};
//     this->AssertNoCharOverlap();
//     return *this;
// }
  
// CSVFormat& CSVFormat::Delimiter(const std::vector<char>& delim) {
//     this->possible_delimiters_ = delim;
//     this->AssertNoCharOverlap();
//     return *this;
// }

// CSVFormat& CSVFormat::Quote(char quote) {
//     this->no_quote_ = false;
//     this->quote_char_ = quote;
//     this->AssertNoCharOverlap();
//     return *this;
// }

// CSVFormat& CSVFormat::Trim(const std::vector<char>& chars) {
//     this->trim_chars_ = chars;
//     this->AssertNoCharOverlap();
//     return *this;
// }

// CSVFormat& CSVFormat::ColNames(const std::vector<std::string>& names) {
//     this->col_names_ = names;
//     this->header_ = -1;
//     return *this;
// }

// CSVFormat& CSVFormat::HeaderRow(int row) {
//     if (row < 0) {
//         this->variable_column_policy_ = VariableColumnPolicy::KEEP;
//     }
//     this->header_ = row;
//     this->col_names_ = {};
//     return *this;
// }

// /** CSVFormat for guessing the delimiter */
// CSVFormat CSVFormat::GuessCSV() {
//     CSVFormat format;
//     format.Delimiter({',', '|', '\t', ';', '^'}).Quote('"').HeaderRow(0);
//     return format;
// }

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