#ifndef CSV_OBJECTS_HPP
#define CSV_OBJECTS_HPP

#include <array>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <fcntl.h>


enum class ParseFlags {
  QUOTE_ESCAPE_QUOTE =
      0,           /**< A quote inside or terminating a quote_escaped field */
  QUOTE = 2 | 1,   /**< Characters which may signify a quote escape */
  NOT_SPECIAL = 4, /**< Characters with no special meaning or escaped delimiters
                      and newlines */
  DELIMITER = 4 | 2,  /**< Characters which signify a new field */
  NEWLINE = 4 | 2 | 1 /**< Characters which signify a new row */
};

/** An array which maps ASCII chars to a parsing flag */
using ParseFlagMap = std::array<ParseFlags, 256>;

/** An array which maps ASCII chars to a flag indicating if it is whitespace */
using WhitespaceMap = std::array<bool, 256>;

/** Transform the ParseFlags given the context of whether or not the current
 *  field is quote escaped */
constexpr ParseFlags QuoteEscapeFlag(ParseFlags flag,
                                     bool quote_escape) noexcept {
  return (ParseFlags)((int)flag & ~((int)ParseFlags::QUOTE * quote_escape));
}





 


#endif  // CSV_OBJECTS_HPP