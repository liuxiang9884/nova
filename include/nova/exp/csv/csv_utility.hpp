#ifndef CSV_UTILITY_HPP
#define CSV_UTILITY_HPP

#include <deque>
#include <mutex>
#include "csv_reader.hpp"
#include "csv_objects.hpp"


CSVReader Parse(std::string_view in, CSVFormat format = CSVFormat());
CSVReader ParseNoHeader(std::string_view in);

/**
 *  Find the position of a column in a CSV file or CSV_NOT_FOUND otherwise
 *
 *  @param[in] filename  Path to CSV file
 *  @param[in] col_name  Column whose position we should resolve
 *  @param[in] format    Format of the CSV file
 */
inline int GetColPos(std::string_view filename, std::string_view col_name, 
  const CSVFormat& format = CSVFormat::GuessCSV()) {
  CSVReader reader(filename, format);
  return reader.IndexOf(col_name);
}

struct CSVFileInfo {
  std::string filename;               /**< Filename */
  std::vector<std::string> col_names; /**< CSV column names */
  char delim;                         /**< Delimiting character */
  size_t n_rows;                      /**< Number of rows in a file */
  size_t n_cols;                      /**< Number of columns in a CSV */
};


// /** @name Shorthand Parsing Functions
//  *  @brief Convienience functions for parsing small strings
//  */
// ///@{
// CSVReader operator ""_csv(const char*, size_t);
// CSVReader operator ""_csv_no_header(const char*, size_t);
/** Parse a RFC 4180 CSV string, returning a collection
 *  of CSVRow objects
 *
 *  @par Example
 *  @snippet tests/test_read_csv.cpp Escaped Comma
 *
 */
inline CSVReader operator ""_csv(const char* in, size_t n) {
  return Parse(std::string_view(in, n));
}

/** A shorthand for csv::parse_no_header() */
inline CSVReader operator ""_csv_no_header(const char* in, size_t n) {
  return ParseNoHeader(std::string_view(in, n));
}



//  /** Shorthand function for parsing an in-memory CSV string
//  *
//  *  @return A collection of CSVRow objects
//  *
//  *  @par Example
//  *  @snippet tests/test_read_csv.cpp Parse Example
//  */
inline CSVReader Parse(std::string_view in, CSVFormat format) {
  std::stringstream stream(in.data());
  return CSVReader(stream, format);
}

/** Parses a CSV string with no headers
*
*  @return A collection of CSVRow objects
*/
inline CSVReader ParseNoHeader(std::string_view in) {
  CSVFormat format;
  format.HeaderRow(-1);

  return Parse(in, format);
}
///@}

/** @name Utility Functions */
///@{
std::unordered_map<std::string, DataType> csv_data_types(const std::string&);
CSVFileInfo GetFileInfo(const std::string& filename);

// constexpr const int UNINITIALIZED_FIELD = -1;



#endif  // CSV_UTILITY_HPP