#include <exception>
#include <fstream>
#include <initializer_list>
#include <ios>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

///
/// @brief Custom exception for insertion of rows with invalid width.
///
class InvalidRowWidthException : public std::exception {
public:
  InvalidRowWidthException(const int max_width, const int row_width)
      : message_{"Invalid attempt to insert row of width " +
                 std::to_string(row_width) + " when maximum allowed width is " +
                 std::to_string(max_width)} {}

  const char *what() const override { return this->message_.c_str(); };

private:
  std::string message_;
};

///
/// @brief Custom exception for insertion of a header row at non-zero index.
///
class InvalidHeaderRowInsertionException : public std::exception {
public:
  InvalidHeaderRowInsertionException(const int current_row_count)
      : message_{"Invalid attempt to set header row at row index " +
                 std::to_string(current_row_count + 1)} {};

  const char *what() const override { return this->message_.c_str(); };

private:
  std::string message_;
};

///
/// @brief Custom exception for out of bound row access.
///
class OutOfBoundRowAccessException : public std::exception {
public:
  OutOfBoundRowAccessException(const int num_rows, const int index)
      : message_{"Failed to access row at index " + std::to_string(index) +
                 " because the Csv only contains " + std::to_string(num_rows) +
                 " rows"} {};

  const char *what() const override { return this->message_.c_str(); };

private:
  std::string message_;
};

///
/// @brief Custom exception for out of bound row access.
///
class OutOfBoundFieldAccessException : public std::exception {
public:
  OutOfBoundFieldAccessException(const int width, const int index)
      : message_{"Failed to access field value at index " +
                 std::to_string(index) + " because the Row only contains " +
                 std::to_string(width) + " fields"} {};

  const char *what() const override { return this->message_.c_str(); };

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
  std::string GetValue(const char separator) const {
    auto escaped_value{this->value_};
    // Quote value if it contains the separator, a new line character but only
    // if it is not yet quoted.
    if ((this->value_.find(separator) != std::string::npos ||
         this->value_.find('\n') != std::string::npos) &&
        this->value_[0] != '"' &&
        this->value_[this->value_.size() - 1] != '"') {
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
          escaped_value.insert(std::distance(escaped_value.begin(), it), 1,
                               '"');
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
  Row(std::initializer_list<Field> &values) {
    for (const auto &value : values) {
      this->fields_.push_back(value);
    }
  }

  ///
  /// @brief Enable vector-like value access for fields in the row.
  ///
  std::string &operator[](int index) {
    if (this->Width() < index + 1) {
      throw OutOfBoundFieldAccessException(static_cast<int>(this->Width()),
                                           index);
    }
    this->field_value_cache_ = this->fields_.at(index).GetValue();
    return this->field_value_cache_;
  }

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
    if (this->RowCount() < index + 1) {
      throw OutOfBoundRowAccessException(static_cast<int>(this->RowCount()),
                                         index);
    }
    return this->rows_.at(index);
  }

  ///
  /// @brief Adds a new data row to the CSV.
  ///
  void AddDataRow(std::initializer_list<Field> values) {
    const Row row(values);
    if (this->IsWidthInitialized() && (this->MaxWidth() != row.Width())) {
      throw InvalidRowWidthException(static_cast<int>(this->MaxWidth()),
                                     static_cast<int>(row.Width()));
    }
    this->InitializeWidth(row.Width());
    this->PushRow(row);
  }

  ///
  /// @brief Adds a header row to the CSV. When calling this method after
  /// AddDataRow() an InvalidHeaderRowInsertionException will be trown.
  ///
  void AddHeaderRow(std::initializer_list<Field> values) {
    if (!this->Empty()) {
      throw InvalidHeaderRowInsertionException(
          static_cast<int>(this->RowCount()));
    }
    const Row header_row(values);
    this->InitializeWidth(header_row.Width());
    this->PushRow(header_row);
    this->SetHasHeaderRow();
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
  /// @brief Returns true if the Csv contains no Rows, otherwise false.
  /// It also returns false if the Csv only contains a header row.
  ///
  bool Empty() const {
    if (!this->has_header_row_) {
      return this->rows_.empty();
    }
    return ((this->rows_.size() - 1) != 0);
  }

  ///
  /// @brief Prints the whole CSV data to standard output including the header
  /// row if present.
  ///
  void Print() {
    for (const auto &row : this->rows_) {
      for (auto it = row.Begin(); it != row.End(); ++it) {
        std::cout << it->GetValue(this->separator_);
        if ((it + 1) != row.End()) {
          std::cout << this->separator_;
        }
      }
      std::cout << "\n";
    }
  }

  ///
  /// @brief Prints the whole CSV data to standard output without the header
  /// row.
  ///
  void PrintDataOnly() {
    auto start_pos =
        this->has_header_row_ ? this->rows_.begin() + 1 : this->rows_.begin();
    for (auto it = start_pos; it != this->End(); ++it) {
      for (auto field = it->Begin(); field != it->End(); ++field) {
        std::cout << field->GetValue(this->separator_);
        if ((field + 1) != it->End()) {
          std::cout << this->separator_;
        }
      }
      std::cout << "\n";
    }
  }

  ///
  /// @brief Saves the CSV to a file.
  ///
  void SaveToFile(const std::string &file_path) {
    std::ofstream file_stream;
    file_stream.open(file_path);
    for (auto row = this->Begin(); row != this->End(); ++row) {
      for (auto field = row->Begin(); field != row->End(); ++field) {
        file_stream << field->GetValue(this->separator_);
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
  size_t max_width_ = 0;
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
    if (this->IsWidthInitialized() == false) {
      this->max_width_ = width;
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

  ///
  /// @brief Returns the maximum allowed width for rows.
  /// Returns 0 if called before inserting the first row.
  ///
  size_t MaxWidth() const { return this->max_width_; }

  ///
  /// @brief Returns the number of rows.
  ///
  size_t RowCount() const { return this->rows_.size(); };
};

class CsvParser {
public:
  Csv Parse(const std::string file_path) {
    std::ifstream file_stream;
    file_stream.open(file_path);
    file_stream.seekg(0, std::ios::end);
    const auto file_length = file_stream.tellg();
    file_stream.seekg(0, std::ios::beg);
    char byte;
    while (file_stream.get(byte)) {
      std::cout << byte << "\n";
    }
    return Csv();
  }

private:
  Csv csv_;
};

int main(int argc, char *argv[]) {
  try {
    Csv csv;
    csv.AddHeaderRow({"one", "two", "three", "four"});
    csv.AddDataRow({"\"hello\"", "csv", 123, 1.234});
    csv.AddDataRow({"hel,lo", "cs\"v", 123, 1.234});
    csv.AddDataRow(
        {"I am a \"scentence\",\nwith a line break", "cs\"v", 123, 1.234});

    auto val = csv[0][0];
    std::cout << "value at at row index 0, at value index 0 = " << val << "\n";
    csv.Print();
    csv.PrintDataOnly();

    /*
    Csv csv2(';');
    csv2.AddHeaderRow({"one", "two", "three", "four"});
    csv2.AddDataRow({"\"hello\"", "csv\niscool", 123, 1.234});
    csv2.AddDataRow({"hel;lo", "csv", 123, 1.234});
    csv2.SaveToFile("./test.csv");*/

    /*   CsvParser csv_parser;
       csv_parser.Parse("test.csv");*/

  } catch (const std::exception &ex) {
    std::cout << ex.what() << "\n";
  }
  return 0;
}
