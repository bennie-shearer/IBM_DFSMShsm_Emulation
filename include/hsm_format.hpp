/**
 * @file hsm_format.hpp
 * @brief Format string utilities for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - Type-safe string formatting
 * - Printf-style formatting with type safety
 * - Numeric formatting utilities
 * - Table and column formatting
 */

#ifndef HSM_FORMAT_HPP
#define HSM_FORMAT_HPP

#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <cstdarg>
#include <algorithm>
#include <stdexcept>

namespace dfsmshsm {
namespace format {

// ============================================================================
// Basic Formatting
// ============================================================================

/**
 * @brief Format arguments into string using stream operations
 * @tparam Args Argument types
 * @param args Arguments to format
 * @return Formatted string
 */
template<typename... Args>
[[nodiscard]] std::string concat(Args&&... args) {
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    return oss.str();
}

/**
 * @brief Format with separator between arguments
 * @tparam Args Argument types
 * @param separator Separator string
 * @param args Arguments to format
 * @return Formatted string with separators
 */
template<typename... Args>
[[nodiscard]] std::string join(std::string_view separator, Args&&... args) {
    std::ostringstream oss;
    bool first = true;
    auto append = [&](auto&& arg) {
        if (!first) oss << separator;
        oss << std::forward<decltype(arg)>(arg);
        first = false;
    };
    (append(std::forward<Args>(args)), ...);
    return oss.str();
}

// ============================================================================
// Numeric Formatting
// ============================================================================

/**
 * @brief Format integer with zero padding
 * @tparam T Integer type
 * @param value Integer value
 * @param width Minimum width
 * @return Zero-padded string
 */
template<typename T>
requires std::integral<T>
[[nodiscard]] std::string zeroPad(T value, int width) {
    std::ostringstream oss;
    oss << std::setw(width) << std::setfill('0') << value;
    return oss.str();
}

/**
 * @brief Format integer with space padding (right-aligned)
 * @tparam T Integer type
 * @param value Integer value
 * @param width Minimum width
 * @return Right-aligned string
 */
template<typename T>
requires std::integral<T>
[[nodiscard]] std::string rightAlign(T value, int width) {
    std::ostringstream oss;
    oss << std::setw(width) << value;
    return oss.str();
}

/**
 * @brief Format integer with space padding (left-aligned)
 * @tparam T Integer type
 * @param value Integer value
 * @param width Minimum width
 * @return Left-aligned string
 */
template<typename T>
requires std::integral<T>
[[nodiscard]] std::string leftAlign(T value, int width) {
    std::ostringstream oss;
    oss << std::left << std::setw(width) << value;
    return oss.str();
}

/**
 * @brief Format floating-point with fixed precision
 * @tparam T Floating-point type
 * @param value Floating-point value
 * @param precision Decimal places
 * @return Fixed-precision string
 */
template<typename T>
requires std::floating_point<T>
[[nodiscard]] std::string fixed(T value, int precision = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

/**
 * @brief Format floating-point with scientific notation
 * @tparam T Floating-point type
 * @param value Floating-point value
 * @param precision Decimal places
 * @return Scientific notation string
 */
template<typename T>
requires std::floating_point<T>
[[nodiscard]] std::string scientific(T value, int precision = 2) {
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(precision) << value;
    return oss.str();
}

/**
 * @brief Format number with thousands separator
 * @tparam T Numeric type
 * @param value Numeric value
 * @param separator Thousands separator character
 * @return Formatted string with separators
 */
template<typename T>
requires std::integral<T>
[[nodiscard]] std::string withSeparators(T value, char separator = ',') {
    std::string str = std::to_string(value);
    bool negative = str[0] == '-';
    std::size_t start = negative ? 1 : 0;
    
    std::string result;
    std::size_t digits = str.length() - start;
    
    if (negative) result += '-';
    
    for (std::size_t i = start; i < str.length(); ++i) {
        if (i > start && (digits - (i - start)) % 3 == 0) {
            result += separator;
        }
        result += str[i];
    }
    return result;
}

/**
 * @brief Format percentage
 * @param value Value (0.0 to 1.0 typically)
 * @param precision Decimal places
 * @param includeSign Include percent sign
 * @return Percentage string
 */
[[nodiscard]] inline std::string percent(double value, int precision = 1, 
                                         bool includeSign = true) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << (value * 100);
    if (includeSign) oss << "%";
    return oss.str();
}

// ============================================================================
// Hexadecimal Formatting
// ============================================================================

/**
 * @brief Format integer as hexadecimal
 * @tparam T Integer type
 * @param value Integer value
 * @param width Minimum width (zero-padded)
 * @param uppercase Use uppercase hex digits
 * @param prefix Include 0x prefix
 * @return Hexadecimal string
 */
template<typename T>
requires std::integral<T>
[[nodiscard]] std::string hex(T value, int width = 0, bool uppercase = true, 
                              bool prefix = false) {
    std::ostringstream oss;
    if (prefix) oss << "0x";
    if (uppercase) oss << std::uppercase;
    if (width > 0) oss << std::setw(width) << std::setfill('0');
    oss << std::hex << value;
    return oss.str();
}

/**
 * @brief Format byte as hexadecimal
 * @param byte Byte value
 * @param uppercase Use uppercase hex digits
 * @return Two-character hex string
 */
[[nodiscard]] inline std::string hexByte(uint8_t byte, bool uppercase = true) {
    return hex(byte, 2, uppercase, false);
}

/**
 * @brief Format bytes as hexadecimal string
 * @param data Pointer to data
 * @param size Size in bytes
 * @param separator Separator between bytes
 * @param uppercase Use uppercase hex digits
 * @return Hexadecimal string
 */
[[nodiscard]] inline std::string hexBytes(const void* data, std::size_t size,
                                          std::string_view separator = "",
                                          bool uppercase = true) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    std::ostringstream oss;
    
