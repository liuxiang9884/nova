#ifndef CSV_Writer_HPP
#define CSV_Writer_HPP

#include <fstream>
#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

static int DECIMAL_PLACES = 5;



/**
 * Calculate the absolute value of a number
 */
template<typename T = int>
inline T CSVAbs(T x) {
    return abs(x);
}

template<>
inline int CSVAbs(int x) {
    return abs(x);
}

template<>
inline long int CSVAbs(long int x) {
    return labs(x);
}

template<>
inline long long int CSVAbs(long long int x) {
    return llabs(x);
}

template< typename T,
std::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
int NumDigits(T x)
{
    x = CSVAbs(x);

    int digits = 0;

    while (x >= 1) {
        x /= 10;
        digits++;
    }

    return digits;
}

// template<>
// inline float CSVAbs(float x) {
//     return fabsf(x);
// }

// template<>
// inline double CSVAbs(double x) {
//     return fabs(x);
// }

// template<>
// inline long double CSVAbs(long double x) {
//     return fabsl(x);
// }


/** to_string() for unsigned integers */
template<typename T,
    std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
inline std::string ToString(T value) {
    std::string digits_reverse = "";

    if (value == 0) return "0";

    while (value > 0) {
        digits_reverse += (char)('0' + (value % 10));
        value /= 10;
    }

    return std::string(digits_reverse.rbegin(), digits_reverse.rend());
}

/** to_string() for signed integers */
template<
    typename T,
    std::enable_if_t<std::is_integral<T>::value && std::is_signed<T>::value, int> = 0
>
inline std::string ToString(T value) {
    if (value >= 0)
        return ToString((size_t)value);

    return "-" + ToString((size_t)(value * -1));
}

template<class OutputStream, char Delim, char Quote, bool Flush>
class DelimWriter {
public:
    /** Construct a DelimWriter over the specified output stream
     *
     *  @param  _out           Stream to write to
     *  @param  _quote_minimal Limit field quoting to only when necessary
    */

    DelimWriter(OutputStream& _out, bool _quote_minimal = true)
        : out(_out), quote_minimal(_quote_minimal) {};

    /** Construct a DelimWriter over the file
     *
     *  @param[out] filename  File to write to
     */
    DelimWriter(const std::string& filename) : DelimWriter(std::ifstream(filename)) {};

    /** Destructor will flush remaining data
     *
     */
    ~DelimWriter() {
        out.flush();
    }

    /** Format a sequence of strings and write to CSV according to RFC 4180
     *
     *  @warning This does not check to make sure row lengths are consistent
     *
     *  @param[in]  record          Sequence of strings to be formatted
     *
     *  @return  The current DelimWriter instance (allowing for operator chaining)
     */
    template<typename T, size_t Size>
    DelimWriter& operator<<(const std::array<T, Size>& record) {
        for (size_t i = 0; i < Size; i++) {
            out << CSVEscape(record[i]);
            if (i + 1 != Size) out << Delim;
        }

        EndOut();
        return *this;
    }

    /** @copydoc operator<< */
    template<typename... T>
    DelimWriter& operator<<(const std::tuple<T...>& record) {
        this->write_tuple<0, T...>(record);
        return *this;
    }

    /**
     * @tparam T A container such as std::vector, std::deque, or std::list
     * 
     * @copydoc operator<<
     */
    template<
        typename T, typename Alloc, template <typename, typename> class Container,

        // Avoid conflicting with tuples with two elements
        csv::enable_if_t<std::is_class<Alloc>::value, int> = 0
    >
        DelimWriter& operator<<(const Container<T, Alloc>& record) {
        const size_t ilen = record.size();
        size_t i = 0;
        for (const auto& field : record) {
            out << CSVEscape(field);
            if (i + 1 != ilen) out << Delim;
            i++;
        }

        EndOut();
        return *this;
    }

    /** Flushes the written data
     *
     */
    void Flush() {
        out.flush();
    }

private:
    template<
        typename T,
        csv::enable_if_t<
            !std::is_convertible<T, std::string>::value
            && !std::is_convertible<T, csv::string_view>::value
        , int> = 0
    >
    std::string CSVEscape(T in) {
        return ToString(in);
    }

