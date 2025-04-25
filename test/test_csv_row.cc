#include "gtest/gtest.h"
// #include "csv_row.hpp"
// #include "csv_reader.hpp"
#include "nova/exp/csv/csv_utility.hpp"


TEST(CSVRowTest, TestCSVRow) {
    {
        auto reader = "A,B,C,D\r\n"
                    "Col1,Col2,Col3,Col4"_csv;

        CSVRow row;
        reader.ReadRow(row);
        // bool err_caught = false;

        EXPECT_EQ(row.Size(), 4);
        EXPECT_EQ(row[1].GetString(), "Col2");
        EXPECT_EQ(row["B"].GetString(), "Col2");
        EXPECT_EQ(row[2].GetString(), "Col3");
        EXPECT_EQ(row["C"].GetString(), "Col3");

        EXPECT_EQ(std::vector<std::string>(row),  std::vector<std::string>({ "Col1", "Col2", "Col3", "Col4" }));

    }
    {
        auto reader = "A,B,C,D\r\n"
                    "1,2,3,3.14"_csv;

        CSVRow row;
        reader.ReadRow(row);

        // EXPECT_EQ(row["A"].Get(), "1");
        EXPECT_EQ(row["B"].Get<int>() , 2);
        EXPECT_EQ(row["C"].GetString() , "3");
        EXPECT_EQ(row["D"].Get<long double>(), 3.14L);
    }
}

  