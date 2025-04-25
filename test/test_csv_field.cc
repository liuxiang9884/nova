#include <cmath>
#include <iostream>
#include <future>
#include <gtest/gtest.h>

#include "float_test_cases.hpp"
#include "nova/exp/csv/csv_row.hpp"


// Define type aliases for tests
typedef signed char SignedChar;
typedef short ShortInt;
typedef int Int;
typedef long long int LongLongInt;
typedef double Double;
typedef long double LongDouble;


// Test for string values
TEST(CSVFieldTest, StringValue) {
  std::string str = "Hello, World!";
  CSVField field(str);
  EXPECT_EQ(field.GetString(), str);
  EXPECT_EQ(field.Type(), DataType::CSV_STRING);
}

// Test for integer values
TEST(CSVFieldTest, IntegerValue) {
  CSVField field("42");
  EXPECT_EQ(field.Type(), DataType::CSV_INT8);
  EXPECT_EQ(field.Get<int32_t>(), 42);
  
}

// Test for floating-point values
TEST(CSVFieldTest, FloatValue) {
  CSVField field("3.14");
  EXPECT_EQ(field.Type(), DataType::CSV_DOUBLE);
  EXPECT_EQ(field.Get<double>(), 3.14);
}


// Test for null values
TEST(CSVFieldTest, NullValue) {
  CSVField field("");
  EXPECT_EQ(field.Type(), DataType::CSV_NULL);
  EXPECT_TRUE(field.IsNull());
}


// Test for whitespace values
TEST(CSVFieldTest, WhitespaceValue) {
  CSVField field("   ");
  EXPECT_EQ(field.Type(), DataType::CSV_NULL);
  EXPECT_TRUE(field.IsNull());
}

// Test for negative values
TEST(CSVFieldTest, NegativeValue) {
  CSVField field("-42");
  EXPECT_EQ(field.Type(), DataType::CSV_INT8);
  EXPECT_EQ(field.Get<int32_t>(), -42);
}


// Test for positive 64bit2 values
TEST(CSVFieldTest, Positive64BitValue) {
  CSVField field("9223372036854775807");
  EXPECT_EQ(field.Type(), DataType::CSV_INT64);
  EXPECT_EQ(field.Get<int64_t>(), 9223372036854775807);
}

// Test for Exponential notation
TEST(CSVFieldTest, ExponentialValue) {
  CSVField field("1.23e4");
  EXPECT_EQ(field.Type(), DataType::CSV_DOUBLE);
  EXPECT_EQ(field.Get<double>(), 12300.0);
}

// Test for Dynamic CSVField array 
TEST(DynamicRawCSVFieldArray_Emplace_Back, DynamicArray) {

  // Array size should be smaller than the number of items we want to push
  CSVFieldList field_list(500);
  size_t offset = 100;

  for (size_t i = 0; i < 9999; ++i) {
    field_list.EmplaceBack(i, i + offset);
    EXPECT_EQ(field_list[i].start, i);
    EXPECT_EQ(field_list[i].length, i + offset);
    EXPECT_EQ(field_list.Size(), i + 1);
  }

  for (size_t i = 0; i < 9999; ++i) {
    EXPECT_EQ(field_list[i].start, i);
    EXPECT_EQ(field_list[i].length, i + offset);
  }
}

TEST(CSVFieldArrayThreadSafety, ThreadSafety) {
  size_t offset = 100;
  CSVFieldList field_list(500);

  for (size_t i = 0; i < 9999; ++i) {
    field_list.EmplaceBack(i, i + offset);
    EXPECT_EQ(field_list[i].start, i);
    EXPECT_EQ(field_list[i].length, i + offset);
    EXPECT_EQ(field_list.Size(), i + 1);
  }

  constexpr size_t num_workers = 4;
  constexpr size_t chunk_size = 9999 / num_workers;
  std::vector<std::future<bool>> workers = {};

  for (size_t i = 0; i < num_workers; ++i) {
    size_t start = i * chunk_size;
    size_t end = start + chunk_size;
    workers.push_back(std::async([](const CSVFieldList& field_list,
                                   size_t start, size_t end, size_t offset) {
      for (size_t i = start; i < end; ++i) {
        if (field_list[i].start != i ||
            field_list[i].length != i + offset) {
          return false;
        }
      }
      return true;
    }, std::ref(field_list), start, end, offset));
  }
  for (size_t i = 9999; i < 1999; ++i) {
    field_list.EmplaceBack(i, i + offset);
    EXPECT_EQ(field_list[i].start, i);
    EXPECT_EQ(field_list[i].length, i + offset);
    EXPECT_EQ(field_list.Size(), i + 1);
  }

  for (auto& worker : workers) {
    EXPECT_TRUE(worker.get());
  }
}