#include <cmath>
#include <iostream>
#include <future>
#include <gtest/gtest.h>

#include "nova/exp/csv/csv_objects.hpp"
#include "nova/exp/csv/csv_format.hpp"

static std::string err_preamble = "There should be no overlap between "
    "the quote character, the set of possible "
    "delimiters and the set of whitespace characters.";

TEST(CSVFormatTest, NoOverlap) {
  CSVFormat format;
  bool err_caught = false;
    // tab
    try {
        format.Delimiter('\t').Quote('"').Trim({'\t'});
        
    } catch (const std::runtime_error& e) {
        err_caught = true;
    }
    EXPECT_TRUE(err_caught);
    //tab eith multiple other characters
    try {
        format.Delimiter({ ',', 't' }).Quote('"').Trim({ ' ', '\t' });
    } catch (const std::runtime_error& e) {
        err_caught = true;
    }
    EXPECT_TRUE(err_caught);

    // Repeat quote
    try {
        format.Delimiter({ ',', '"' }).Quote('"').Trim({ ' ', '\t' });
    } catch (const std::runtime_error& e) {
        err_caught = true;
    }
    EXPECT_TRUE(err_caught);

    // multiple offenders
    try {
        format.Delimiter({ ',', '\t', ' '}).Quote('"').Trim({ ' ', '\t' });
    } catch (const std::runtime_error& e) {
        err_caught = true;
    }
    EXPECT_TRUE(err_caught);
}