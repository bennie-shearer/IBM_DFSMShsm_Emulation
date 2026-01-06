/**
 * @file hsm_string_utils.hpp
 * @brief String manipulation utilities for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - String trimming and padding
 * - Case conversion
 * - String splitting and joining
 * - Format validation
 * - Dataset name utilities
 */

#ifndef HSM_STRING_UTILS_HPP
#define HSM_STRING_UTILS_HPP

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <charconv>
#include <optional>
#include <regex>

namespace dfsmshsm {
namespace string_utils {

// ============================================================================
// Trimming Functions
// ============================================================================

/**
 * @brief Trim whitespace from left side of string
 * @param s Input string
 * @return Trimmed string
 */
[[nodiscard]] inline std::string ltrim(std::string_view s) noexcept {
    auto it = std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    return std::string(it, s.end());
}

/**
 * @brief Trim whitespace from right side of string
 * @param s Input string
 * @return Trimmed string
 */
[[nodiscard]] inline std::string rtrim(std::string_view s) noexcept {
    auto it = std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    return std::string(s.begin(), it.base());
}

/**
 * @brief Trim whitespace from both sides of string
 * @param s Input string
 * @return Trimmed string
 */
[[nodiscard]] inline std::string trim(std::string_view s) noexcept {
    return ltrim(rtrim(s));
}

/**
 * @brief Trim specific characters from left
 * @param s Input string
 * @param chars Characters to trim
 * @return Trimmed string
 */
[[nodiscard]] inline std::string ltrim(std::string_view s, std::string_view chars) noexcept {
    auto pos = s.find_first_not_of(chars);
    return pos == std::string_view::npos ? "" : std::string(s.substr(pos));
}

/**
 * @brief Trim specific characters from right
 * @param s Input string
 * @param chars Characters to trim
 * @return Trimmed string
 */
[[nodiscard]] inline std::string rtrim(std::string_view s, std::string_view chars) noexcept {
    auto pos = s.find_last_not_of(chars);
    return pos == std::string_view::npos ? "" : std::string(s.substr(0, pos + 1));
}

/**
 * @brief Trim specific characters from both sides
 * @param s Input string
 * @param chars Characters to trim
 * @return Trimmed string
 */
[[nodiscard]] inline std::string trim(std::string_view s, std::string_view chars) noexcept {
    return ltrim(rtrim(s, chars), chars);
}

// ============================================================================
// Case Conversion
// ============================================================================

/**
 * @brief Convert string to uppercase
 * @param s Input string
 * @return Uppercase string
 */
[[nodiscard]] inline std::string toUpper(std::string_view s) noexcept {
    std::string result(s);
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

/**
 * @brief Convert string to lowercase
 * @param s Input string
 * @return Lowercase string
 */
[[nodiscard]] inline std::string toLower(std::string_view s) noexcept {
    std::string result(s);
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

/**
 * @brief Convert to title case (first letter uppercase)
 * @param s Input string
 * @return Title case string
 */
[[nodiscard]] inline std::string toTitleCase(std::string_view s) noexcept {
    std::string result = toLower(s);
    if (!result.empty()) {
        result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
    }
    return result;
}

/**
 * @brief Check if string is uppercase
 * @param s Input string
 * @return true if all alphabetic characters are uppercase
 */
[[nodiscard]] inline bool isUpper(std::string_view s) noexcept {
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return !std::isalpha(c) || std::isupper(c);
    });
}

/**
 * @brief Check if string is lowercase
 * @param s Input string
 * @return true if all alphabetic characters are lowercase
 */
[[nodiscard]] inline bool isLower(std::string_view s) noexcept {
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return !std::isalpha(c) || std::islower(c);
    });
}

// ============================================================================
// Splitting and Joining
// ============================================================================

/**
 * @brief Split string by delimiter
 * @param s Input string
 * @param delimiter Delimiter character
 * @return Vector of substrings
 */
[[nodiscard]] inline std::vector<std::string> split(std::string_view s, char delimiter) noexcept {
    std::vector<std::string> result;
    std::size_t start = 0;
    std::size_t end = s.find(delimiter);
    
    while (end != std::string_view::npos) {
        result.emplace_back(s.substr(start, end - start));
        start = end + 1;
        end = s.find(delimiter, start);
    }
    result.emplace_back(s.substr(start));
    
    return result;
}

/**
 * @brief Split string by string delimiter
 * @param s Input string
 * @param delimiter Delimiter string
 * @return Vector of substrings
 */
[[nodiscard]] inline std::vector<std::string> split(std::string_view s, std::string_view delimiter) noexcept {
    std::vector<std::string> result;
    std::size_t start = 0;
    std::size_t end = s.find(delimiter);
    
    while (end != std::string_view::npos) {
        result.emplace_back(s.substr(start, end - start));
        start = end + delimiter.length();
        end = s.find(delimiter, start);
    }
    result.emplace_back(s.substr(start));
    
    return result;
}

