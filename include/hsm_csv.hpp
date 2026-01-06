/**
 * @file hsm_csv.hpp
 * @brief CSV parsing and writing utilities for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - CSV parsing with configurable delimiters
 * - CSV writing with proper escaping
 * - Row and column access
 * - Type conversion utilities
 */

#ifndef HSM_CSV_HPP
#define HSM_CSV_HPP

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace dfsmshsm {
namespace csv {

// ============================================================================
// CSV Row
// ============================================================================

/**
 * @brief Represents a single CSV row
 */
class CsvRow {
public:
    CsvRow() = default;
    explicit CsvRow(std::vector<std::string> fields) : fields_(std::move(fields)) {}
    
    /**
     * @brief Get number of fields
     */
    [[nodiscard]] std::size_t size() const noexcept { return fields_.size(); }
    
    /**
     * @brief Check if row is empty
     */
    [[nodiscard]] bool empty() const noexcept { return fields_.empty(); }
    
    /**
     * @brief Access field by index
     */
    [[nodiscard]] const std::string& operator[](std::size_t index) const {
        return fields_.at(index);
    }
    
    /**
     * @brief Access field by index (mutable)
     */
    [[nodiscard]] std::string& operator[](std::size_t index) {
        return fields_.at(index);
    }
    
    /**
     * @brief Get field with default value
     */
    [[nodiscard]] std::string get(std::size_t index, 
                                  const std::string& defaultValue = "") const {
        return index < fields_.size() ? fields_[index] : defaultValue;
    }
    
    /**
     * @brief Get field as integer
     */
    [[nodiscard]] std::optional<int> getInt(std::size_t index) const {
        if (index >= fields_.size()) return std::nullopt;
        try {
            return std::stoi(fields_[index]);
        } catch (...) {
            return std::nullopt;
        }
    }
    
    /**
     * @brief Get field as int64
     */
    [[nodiscard]] std::optional<int64_t> getInt64(std::size_t index) const {
        if (index >= fields_.size()) return std::nullopt;
        try {
            return std::stoll(fields_[index]);
        } catch (...) {
            return std::nullopt;
        }
    }
    
    /**
     * @brief Get field as double
     */
    [[nodiscard]] std::optional<double> getDouble(std::size_t index) const {
        if (index >= fields_.size()) return std::nullopt;
        try {
            return std::stod(fields_[index]);
        } catch (...) {
            return std::nullopt;
        }
    }
    
    /**
     * @brief Get field as boolean
     */
    [[nodiscard]] std::optional<bool> getBool(std::size_t index) const {
        if (index >= fields_.size()) return std::nullopt;
        std::string val = fields_[index];
        std::transform(val.begin(), val.end(), val.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        if (val == "true" || val == "yes" || val == "1") return true;
        if (val == "false" || val == "no" || val == "0") return false;
        return std::nullopt;
    }
    
    /**
     * @brief Add field to row
     */
    void push_back(std::string field) {
        fields_.push_back(std::move(field));
    }
    
    /**
     * @brief Get all fields
     */
    [[nodiscard]] const std::vector<std::string>& fields() const noexcept {
        return fields_;
    }
    
    /**
     * @brief Iterator support
     */
    [[nodiscard]] auto begin() const noexcept { return fields_.begin(); }
    [[nodiscard]] auto end() const noexcept { return fields_.end(); }
    [[nodiscard]] auto begin() noexcept { return fields_.begin(); }
    [[nodiscard]] auto end() noexcept { return fields_.end(); }

private:
    std::vector<std::string> fields_;
};

// ============================================================================
// CSV Options
// ============================================================================

/**
 * @brief CSV parsing/writing options
 */
struct CsvOptions {
    char delimiter = ',';
    char quote = '"';
    char escape = '"';
    bool hasHeader = true;
    bool trimWhitespace = true;
    bool skipEmptyLines = true;
};

// ============================================================================
// CSV Document
// ============================================================================

/**
 * @brief Represents a complete CSV document
 */
class CsvDocument {
public:
    CsvDocument() = default;
    explicit CsvDocument(CsvOptions options) : options_(options) {}
    
    /**
     * @brief Parse CSV from string
     */
    static CsvDocument parse(std::string_view content, CsvOptions options = {});
    
    /**
     * @brief Load CSV from file
     */
    static std::optional<CsvDocument> load(const std::string& path, 
                                           CsvOptions options = {});
    
    /**
     * @brief Save CSV to file
     */
    [[nodiscard]] bool save(const std::string& path) const;
    
    /**
     * @brief Serialize to string
     */
    [[nodiscard]] std::string dump() const;
    
    /**
     * @brief Get header row
     */
    [[nodiscard]] const CsvRow& header() const noexcept { return header_; }
    
    /**
     * @brief Set header row
     */
    void setHeader(CsvRow header) { header_ = std::move(header); }
    
    /**
     * @brief Check if document has header
     */
    [[nodiscard]] bool hasHeader() const noexcept { return !header_.empty(); }
    
    /**
     * @brief Get column index by name
     */
    [[nodiscard]] std::optional<std::size_t> columnIndex(const std::string& name) const {
        for (std::size_t i = 0; i < header_.size(); ++i) {
            if (header_[i] == name) return i;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Get number of data rows
     */
    [[nodiscard]] std::size_t rowCount() const noexcept { return rows_.size(); }
    
    /**
     * @brief Get number of columns
     */
    [[nodiscard]] std::size_t columnCount() const noexcept {
        if (!header_.empty()) return header_.size();
        if (!rows_.empty()) return rows_[0].size();
        return 0;
    }
    
    /**
     * @brief Check if document is empty
     */
    [[nodiscard]] bool empty() const noexcept { return rows_.empty(); }
    
    /**
     * @brief Access row by index
     */
    [[nodiscard]] const CsvRow& operator[](std::size_t index) const {
        return rows_.at(index);
    }
    
    /**
     * @brief Access row by index (mutable)
     */
    [[nodiscard]] CsvRow& operator[](std::size_t index) {
        return rows_.at(index);
    }
    
    /**
     * @brief Get cell value by row and column name
     */
    [[nodiscard]] std::optional<std::string> get(std::size_t row, 
                                                  const std::string& column) const {
        auto idx = columnIndex(column);
        if (!idx || row >= rows_.size()) return std::nullopt;
        if (*idx >= rows_[row].size()) return std::nullopt;
        return rows_[row][*idx];
    }
    
    /**
     * @brief Add row
     */
    void addRow(CsvRow row) {
        rows_.push_back(std::move(row));
    }
    
    /**
     * @brief Add row from vector
     */
    void addRow(std::vector<std::string> fields) {
        rows_.emplace_back(std::move(fields));
    }
    
    /**
     * @brief Get all rows
     */
    [[nodiscard]] const std::vector<CsvRow>& rows() const noexcept {
        return rows_;
    }
    
    /**
     * @brief Get column values
     */
    [[nodiscard]] std::vector<std::string> column(std::size_t index) const {
        std::vector<std::string> result;
        result.reserve(rows_.size());
        for (const auto& row : rows_) {
            result.push_back(row.get(index));
        }
        return result;
    }
    
    /**
     * @brief Get column values by name
     */
    [[nodiscard]] std::vector<std::string> column(const std::string& name) const {
        auto idx = columnIndex(name);
        return idx ? column(*idx) : std::vector<std::string>{};
    }
    
    /**
     * @brief Iterator support
     */
    [[nodiscard]] auto begin() const noexcept { return rows_.begin(); }
    [[nodiscard]] auto end() const noexcept { return rows_.end(); }
    [[nodiscard]] auto begin() noexcept { return rows_.begin(); }
    [[nodiscard]] auto end() noexcept { return rows_.end(); }
    
    /**
     * @brief Get options
     */
    [[nodiscard]] const CsvOptions& options() const noexcept { return options_; }

private:
    CsvOptions options_;
    CsvRow header_;
    std::vector<CsvRow> rows_;
    
    static CsvRow parseLine(std::string_view line, const CsvOptions& options);
    static std::string escapeField(const std::string& field, const CsvOptions& options);
};

// ============================================================================
// Implementation
// ============================================================================

inline CsvRow CsvDocument::parseLine(std::string_view line, const CsvOptions& options) {
    CsvRow row;
    std::string field;
    bool inQuotes = false;
    bool hadQuote = false;
    
    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        
        if (inQuotes) {
            if (c == options.quote) {
                if (i + 1 < line.size() && line[i + 1] == options.quote) {
                    // Escaped quote
                    field += options.quote;
                    ++i;
                } else {
                    // End of quoted field
                    inQuotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == options.quote && field.empty()) {
                // Start of quoted field
                inQuotes = true;
                hadQuote = true;
            } else if (c == options.delimiter) {
                // End of field
                if (options.trimWhitespace && !hadQuote) {
                    // Trim whitespace
                    auto start = field.find_first_not_of(" \t");
                    auto end = field.find_last_not_of(" \t");
                    if (start != std::string::npos) {
                        field = field.substr(start, end - start + 1);
                    } else {
                        field.clear();
                    }
                }
                row.push_back(std::move(field));
                field.clear();
                hadQuote = false;
            } else {
                field += c;
            }
        }
    }
    
    // Add last field
    if (options.trimWhitespace && !hadQuote) {
        auto start = field.find_first_not_of(" \t");
        auto end = field.find_last_not_of(" \t");
        if (start != std::string::npos) {
            field = field.substr(start, end - start + 1);
        } else {
            field.clear();
        }
    }
    row.push_back(std::move(field));
    
    return row;
}

inline std::string CsvDocument::escapeField(const std::string& field, 
                                            const CsvOptions& options) {
    bool needsQuoting = false;
    
    // Check if field needs quoting
    for (char c : field) {
        if (c == options.delimiter || c == options.quote || 
            c == '\n' || c == '\r') {
            needsQuoting = true;
            break;
        }
    }
    
    if (!needsQuoting) {
        return field;
    }
    
    // Quote and escape
    std::string result;
    result += options.quote;
    for (char c : field) {
        if (c == options.quote) {
            result += options.quote;
        }
        result += c;
    }
    result += options.quote;
    
    return result;
}

inline CsvDocument CsvDocument::parse(std::string_view content, CsvOptions options) {
    CsvDocument doc(options);
    
    std::istringstream iss(std::string(content));
    std::string line;
    bool firstLine = true;
    
    while (std::getline(iss, line)) {
        // Remove trailing CR if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Skip empty lines
        if (options.skipEmptyLines && line.empty()) {
            continue;
        }
        
        CsvRow row = parseLine(line, options);
        
        if (firstLine && options.hasHeader) {
            doc.header_ = std::move(row);
            firstLine = false;
        } else {
            doc.rows_.push_back(std::move(row));
            firstLine = false;
        }
    }
    
    return doc;
}

inline std::optional<CsvDocument> CsvDocument::load(const std::string& path,
                                                    CsvOptions options) {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }
    
    std::ostringstream oss;
    oss << file.rdbuf();
    return parse(oss.str(), options);
}

inline bool CsvDocument::save(const std::string& path) const {
    std::ofstream file(path);
    if (!file) {
        return false;
    }
    
    file << dump();
    return file.good();
}

inline std::string CsvDocument::dump() const {
    std::ostringstream oss;
    
    // Write header
    if (!header_.empty()) {
        for (std::size_t i = 0; i < header_.size(); ++i) {
            if (i > 0) oss << options_.delimiter;
            oss << escapeField(header_[i], options_);
        }
        oss << "\n";
    }
    
    // Write rows
    for (const auto& row : rows_) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            if (i > 0) oss << options_.delimiter;
            oss << escapeField(row[i], options_);
        }
        oss << "\n";
    }
    
    return oss.str();
}

// ============================================================================
// CSV Builder
// ============================================================================

/**
 * @brief Fluent CSV document builder
 */
class CsvBuilder {
public:
    explicit CsvBuilder(CsvOptions options = {}) : doc_(options) {}
    
    /**
     * @brief Set header
     */
    template<typename... Args>
    CsvBuilder& header(Args&&... args) {
        doc_.setHeader(CsvRow({std::string(std::forward<Args>(args))...}));
        return *this;
    }
    
    /**
     * @brief Add row
     */
    template<typename... Args>
    CsvBuilder& row(Args&&... args) {
        doc_.addRow({std::string(std::forward<Args>(args))...});
        return *this;
    }
    
    /**
     * @brief Build document
     */
    [[nodiscard]] CsvDocument build() {
        return std::move(doc_);
    }

private:
    CsvDocument doc_;
};

/**
 * @brief Create CSV builder
 */
[[nodiscard]] inline CsvBuilder builder(CsvOptions options = {}) {
    return CsvBuilder(options);
}

} // namespace csv
} // namespace dfsmshsm

#endif // HSM_CSV_HPP
