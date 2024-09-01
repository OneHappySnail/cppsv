#include <exception>
#include <fstream>
#include <initializer_list>
#include <ios>
#include <iostream>
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
/// @brief A Field represents an individual value in the CSV.
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
  /// @brief Returns the field value with escaped separator and/or new line
  /// characters.
  ///
  std::string GetValue(const char separator) const {
    // Quote value if it contains the separator
    if (this->value_.find(separator) != std::string::npos) {
      return "\"" + this->value_ + "\"";
    }
    // Quote value if it contains line breaks
    if (this->value_.find('\n') != std::string::npos) {
      return "\"" + this->value_ + "\"";
    }
    return this->value_;
  }

private:
  std::string value_;
};

///
/// @brief A Row represents an individual row in the CSV and contains Fields.
///
class Row {
public:
  Row(std::initializer_list<Field> values) {
    for (const auto value : values) {
      this->fields_.push_back(value);
    }
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
};

///
/// @brief A CSV contains rows with fields and provides a simple interface to
/// build in-memory CSVs.
///
class Csv {
public:
  Csv() = default;

  Csv(const char separator) : separator_(separator){};

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
    if (!this->rows_.empty()) {
      throw InvalidHeaderRowInsertionException(
          static_cast<int>(this->rows_.size() + 1));
    }
    const Row header_row(values);
    this->InitializeWidth(header_row.Width());
    this->PushRow(header_row);
    this->SetHasHeaderRow();
  }

  ///
  /// @brief Prints the whole CSV data to stdandard output.
  ///
  void Print() {
    for (const auto row : this->rows_) {
      std::stringstream row_stream;
      for (auto it = row.Begin(); it != row.End(); ++it) {
        row_stream << it->GetValue(this->separator_);
        if ((it + 1) != row.End()) {
          row_stream << this->separator_;
        }
      }
      row_stream << "\n";
      std::cout << row_stream.str();
    }
  }

  ///
  /// @brief Saves the CSV to a file.
  ///
  void SaveToFile(const std::string &file_path) {
    std::ofstream file_stream(file_path);
    for (const auto row : this->rows_) {
      for (auto it = row.Begin(); it != row.End(); ++it) {
        file_stream << it->GetValue(this->separator_);
        if ((it + 1) != row.End()) {
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
    if (this->is_width_initialized == false) {
      this->max_width_ = width;
      is_width_initialized = true;
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
  ///
  size_t MaxWidth() const { return this->max_width_; }
};

class CsvParser {
public:
  void Parse(const std::string file_path) {
    std::ifstream file_stream;
    file_stream.open(file_path);

    file_stream.seekg(0, std::ios::end);
    auto file_length = file_stream.tellg();
    file_stream.seekg(0, std::ios::beg);

    const size_t buffer_size = 10;
    char buffer[buffer_size];

    while (file_stream.read(buffer, buffer_size)) {
      std::streamsize bytes_read = file_stream.gcount();
      std::cout.write(buffer, bytes_read);
      std::cout << "\n\n";
    }

    std::streamsize bytes_read = file_stream.gcount();
    if (bytes_read > 0) {
      std::cout.write(buffer, bytes_read);
    }
    file_stream.close();
  };

private:
  Csv csv_;
};

int main(int argc, char *argv[]) {
  try {
    /*Csv csv;
    csv.AddHeaderRow({"one", "two", "three", "four"});
    csv.AddDataRow({"\"hello\"", "csv", 123, 1.234});
    csv.AddDataRow({"hel,lo", "csv", 123, 1.234});
    csv.Print();
    Csv csv2(';');
    csv2.AddHeaderRow({"one", "two", "three", "four"});
    csv2.AddDataRow({"\"hello\"", "csv\niscool", 123, 1.234});
    csv2.AddDataRow({"hel;lo", "csv", 123, 1.234});
    csv2.SaveToFile("./test.csv");*/

    CsvParser csv_parser;
    csv_parser.Parse("test.csv");

  } catch (const std::exception &ex) {
    std::cout << ex.what() << "\n";
  }
  return 0;
}
