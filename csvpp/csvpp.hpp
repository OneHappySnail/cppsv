//     _____    _____  __      __  _____    _____
//    / ____|  / ____| \ \    / / |  __ \  |  __ \
//   | |      | (___    \ \  / /  | |__) | | |__) |
//   | |       \___ \    \ \/ /   |  ___/  |  ___/
//   | |____   ____) |    \  /    | |      | |
//    \_____| |_____/      \/     |_|      |_|
//
//
// MIT License
//
// Copyright (c) 2024 Dino Pergola
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef CSVPP_HPP
#define CSVPP_HPP

#include <exception>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <ios>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace csvpp {

/// Custom exception
class CsvppException : public std::exception {
 public:
  CsvppException(const int error_code, const std::string message, ...)
      : error_code_(error_code), message_(std::move(message)){};

  const char *what() const noexcept override { return this->message_.c_str(); };

  /// The error code is helpful for testing.
  int ErrorCode() const { return this->error_code_; };

 private:
  int error_code_;
  std::string message_;
};

/// A Field represents an individual value in the CSV.
/// All values are stored as std::string.
class Field {
 public:
  /// Constructor for a string value.
  Field(std::string value) : value_(value){};

  /// Constructor for an integer value.
  Field(int value) : value_(std::to_string(value)){};

  /// Constructor for a double value.
  Field(double value) : value_("") {
    // The stream gets rid of trailing zeros.
    std::stringstream ss;
    ss << value;
    this->value_ = ss.str();
  };

  /// Constructor for a const char* value.
  Field(const char value[]) : value_(value){};

  /// Returns the field
  std::string GetValue() const { return this->value_; }

  /// Returns the field value with escaped separator and/or new line
  /// characters.
  std::string GetValueEscaped(const char separator) const {
    auto escaped_value{this->value_};
    this->QuoteValueIfNeeded(escaped_value, separator);
    this->EscapeQuotesWithinValue(escaped_value);
    return escaped_value;
  }

 private:
  std::string value_;

  // Quote value if it contains the separator or a new line character but only
  // if it is not yet quoted.
  void QuoteValueIfNeeded(std::string &value, const char separator) const {
    if ((this->value_.find(separator) != std::string::npos ||
         this->value_.find('\n') != std::string::npos) &&
        this->value_[0] != '"' &&
        this->value_[this->value_.size() - 1] != '"') {
      value = "\"" + this->value_ + "\"";
    }
  }

  // Add additional quotes for quotes within the value
  void EscapeQuotesWithinValue(std::string &value) const {
    if (value.find('"') != std::string::npos) {
      for (auto character = value.begin(); character != value.end();
           ++character) {
        // Leave quotes at the beginning and end of the value
        if (character == value.begin() || character == (value.end() - 1)) {
          continue;
        }
        if (*character == '"') {
          value.insert(std::distance(value.begin(), character), 1, '"');
          ++character;
        }
      }
    }
  }
};

/// A Row represents an individual row in the CSV and contains Fields.
class Row {
 public:
  /// Default constructor.
  Row() = default;

  /// Constructor for easy field value insertion.
  Row(std::initializer_list<Field> values) {
    for (const auto &value : values) {
      this->fields_.push_back(value);
    }
  }

  /// Adds the value to the row
  void AddValue(const Field &field) { this->fields_.push_back(field); }

  /// Returns the value at the field index. It throws a CsvppException if the
  /// index is out of bounds.
  std::string ValueAt(const int index) const {
    if (index > this->GetWidth() - 1) {
      throw CsvppException(100, "Row::GetWidth index out of bound error");
    }
    return this->fields_[static_cast<size_t>(index)].GetValue();
  }

  /// Clears the row
  void Clear() { this->fields_.clear(); }

  /// Returns an iterator pointing to the first field in the row.
  auto Begin() const { return this->fields_.begin(); };

  /// Returns an iterator pointing to the end of the fields in the row.
  auto End() const { return this->fields_.end(); };

  /// Returns the with aka. number of fields in the row.
  size_t GetWidth() const { return this->fields_.size(); };

  /// Returns true if the Row does not contain any fields, otherwise false.
  bool IsEmpty() const { return this->fields_.empty(); };

