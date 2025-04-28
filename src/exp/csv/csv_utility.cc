#include "nova/exp/csv/csv_utility.hpp"


 /** Shorthand function for parsing an in-memory CSV string
 *
 *  @return A collection of CSVRow objects
 *
 *  @par Example
 *  @snippet tests/test_read_csv.cpp Parse Example
 */
CSVReader Parse(std::string_view in, CSVFormat format) ;

/** Parses a CSV string with no headers
 *
 *  @return A collection of CSVRow objects
 */
CSVReader parse_no_header(std::string_view in) {
    CSVFormat format;
    format.HeaderRow(-1);

    return Parse(in, format);
}

/** Parse a RFC 4180 CSV string, returning a collection
 *  of CSVRow objects
 *
 *  @par Example
 *  @snippet tests/test_read_csv.cpp Escaped Comma
 *
 */
CSVReader operator ""_csv(const char* in, size_t n) ;

/** A shorthand for csv::parse_no_header() */
CSVReader operator ""_csv_no_header(const char* in, size_t n) ;



/** Get basic information about a CSV file
 *  @include programs/csv_info.cpp
 */
CSVFileInfo GetFileInfo(const std::string& filename) {
    CSVReader reader(filename);
    CSVFormat format = reader.GetFormat();
    for (auto it = reader.Begin(); it != reader.End(); ++it);

    CSVFileInfo info = {
        filename,
        reader.GetColNames(),
        format.GetDelim(),
        reader.n_rows_,
        reader.GetColNames().size()
    };

    return info;
}