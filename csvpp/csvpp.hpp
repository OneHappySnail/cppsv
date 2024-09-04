//
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
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

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

///
/// @brief Exception for insertion of rows with invalid width.
///
class InvalidRowWidthException : public std::exception {
 public:
  InvalidRowWidthException(const int max_width, const int row_width)
      : message_{"Invalid attempt to insert row of width " + std::to_string(row_width) +
                 " when maximum allowed width is " + std::to_string(max_width)} {}

  const char *what() const noexcept override { return this->message_.c_str(); };

 private:
  std::string message_;
};

///
/// @brief Exception for insertion of a header row at non-zero index.
///
class InvalidHeaderRowInsertionException : public std::exception {
 public:
  InvalidHeaderRowInsertionException(const int current_row_count)
      : message_{"Invalid attempt to set header row at row index " + std::to_string(current_row_count + 1)} {};

  const char *what() const noexcept override { return this->message_.c_str(); };

 private:
  std::string message_;
};

///
/// @brief Exception for out of bound row access.
///
class OutOfBoundRowAccessException : public std::exception {
 public:
  OutOfBoundRowAccessException(const int num_rows, const int index)
      : message_{"Failed to access row at index " + std::to_string(index) + " because the Csv only contains " +
                 std::to_string(num_rows) + " rows"} {};

  const char *what() const noexcept override { return this->message_.c_str(); };

 private:
  std::string message_;
};

class EmptyCsvRowAccessException : public std::exception {
 public:
  EmptyCsvRowAccessException() = default;

  const char *what() const noexcept override { return "Failed to access row because the Csv is empty"; };
};

///
/// @brief Custom exception for out of bound row access.
///
class OutOfBoundFieldAccessException : public std::exception {
 public:
  OutOfBoundFieldAccessException(const int width, const int index)
      : message_{"Failed to access field value at index " + std::to_string(index) + " because the Row only contains " +
                 std::to_string(width) + " fields"} {};

  const char *what() const noexcept override { return this->message_.c_str(); };

 private:
  std::string message_;
};

///
/// @brief A Field represents an individual value in the CSV.
/// All values are stored as std::string.
///
class Field {
 public:
  ///
  /// @brief Constructor for a string value.
  ///
  Field(std::string value) : value_(value){};

  ///
  /// @brief Constructor for an integer value.
  ///
  Field(int value) : value_(std::to_string(value)){};

  ///
  /// @brief Constructor for a double value.
  ///
  Field(double value) : value_("") {
    // The stream gets rid of trailing zeros.
    std::stringstream ss;
    ss << value;
    this->value_ = ss.str();
  };

  ///
  /// @brief Constructor for a const char* value.
  ///
  Field(const char *value) : value_(value){};

  ///
  /// @brief Returns the field
  ///
  std::string GetValue() const { return this->value_; }

  ///
  /// @brief Returns the field value with escaped separator and/or new line
  /// characters.
  ///
  std::string GetValueEscaped(const char separator) const {
    auto escaped_value{this->value_};
    // Quote value if it contains the separator, a new line character but only
    // if it is not yet quoted.
    if ((this->value_.find(separator) != std::string::npos || this->value_.find('\n') != std::string::npos) &&
        this->value_[0] != '"' && this->value_[this->value_.size() - 1] != '"') {
      escaped_value = "\"" + this->value_ + "\"";
    }
    // Add additional quotes for quotes within the value
    if (escaped_value.find('"') != std::string::npos) {
      for (auto it = escaped_value.begin(); it != escaped_value.end(); ++it) {
        // Leave quotes at the beginning and end of the value
        if (it == escaped_value.begin() || it == (escaped_value.end() - 1)) {
          continue;
        }
        if (*it == '"') {
          escaped_value.insert(std::distance(escaped_value.begin(), it), 1, '"');
          ++it;
        }
      }
    }
    return escaped_value;
  }

 private:
  std::string value_;
};

///
/// @brief A Row represents an individual row in the CSV and contains Fields.
///
class Row {
 public:
  ///
  /// @brief Default constructor.
  ///
  Row() = default;

  ///
  /// @brief Constructor for easy field value insertion.
  ///
  Row(std::initializer_list<Field> values) {
    for (const auto &value : values) {
      this->fields_.push_back(value);
    }
  }

