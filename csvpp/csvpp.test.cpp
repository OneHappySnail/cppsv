#include "csvpp.hpp"

#include "gtest/gtest.h"

// Unit tests for the Field class public interface.

TEST(Field, StdStringConstructor) {
  csvpp::Field field("string");
  EXPECT_STREQ(field.GetValue().c_str(), "string");
}

TEST(Field, StdIntConstructor) {
  csvpp::Field field(111);
  EXPECT_STREQ(field.GetValue().c_str(), "111");
}

TEST(Field, StdDoubleConstructor) {
  csvpp::Field field(1.11);
  EXPECT_STREQ(field.GetValue().c_str(), "1.11");
}

TEST(Field, CharStarConstructor) {
  const char *value = "cstring";
  csvpp::Field field(value);
  EXPECT_STREQ(field.GetValue().c_str(), "cstring");
}

TEST(Field, GetValueEscapedLineBreak) {
  csvpp::Field field("line\nbreak");
  EXPECT_STREQ(field.GetValueEscaped(',').c_str(), "\"line\nbreak\"");
}

TEST(Field, GetValueEscapedLineSeparator) {
  csvpp::Field field("separated,value");
  EXPECT_STREQ(field.GetValueEscaped(',').c_str(), "\"separated,value\"");
}

TEST(Field, GetValueEscapedLineQuote) {
  csvpp::Field field("this\"value\"isquoted");
  EXPECT_STREQ(field.GetValueEscaped(',').c_str(), "this\"\"value\"\"isquoted");
}

TEST(Field, GetValueEscapedQuoted) {
  csvpp::Field field("\"quoted\"");
  EXPECT_STREQ(field.GetValueEscaped(',').c_str(), "\"quoted\"");
}

TEST(Field, GetValueEscapedQuotedQuote) {
  csvpp::Field field("\"this\"value\"isquoted\"");
  EXPECT_STREQ(field.GetValueEscaped(',').c_str(),
               "\"this\"\"value\"\"isquoted\"");
}

// Unit tests for the Row class public interface.

// Test fixture for the Row class
class RowTest : public testing::Test {
 protected:
  RowTest() {
    row0_.AddValue("value1");
    row0_.AddValue("value2");
    row0_.AddValue("value3");
  }

  csvpp::Row row0_;
};

TEST(RowTest, AddAndGetValue) {
  csvpp::Row row;
  row.AddValue("value1");
  EXPECT_STREQ(row.ValueAt(0).c_str(), "value1");
}

TEST(RowTest, OutOfBoundFieldAccessException) {
  csvpp::Row row({"value1", "value2", "value3"});
  bool did_catch_exception{false};
  try {
    auto value = row.ValueAt(4);
  } catch (csvpp::OutOfBoundFieldAccessException &ex) {
    if (ex.what()) {
      did_catch_exception = true;
    }
  }
  EXPECT_TRUE(did_catch_exception);
}

TEST(RowTest, InitializerListConstructorAndWidth) {
  csvpp::Row row({"value1", "value2", "value3"});
  EXPECT_EQ(static_cast<int>(row.GetWidth()), 3);
}

TEST(RowTest, ClearRow) {
  csvpp::Row row({"value1", "value2", "value3"});
  EXPECT_EQ(static_cast<int>(row.GetWidth()), 3);
  row.Clear();
  EXPECT_EQ(static_cast<int>(row.GetWidth()), 0);
}

TEST(RowTest, Iterators) {
  csvpp::Row row({"value1", "value2", "value3"});
  int counter{0};
  for (auto it = row.Begin(); it != row.End(); ++it) {
    ++counter;
  }
  EXPECT_EQ(counter, 3);
}

// Unit tests for Csv class public interface.

class CsvTest : public testing::Test {
 protected:
  CsvTest() {
    this->csv_with_header_.AddHeaderRow({"header1", "header2", "header3"});
    this->csv_with_header_.AddDataRow({"value1_1", "value1_2", "value1_3"});
    this->csv_with_header_.AddDataRow({"value2_1", "value2_2", "value2_3"});
    this->csv_with_header_.AddDataRow({"value3_1", "value3_2", "value3_3"});

    this->csv_data_only_.AddDataRow({"value1_1", "value1_2", "value1_3"});
    this->csv_data_only_.AddDataRow({"value2_1", "value2_2", "value2_3"});
    this->csv_data_only_.AddDataRow({"value3_1", "value3_2", "value3_3"});
  }

  csvpp::Csv csv_with_header_;
  csvpp::Csv csv_data_only_;
};

TEST_F(CsvTest, InvalidRowWidth) {
  bool did_catch_exception{false};
  try {
    csv_data_only_.AddDataRow({"value4_1", "value4_2", "value4_3", "value4_4"});
  } catch (csvpp::InvalidRowWidthException &ex) {
    if (ex.what()) {
      did_catch_exception = true;
    }
  }
  EXPECT_TRUE(did_catch_exception);
}

TEST_F(CsvTest, InvalidHeaderRowInsertionException) {
  bool did_catch_exception{false};
  try {
    csv_with_header_.AddHeaderRow({"header1", "header2", "header3"});
  } catch (csvpp::InvalidHeaderRowInsertionException &ex) {
    if (ex.what()) {
      did_catch_exception = true;
    }
  }
  EXPECT_TRUE(did_catch_exception);
}

TEST(CsvTest, EmptyCsvRowAccessException) {
  bool did_catch_exception{false};
  csvpp::Csv csv;
  try {
    auto value = csv.RowAt(1);
  } catch (csvpp::EmptyCsvRowAccessException &ex) {
    if (ex.what()) {
      did_catch_exception = true;
    }
  }
  EXPECT_TRUE(did_catch_exception);
}

TEST_F(CsvTest, OutOfBoundRowAccessException) {
  bool did_catch_exception{false};
  try {
    auto value = csv_data_only_.RowAt(3);
  } catch (csvpp::OutOfBoundRowAccessException &ex) {
    if (ex.what()) {
      did_catch_exception = true;
    }
  }
  EXPECT_TRUE(did_catch_exception);
}

TEST_F(CsvTest, GetHeaderRow) {
  bool is_header_row{true};
  auto header_row = csv_with_header_.GetHeaderRow();
  std::vector<std::string> header_row_should{"header1", "header2", "header3"};
  for (int i = 0; i < static_cast<int>(header_row.GetWidth()); ++i) {
    is_header_row = header_row.ValueAt(i) == header_row_should.at(i);
  }
  EXPECT_TRUE(is_header_row);
}

TEST_F(CsvTest, GetEmptyRowForHeadlesCsv) {
  auto header = csv_data_only_.GetHeaderRow();
  EXPECT_EQ(static_cast<int>(header.GetWidth()), 0);
}