/**
 * @brief Join strings with delimiter
 * @param parts Vector of strings to join
 * @param delimiter Delimiter string
 * @return Joined string
 */
[[nodiscard]] inline std::string join(const std::vector<std::string>& parts, 
                                      std::string_view delimiter) noexcept {
    if (parts.empty()) return "";
    
    std::ostringstream oss;
    oss << parts[0];
    for (std::size_t i = 1; i < parts.size(); ++i) {
        oss << delimiter << parts[i];
    }
    return oss.str();
}

/**
 * @brief Join strings with character delimiter
 * @param parts Vector of strings to join
 * @param delimiter Delimiter character
 * @return Joined string
 */
[[nodiscard]] inline std::string join(const std::vector<std::string>& parts, 
                                      char delimiter) noexcept {
    return join(parts, std::string_view(&delimiter, 1));
}

// ============================================================================
// Padding Functions
// ============================================================================

/**
 * @brief Pad string on left to specified width
 * @param s Input string
 * @param width Target width
 * @param pad Padding character
 * @return Padded string
 */
[[nodiscard]] inline std::string padLeft(std::string_view s, std::size_t width, 
                                         char pad = ' ') noexcept {
    if (s.length() >= width) return std::string(s);
    return std::string(width - s.length(), pad) + std::string(s);
}

/**
 * @brief Pad string on right to specified width
 * @param s Input string
 * @param width Target width
 * @param pad Padding character
 * @return Padded string
 */
[[nodiscard]] inline std::string padRight(std::string_view s, std::size_t width, 
                                          char pad = ' ') noexcept {
    if (s.length() >= width) return std::string(s);
    return std::string(s) + std::string(width - s.length(), pad);
}

/**
 * @brief Center string in specified width
 * @param s Input string
 * @param width Target width
 * @param pad Padding character
 * @return Centered string
 */
[[nodiscard]] inline std::string center(std::string_view s, std::size_t width, 
                                        char pad = ' ') noexcept {
    if (s.length() >= width) return std::string(s);
    std::size_t total_pad = width - s.length();
    std::size_t left_pad = total_pad / 2;
    std::size_t right_pad = total_pad - left_pad;
    return std::string(left_pad, pad) + std::string(s) + std::string(right_pad, pad);
}

// ============================================================================
// Search and Replace
// ============================================================================

/**
 * @brief Replace all occurrences of a substring
 * @param s Input string
 * @param from Substring to replace
 * @param to Replacement string
 * @return Modified string
 */
[[nodiscard]] inline std::string replaceAll(std::string s, std::string_view from, 
                                            std::string_view to) noexcept {
    if (from.empty()) return s;
    
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.length(), to);
        pos += to.length();
    }
    return s;
}

/**
 * @brief Replace first occurrence of a substring
 * @param s Input string
 * @param from Substring to replace
 * @param to Replacement string
 * @return Modified string
 */
[[nodiscard]] inline std::string replaceFirst(std::string s, std::string_view from, 
                                              std::string_view to) noexcept {
    std::size_t pos = s.find(from);
    if (pos != std::string::npos) {
        s.replace(pos, from.length(), to);
    }
    return s;
}

/**
 * @brief Check if string contains substring
 * @param s Input string
 * @param substr Substring to find
 * @return true if found
 */
[[nodiscard]] inline bool contains(std::string_view s, std::string_view substr) noexcept {
    return s.find(substr) != std::string_view::npos;
}

/**
 * @brief Check if string starts with prefix
 * @param s Input string
 * @param prefix Prefix to check
 * @return true if starts with prefix
 */
[[nodiscard]] inline bool startsWith(std::string_view s, std::string_view prefix) noexcept {
    return s.length() >= prefix.length() && s.substr(0, prefix.length()) == prefix;
}

/**
 * @brief Check if string ends with suffix
 * @param s Input string
 * @param suffix Suffix to check
 * @return true if ends with suffix
 */
[[nodiscard]] inline bool endsWith(std::string_view s, std::string_view suffix) noexcept {
    return s.length() >= suffix.length() && 
           s.substr(s.length() - suffix.length()) == suffix;
}

// ============================================================================
// Numeric Conversions
// ============================================================================

/**
 * @brief Convert string to integer
 * @param s Input string
 * @return Optional integer value
 */
template<typename T = int>
[[nodiscard]] inline std::optional<T> toInt(std::string_view s) noexcept {
    T value{};
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec == std::errc{} && ptr == s.data() + s.size()) {
        return value;
    }
    return std::nullopt;
}

/**
 * @brief Convert integer to string with zero padding
 * @param value Integer value
 * @param width Minimum width
 * @return Formatted string
 */
template<typename T>
[[nodiscard]] inline std::string toZeroPaddedString(T value, int width) noexcept {
    std::ostringstream oss;
    oss << std::setw(width) << std::setfill('0') << value;
    return oss.str();
}

