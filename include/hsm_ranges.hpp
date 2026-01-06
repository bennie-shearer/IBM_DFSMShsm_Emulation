/**
 * @file hsm_ranges.hpp
 * @brief C++20 Ranges utilities for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - Range-based algorithms
 * - View adapters and utilities
 * - Range conversion helpers
 * - Pipeline operations
 */

#ifndef HSM_RANGES_HPP
#define HSM_RANGES_HPP

#include <ranges>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include <functional>
#include <optional>

namespace dfsmshsm {
namespace ranges {

// ============================================================================
// Range Concepts (re-exported for convenience)
// ============================================================================

template<typename R>
concept Range = std::ranges::range<R>;

template<typename R>
concept SizedRange = std::ranges::sized_range<R>;

template<typename R>
concept RandomAccessRange = std::ranges::random_access_range<R>;

template<typename R>
concept ContiguousRange = std::ranges::contiguous_range<R>;

// ============================================================================
// Range to Container Conversions
// ============================================================================

/**
 * @brief Convert any range to vector
 * @tparam R Range type
 * @param range Input range
 * @return Vector containing range elements
 */
template<Range R>
[[nodiscard]] auto toVector(R&& range) {
    using ValueType = std::ranges::range_value_t<R>;
    std::vector<ValueType> result;
    
    if constexpr (SizedRange<R>) {
        result.reserve(std::ranges::size(range));
    }
    
    for (auto&& elem : range) {
        result.push_back(std::forward<decltype(elem)>(elem));
    }
    return result;
}

/**
 * @brief Convert range to string (for char ranges)
 * @tparam R Range type
 * @param range Input range of characters
 * @return String containing range characters
 */
template<Range R>
requires std::same_as<std::ranges::range_value_t<R>, char>
[[nodiscard]] std::string toString(R&& range) {
    std::string result;
    
    if constexpr (SizedRange<R>) {
        result.reserve(std::ranges::size(range));
    }
    
    for (char c : range) {
        result += c;
    }
    return result;
}

// ============================================================================
// Range Algorithms
// ============================================================================

/**
 * @brief Check if all elements satisfy predicate
 * @tparam R Range type
 * @tparam Pred Predicate type
 * @param range Input range
 * @param pred Predicate function
 * @return true if all elements satisfy predicate
 */
template<Range R, typename Pred>
[[nodiscard]] bool allOf(R&& range, Pred pred) {
    return std::ranges::all_of(range, pred);
}

/**
 * @brief Check if any element satisfies predicate
 * @tparam R Range type
 * @tparam Pred Predicate type
 * @param range Input range
 * @param pred Predicate function
 * @return true if any element satisfies predicate
 */
template<Range R, typename Pred>
[[nodiscard]] bool anyOf(R&& range, Pred pred) {
    return std::ranges::any_of(range, pred);
}

/**
 * @brief Check if no element satisfies predicate
 * @tparam R Range type
 * @tparam Pred Predicate type
 * @param range Input range
 * @param pred Predicate function
 * @return true if no element satisfies predicate
 */
template<Range R, typename Pred>
[[nodiscard]] bool noneOf(R&& range, Pred pred) {
    return std::ranges::none_of(range, pred);
}

/**
 * @brief Count elements equal to value
 * @tparam R Range type
 * @tparam T Value type
 * @param range Input range
 * @param value Value to count
 * @return Number of matching elements
 */
template<Range R, typename T>
[[nodiscard]] auto count(R&& range, const T& value) {
    return std::ranges::count(range, value);
}

/**
 * @brief Count elements satisfying predicate
 * @tparam R Range type
 * @tparam Pred Predicate type
 * @param range Input range
 * @param pred Predicate function
 * @return Number of matching elements
 */
template<Range R, typename Pred>
[[nodiscard]] auto countIf(R&& range, Pred pred) {
    return std::ranges::count_if(range, pred);
}

/**
 * @brief Find first element equal to value
 * @tparam R Range type
 * @tparam T Value type
 * @param range Input range
 * @param value Value to find
 * @return Iterator to found element or end
 */
template<Range R, typename T>
[[nodiscard]] auto find(R&& range, const T& value) {
    return std::ranges::find(range, value);
}

/**
 * @brief Find first element satisfying predicate
 * @tparam R Range type
 * @tparam Pred Predicate type
 * @param range Input range
 * @param pred Predicate function
 * @return Iterator to found element or end
 */
template<Range R, typename Pred>
[[nodiscard]] auto findIf(R&& range, Pred pred) {
    return std::ranges::find_if(range, pred);
}

/**
 * @brief Check if range contains value
 * @tparam R Range type
 * @tparam T Value type
 * @param range Input range
 * @param value Value to find
 * @return true if value is found
 */
template<Range R, typename T>
[[nodiscard]] bool contains(R&& range, const T& value) {
    return std::ranges::find(range, value) != std::ranges::end(range);
}

// ============================================================================
// Range Aggregation
// ============================================================================

/**
 * @brief Sum all elements in range
 * @tparam R Range type
 * @param range Input range
 * @return Sum of elements
 */
template<Range R>
[[nodiscard]] auto sum(R&& range) {
    using ValueType = std::ranges::range_value_t<R>;
    ValueType result{};
    for (const auto& elem : range) {
        result += elem;
    }
    return result;
}

/**
 * @brief Calculate product of all elements
 * @tparam R Range type
 * @param range Input range
 * @return Product of elements
 */
template<Range R>
[[nodiscard]] auto product(R&& range) {
    using ValueType = std::ranges::range_value_t<R>;
    ValueType result{1};
    for (const auto& elem : range) {
        result *= elem;
    }
    return result;
}

/**
 * @brief Find minimum element
 * @tparam R Range type
 * @param range Input range
 * @return Optional containing minimum or nullopt if empty
 */
template<Range R>
[[nodiscard]] auto min(R&& range) -> std::optional<std::ranges::range_value_t<R>> {
    auto it = std::ranges::min_element(range);
    if (it == std::ranges::end(range)) {
        return std::nullopt;
    }
    return *it;
}

/**
 * @brief Find maximum element
 * @tparam R Range type
 * @param range Input range
 * @return Optional containing maximum or nullopt if empty
 */
template<Range R>
[[nodiscard]] auto max(R&& range) -> std::optional<std::ranges::range_value_t<R>> {
    auto it = std::ranges::max_element(range);
    if (it == std::ranges::end(range)) {
        return std::nullopt;
    }
    return *it;
}

/**
 * @brief Calculate average of elements
 * @tparam R Range type
 * @param range Input range
 * @return Optional containing average or nullopt if empty
 */
template<Range R>
[[nodiscard]] auto average(R&& range) -> std::optional<double> {
    if (std::ranges::empty(range)) {
        return std::nullopt;
    }
    
    double total = 0.0;
    std::size_t count = 0;
    
    for (const auto& elem : range) {
        total += static_cast<double>(elem);
        ++count;
    }
    
    return total / static_cast<double>(count);
}

// ============================================================================
// Range Transformation
// ============================================================================

/**
 * @brief Transform range with function and collect to vector
 * @tparam R Range type
 * @tparam Func Transformation function type
 * @param range Input range
 * @param func Transformation function
 * @return Vector of transformed elements
 */
template<Range R, typename Func>
[[nodiscard]] auto map(R&& range, Func func) {
    using ResultType = std::invoke_result_t<Func, std::ranges::range_value_t<R>>;
    std::vector<ResultType> result;
    
    if constexpr (SizedRange<R>) {
        result.reserve(std::ranges::size(range));
    }
    
    for (auto&& elem : range) {
        result.push_back(func(std::forward<decltype(elem)>(elem)));
    }
    return result;
}

/**
 * @brief Filter range with predicate and collect to vector
 * @tparam R Range type
 * @tparam Pred Predicate type
 * @param range Input range
 * @param pred Filter predicate
 * @return Vector of elements satisfying predicate
 */
template<Range R, typename Pred>
[[nodiscard]] auto filter(R&& range, Pred pred) {
    using ValueType = std::ranges::range_value_t<R>;
    std::vector<ValueType> result;
    
    for (auto&& elem : range) {
        if (pred(elem)) {
            result.push_back(std::forward<decltype(elem)>(elem));
        }
    }
    return result;
}

/**
 * @brief Reduce range with binary operation
 * @tparam R Range type
 * @tparam T Initial value type
 * @tparam BinaryOp Binary operation type
 * @param range Input range
 * @param init Initial value
 * @param op Binary operation
 * @return Reduced value
 */
template<Range R, typename T, typename BinaryOp>
[[nodiscard]] T reduce(R&& range, T init, BinaryOp op) {
    for (const auto& elem : range) {
        init = op(std::move(init), elem);
    }
    return init;
}

/**
 * @brief Take first n elements
 * @tparam R Range type
 * @param range Input range
 * @param n Number of elements to take
 * @return Vector of first n elements
 */
template<Range R>
[[nodiscard]] auto take(R&& range, std::size_t n) {
    using ValueType = std::ranges::range_value_t<R>;
    std::vector<ValueType> result;
    result.reserve(n);
    
    std::size_t count = 0;
    for (auto&& elem : range) {
        if (count >= n) break;
        result.push_back(std::forward<decltype(elem)>(elem));
        ++count;
    }
    return result;
}

/**
 * @brief Skip first n elements
 * @tparam R Range type
 * @param range Input range
 * @param n Number of elements to skip
 * @return Vector of remaining elements
 */
template<Range R>
[[nodiscard]] auto skip(R&& range, std::size_t n) {
    using ValueType = std::ranges::range_value_t<R>;
    std::vector<ValueType> result;
    
    std::size_t count = 0;
    for (auto&& elem : range) {
        if (count >= n) {
            result.push_back(std::forward<decltype(elem)>(elem));
        }
        ++count;
    }
    return result;
}

/**
 * @brief Take elements while predicate is true
 * @tparam R Range type
 * @tparam Pred Predicate type
 * @param range Input range
 * @param pred Predicate function
 * @return Vector of taken elements
 */
template<Range R, typename Pred>
[[nodiscard]] auto takeWhile(R&& range, Pred pred) {
    using ValueType = std::ranges::range_value_t<R>;
    std::vector<ValueType> result;
    
    for (auto&& elem : range) {
        if (!pred(elem)) break;
        result.push_back(std::forward<decltype(elem)>(elem));
    }
    return result;
}

/**
 * @brief Skip elements while predicate is true
 * @tparam R Range type
 * @tparam Pred Predicate type
 * @param range Input range
 * @param pred Predicate function
 * @return Vector of remaining elements
 */
template<Range R, typename Pred>
[[nodiscard]] auto skipWhile(R&& range, Pred pred) {
    using ValueType = std::ranges::range_value_t<R>;
    std::vector<ValueType> result;
    bool skipping = true;
    
    for (auto&& elem : range) {
        if (skipping && pred(elem)) {
            continue;
        }
        skipping = false;
        result.push_back(std::forward<decltype(elem)>(elem));
    }
    return result;
}

// ============================================================================
// Range Partitioning
// ============================================================================

/**
 * @brief Partition range by predicate
 * @tparam R Range type
 * @tparam Pred Predicate type
 * @param range Input range
 * @param pred Partition predicate
 * @return Pair of vectors (matching, not matching)
 */
template<Range R, typename Pred>
[[nodiscard]] auto partition(R&& range, Pred pred) {
    using ValueType = std::ranges::range_value_t<R>;
    std::vector<ValueType> matching;
    std::vector<ValueType> notMatching;
    
    for (auto&& elem : range) {
        if (pred(elem)) {
            matching.push_back(std::forward<decltype(elem)>(elem));
        } else {
            notMatching.push_back(std::forward<decltype(elem)>(elem));
        }
    }
    return std::make_pair(std::move(matching), std::move(notMatching));
}

/**
 * @brief Group consecutive equal elements
 * @tparam R Range type
 * @param range Input range
 * @return Vector of groups (each group is a vector)
 */
template<Range R>
[[nodiscard]] auto group(R&& range) {
    using ValueType = std::ranges::range_value_t<R>;
    std::vector<std::vector<ValueType>> result;
    
    std::optional<ValueType> lastValue;
    
    for (auto&& elem : range) {
        if (!lastValue || *lastValue != elem) {
            result.emplace_back();
            lastValue = elem;
        }
        result.back().push_back(std::forward<decltype(elem)>(elem));
    }
    return result;
}

/**
 * @brief Remove consecutive duplicates
 * @tparam R Range type
 * @param range Input range
 * @return Vector with consecutive duplicates removed
 */
template<Range R>
[[nodiscard]] auto unique(R&& range) {
    using ValueType = std::ranges::range_value_t<R>;
    std::vector<ValueType> result;
    
    std::optional<ValueType> lastValue;
    
    for (auto&& elem : range) {
        if (!lastValue || *lastValue != elem) {
            result.push_back(std::forward<decltype(elem)>(elem));
            lastValue = elem;
        }
    }
    return result;
}

// ============================================================================
// Range Combination
// ============================================================================

/**
 * @brief Zip two ranges together
 * @tparam R1 First range type
 * @tparam R2 Second range type
 * @param range1 First range
 * @param range2 Second range
 * @return Vector of pairs
 */
template<Range R1, Range R2>
[[nodiscard]] auto zip(R1&& range1, R2&& range2) {
    using V1 = std::ranges::range_value_t<R1>;
    using V2 = std::ranges::range_value_t<R2>;
    std::vector<std::pair<V1, V2>> result;
    
    auto it1 = std::ranges::begin(range1);
    auto it2 = std::ranges::begin(range2);
    auto end1 = std::ranges::end(range1);
    auto end2 = std::ranges::end(range2);
    
    while (it1 != end1 && it2 != end2) {
        result.emplace_back(*it1, *it2);
        ++it1;
        ++it2;
    }
    return result;
}

/**
 * @brief Enumerate range with indices
 * @tparam R Range type
 * @param range Input range
 * @return Vector of pairs (index, element)
 */
template<Range R>
[[nodiscard]] auto enumerate(R&& range) {
    using ValueType = std::ranges::range_value_t<R>;
    std::vector<std::pair<std::size_t, ValueType>> result;
    
    if constexpr (SizedRange<R>) {
        result.reserve(std::ranges::size(range));
    }
    
    std::size_t index = 0;
    for (auto&& elem : range) {
        result.emplace_back(index++, std::forward<decltype(elem)>(elem));
    }
    return result;
}

/**
 * @brief Flatten nested range
 * @tparam R Range of ranges type
 * @param range Nested range
 * @return Flattened vector
 */
template<Range R>
requires Range<std::ranges::range_value_t<R>>
[[nodiscard]] auto flatten(R&& range) {
    using InnerRange = std::ranges::range_value_t<R>;
    using ValueType = std::ranges::range_value_t<InnerRange>;
    std::vector<ValueType> result;
    
    for (auto&& inner : range) {
        for (auto&& elem : inner) {
            result.push_back(std::forward<decltype(elem)>(elem));
        }
    }
    return result;
}

// ============================================================================
// Range Sorting
// ============================================================================

/**
 * @brief Sort range and return as vector
 * @tparam R Range type
 * @param range Input range
 * @return Sorted vector
 */
template<Range R>
[[nodiscard]] auto sorted(R&& range) {
    auto result = toVector(std::forward<R>(range));
    std::ranges::sort(result);
    return result;
}

/**
 * @brief Sort range with custom comparator
 * @tparam R Range type
 * @tparam Compare Comparator type
 * @param range Input range
 * @param comp Comparator function
 * @return Sorted vector
 */
template<Range R, typename Compare>
[[nodiscard]] auto sorted(R&& range, Compare comp) {
    auto result = toVector(std::forward<R>(range));
    std::ranges::sort(result, comp);
    return result;
}

/**
 * @brief Sort range by key
 * @tparam R Range type
 * @tparam KeyFunc Key extraction function type
 * @param range Input range
 * @param keyFunc Key extraction function
 * @return Sorted vector
 */
template<Range R, typename KeyFunc>
[[nodiscard]] auto sortedBy(R&& range, KeyFunc keyFunc) {
    auto result = toVector(std::forward<R>(range));
    std::ranges::sort(result, [&](const auto& a, const auto& b) {
        return keyFunc(a) < keyFunc(b);
    });
    return result;
}

/**
 * @brief Reverse range and return as vector
 * @tparam R Range type
 * @param range Input range
 * @return Reversed vector
 */
template<Range R>
[[nodiscard]] auto reversed(R&& range) {
    auto result = toVector(std::forward<R>(range));
    std::ranges::reverse(result);
    return result;
}

// ============================================================================
// Range Generation
// ============================================================================

/**
 * @brief Generate range of integers [start, end)
 * @param start Start value (inclusive)
 * @param end End value (exclusive)
 * @return Vector of integers
 */
[[nodiscard]] inline std::vector<int> iota(int start, int end) {
    std::vector<int> result;
    if (start < end) {
        result.reserve(static_cast<std::size_t>(end - start));
        for (int i = start; i < end; ++i) {
            result.push_back(i);
        }
    }
    return result;
}

/**
 * @brief Generate range of integers [0, n)
 * @param n Number of elements
 * @return Vector of integers
 */
[[nodiscard]] inline std::vector<int> iota(int n) {
    return iota(0, n);
}

/**
 * @brief Repeat value n times
 * @tparam T Value type
 * @param value Value to repeat
 * @param n Number of repetitions
 * @return Vector of repeated values
 */
template<typename T>
[[nodiscard]] std::vector<T> repeat(const T& value, std::size_t n) {
    return std::vector<T>(n, value);
}

} // namespace ranges
} // namespace dfsmshsm

#endif // HSM_RANGES_HPP