    for (std::size_t i = 0; i < size; ++i) {
        if (i > 0 && !separator.empty()) oss << separator;
        if (uppercase) oss << std::uppercase;
        oss << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

// ============================================================================
// Binary Formatting
// ============================================================================

/**
 * @brief Format integer as binary
 * @tparam T Integer type
 * @param value Integer value
 * @param groupSize Bits per group (0 for no grouping)
 * @param separator Group separator
 * @return Binary string
 */
template<typename T>
requires std::unsigned_integral<T>
[[nodiscard]] std::string binary(T value, int groupSize = 0, char separator = ' ') {
    std::string result;
    constexpr int bits = sizeof(T) * 8;
    
    bool started = false;
    for (int i = bits - 1; i >= 0; --i) {
        bool bit = (value >> i) & 1;
        if (bit || started || i == 0) {
            if (started && groupSize > 0 && (i + 1) % groupSize == 0) {
                result += separator;
            }
            result += bit ? '1' : '0';
            started = true;
        }
    }
    return result;
}

// ============================================================================
// Size Formatting
// ============================================================================

/**
 * @brief Format bytes as human-readable size
 * @param bytes Number of bytes
 * @param precision Decimal places
 * @param binary Use binary units (KiB, MiB) vs decimal (KB, MB)
 * @return Human-readable size string
 */
[[nodiscard]] inline std::string bytes(uint64_t bytes, int precision = 2, 
                                       bool binary = true) {
    const char* binaryUnits[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    const char* decimalUnits[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    
    const char** units = binary ? binaryUnits : decimalUnits;
    double divisor = binary ? 1024.0 : 1000.0;
    
    double size = static_cast<double>(bytes);
    int unit = 0;
    
    while (size >= divisor && unit < 6) {
        size /= divisor;
        ++unit;
    }
    
    std::ostringstream oss;
    if (unit == 0) {
        oss << bytes << " " << units[unit];
    } else {
        oss << std::fixed << std::setprecision(precision) << size << " " << units[unit];
    }
    return oss.str();
}

/**
 * @brief Format transfer rate (bytes per second)
 * @param bytesPerSecond Rate in bytes per second
 * @param precision Decimal places
 * @return Human-readable rate string
 */
[[nodiscard]] inline std::string transferRate(uint64_t bytesPerSecond, int precision = 2) {
    return bytes(bytesPerSecond, precision) + "/s";
}

// ============================================================================
// Duration Formatting
// ============================================================================

/**
 * @brief Format duration in seconds to human-readable string
 * @param seconds Duration in seconds
 * @param compact Use compact format (1h 30m vs 1 hour 30 minutes)
 * @return Human-readable duration
 */
[[nodiscard]] inline std::string duration(double seconds, bool compact = true) {
    std::ostringstream oss;
    
    if (seconds < 0) {
        oss << "-";
        seconds = -seconds;
    }
    
    uint64_t totalSecs = static_cast<uint64_t>(seconds);
    uint64_t days = totalSecs / 86400;
    uint64_t hours = (totalSecs % 86400) / 3600;
    uint64_t mins = (totalSecs % 3600) / 60;
    uint64_t secs = totalSecs % 60;
    
    if (compact) {
        if (days > 0) oss << days << "d ";
        if (hours > 0 || days > 0) oss << hours << "h ";
        if (mins > 0 || hours > 0 || days > 0) oss << mins << "m ";
        oss << secs << "s";
    } else {
        if (days > 0) oss << days << (days == 1 ? " day " : " days ");
        if (hours > 0) oss << hours << (hours == 1 ? " hour " : " hours ");
        if (mins > 0) oss << mins << (mins == 1 ? " minute " : " minutes ");
        oss << secs << (secs == 1 ? " second" : " seconds");
    }
    
    return oss.str();
}

/**
 * @brief Format milliseconds to human-readable string
 * @param ms Duration in milliseconds
 * @return Human-readable duration
 */
[[nodiscard]] inline std::string milliseconds(int64_t ms) {
    if (ms < 1000) {
        return std::to_string(ms) + "ms";
    }
    return duration(static_cast<double>(ms) / 1000.0);
}

// ============================================================================
// Table Formatting
// ============================================================================

/**
 * @brief Column alignment
 */
enum class Align {
    Left,
    Right,
    Center
};

/**
 * @brief Format string with alignment
 * @param str String to format
 * @param width Column width
 * @param align Alignment
 * @param fill Fill character
 * @return Aligned string
 */
[[nodiscard]] inline std::string align(std::string_view str, std::size_t width,
                                       Align alignment = Align::Left, char fill = ' ') {
    if (str.length() >= width) {
        return std::string(str.substr(0, width));
    }
    
    std::size_t padding = width - str.length();
    
    switch (alignment) {
        case Align::Left:
            return std::string(str) + std::string(padding, fill);
        case Align::Right:
            return std::string(padding, fill) + std::string(str);
        case Align::Center: {
            std::size_t leftPad = padding / 2;
            std::size_t rightPad = padding - leftPad;
            return std::string(leftPad, fill) + std::string(str) + std::string(rightPad, fill);
        }
    }
    return std::string(str);
}

/**
 * @brief Table formatter for aligned columnar output
 */
class Table {
public:
    /**
     * @brief Column definition
     */
    struct Column {
        std::string header;
        std::size_t width;
        Align alignment = Align::Left;
    };
    
    /**
     * @brief Constructor
     * @param columns Column definitions
     */
    explicit Table(std::vector<Column> columns) : columns_(std::move(columns)) {
        // Auto-size columns based on headers
        for (auto& col : columns_) {
            if (col.width == 0) {
                col.width = col.header.length();
            }
        }
    }
    
    /**
     * @brief Add a row of data
     * @param values Row values (must match column count)
     */
    void addRow(std::vector<std::string> values) {
        rows_.push_back(std::move(values));
        
        // Update column widths if necessary
        auto& row = rows_.back();
        for (std::size_t i = 0; i < row.size() && i < columns_.size(); ++i) {
            columns_[i].width = std::max(columns_[i].width, row[i].length());
        }
    }
    
    /**
     * @brief Format table as string
     * @param border Include border characters
     * @return Formatted table string
     */
    [[nodiscard]] std::string format(bool border = false) const {
        std::ostringstream oss;
        
        if (border) {
            oss << formatBorder() << "\n";
        }
        
        // Header row
        oss << formatRow(getHeaders(), border) << "\n";
        
        if (border) {
            oss << formatBorder() << "\n";
        } else {
            oss << formatSeparator() << "\n";
        }
        
        // Data rows
        for (const auto& row : rows_) {
            oss << formatRow(row, border) << "\n";
        }
        
        if (border) {
            oss << formatBorder();
        }
        
        return oss.str();
    }

private:
    std::vector<Column> columns_;
    std::vector<std::vector<std::string>> rows_;
    
    [[nodiscard]] std::vector<std::string> getHeaders() const {
        std::vector<std::string> headers;
        for (const auto& col : columns_) {
            headers.push_back(col.header);
        }
        return headers;
    }
    
    [[nodiscard]] std::string formatRow(const std::vector<std::string>& values, 
                                        bool border) const {
        std::ostringstream oss;
        if (border) oss << "| ";
        
        for (std::size_t i = 0; i < columns_.size(); ++i) {
            if (i > 0) {
                oss << (border ? " | " : "  ");
            }
            std::string value = i < values.size() ? values[i] : "";
            oss << align(value, columns_[i].width, columns_[i].alignment);
        }
        
        if (border) oss << " |";
        return oss.str();
    }
    
    [[nodiscard]] std::string formatSeparator() const {
        std::ostringstream oss;
        for (std::size_t i = 0; i < columns_.size(); ++i) {
            if (i > 0) oss << "  ";
            oss << std::string(columns_[i].width, '-');
        }
        return oss.str();
    }
    
    [[nodiscard]] std::string formatBorder() const {
        std::ostringstream oss;
        oss << "+-";
        for (std::size_t i = 0; i < columns_.size(); ++i) {
            if (i > 0) oss << "-+-";
            oss << std::string(columns_[i].width, '-');
        }
        oss << "-+";
        return oss.str();
    }
};

// ============================================================================
// Progress Bar
// ============================================================================

/**
 * @brief Format progress bar
 * @param progress Progress (0.0 to 1.0)
 * @param width Total width of bar
 * @param filled Filled character
 * @param empty Empty character
 * @return Progress bar string
 */
[[nodiscard]] inline std::string progressBar(double progress, int width = 40,
                                             char filled = '=', char empty = ' ') {
    progress = std::max(0.0, std::min(1.0, progress));
    int filledWidth = static_cast<int>(progress * width);
    
    std::ostringstream oss;
    oss << "[";
    oss << std::string(filledWidth, filled);
    if (filledWidth < width) {
        oss << ">";
        oss << std::string(width - filledWidth - 1, empty);
    }
    oss << "] " << std::fixed << std::setprecision(1) << (progress * 100) << "%";
    
    return oss.str();
}

// ============================================================================
// Text Wrapping
// ============================================================================

/**
 * @brief Wrap text to specified width
 * @param text Text to wrap
 * @param width Maximum line width
 * @param indent Indentation for wrapped lines
 * @return Wrapped text
 */
[[nodiscard]] inline std::string wrap(std::string_view text, std::size_t width,
                                      std::string_view indent = "") {
    std::ostringstream oss;
    std::size_t pos = 0;
    bool firstLine = true;
    
    while (pos < text.length()) {
        if (!firstLine) {
            oss << "\n" << indent;
        }
        firstLine = false;
        
        std::size_t lineWidth = firstLine ? width : (width - indent.length());
        std::size_t end = pos + lineWidth;
        
        if (end >= text.length()) {
            oss << text.substr(pos);
            break;
        }
        
        // Find last space before end
        std::size_t breakPos = text.rfind(' ', end);
        if (breakPos == std::string_view::npos || breakPos <= pos) {
            breakPos = end;
        }
        
        oss << text.substr(pos, breakPos - pos);
        pos = breakPos + 1;
        
        // Skip leading spaces
        while (pos < text.length() && text[pos] == ' ') {
            ++pos;
        }
    }
    
    return oss.str();
}

// ============================================================================
// Printf-style Formatting (Type-safe)
// ============================================================================

namespace detail {

inline void appendArg(std::ostringstream& oss, int value) { oss << value; }
inline void appendArg(std::ostringstream& oss, long value) { oss << value; }
inline void appendArg(std::ostringstream& oss, long long value) { oss << value; }
inline void appendArg(std::ostringstream& oss, unsigned value) { oss << value; }
inline void appendArg(std::ostringstream& oss, unsigned long value) { oss << value; }
inline void appendArg(std::ostringstream& oss, unsigned long long value) { oss << value; }
inline void appendArg(std::ostringstream& oss, float value) { oss << value; }
inline void appendArg(std::ostringstream& oss, double value) { oss << value; }
inline void appendArg(std::ostringstream& oss, char value) { oss << value; }
inline void appendArg(std::ostringstream& oss, const char* value) { oss << (value ? value : "(null)"); }
inline void appendArg(std::ostringstream& oss, const std::string& value) { oss << value; }
inline void appendArg(std::ostringstream& oss, std::string_view value) { oss << value; }
inline void appendArg(std::ostringstream& oss, bool value) { oss << (value ? "true" : "false"); }
inline void appendArg(std::ostringstream& oss, const void* value) { oss << value; }

template<typename T>
void appendArg(std::ostringstream& oss, const T& value) {
    oss << value;
}

inline void formatImpl(std::ostringstream& oss, std::string_view fmt) {
    oss << fmt;
}

template<typename T, typename... Args>
void formatImpl(std::ostringstream& oss, std::string_view fmt, const T& value, Args&&... args) {
    auto pos = fmt.find("{}");
    if (pos == std::string_view::npos) {
        oss << fmt;
        return;
    }
    
    oss << fmt.substr(0, pos);
    appendArg(oss, value);
    formatImpl(oss, fmt.substr(pos + 2), std::forward<Args>(args)...);
}

} // namespace detail

/**
 * @brief Format string with {} placeholders (type-safe printf alternative)
 * @tparam Args Argument types
 * @param fmt Format string with {} placeholders
 * @param args Arguments to substitute
 * @return Formatted string
 */
template<typename... Args>
[[nodiscard]] std::string fmt(std::string_view fmt, Args&&... args) {
    std::ostringstream oss;
    detail::formatImpl(oss, fmt, std::forward<Args>(args)...);
    return oss.str();
}

// ============================================================================
// Escape Sequences
// ============================================================================

/**
 * @brief Escape special characters in string
 * @param str Input string
 * @return Escaped string
 */
[[nodiscard]] inline std::string escape(std::string_view str) {
    std::string result;
    result.reserve(str.length() * 2);
    
    for (char c : str) {
        switch (c) {
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\'': result += "\\'"; break;
            default:
                if (static_cast<unsigned char>(c) < 32) {
                    result += "\\x";
                    result += hex(static_cast<uint8_t>(c), 2, true, false);
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}

/**
 * @brief Quote string for output
 * @param str Input string
 * @param quote Quote character
 * @return Quoted and escaped string
 */
[[nodiscard]] inline std::string quote(std::string_view str, char quoteChar = '"') {
    std::string result;
    result += quoteChar;
    result += escape(str);
    result += quoteChar;
    return result;
}

} // namespace format
} // namespace dfsmshsm

#endif // HSM_FORMAT_HPP
