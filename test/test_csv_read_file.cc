#include "gtest/gtest.h"
#include <string>
#include "nova/exp/csv/csv_reader.hpp"
#include "nova/exp/csv/csv_utility.hpp"

TEST(CSVReadFileTest, GetColPositon) {
    auto format = CSVFormat::GuessCSV();
    int pos = GetColPos(
        "/home/lianyun/nova/data/examples/csv/20230614.csv", 
        "股票代號"
    );
    EXPECT_EQ(pos, 2);
   
};

TEST(CSVReadFileTest, CSVColNamesOverwrite) {
    std::vector<std::string> column_names = { "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "A10" };
    
    // Test against a variety of different CSVFormat objects
    std::vector<CSVFormat> formats = {};
    formats.push_back(CSVFormat::GuessCSV());
    formats.push_back(CSVFormat());
    formats.back().Delimiter(std::vector<char>({ ',', '\t', '|'}));
    formats.push_back(CSVFormat());
    formats.back().Delimiter(std::vector<char>({ ',', '~'}));

    for (auto& format_in : formats) {
        // Set up the CSVReader
        format_in.ColNames(column_names);
        CSVReader reader(std::string_view("/home/lianyun/nova/data/examples/csv/ints_comments.csv"), format_in);

        // Assert that column names weren't overwritten
        CSVFormat format_out = reader.GetFormat();
        EXPECT_EQ(reader.GetColNames(), column_names);
        EXPECT_EQ(format_out.GetDelim(), ',');
        EXPECT_EQ(format_out.GetHeader(), 5);
    }
};