#ifndef CSV_DATATYPE_HPP
#define CSV_DATATYPE_HPP

#include <cassert>
#include <cmath>
#include <limits>
#include <string>

enum class DataType {
  UNKNOWN = -1,
  CSV_NULL,   /**< Empty string */
  CSV_STRING, /**< Non-numeric string */
  CSV_INT8,   /**< 8-bit integer */
  CSV_INT16,  /**< 16-bit integer */
  CSV_INT32,  /**< 32-bit integer  */
  CSV_INT64,  /**< 64-bit integer */
  CSV_FLOAT,  /**< 32-bit floating point value */
  CSV_DOUBLE  /**< Floating point value */
};

// Forward declarations
template <typename T>
long double Pow10(const T& n) noexcept;
template <>
long double Pow10(const unsigned& n) noexcept;
DataType GetDataType(std::string_view str, long double* out = nullptr,
                     const char decimalSymbol = '.');
inline DataType DetermineIntegralType(const long double& number) noexcept;

inline DataType ProcessPotentialExponential(std::string_view exponential_part,
                                            const long double& coeff,
                                            long double* const out) {
  long double exponent = 0;
  auto result = GetDataType(exponential_part, &exponent);

  // Exponents in scientific notation should not be decimal numbers
  if (result >= DataType::CSV_INT8 && result < DataType::CSV_DOUBLE) {
    if (out) *out = coeff * Pow10(exponent);
    return DataType::CSV_DOUBLE;
  }

  return DataType::CSV_STRING;
}

inline DataType GetDataType(std::string_view str, long double* out,
                            const char decimalSymbol) {
  // Empty string --> NULL
  if (str.size() == 0) return DataType::CSV_NULL;

  bool ws_allowed = true, dot_allowed = true, digit_allowed = true,
       is_negative = false, has_digit = false, prob_float = false;

  unsigned places_after_decimal = 0;
  long double integral_part = 0, decimal_part = 0;

  for (size_t i = 0, ilen = str.size(); i < ilen; i++) {
    const char& current = str[i];

    switch (current) {
      case ' ':
        if (!ws_allowed) {
          if (isdigit(str[i - 1])) {
            digit_allowed = false;
            ws_allowed = true;
          } else {
            // Ex: '510 123 4567'
            return DataType::CSV_STRING;
          }
        }
        break;
      case '+':
        if (!ws_allowed) {
          return DataType::CSV_STRING;
        }
        break;
      case '-':
        if (!ws_allowed) {
          // Ex: '510-123-4567'
          return DataType::CSV_STRING;
        }

        is_negative = true;
        break;
        // case decimalSymbol: not allowed because decimalSymbol is not a
        // literal, it is handled in the default block
      case 'e':
      case 'E':
        // Process scientific notation
        if (prob_float || (i && i + 1 < ilen && isdigit(str[i - 1]))) {
          size_t exponent_start_idx = i + 1;
          prob_float = true;

          // Strip out plus sign
          if (str[i + 1] == '+') {
            exponent_start_idx++;
          }

          return ProcessPotentialExponential(
              str.substr(exponent_start_idx),
              is_negative ? -(integral_part + decimal_part)
                          : integral_part + decimal_part,
              out);
        }

        return DataType::CSV_STRING;
        break;
      default:
        short digit = static_cast<short>(current - '0');
        if (digit >= 0 && digit <= 9) {
          // Process digit
          has_digit = true;

          if (!digit_allowed)
            return DataType::CSV_STRING;
          else if (ws_allowed)  // Ex: '510 456'
            ws_allowed = false;

          // Build current number
          if (prob_float)
            decimal_part += digit / Pow10(++places_after_decimal);
          else
            integral_part = (integral_part * 10) + digit;
        }
        // case decimalSymbol: not allowed because decimalSymbol is not a
        // literal.
        else if (dot_allowed && current == decimalSymbol) {
          dot_allowed = false;
          prob_float = true;
        } else {
          return DataType::CSV_STRING;
        }
    }
  }

  // No non-numeric/non-whitespace characters found
  if (has_digit) {
    long double number = integral_part + decimal_part;
    if (out) {
      *out = is_negative ? -number : number;
    }

    return prob_float ? DataType::CSV_DOUBLE : DetermineIntegralType(number);
  }
  // Just whitespace
  return DataType::CSV_NULL;
}