  ///
  /// @brief Enable vector-like value access for fields in the row.
  ///
  std::string &operator[](int index) {
    if (index > this->Width() - 1) {
      throw OutOfBoundFieldAccessException(static_cast<int>(this->Width()), index);
    }
    this->field_value_cache_ = this->fields_.at(index).GetValue();
    return this->field_value_cache_;
  }

  ///
  /// @brief Adds the value to the row
  ///
  void AddValue(const Field &field) { this->fields_.push_back(field); }

  ///
  /// @brief Returns the value at the field index.
  ///
  std::string ValueAt(const int index) {
    if (index > this->Width() - 1) {
      throw OutOfBoundFieldAccessException(static_cast<int>(this->Width()), index);
    }
    return this->fields_[static_cast<size_t>(index)].GetValue();
  }

  ///
  /// @brief Clears the row
  ///
  void Clear() { this->fields_.clear(); }

  ///
  /// @brief Returns an iterator pointing to the first field in the row.
  ///
  auto Begin() const { return this->fields_.begin(); };

  ///
  /// @brief Returns an iterator pointing to the end of the fields in the row.
  ///
  auto End() const { return this->fields_.end(); };

  ///
  /// @brief Returns the with aka. number of fields in the row.
  ///
  size_t Width() const { return this->fields_.size(); };

 private:
  std::vector<Field> fields_;
  std::string field_value_cache_;
};

///
/// @brief A CSV contains rows with fields and provides a simple interface to
/// build in-memory CSVs.
///
class Csv {
 public:
  ///
  /// @brief Default constructor creating a Csv with the default separator ","
  /// (comma).
  ///
  Csv() = default;

  ///
  /// @brief Constructor to create a Csv with a custom separator.
  ///
  Csv(const char separator) : separator_(separator){};

  ///
  /// @brief Enable vector-like row access.
  ///
  Row &operator[](int index) {
    if (index > this->RowCount() - 1) {
      throw OutOfBoundRowAccessException(static_cast<int>(this->RowCount()), index);
    }
    return this->rows_.at(index);
  }
  
  ///
  /// @brief Adds a new data row to the CSV.
  ///
  void AddDataRow(const Row &row) {
    if (this->IsWidthInitialized() && (this->AllowedWidth() != row.Width())) {
      throw InvalidRowWidthException(static_cast<int>(this->AllowedWidth()), static_cast<int>(row.Width()));
    }
    this->InitializeWidth(row.Width());
    this->PushRow(row);
  }

  ///
  /// @brief Adds a new data row to the CSV.
  ///
  void AddDataRow(std::initializer_list<Field> values) {
    const Row row(values);
    this->AddDataRow(row);
  }

  ///
  /// @brief Adds a header row to the CSV. When calling this method after
  /// AddDataRow() an InvalidHeaderRowInsertionException will be trown.
  ///
  void AddHeaderRow(std::initializer_list<Field> values) {
    if (!this->Empty()) {
      throw InvalidHeaderRowInsertionException(static_cast<int>(this->RowCount()));
    }
    const Row row(values);
    this->InitializeWidth(row.Width());
    this->PushRow(row);
    this->SetHasHeaderRow();
  }

  ///
  /// @brief Adds a header row to the CSV. When calling this method after
  /// AddDataRow() an InvalidHeaderRowInsertionException will be trown.
  ///
  void AddHeaderRow(const Row &row) {
    if (!this->Empty()) {
      throw InvalidHeaderRowInsertionException(static_cast<int>(this->RowCount()));
    }
    this->InitializeWidth(row.Width());
    this->PushRow(row);
    this->SetHasHeaderRow();
  }

  ///
  /// @brief Returns the row at the specified index. It throws an EmptyCsvRowAccessException when trying to get a row
  /// from an empty Csv and a OutOfBoundRowAccessException when trying to access a row that is outside the row count.
  ///
  Row RowAt(const int index) const {
    if (this->Empty()) {
      throw EmptyCsvRowAccessException();
    }
    if (this->RowCount() + 1 < index) {
      throw OutOfBoundRowAccessException(index, static_cast<int>(this->RowCount()));
    }
    return this->rows_[static_cast<size_t>(index)];
  }