/**
 * @brief Format bytes as human-readable size
 * @param bytes Number of bytes
 * @return Formatted string (e.g., "1.5 GB")
 */
[[nodiscard]] inline std::string formatBytes(uint64_t bytes) noexcept {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 5) {
        size /= 1024.0;
        ++unit;
    }
    
    std::ostringstream oss;
    if (unit == 0) {
        oss << bytes << " " << units[unit];
    } else {
        oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    }
    return oss.str();
}

// ============================================================================
// Dataset Name Utilities
// ============================================================================

/**
 * @brief Validate dataset name format
 * @param name Dataset name to validate
 * @return true if valid z/OS dataset name format
 */
[[nodiscard]] inline bool isValidDatasetName(std::string_view name) noexcept {
    if (name.empty() || name.length() > 44) return false;
    
    auto qualifiers = split(name, '.');
    if (qualifiers.empty() || qualifiers.size() > 22) return false;
    
    for (const auto& qual : qualifiers) {
        if (qual.empty() || qual.length() > 8) return false;
        
        // First character must be alphabetic or national (@, #, $)
        char first = qual[0];
        if (!std::isalpha(static_cast<unsigned char>(first)) && 
            first != '@' && first != '#' && first != '$') {
            return false;
        }
        
        // Remaining characters must be alphanumeric or national
        for (std::size_t i = 1; i < qual.length(); ++i) {
            char c = qual[i];
            if (!std::isalnum(static_cast<unsigned char>(c)) && 
                c != '@' && c != '#' && c != '$') {
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief Extract high-level qualifier from dataset name
 * @param name Full dataset name
 * @return High-level qualifier
 */
[[nodiscard]] inline std::string getHLQ(std::string_view name) noexcept {
    auto pos = name.find('.');
    return std::string(pos == std::string_view::npos ? name : name.substr(0, pos));
}

/**
 * @brief Extract low-level qualifier from dataset name
 * @param name Full dataset name
 * @return Low-level qualifier
 */
[[nodiscard]] inline std::string getLLQ(std::string_view name) noexcept {
    auto pos = name.rfind('.');
    return std::string(pos == std::string_view::npos ? name : name.substr(pos + 1));
}

/**
 * @brief Validate volume serial format
 * @param volser Volume serial to validate
 * @return true if valid 6-character volume serial
 */
[[nodiscard]] inline bool isValidVolser(std::string_view volser) noexcept {
    if (volser.length() != 6) return false;
    
    return std::all_of(volser.begin(), volser.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '@' || c == '#' || c == '$';
    });
}

// ============================================================================
// Miscellaneous
// ============================================================================

/**
 * @brief Repeat string n times
 * @param s Input string
 * @param n Number of repetitions
 * @return Repeated string
 */
[[nodiscard]] inline std::string repeat(std::string_view s, std::size_t n) noexcept {
    std::string result;
    result.reserve(s.length() * n);
    for (std::size_t i = 0; i < n; ++i) {
        result += s;
    }
    return result;
}

/**
 * @brief Reverse string
 * @param s Input string
 * @return Reversed string
 */
[[nodiscard]] inline std::string reverse(std::string_view s) noexcept {
    return std::string(s.rbegin(), s.rend());
}

/**
 * @brief Check if string is numeric
 * @param s Input string
 * @return true if string contains only digits
 */
[[nodiscard]] inline bool isNumeric(std::string_view s) noexcept {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isdigit(c);
    });
}

/**
 * @brief Check if string is alphanumeric
 * @param s Input string
 * @return true if string contains only alphanumeric characters
 */
[[nodiscard]] inline bool isAlphaNumeric(std::string_view s) noexcept {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isalnum(c);
    });
}

/**
 * @brief Remove duplicate consecutive characters
 * @param s Input string
 * @return String without consecutive duplicates
 */
[[nodiscard]] inline std::string removeDuplicates(std::string_view s) noexcept {
    if (s.empty()) return "";
    
    std::string result;
    result.reserve(s.length());
    result += s[0];
    
    for (std::size_t i = 1; i < s.length(); ++i) {
        if (s[i] != s[i-1]) {
            result += s[i];
        }
    }
    return result;
}

/**
 * @brief Truncate string to maximum length
 * @param s Input string
 * @param maxLen Maximum length
 * @param suffix Suffix to append if truncated
 * @return Truncated string
 */
[[nodiscard]] inline std::string truncate(std::string_view s, std::size_t maxLen, 
                                          std::string_view suffix = "...") noexcept {
    if (s.length() <= maxLen) return std::string(s);
    if (maxLen <= suffix.length()) return std::string(suffix.substr(0, maxLen));
    return std::string(s.substr(0, maxLen - suffix.length())) + std::string(suffix);
}

} // namespace string_utils
} // namespace dfsmshsm

#endif // HSM_STRING_UTILS_HPP
