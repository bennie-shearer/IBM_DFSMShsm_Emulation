/**
 * @file hsm_algorithm.hpp
 * @brief Algorithm utilities and common patterns for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - Numeric algorithms
 * - Search algorithms
 * - Sorting utilities
 * - Set operations
 * - Statistical functions
 */

#ifndef HSM_ALGORITHM_HPP
#define HSM_ALGORITHM_HPP

#include <algorithm>
#include <numeric>
#include <vector>
#include <cmath>
#include <limits>
#include <optional>
#include <functional>
#include <unordered_set>
#include <map>

namespace dfsmshsm {
namespace algorithm {

// ============================================================================
// Numeric Utilities
// ============================================================================

/**
 * @brief Clamp value to range [min, max]
 * @tparam T Numeric type
 * @param value Value to clamp
 * @param minVal Minimum value
 * @param maxVal Maximum value
 * @return Clamped value
 */
template<typename T>
[[nodiscard]] constexpr T clamp(T value, T minVal, T maxVal) noexcept {
    return std::max(minVal, std::min(value, maxVal));
}

/**
 * @brief Linear interpolation
 * @tparam T Numeric type
 * @param a Start value
 * @param b End value
 * @param t Interpolation factor [0, 1]
 * @return Interpolated value
 */
template<typename T>
[[nodiscard]] constexpr T lerp(T a, T b, double t) noexcept {
    return static_cast<T>(a + t * (b - a));
}

/**
 * @brief Map value from one range to another
 * @tparam T Numeric type
 * @param value Input value
 * @param inMin Input range minimum
 * @param inMax Input range maximum
 * @param outMin Output range minimum
 * @param outMax Output range maximum
 * @return Mapped value
 */
template<typename T>
[[nodiscard]] constexpr T mapRange(T value, T inMin, T inMax, T outMin, T outMax) noexcept {
    if (inMax == inMin) return outMin;
    double ratio = static_cast<double>(value - inMin) / static_cast<double>(inMax - inMin);
    return static_cast<T>(outMin + ratio * (outMax - outMin));
}

/**
 * @brief Check if value is in range [min, max]
 * @tparam T Comparable type
 * @param value Value to check
 * @param minVal Minimum value
 * @param maxVal Maximum value
 * @return true if value is in range
 */
template<typename T>
[[nodiscard]] constexpr bool inRange(T value, T minVal, T maxVal) noexcept {
    return value >= minVal && value <= maxVal;
}

/**
 * @brief Check if two floating-point values are approximately equal
 * @tparam T Floating-point type
 * @param a First value
 * @param b Second value
 * @param epsilon Tolerance (default: numeric_limits epsilon * 100)
 * @return true if approximately equal
 */
template<typename T>
requires std::floating_point<T>
[[nodiscard]] bool approxEqual(T a, T b, T epsilon = std::numeric_limits<T>::epsilon() * 100) noexcept {
    return std::abs(a - b) <= epsilon * std::max(T{1}, std::max(std::abs(a), std::abs(b)));
}

/**
 * @brief Calculate greatest common divisor
 * @tparam T Integral type
 * @param a First value
 * @param b Second value
 * @return GCD of a and b
 */
template<typename T>
requires std::integral<T>
[[nodiscard]] constexpr T gcd(T a, T b) noexcept {
    while (b != 0) {
        T temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

/**
 * @brief Calculate least common multiple
 * @tparam T Integral type
 * @param a First value
 * @param b Second value
 * @return LCM of a and b
 */
template<typename T>
requires std::integral<T>
[[nodiscard]] constexpr T lcm(T a, T b) noexcept {
    return (a / gcd(a, b)) * b;
}

/**
 * @brief Check if integer is power of two
 * @tparam T Integral type
 * @param n Value to check
 * @return true if power of two
 */
template<typename T>
requires std::integral<T>
[[nodiscard]] constexpr bool isPowerOfTwo(T n) noexcept {
    return n > 0 && (n & (n - 1)) == 0;
}

/**
 * @brief Round up to next power of two
 * @param n Input value
 * @return Next power of two >= n
 */
[[nodiscard]] constexpr uint64_t nextPowerOfTwo(uint64_t n) noexcept {
    if (n == 0) return 1;
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

// ============================================================================
// Statistical Functions
// ============================================================================

/**
 * @brief Calculate mean of container elements
 * @tparam Container Container type
 * @param container Input container
 * @return Mean value or nullopt if empty
 */
template<typename Container>
[[nodiscard]] std::optional<double> mean(const Container& container) {
    if (container.empty()) return std::nullopt;
    
    double sum = 0.0;
    for (const auto& elem : container) {
        sum += static_cast<double>(elem);
    }
    return sum / static_cast<double>(container.size());
}

/**
 * @brief Calculate median of container elements
 * @tparam Container Container type
 * @param container Input container
 * @return Median value or nullopt if empty
 */
template<typename Container>
[[nodiscard]] std::optional<double> median(const Container& container) {
    if (container.empty()) return std::nullopt;
    
    std::vector<typename Container::value_type> sorted(container.begin(), container.end());
    std::sort(sorted.begin(), sorted.end());
    
    std::size_t n = sorted.size();
    if (n % 2 == 0) {
        return (static_cast<double>(sorted[n/2 - 1]) + static_cast<double>(sorted[n/2])) / 2.0;
    }
    return static_cast<double>(sorted[n/2]);
}

/**
 * @brief Calculate mode of container elements
 * @tparam Container Container type
 * @param container Input container
 * @return Most frequent element or nullopt if empty
 */
template<typename Container>
[[nodiscard]] std::optional<typename Container::value_type> mode(const Container& container) {
    if (container.empty()) return std::nullopt;
    
    std::map<typename Container::value_type, std::size_t> counts;
    for (const auto& elem : container) {
        ++counts[elem];
    }
    
    auto maxIt = std::max_element(counts.begin(), counts.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    return maxIt->first;
}

/**
 * @brief Calculate variance of container elements
 * @tparam Container Container type
 * @param container Input container
 * @param population true for population variance, false for sample variance
 * @return Variance or nullopt if insufficient elements
 */
template<typename Container>
[[nodiscard]] std::optional<double> variance(const Container& container, bool population = true) {
    std::size_t n = container.size();
    if (n == 0 || (!population && n < 2)) return std::nullopt;
    
    auto m = mean(container);
    if (!m) return std::nullopt;
    
    double sumSquares = 0.0;
    for (const auto& elem : container) {
        double diff = static_cast<double>(elem) - *m;
        sumSquares += diff * diff;
    }
    
    return sumSquares / static_cast<double>(population ? n : n - 1);
}

/**
 * @brief Calculate standard deviation
 * @tparam Container Container type
 * @param container Input container
 * @param population true for population std dev, false for sample std dev
 * @return Standard deviation or nullopt if insufficient elements
 */
template<typename Container>
[[nodiscard]] std::optional<double> standardDeviation(const Container& container, bool population = true) {
    auto var = variance(container, population);
    if (!var) return std::nullopt;
    return std::sqrt(*var);
}

/**
 * @brief Calculate percentile
 * @tparam Container Container type
 * @param container Input container
 * @param percentile Percentile (0-100)
 * @return Percentile value or nullopt if empty
 */
template<typename Container>
[[nodiscard]] std::optional<double> percentile(const Container& container, double percentile) {
    if (container.empty() || percentile < 0 || percentile > 100) return std::nullopt;
    
    std::vector<typename Container::value_type> sorted(container.begin(), container.end());
    std::sort(sorted.begin(), sorted.end());
    
    double index = (percentile / 100.0) * (sorted.size() - 1);
    std::size_t lower = static_cast<std::size_t>(index);
    std::size_t upper = std::min(lower + 1, sorted.size() - 1);
    double fraction = index - lower;
    
    return static_cast<double>(sorted[lower]) * (1 - fraction) + 
           static_cast<double>(sorted[upper]) * fraction;
}

// ============================================================================
// Search Algorithms
// ============================================================================

/**
 * @brief Binary search for value
 * @tparam Container Container type
 * @tparam T Value type
 * @param container Sorted container
 * @param value Value to find
 * @return Index of value or nullopt if not found
 */
template<typename Container, typename T>
[[nodiscard]] std::optional<std::size_t> binarySearch(const Container& container, const T& value) {
    auto it = std::lower_bound(container.begin(), container.end(), value);
    if (it != container.end() && *it == value) {
        return std::distance(container.begin(), it);
    }
    return std::nullopt;
}

/**
 * @brief Find lower bound index
 * @tparam Container Container type
 * @tparam T Value type
 * @param container Sorted container
 * @param value Value to find
 * @return Index of first element >= value
 */
template<typename Container, typename T>
[[nodiscard]] std::size_t lowerBoundIndex(const Container& container, const T& value) {
    auto it = std::lower_bound(container.begin(), container.end(), value);
    return std::distance(container.begin(), it);
}

/**
 * @brief Find upper bound index
 * @tparam Container Container type
 * @tparam T Value type
 * @param container Sorted container
 * @param value Value to find
 * @return Index of first element > value
 */
template<typename Container, typename T>
[[nodiscard]] std::size_t upperBoundIndex(const Container& container, const T& value) {
    auto it = std::upper_bound(container.begin(), container.end(), value);
    return std::distance(container.begin(), it);
}

/**
 * @brief Find k smallest elements
 * @tparam Container Container type
 * @param container Input container
 * @param k Number of elements to find
 * @return Vector of k smallest elements
 */
template<typename Container>
[[nodiscard]] std::vector<typename Container::value_type> kSmallest(const Container& container, std::size_t k) {
    std::vector<typename Container::value_type> result(container.begin(), container.end());
    if (k >= result.size()) {
        std::sort(result.begin(), result.end());
        return result;
    }
    std::partial_sort(result.begin(), result.begin() + k, result.end());
    result.resize(k);
    return result;
}

/**
 * @brief Find k largest elements
 * @tparam Container Container type
 * @param container Input container
 * @param k Number of elements to find
 * @return Vector of k largest elements
 */
template<typename Container>
[[nodiscard]] std::vector<typename Container::value_type> kLargest(const Container& container, std::size_t k) {
    std::vector<typename Container::value_type> result(container.begin(), container.end());
    if (k >= result.size()) {
        std::sort(result.begin(), result.end(), std::greater<>());
        return result;
    }
    std::partial_sort(result.begin(), result.begin() + k, result.end(), std::greater<>());
    result.resize(k);
    return result;
}

// ============================================================================
// Set Operations
// ============================================================================

/**
 * @brief Compute set intersection
 * @tparam Container Container type
 * @param a First container
 * @param b Second container
 * @return Intersection of elements
 */
template<typename Container>
[[nodiscard]] std::vector<typename Container::value_type> intersection(const Container& a, const Container& b) {
    std::unordered_set<typename Container::value_type> setB(b.begin(), b.end());
    std::vector<typename Container::value_type> result;
    
    for (const auto& elem : a) {
        if (setB.count(elem)) {
            result.push_back(elem);
        }
    }
    return result;
}

/**
 * @brief Compute set union
 * @tparam Container Container type
 * @param a First container
 * @param b Second container
 * @return Union of elements
 */
template<typename Container>
[[nodiscard]] std::vector<typename Container::value_type> setUnion(const Container& a, const Container& b) {
    std::unordered_set<typename Container::value_type> combined(a.begin(), a.end());
    combined.insert(b.begin(), b.end());
    return std::vector<typename Container::value_type>(combined.begin(), combined.end());
}

/**
 * @brief Compute set difference (a - b)
 * @tparam Container Container type
 * @param a First container
 * @param b Second container
 * @return Elements in a but not in b
 */
template<typename Container>
[[nodiscard]] std::vector<typename Container::value_type> difference(const Container& a, const Container& b) {
    std::unordered_set<typename Container::value_type> setB(b.begin(), b.end());
    std::vector<typename Container::value_type> result;
    
    for (const auto& elem : a) {
        if (!setB.count(elem)) {
            result.push_back(elem);
        }
    }
    return result;
}

/**
 * @brief Compute symmetric difference
 * @tparam Container Container type
 * @param a First container
 * @param b Second container
 * @return Elements in a or b but not both
 */
template<typename Container>
[[nodiscard]] std::vector<typename Container::value_type> symmetricDifference(const Container& a, const Container& b) {
    std::unordered_set<typename Container::value_type> setA(a.begin(), a.end());
    std::unordered_set<typename Container::value_type> setB(b.begin(), b.end());
    std::vector<typename Container::value_type> result;
    
    for (const auto& elem : a) {
        if (!setB.count(elem)) {
            result.push_back(elem);
        }
    }
    for (const auto& elem : b) {
        if (!setA.count(elem)) {
            result.push_back(elem);
        }
    }
    return result;
}

/**
 * @brief Check if a is subset of b
 * @tparam Container Container type
 * @param a Potential subset
 * @param b Potential superset
 * @return true if a is subset of b
 */
template<typename Container>
[[nodiscard]] bool isSubset(const Container& a, const Container& b) {
    std::unordered_set<typename Container::value_type> setB(b.begin(), b.end());
    for (const auto& elem : a) {
        if (!setB.count(elem)) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Container Transformation
// ============================================================================

/**
 * @brief Remove duplicates from container (preserves order)
 * @tparam Container Container type
 * @param container Input container
 * @return Container with duplicates removed
 */
template<typename Container>
[[nodiscard]] std::vector<typename Container::value_type> removeDuplicates(const Container& container) {
    std::unordered_set<typename Container::value_type> seen;
    std::vector<typename Container::value_type> result;
    
    for (const auto& elem : container) {
        if (seen.insert(elem).second) {
            result.push_back(elem);
        }
    }
    return result;
}

/**
 * @brief Rotate container elements
 * @tparam Container Container type
 * @param container Input container
 * @param n Number of positions to rotate (positive = left, negative = right)
 * @return Rotated container
 */
template<typename Container>
[[nodiscard]] std::vector<typename Container::value_type> rotate(const Container& container, int n) {
    if (container.empty()) return {};
    
    std::vector<typename Container::value_type> result(container.begin(), container.end());
    int size = static_cast<int>(result.size());
    n = ((n % size) + size) % size;  // Normalize to positive
    
    std::rotate(result.begin(), result.begin() + n, result.end());
    return result;
}

/**
 * @brief Shuffle container randomly
 * @tparam Container Container type
 * @tparam RNG Random number generator type
 * @param container Input container
 * @param rng Random number generator
 * @return Shuffled container
 */
template<typename Container, typename RNG>
[[nodiscard]] std::vector<typename Container::value_type> shuffle(const Container& container, RNG& rng) {
    std::vector<typename Container::value_type> result(container.begin(), container.end());
    std::shuffle(result.begin(), result.end(), rng);
    return result;
}

/**
 * @brief Get indices that would sort the container
 * @tparam Container Container type
 * @param container Input container
 * @return Vector of indices
 */
template<typename Container>
[[nodiscard]] std::vector<std::size_t> argsort(const Container& container) {
    std::vector<std::size_t> indices(container.size());
    std::iota(indices.begin(), indices.end(), 0);
    
    std::sort(indices.begin(), indices.end(), [&container](std::size_t i, std::size_t j) {
        auto it_i = container.begin();
        auto it_j = container.begin();
        std::advance(it_i, i);
        std::advance(it_j, j);
        return *it_i < *it_j;
    });
    
    return indices;
}

/**
 * @brief Reorder container by indices
 * @tparam Container Container type
 * @param container Input container
 * @param indices Index order
 * @return Reordered container
 */
template<typename Container>
[[nodiscard]] std::vector<typename Container::value_type> reorder(const Container& container, 
                                                                    const std::vector<std::size_t>& indices) {
    std::vector<typename Container::value_type> result;
    result.reserve(indices.size());
    
    std::vector<typename Container::value_type> vec(container.begin(), container.end());
    for (std::size_t idx : indices) {
        if (idx < vec.size()) {
            result.push_back(vec[idx]);
        }
    }
    return result;
}

// ============================================================================
// Batch Processing
// ============================================================================

/**
 * @brief Process container in batches
 * @tparam Container Container type
 * @tparam Func Function type
 * @param container Input container
 * @param batchSize Size of each batch
 * @param func Function to call on each batch
 */
template<typename Container, typename Func>
void processBatches(const Container& container, std::size_t batchSize, Func func) {
    std::vector<typename Container::value_type> vec(container.begin(), container.end());
    
    for (std::size_t i = 0; i < vec.size(); i += batchSize) {
        std::size_t end = std::min(i + batchSize, vec.size());
        std::vector<typename Container::value_type> batch(vec.begin() + i, vec.begin() + end);
        func(batch);
    }
}

/**
 * @brief Split container into equal parts
 * @tparam Container Container type
 * @param container Input container
 * @param parts Number of parts
 * @return Vector of container parts
 */
template<typename Container>
[[nodiscard]] std::vector<std::vector<typename Container::value_type>> 
split(const Container& container, std::size_t parts) {
    std::vector<typename Container::value_type> vec(container.begin(), container.end());
    std::vector<std::vector<typename Container::value_type>> result;
    
    if (parts == 0) return result;
    
    std::size_t baseSize = vec.size() / parts;
    std::size_t remainder = vec.size() % parts;
    
    std::size_t start = 0;
    for (std::size_t i = 0; i < parts && start < vec.size(); ++i) {
        std::size_t size = baseSize + (i < remainder ? 1 : 0);
        result.emplace_back(vec.begin() + start, vec.begin() + start + size);
        start += size;
    }
    
    return result;
}

} // namespace algorithm
} // namespace dfsmshsm

#endif // HSM_ALGORITHM_HPP
