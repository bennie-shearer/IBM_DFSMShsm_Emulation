/**
 * @file hsm_span.hpp
 * @brief C++20 std::span utilities and helpers for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - Span creation utilities
 * - Span manipulation functions
 * - Safe span operations
 * - Span conversion helpers
 */

#ifndef HSM_SPAN_HPP
#define HSM_SPAN_HPP

#include <span>
#include <vector>
#include <array>
#include <string>
#include <string_view>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace dfsmshsm {
namespace span_utils {

// ============================================================================
// Type Aliases
// ============================================================================

/// Byte span (read-only)
using ByteSpan = std::span<const uint8_t>;

/// Mutable byte span
using MutableByteSpan = std::span<uint8_t>;

/// Character span
using CharSpan = std::span<const char>;

/// Mutable character span
using MutableCharSpan = std::span<char>;

// ============================================================================
// Span Creation Utilities
// ============================================================================

/**
 * @brief Create span from C-style array
 * @tparam T Element type
 * @tparam N Array size
 * @param arr C-style array
 * @return Span over the array
 */
template<typename T, std::size_t N>
[[nodiscard]] constexpr std::span<T> makeSpan(T (&arr)[N]) noexcept {
    return std::span<T>(arr, N);
}

/**
 * @brief Create span from pointer and size
 * @tparam T Element type
 * @param ptr Pointer to data
 * @param size Number of elements
 * @return Span over the data
 */
template<typename T>
[[nodiscard]] constexpr std::span<T> makeSpan(T* ptr, std::size_t size) noexcept {
    return std::span<T>(ptr, size);
}

/**
 * @brief Create span from iterators
 * @tparam T Element type
 * @param first Begin iterator
 * @param last End iterator
 * @return Span over the range
 */
template<typename T>
[[nodiscard]] constexpr std::span<T> makeSpan(T* first, T* last) noexcept {
    return std::span<T>(first, last);
}

/**
 * @brief Create byte span from any trivially copyable type
 * @tparam T Source type (must be trivially copyable)
 * @param value Reference to value
 * @return Byte span over the value's memory
 */
template<typename T>
requires std::is_trivially_copyable_v<T>
[[nodiscard]] ByteSpan asBytes(const T& value) noexcept {
    return ByteSpan(reinterpret_cast<const uint8_t*>(&value), sizeof(T));
}

/**
 * @brief Create mutable byte span from any trivially copyable type
 * @tparam T Source type (must be trivially copyable)
 * @param value Reference to value
 * @return Mutable byte span over the value's memory
 */
template<typename T>
requires std::is_trivially_copyable_v<T>
[[nodiscard]] MutableByteSpan asMutableBytes(T& value) noexcept {
    return MutableByteSpan(reinterpret_cast<uint8_t*>(&value), sizeof(T));
}

/**
 * @brief Create span from vector
 * @tparam T Element type
 * @param vec Vector
 * @return Span over vector data
 */
template<typename T>
[[nodiscard]] std::span<T> fromVector(std::vector<T>& vec) noexcept {
    return std::span<T>(vec);
}

/**
 * @brief Create const span from vector
 * @tparam T Element type
 * @param vec Vector
 * @return Const span over vector data
 */
template<typename T>
[[nodiscard]] std::span<const T> fromVector(const std::vector<T>& vec) noexcept {
    return std::span<const T>(vec);
}

/**
 * @brief Create span from string
 * @param str String
 * @return Character span
 */
[[nodiscard]] inline MutableCharSpan fromString(std::string& str) noexcept {
    return MutableCharSpan(str.data(), str.size());
}

/**
 * @brief Create const span from string
 * @param str String
 * @return Const character span
 */
[[nodiscard]] inline CharSpan fromString(const std::string& str) noexcept {
    return CharSpan(str.data(), str.size());
}

/**
 * @brief Create span from string_view
 * @param sv String view
 * @return Character span
 */
[[nodiscard]] inline CharSpan fromStringView(std::string_view sv) noexcept {
    return CharSpan(sv.data(), sv.size());
}

// ============================================================================
// Span Subspan Operations
// ============================================================================

/**
 * @brief Get first N elements of span
 * @tparam T Element type
 * @param sp Source span
 * @param count Number of elements
 * @return Subspan of first N elements
 */
template<typename T>
[[nodiscard]] constexpr std::span<T> takeFirst(std::span<T> sp, std::size_t count) noexcept {
    return sp.first(std::min(count, sp.size()));
}

/**
 * @brief Get last N elements of span
 * @tparam T Element type
 * @param sp Source span
 * @param count Number of elements
 * @return Subspan of last N elements
 */
template<typename T>
[[nodiscard]] constexpr std::span<T> takeLast(std::span<T> sp, std::size_t count) noexcept {
    return sp.last(std::min(count, sp.size()));
}

/**
 * @brief Drop first N elements from span
 * @tparam T Element type
 * @param sp Source span
 * @param count Number of elements to drop
 * @return Remaining span after dropping
 */
template<typename T>
[[nodiscard]] constexpr std::span<T> dropFirst(std::span<T> sp, std::size_t count) noexcept {
    count = std::min(count, sp.size());
    return sp.subspan(count);
}

/**
 * @brief Drop last N elements from span
 * @tparam T Element type
 * @param sp Source span
 * @param count Number of elements to drop
 * @return Remaining span after dropping
 */
template<typename T>
[[nodiscard]] constexpr std::span<T> dropLast(std::span<T> sp, std::size_t count) noexcept {
    count = std::min(count, sp.size());
    return sp.first(sp.size() - count);
}

/**
 * @brief Split span at index
 * @tparam T Element type
 * @param sp Source span
 * @param index Split point
 * @return Pair of spans (before and after index)
 */
template<typename T>
[[nodiscard]] constexpr std::pair<std::span<T>, std::span<T>> 
splitAt(std::span<T> sp, std::size_t index) noexcept {
    index = std::min(index, sp.size());
    return {sp.first(index), sp.subspan(index)};
}

// ============================================================================
// Safe Span Access
// ============================================================================

/**
 * @brief Safe element access with bounds checking
 * @tparam T Element type
 * @param sp Span
 * @param index Element index
 * @return Pointer to element or nullptr if out of bounds
 */
template<typename T>
[[nodiscard]] constexpr T* safeAt(std::span<T> sp, std::size_t index) noexcept {
    return index < sp.size() ? &sp[index] : nullptr;
}

/**
 * @brief Safe element access with default value
 * @tparam T Element type
 * @param sp Span
 * @param index Element index
 * @param defaultValue Default value if out of bounds
 * @return Element value or default
 */
template<typename T>
[[nodiscard]] constexpr T safeGet(std::span<const T> sp, std::size_t index, 
                                   const T& defaultValue = T{}) noexcept {
    return index < sp.size() ? sp[index] : defaultValue;
}

/**
 * @brief Check if index is valid for span
 * @tparam T Element type
 * @param sp Span
 * @param index Index to check
 * @return true if index is valid
 */
template<typename T>
[[nodiscard]] constexpr bool isValidIndex(std::span<T> sp, std::size_t index) noexcept {
    return index < sp.size();
}

/**
 * @brief Get front element safely
 * @tparam T Element type
 * @param sp Span
 * @return Pointer to front or nullptr if empty
 */
template<typename T>
[[nodiscard]] constexpr T* safeFront(std::span<T> sp) noexcept {
    return sp.empty() ? nullptr : &sp.front();
}

/**
 * @brief Get back element safely
 * @tparam T Element type
 * @param sp Span
 * @return Pointer to back or nullptr if empty
 */
template<typename T>
[[nodiscard]] constexpr T* safeBack(std::span<T> sp) noexcept {
    return sp.empty() ? nullptr : &sp.back();
}

// ============================================================================
// Span Comparison
// ============================================================================

/**
 * @brief Check if two spans are equal
 * @tparam T Element type
 * @param lhs First span
 * @param rhs Second span
 * @return true if spans have same size and equal elements
 */
template<typename T>
[[nodiscard]] constexpr bool equal(std::span<const T> lhs, std::span<const T> rhs) noexcept {
    if (lhs.size() != rhs.size()) return false;
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
 * @brief Check if span starts with prefix
 * @tparam T Element type
 * @param sp Source span
 * @param prefix Prefix to check
 * @return true if sp starts with prefix
 */
template<typename T>
[[nodiscard]] constexpr bool startsWith(std::span<const T> sp, 
                                        std::span<const T> prefix) noexcept {
    if (prefix.size() > sp.size()) return false;
    return equal(sp.first(prefix.size()), prefix);
}

/**
 * @brief Check if span ends with suffix
 * @tparam T Element type
 * @param sp Source span
 * @param suffix Suffix to check
 * @return true if sp ends with suffix
 */
template<typename T>
[[nodiscard]] constexpr bool endsWith(std::span<const T> sp, 
                                      std::span<const T> suffix) noexcept {
    if (suffix.size() > sp.size()) return false;
    return equal(sp.last(suffix.size()), suffix);
}

/**
 * @brief Check if span contains element
 * @tparam T Element type
 * @param sp Span to search
 * @param value Value to find
 * @return true if found
 */
template<typename T>
[[nodiscard]] constexpr bool contains(std::span<const T> sp, const T& value) noexcept {
    return std::find(sp.begin(), sp.end(), value) != sp.end();
}

// ============================================================================
// Span Search
// ============================================================================

/**
 * @brief Find element in span
 * @tparam T Element type
 * @param sp Span to search
 * @param value Value to find
 * @return Index of element or size() if not found
 */
template<typename T>
[[nodiscard]] constexpr std::size_t find(std::span<const T> sp, const T& value) noexcept {
    auto it = std::find(sp.begin(), sp.end(), value);
    return it != sp.end() ? std::distance(sp.begin(), it) : sp.size();
}

/**
 * @brief Find element matching predicate
 * @tparam T Element type
 * @tparam Pred Predicate type
 * @param sp Span to search
 * @param pred Predicate function
 * @return Index of element or size() if not found
 */
template<typename T, typename Pred>
[[nodiscard]] constexpr std::size_t findIf(std::span<const T> sp, Pred pred) noexcept {
    auto it = std::find_if(sp.begin(), sp.end(), pred);
    return it != sp.end() ? std::distance(sp.begin(), it) : sp.size();
}

/**
 * @brief Count occurrences of value in span
 * @tparam T Element type
 * @param sp Span to search
 * @param value Value to count
 * @return Number of occurrences
 */
template<typename T>
[[nodiscard]] constexpr std::size_t count(std::span<const T> sp, const T& value) noexcept {
    return std::count(sp.begin(), sp.end(), value);
}

// ============================================================================
// Span Modification (for mutable spans)
// ============================================================================

/**
 * @brief Fill span with value
 * @tparam T Element type
 * @param sp Span to fill
 * @param value Fill value
 */
template<typename T>
constexpr void fill(std::span<T> sp, const T& value) noexcept {
    std::fill(sp.begin(), sp.end(), value);
}

/**
 * @brief Copy from source span to destination span
 * @tparam T Element type
 * @param dest Destination span
 * @param src Source span
 * @return Number of elements copied
 */
template<typename T>
constexpr std::size_t copy(std::span<T> dest, std::span<const T> src) noexcept {
    std::size_t count = std::min(dest.size(), src.size());
    std::copy_n(src.begin(), count, dest.begin());
    return count;
}

/**
 * @brief Reverse span in place
 * @tparam T Element type
 * @param sp Span to reverse
 */
template<typename T>
constexpr void reverse(std::span<T> sp) noexcept {
    std::reverse(sp.begin(), sp.end());
}

/**
 * @brief Sort span in place
 * @tparam T Element type
 * @param sp Span to sort
 */
template<typename T>
constexpr void sort(std::span<T> sp) noexcept {
    std::sort(sp.begin(), sp.end());
}

/**
 * @brief Sort span with custom comparator
 * @tparam T Element type
 * @tparam Compare Comparator type
 * @param sp Span to sort
 * @param comp Comparator function
 */
template<typename T, typename Compare>
constexpr void sort(std::span<T> sp, Compare comp) noexcept {
    std::sort(sp.begin(), sp.end(), comp);
}

// ============================================================================
// Span Conversion
// ============================================================================

/**
 * @brief Convert span to vector
 * @tparam T Element type
 * @param sp Source span
 * @return Vector containing span elements
 */
template<typename T>
[[nodiscard]] std::vector<std::remove_cv_t<T>> toVector(std::span<T> sp) {
    return std::vector<std::remove_cv_t<T>>(sp.begin(), sp.end());
}

/**
 * @brief Convert character span to string
 * @param sp Character span
 * @return String containing span characters
 */
[[nodiscard]] inline std::string toString(CharSpan sp) {
    return std::string(sp.begin(), sp.end());
}

/**
 * @brief Convert character span to string_view
 * @param sp Character span
 * @return String view over span
 */
[[nodiscard]] inline std::string_view toStringView(CharSpan sp) noexcept {
    return std::string_view(sp.data(), sp.size());
}

// ============================================================================
// Byte Span Utilities
// ============================================================================

/**
 * @brief Convert byte span to hex string
 * @param sp Byte span
 * @param separator Separator between bytes (default: none)
 * @return Hex string representation
 */
[[nodiscard]] inline std::string toHexString(ByteSpan sp, 
                                             std::string_view separator = "") {
    static constexpr char hexChars[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(sp.size() * (2 + separator.size()));
    
    for (std::size_t i = 0; i < sp.size(); ++i) {
        if (i > 0 && !separator.empty()) {
            result += separator;
        }
        result += hexChars[(sp[i] >> 4) & 0x0F];
        result += hexChars[sp[i] & 0x0F];
    }
    return result;
}

/**
 * @brief Calculate simple checksum of byte span
 * @param sp Byte span
 * @return 32-bit checksum
 */
[[nodiscard]] inline uint32_t checksum(ByteSpan sp) noexcept {
    uint32_t sum = 0;
    for (auto byte : sp) {
        sum += byte;
    }
    return sum;
}

/**
 * @brief XOR all bytes in span
 * @param sp Byte span
 * @return XOR of all bytes
 */
[[nodiscard]] inline uint8_t xorBytes(ByteSpan sp) noexcept {
    uint8_t result = 0;
    for (auto byte : sp) {
        result ^= byte;
    }
    return result;
}

/**
 * @brief Check if byte span contains only zeros
 * @param sp Byte span
 * @return true if all bytes are zero
 */
[[nodiscard]] inline bool isAllZeros(ByteSpan sp) noexcept {
    return std::all_of(sp.begin(), sp.end(), [](uint8_t b) { return b == 0; });
}

/**
 * @brief Check if byte span contains only 0xFF
 * @param sp Byte span
 * @return true if all bytes are 0xFF
 */
[[nodiscard]] inline bool isAllOnes(ByteSpan sp) noexcept {
    return std::all_of(sp.begin(), sp.end(), [](uint8_t b) { return b == 0xFF; });
}

// ============================================================================
// Span Iteration Utilities
// ============================================================================

/**
 * @brief Iterate over span with index
 * @tparam T Element type
 * @tparam Func Function type (takes index and element)
 * @param sp Span to iterate
 * @param func Function to call for each element
 */
template<typename T, typename Func>
constexpr void forEachIndexed(std::span<T> sp, Func func) {
    for (std::size_t i = 0; i < sp.size(); ++i) {
        func(i, sp[i]);
    }
}

/**
 * @brief Chunk span into fixed-size pieces
 * @tparam T Element type
 * @param sp Source span
 * @param chunkSize Size of each chunk
 * @return Vector of span chunks
 */
template<typename T>
[[nodiscard]] std::vector<std::span<T>> chunk(std::span<T> sp, std::size_t chunkSize) {
    std::vector<std::span<T>> chunks;
    if (chunkSize == 0) return chunks;
    
    chunks.reserve((sp.size() + chunkSize - 1) / chunkSize);
    
    for (std::size_t i = 0; i < sp.size(); i += chunkSize) {
        std::size_t remaining = sp.size() - i;
        chunks.push_back(sp.subspan(i, std::min(chunkSize, remaining)));
    }
    return chunks;
}

/**
 * @brief Create sliding window over span
 * @tparam T Element type
 * @param sp Source span
 * @param windowSize Window size
 * @return Vector of overlapping span windows
 */
template<typename T>
[[nodiscard]] std::vector<std::span<T>> slidingWindow(std::span<T> sp, std::size_t windowSize) {
    std::vector<std::span<T>> windows;
    if (windowSize == 0 || windowSize > sp.size()) return windows;
    
    windows.reserve(sp.size() - windowSize + 1);
    
    for (std::size_t i = 0; i <= sp.size() - windowSize; ++i) {
        windows.push_back(sp.subspan(i, windowSize));
    }
    return windows;
}

} // namespace span_utils
} // namespace dfsmshsm

#endif // HSM_SPAN_HPP
