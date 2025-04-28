#include <stdio.h> // For remove()
#include <sstream>
#include <queue>
#include <list>
#include <deque>
#include <string>
#include "gtest/gtest.h"

#include "nova/exp/csv/csv.hpp"

TEST(NumericConverter, TestWriteCSV) {
    {
        EXPECT_EQ(NumDigits(99.0) , 2);
        EXPECT_EQ(NumDigits(100.0) , 3);
    }

    { //Large numbers
        // Large numbers: integer larger than uint64 capacity
        EXPECT_EQ(ToString(200000000000000000000.0) , "200000000000000000000.0");
        EXPECT_EQ(ToString(310000000000000000000.0) , "310000000000000000000.0");
    }
}

TEST(CustomPrecision, TestWriteCSV) {
    // Test setting precision
    EXPECT_EQ(ToString(1.234) , "1.23400");
    EXPECT_EQ(ToString(20.0045) , "20.00450");

    SetDecimalPlaces(2);
    EXPECT_EQ(ToString(1.234) , "1.23");

    // Reset
    SetDecimalPlaces(5);


    {
        EXPECT_EQ(ToString(-0.25) , "-0.25000");
        EXPECT_EQ(ToString(-0.625) , "-0.62500");
        EXPECT_EQ(ToString(-0.666) , "-0.66600");
    }


    {
        EXPECT_EQ(ToString(10.0) , "10.0");
        EXPECT_EQ(ToString(100.0) , "100.0");
        EXPECT_EQ(ToString(1000.0) ,"1000.0");
        EXPECT_EQ(ToString(10000.0) , "10000.0");
        EXPECT_EQ(ToString(100000.0) , "100000.0");
        EXPECT_EQ(ToString(1000000.0) , "1000000.0");
    }
}
TEST(BasicCSVWritingCases, TestWriteCSV) {
    std::stringstream output, correct;
    auto writer = MakeCSVWriter(output);

    { // Escaped Comma
        writer << std::array<std::string, 1>({"Furthermore, this should be quoted."});
        correct << "\"Furthermore, this should be quoted.\"\n";
    }

    { // Escaped Quote
        writer << std::array<std::string, 1>({"\"What does it mean to be RFC 4180 compliant?\" she asked."});
        correct << "\"\"\"What does it mean to be RFC 4180 compliant?\"\" she asked.\"\n";
    }

    { // Escaped Newline
        writer << std::array<std::string, 1>({"Line 1\nLine2"});
        correct << "\"Line 1\nLine2\"\n";
    }

    { // Leading and Trailing Quote Escape
        writer << std::array<std::string, 1>({"\"\""});
        correct << "\"\"\"\"\"\"\n";
    }

    { // Quote minimal
        writer << std::array<std::string, 1>({ "This should not be quoted" });
        correct << "This should not be quoted";
    }

    correct << std::endl;
    EXPECT_EQ(output.str() , correct.str());
}

TEST(CSVQuoteAll, TestWriteCSV) {
    std::stringstream output, correct;
    auto writer = MakeCSVWriter(output, false);

    writer << std::array<std::string, 1>({ "This should be quoted" });
    correct << "\"This should be quoted\"" << std::endl;

    EXPECT_EQ(output.str() , correct.str());
}

// //! [CSV Writer Example]
TEST(CSVWriterOperation, TestWriteCSV) {
    std::stringstream correct_comma, correct_tab;

    // Build correct strings
    correct_comma << "A,B,C" << std::endl << "\"1,1\",2,3" << std::endl;
    correct_tab << "A\tB\tC" << std::endl << "1,1\t2\t3" << std::endl;

    // Test input
    auto test_row_1 = std::deque<std::string>({ "A", "B", "C" }),
        test_row_2 = std::deque<std::string>({ "1,1", "2", "3" });

    { // CSV Writer
        std::stringstream output;
        auto csv_writer = MakeCSVWriter(output);
        csv_writer << test_row_1 << test_row_2;

        EXPECT_EQ(output.str() , correct_comma.str());
    }

    { // TSV Writer
        std::stringstream output;
        auto tsv_writer = MakeTSVWriter(output);
        tsv_writer << test_row_1 << test_row_2;

        EXPECT_EQ(output.str() , correct_tab.str());
    }
}
// //! [CSV Writer Example]

// //! [CSV Writer Tuple Example]
struct Time {
    std::string hour;
    std::string minute;

    operator std::string() const {
        std::string ret = hour;
        ret += ":";
        ret += minute;
        
        return ret;
    }
};


TEST(CSVTuple, TestWriteCSV) {

    std::string time = "5:30";
    std::stringstream output, correct_output;
    auto csv_writer = MakeCSVWriter(output);

    csv_writer << std::make_tuple("One", 2, "Three", 4.0, time)
        << std::make_tuple("One", (short)2, "Three", 4.0f, time)
        << std::make_tuple(-1, -2.0)
        << std::make_tuple(20.2, -20.3, -20.123)
        << std::make_tuple(0.0, 0.0f, 0);

    correct_output << "One,2,Three,4.0,5:30" << std::endl
        << "One,2,Three,4.0,5:30" << std::endl
        << "-1,-2.0" << std::endl
        << "20.19999,-20.30000,-20.12300" << std::endl
        << "0.0,0.0,0" << std::endl;

        EXPECT_EQ(output.str() , correct_output.str());
}