 private:
  std::vector<Field> fields_;
  std::string field_value_cache_;
};

/// A CSV contains rows with fields and provides a simple interface to build
/// in-memory CSVs.
class Csv {
 public:
  /// Constructor to create a Csv with a custom separator.
  Csv(const char separator = ',') : separator_(separator){};

  /// Adds a new data row to the CSV.
  void AddDataRow(const Row &row) {
    if (this->IsWidthInitialized() &&
        (this->GetAllowedWidth() != row.GetWidth())) {
      throw CsvppException(200, "Csv::AddDataRow invalid width error");
    }
    this->InitializeWidth(row.GetWidth());
    this->PushRow(row);
  }

  /// Adds a new data row to the CSV.
  void AddDataRow(std::initializer_list<Field> values) {
    const Row row(values);
    this->AddDataRow(row);
  }

  /// Adds a header row to the CSV. When calling this method after AddDataRow()
  /// an CsvppException will be trown.
  void AddHeaderRow(const Row &row) {
    if (!this->IsEmpty()) {
      throw CsvppException(
          300, "Csv::AddHeaderRow csv must only have one header row");
    }
    this->InitializeWidth(row.GetWidth());
    this->PushRow(row);
    this->SetHasHeaderRow();
  }

  /// Adds a header row to the CSV. When calling this method after AddDataRow()
  /// an CsvppException will be trown.
  void AddHeaderRow(std::initializer_list<Field> values) {
    const Row row(values);
    this->AddHeaderRow(row);
  }

  /// Returns the row at the specified index. It throws an
  /// CsvppException when trying to get a row from an empty Csv and
  /// a CsvppException when trying to access a row that is
  /// outsidethe row count.
  Row RowAt(const int index) const {
    if (this->IsEmpty()) {
      throw CsvppException(400, "Csv::RowAt empty csv row access exception");
    }
    if (this->GetRowCount() + 1 < index) {
      throw CsvppException(100, "Row::RowAt index out of bound error");
    }
    return this->rows_[static_cast<size_t>(index)];
  }

  /// Returns the data row at index. If the Csv has a header row and index = 0
  /// is specified, the first row after the header row is returned. It throws an
  /// CsvppException when trying to get a row from an empty Csv and
  /// a CsvppException when trying to access a row that is outside
  /// the row count.
  Row DataRowAt(const int index) const {
    int first_data_index = this->has_header_row_ ? index + 1 : index;
    return this->RowAt(first_data_index);
  }

  /// Returns the header row or an empty row if there is no header row.
  /// It throws an CsvppException when trying to get a row from an
  /// empty Csv.
  Row GetHeaderRow() const {
    if (this->IsEmpty()) {
      throw CsvppException(400, "Csv::RowAt empty csv row access exception");
    }
    if (this->has_header_row_) {
      return this->rows_[0];
    }
    return Row();
  }

  /// Returns an iterator pointing to the first Row in the Csv.
  auto Begin() const { return this->rows_.begin(); }

  /// Returns an iterator pointing to the end of the rows in the Csv.
  auto End() const { return this->rows_.end(); }

  /// Returns true if the Csv contains no Rows, otherwise false.
  /// It also returns false if the Csv only contains a header row.
  bool IsEmpty() const { return this->rows_.empty(); }

  /// Returns the number of rows.
  size_t GetRowCount() const { return this->rows_.size(); };

  /// Returns the maximum allowed width for rows.
  /// Returns 0 if called before inserting the first row.
  size_t GetAllowedWidth() const { return this->allowed_width_; }

  /// Applies the callback to each row in the Csv including the header row if
  /// present.
  void ForEachRow(std::function<void(Row)> cb) const {
    for (auto row = this->Begin(); row != this->End(); ++row) {
      cb(std::move(*row));
    }
  }

  /// Applies the callback to each row in the Csv except the header row.
  void ForEachDataRow(std::function<void(Row)> cb) const {
    auto start_pos =
        this->has_header_row_ ? this->rows_.begin() + 1 : this->rows_.begin();
    for (auto row = start_pos; row != this->End(); ++row) {
      cb(std::move(*row));
    }
  }