    template<
        typename T,
        csv::enable_if_t<
            std::is_convertible<T, std::string>::value
            || std::is_convertible<T, csv::string_view>::value
        , int> = 0
    >
    std::string CSVEscape(T in) {
        IF_CONSTEXPR(std::is_convertible<T, csv::string_view>::value) {
            return _CSVEscape(in);
        }
        
        return _CSVEscape(std::string(in));
    }

    std::string _CSVEscape(csv::string_view in) {
        /** Format a string to be RFC 4180-compliant
         *  @param[in]  in              String to be CSV-formatted
         *  @param[out] quote_minimal   Only quote fields if necessary.
         *                              If False, everything is quoted.
         */

        // Do we need a quote escape
        bool quote_escape = false;

        for (auto ch : in) {
            if (ch == Quote || ch == Delim || ch == '\r' || ch == '\n') {
                quote_escape = true;
                break;
            }
        }

        if (!quote_escape) {
            if (quote_minimal) return std::string(in);
            else {
                std::string ret(1, Quote);
                ret += in.data();
                ret += Quote;
                return ret;
            }
        }

        // Start initial quote escape sequence
        std::string ret(1, Quote);
        for (auto ch: in) {
            if (ch == Quote) ret += std::string(2, Quote);
            else ret += ch;
        }

        // Finish off quote escape
        ret += Quote;
        return ret;
    }

    /** Recurisve template for writing std::tuples */
    template<size_t Index = 0, typename... T>
    typename std::enable_if<Index < sizeof...(T), void>::type WriteTuple(const std::tuple<T...>& record) {
        out << CSVEscape(std::get<Index>(record));

        IF_CONSTEXPR (Index + 1 < sizeof...(T)) out << Delim;

        this->WriteTuple<Index + 1>(record);
    }

    /** Base case for writing std::tuples */
    template<size_t Index = 0, typename... T>
    typename std::enable_if<Index == sizeof...(T), void>::type WriteTuple(const std::tuple<T...>& record) {
        (void)record;
        EndOut();
    }

    /** Ends a line in 'out' and flushes, if Flush is true.*/
    void EndOut() {
        out_ << '\n';
        IF_CONSTEXPR(Flush) out.flush();
    }

    OutputStream & out_;
    bool quote_minimal_;
};

/** An alias for csv::DelimWriter for writing standard CSV files
 *
 *  @sa csv::DelimWriter::operator<<()
 *
 *  @note Use `csv::make_csv_writer()` to in instatiate this class over
 *        an actual output stream.
 */
template<class OutputStream, bool Flush = true>
using CSVWriter = DelimWriter<OutputStream, ',', '"', Flush>;

/** Class for writing tab-separated values files
*
 *  @sa csv::DelimWriter::write_row()
 *  @sa csv::DelimWriter::operator<<()
 *
 *  @note Use `csv::make_tsv_writer()` to in instatiate this class over
 *        an actual output stream.
 */
template<class OutputStream, bool Flush = true>
using TSVWriter = DelimWriter<OutputStream, '\t', '"', Flush>;

/** Return a csv::CSVWriter over the output stream */
template<class OutputStream>
inline CSVWriter<OutputStream> MakeCSVWriter(OutputStream& out, bool quote_minimal=true) {
    return CSVWriter<OutputStream>(out, quote_minimal);
}

/** Return a buffered csv::CSVWriter over the output stream (does not auto flush) */
template<class OutputStream>
inline CSVWriter<OutputStream, false> MakeCSVWriterBuffered(OutputStream& out, bool quote_minimal=true) {
    return CSVWriter<OutputStream, false>(out, quote_minimal);
}

/** Return a csv::TSVWriter over the output stream */
template<class OutputStream>
inline TSVWriter<OutputStream> MakeTSVWriter(OutputStream& out, bool quote_minimal=true) {
    return TSVWriter<OutputStream>(out, quote_minimal);
}

/** Return a buffered csv::TSVWriter over the output stream (does not auto flush) */
template<class OutputStream>
inline TSVWriter<OutputStream, false> MakeTSVWriterBuffered(OutputStream& out, bool quote_minimal=true) {
    return TSVWriter<OutputStream, false>(out, quote_minimal);
}

#endif //CSV_Writer_HPP