  ///
  /// @brief Returns the data row at index. If the Csv has a header row and index = 0 is specified, the first row after
  /// the header row is returned. It throws an EmptyCsvRowAccessException when trying to get a row from an empty Csv and
  /// a OutOfBoundRowAccessException when trying to access a row that is outside the row count.
  ///
  Row DataRowAt(const int index) const {
    int first_data_index = this->has_header_row_ ? index + 1 : index;
    return this->RowAt(first_data_index);
  }

  ///
  /// @brief Returns the header row or an empty row if there is no header row. It throws an EmptyCsvRowAccessException
  /// when trying to get a row from an empty Csv.
  ///
  Row GetHeaderRow() const {
    if (this->Empty()) {
      throw EmptyCsvRowAccessException();
    }
    if (this->has_header_row_) {
      return this->rows_[0];
    }
    return Row();
  }

  ///
  /// @brief Returns an iterator pointing to the first Row in the Csv.
  ///
  auto Begin() const { return this->rows_.begin(); }

  ///
  /// @brief Returns an iterator pointing to the end of the rows in the Csv.
  ///
  auto End() const { return this->rows_.end(); }

  ///
  /// @brief Applies the callback to each row in the Csv including the header row if present.
  ///
  void ForEachRow(std::function<void(Row)> cb) {
    for (auto row = this->Begin(); row != this->End(); ++row) {
      cb(*row);
    }
  }

  ///
  /// @brief Applies the callback to each row in the Csv except the header row.
  ///
  void ForEachDataRow(std::function<void(Row)> cb) {
    auto start_pos = this->has_header_row_ ? this->rows_.begin() + 1 : this->rows_.begin();
    for (auto it = start_pos; it != this->End(); ++it) {
      cb(*it);
    }
  }

  ///
  /// @brief Returns true if the Csv contains no Rows, otherwise false.
  /// It also returns false if the Csv only contains a header row.
  ///
  bool Empty() const { return this->rows_.empty(); }

  ///
  /// @brief Returns the number of rows.
  ///
  size_t RowCount() const { return this->rows_.size(); };

  ///
  /// @brief Returns the maximum allowed width for rows.
  /// Returns 0 if called before inserting the first row.
  ///
  size_t AllowedWidth() const { return this->allowed_width_; }

  ///
  /// @brief Saves the CSV to a file.
  ///
  void SaveToFile(const std::string &file_path) {
    std::ofstream file_stream;
    file_stream.open(file_path);
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
  char separator_ = ',';
  size_t allowed_width_ = 0;
  std::vector<Row> rows_;

  ///
  /// @brief Sets the header row indicator to true.
  ///
  void SetHasHeaderRow() { this->has_header_row_ = true; }

  ///
  /// @brief Initializes the maximum allowed width for rows. The maximum allowed
  /// width is initialized with the width of the first inserted row through
  /// either AddHeaderRow() or AddDataRow().
  ///
  void InitializeWidth(const size_t width) {
    if (!this->IsWidthInitialized()) {
      this->allowed_width_ = width;
      this->is_width_initialized = true;
    }
  }

  ///
  /// @brief Returns true if the maximum allowed width is initialized, otherwise
  /// false.
  ///
  bool IsWidthInitialized() const { return this->is_width_initialized; }

  ///
  /// @brief Inserts a row.
  ///
  void PushRow(const Row &row) { this->rows_.push_back(row); }
};

class CsvParser {
 public:
  Csv Parse(const std::string file_path, const char separator = ',', bool has_header_row = true) {
    std::ifstream file_stream;
    file_stream.open(file_path);

    file_stream.seekg(0, std::ios::end);
    std::streamsize file_length = file_stream.tellg();
    file_stream.seekg(0, std::ios::beg);

    constexpr std::streamsize byte_buffer_size = 64 * 1014;
    std::vector<char> byte_buffer(byte_buffer_size);

    const char quote_char{'"'};
    bool is_quoted{false};
    std::string field_value{""};

    Csv csv;
    Row row;

    while (file_length > 0) {
      const std::streamsize bytes_to_read = std::min<std::streamsize>(file_length, byte_buffer_size);
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
            if (csv.Empty() && has_header_row) {
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
    return csv;
  }

 private:
  void SanitizeCellValue(std::string &value) {
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