  /// Saves the CSV to a file.
  void SaveToFile(const std::string &file_path) const {
    std::ofstream file_stream;
    file_stream.open(file_path);
    if (!file_stream.is_open()) {
      throw CsvppException(500, "Csv::SaveToFile Error opening file");
    }
    for (auto row = this->Begin(); row != this->End(); ++row) {
      for (auto field = row->Begin(); field != row->End(); ++field) {
        file_stream << field->GetValueEscaped(this->separator_);
        if (std::next(field) != row->End()) {
          file_stream << this->separator_;
        }
      }
      file_stream << "\n";
    }
    file_stream.close();
  }

 private:
  bool has_header_row_ = false;
  bool is_width_initialized = false;
  char separator_;
  size_t allowed_width_ = 0;
  std::vector<Row> rows_;

  /// Sets the header row indicator to true.
  void SetHasHeaderRow() { this->has_header_row_ = true; }

  /// Initializes the maximum allowed width for rows. The maximum allowed width
  /// is initialized with the width of the first inserted row through either
  /// AddHeaderRow() or AddDataRow().
  void InitializeWidth(const size_t width) {
    if (!this->IsWidthInitialized()) {
      this->allowed_width_ = width;
      this->is_width_initialized = true;
    }
  }

  /// Returns true if the maximum allowed width is initialized, otherwise false.
  bool IsWidthInitialized() const { return this->is_width_initialized; }

  /// @brief Inserts a row.
  void PushRow(const Row &row) { this->rows_.push_back(row); }
};

/// CsvParser can read csv text files and parse their content into a Csv.
class CsvParser {
 public:
  /// Parses the specified file into a Csv using the default separator ',' if
  /// not specified otherwise. By default it expects the csv file to have a
  /// header row.
  Csv Parse(const std::string file_path, const char separator = ',',
            bool has_header_row = true) {
    std::ifstream file_stream;
    file_stream.open(file_path);

    file_stream.seekg(0, std::ios::end);
    std::streamsize file_length = file_stream.tellg();
    file_stream.seekg(0, std::ios::beg);

    constexpr std::streamsize byte_buffer_size = 64 * 1014;
    std::vector<char> byte_buffer(byte_buffer_size);

    constexpr char quote_char{'"'};
    bool is_quoted{false};
    std::string field_value{""};

    Csv csv;
    Row row;

    while (file_length > 0) {
      const std::streamsize bytes_to_read =
          std::min<std::streamsize>(file_length, byte_buffer_size);
      file_stream.read(byte_buffer.data(), bytes_to_read);
      for (size_t i = 0; i < static_cast<int>(bytes_to_read); ++i) {
        if (byte_buffer[i] == quote_char) {
          if (field_value.empty() || (field_value[0] == quote_char)) {
            is_quoted = !is_quoted;
          }
          field_value += byte_buffer[i];
        } else if (byte_buffer[i] == separator) {
          if (is_quoted) {
            field_value += byte_buffer[i];
          } else {
            is_quoted = false;
            this->SanitizeCellValue(field_value);
            Field field(field_value);
            row.AddValue(field);
            field_value.clear();
          }
        } else if (byte_buffer[i] == '\n') {
          if (is_quoted) {
            field_value += byte_buffer[i];
          } else {
            is_quoted = false;
            this->SanitizeCellValue(field_value);
            Field field(field_value);
            row.AddValue(field);
            if (csv.IsEmpty() && has_header_row) {
              csv.AddHeaderRow(row);
            } else {
              csv.AddDataRow(row);
            }
            row.Clear();
            field_value.clear();
          }
        } else {
          field_value += byte_buffer[i];
        }
      }
      file_length -= bytes_to_read;
    }
    // TODO Handle end of file
    return csv;
  }

 private:
  // Removes quotes at the beginning and end of a value and replaces escaped
  // quotes with single quotes. The expected quote character is '"'.
  void SanitizeCellValue(std::string &value) const {
    // Remove begin and end quotes,
    if (value[0] == '"' && value[value.size() - 1] == '"') {
      value = value.substr(1, value.size() - 2);
    }
    // Replace double quotes with single quote.
    size_t pos{0};
    while ((pos = value.find("\"\"", pos)) != std::string::npos) {
      value.replace(pos, 2, "\"");
      ++pos;
    }
  }
};

}  // namespace csvpp
#endif  // CSVPP_HPP