template <typename T>
inline long double Pow10(const T& n) noexcept {
  long double multiplicand = n > 0 ? 10 : 0.1, ret = 1;

  // Make all numbers positive
  T iterations = n > 0 ? n : -n;

  for (T i = 0; i < iterations; i++) {
    ret *= multiplicand;
  }

  return ret;
}

template <>
inline long double Pow10(const unsigned& n) noexcept {
  long double ret = 1;
  for (unsigned i = 0; i < n; i++) {
    ret *= 10;
  }
  return ret;
}
template <size_t Bytes>
long double GetIntMax() {
  static_assert(Bytes == 1 || Bytes == 2 || Bytes == 4 || Bytes == 8,
                "Bytes must be a power of 2 below 8.");

  if constexpr (sizeof(signed char) == Bytes) {
    return (long double)std::numeric_limits<signed char>::max();
  }

  if constexpr (sizeof(short) == Bytes) {
    return (long double)std::numeric_limits<short>::max();
  }

  if constexpr (sizeof(int) == Bytes) {
    return (long double)std::numeric_limits<int>::max();
  }

  if constexpr (sizeof(long int) == Bytes) {
    return (long double)std::numeric_limits<long int>::max();
  }

  if constexpr (sizeof(long long int) == Bytes) {
    return (long double)std::numeric_limits<long long int>::max();
  }

  // HEDLEY_UNREACHABLE();
}

/** Given a byte size, return the largest number than can be stored in
 *  an unsigned integer of that size
 */
template <size_t Bytes>
long double GetUintMax() {
  static_assert(Bytes == 1 || Bytes == 2 || Bytes == 4 || Bytes == 8,
                "Bytes must be a power of 2 below 8.");

  if constexpr (sizeof(unsigned char) == Bytes) {
    return (long double)std::numeric_limits<unsigned char>::max();
  }

  if constexpr (sizeof(unsigned short) == Bytes) {
    return (long double)std::numeric_limits<unsigned short>::max();
  }

  if constexpr (sizeof(unsigned int) == Bytes) {
    return (long double)std::numeric_limits<unsigned int>::max();
  }

  if constexpr (sizeof(unsigned long int) == Bytes) {
    return (long double)std::numeric_limits<unsigned long int>::max();
  }

  if constexpr (sizeof(unsigned long long int) == Bytes) {
    return (long double)std::numeric_limits<unsigned long long int>::max();
  }

  // HEDLEY_UNREACHABLE();
}

/** Largest number that can be stored in a 8-bit integer */
inline long double CSV_INT8_MAX = GetIntMax<1>();

/** Largest number that can be stored in a 16-bit integer */
inline long double CSV_INT16_MAX = GetIntMax<2>();

/** Largest number that can be stored in a 32-bit integer */
inline long double CSV_INT32_MAX = GetIntMax<4>();

/** Largest number that can be stored in a 64-bit integer */
inline long double CSV_INT64_MAX = GetIntMax<8>();

/** Largest number that can be stored in a 8-bit ungisned integer */
inline long double CSV_UINT8_MAX = GetIntMax<1>();

/** Largest number that can be stored in a 16-bit unsigned integer */
inline long double CSV_UINT16_MAX = GetIntMax<2>();

/** Largest number that can be stored in a 32-bit unsigned integer */
inline long double CSV_UINT32_MAX = GetIntMax<4>();

/** Largest number that can be stored in a 64-bit unsigned integer */
inline long double CSV_UINT64_MAX = GetIntMax<8>();

inline DataType DetermineIntegralType(const long double& number) noexcept {
  // We can assume number is always non-negative
  // assert(number >= 0);

  if (number <= CSV_INT8_MAX)
    return DataType::CSV_INT8;
  else if (number <= CSV_INT16_MAX)
    return DataType::CSV_INT16;
  else if (number <= CSV_INT32_MAX)
    return DataType::CSV_INT32;
  else if (number <= CSV_INT64_MAX)
    return DataType::CSV_INT64;
  else  // Conversion to long long will cause an overflow
    return DataType::CSV_DOUBLE;
}

#endif  // CSV_READER_DATATYPE_H