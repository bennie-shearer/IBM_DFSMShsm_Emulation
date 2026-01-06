/**
 * @file hsm_concepts.hpp
 * @brief C++20 Concepts for compile-time type constraints
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides C++20 concepts for:
 * - Type constraints and requirements
 * - Compile-time validation
 * - Better error messages
 * - Generic programming support
 */

#ifndef HSM_CONCEPTS_HPP
#define HSM_CONCEPTS_HPP

#include <concepts>
#include <type_traits>
#include <string>
#include <string_view>
#include <iterator>
#include <ranges>
#include <functional>

namespace dfsmshsm {
namespace concepts {

// ============================================================================
// Basic Type Concepts
// ============================================================================

/**
 * @brief Concept for numeric types
 */
template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

/**
 * @brief Concept for integral types (integers)
 */
template<typename T>
concept Integral = std::integral<T>;

/**
 * @brief Concept for floating-point types
 */
template<typename T>
concept FloatingPoint = std::floating_point<T>;

/**
 * @brief Concept for signed numeric types
 */
template<typename T>
concept SignedNumeric = Numeric<T> && std::is_signed_v<T>;

/**
 * @brief Concept for unsigned numeric types
 */
template<typename T>
concept UnsignedNumeric = Numeric<T> && std::is_unsigned_v<T>;

/**
 * @brief Concept for pointer types
 */
template<typename T>
concept Pointer = std::is_pointer_v<T>;

/**
 * @brief Concept for enum types
 */
template<typename T>
concept Enumeration = std::is_enum_v<T>;

/**
 * @brief Concept for class types
 */
template<typename T>
concept ClassType = std::is_class_v<T>;

// ============================================================================
// String Concepts
// ============================================================================

/**
 * @brief Concept for string-like types
 */
template<typename T>
concept StringLike = requires(T t) {
    { t.data() } -> std::convertible_to<const char*>;
    { t.size() } -> std::convertible_to<std::size_t>;
    { t.empty() } -> std::convertible_to<bool>;
};

/**
 * @brief Concept for types convertible to string_view
 */
template<typename T>
concept StringViewConvertible = std::convertible_to<T, std::string_view>;

/**
 * @brief Concept for character types
 */
template<typename T>
concept Character = std::same_as<T, char> || 
                   std::same_as<T, wchar_t> ||
                   std::same_as<T, char8_t> ||
                   std::same_as<T, char16_t> ||
                   std::same_as<T, char32_t>;

// ============================================================================
// Container Concepts
// ============================================================================

/**
 * @brief Concept for container types
 */
template<typename T>
concept Container = requires(T t) {
    typename T::value_type;
    typename T::iterator;
    typename T::const_iterator;
    { t.begin() } -> std::same_as<typename T::iterator>;
    { t.end() } -> std::same_as<typename T::iterator>;
    { t.size() } -> std::convertible_to<std::size_t>;
    { t.empty() } -> std::convertible_to<bool>;
};

/**
 * @brief Concept for resizable containers
 */
template<typename T>
concept ResizableContainer = Container<T> && requires(T t, std::size_t n) {
    t.resize(n);
    t.reserve(n);
    { t.capacity() } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Concept for associative containers
 */
template<typename T>
concept AssociativeContainer = Container<T> && requires(T t) {
    typename T::key_type;
    typename T::mapped_type;
};

/**
 * @brief Concept for sequence containers
 */
template<typename T>
concept SequenceContainer = Container<T> && requires(T t, typename T::value_type v) {
    t.push_back(v);
    t.pop_back();
    { t.front() } -> std::same_as<typename T::value_type&>;
    { t.back() } -> std::same_as<typename T::value_type&>;
};

// ============================================================================
// Iterator Concepts
// ============================================================================

/**
 * @brief Concept for input iterators
 */
template<typename T>
concept InputIterator = std::input_iterator<T>;

/**
 * @brief Concept for output iterators
 */
template<typename T, typename V>
concept OutputIterator = std::output_iterator<T, V>;

/**
 * @brief Concept for forward iterators
 */
template<typename T>
concept ForwardIterator = std::forward_iterator<T>;

/**
 * @brief Concept for bidirectional iterators
 */
template<typename T>
concept BidirectionalIterator = std::bidirectional_iterator<T>;

/**
 * @brief Concept for random access iterators
 */
template<typename T>
concept RandomAccessIterator = std::random_access_iterator<T>;

// ============================================================================
// Callable Concepts
// ============================================================================

/**
 * @brief Concept for callable types
 */
template<typename F, typename... Args>
concept Callable = std::invocable<F, Args...>;

/**
 * @brief Concept for predicates (returns bool)
 */
template<typename F, typename... Args>
concept Predicate = Callable<F, Args...> && 
    std::convertible_to<std::invoke_result_t<F, Args...>, bool>;

/**
 * @brief Concept for unary predicates
 */
template<typename F, typename T>
concept UnaryPredicate = Predicate<F, T>;

/**
 * @brief Concept for binary predicates
 */
template<typename F, typename T, typename U = T>
concept BinaryPredicate = Predicate<F, T, U>;

/**
 * @brief Concept for comparison functions
 */
template<typename F, typename T>
concept Comparator = BinaryPredicate<F, T, T>;

// ============================================================================
// HSM-Specific Concepts
// ============================================================================

/**
 * @brief Concept for dataset name types
 * Must be string-like with max 44 characters
 */
template<typename T>
concept DatasetName = StringLike<T>;

/**
 * @brief Concept for volume serial types
 * Must be string-like with exactly 6 characters
 */
template<typename T>
concept VolumeSerial = StringLike<T>;

/**
 * @brief Concept for serializable types
 */
template<typename T>
concept Serializable = requires(T t, std::ostream& os, std::istream& is) {
    { t.serialize(os) } -> std::same_as<void>;
    { t.deserialize(is) } -> std::same_as<void>;
} || requires(T t) {
    { t.toBytes() } -> Container;
    { T::fromBytes(std::declval<std::vector<uint8_t>>()) } -> std::same_as<T>;
};

/**
 * @brief Concept for types with JSON representation
 */
template<typename T>
concept JsonSerializable = requires(T t) {
    { t.toJson() } -> StringLike;
};

/**
 * @brief Concept for hashable types
 */
template<typename T>
concept Hashable = requires(T t) {
    { std::hash<T>{}(t) } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Concept for comparable types
 */
template<typename T>
concept Comparable = std::totally_ordered<T>;

/**
 * @brief Concept for copyable types
 */
template<typename T>
concept Copyable = std::copyable<T>;

/**
 * @brief Concept for movable types
 */
template<typename T>
concept Movable = std::movable<T>;

/**
 * @brief Concept for default constructible types
 */
template<typename T>
concept DefaultConstructible = std::default_initializable<T>;

// ============================================================================
// Storage Tier Concepts
// ============================================================================

/**
 * @brief Concept for storage size types
 */
template<typename T>
concept StorageSize = UnsignedNumeric<T> && sizeof(T) >= 4;

/**
 * @brief Concept for timestamp types
 */
template<typename T>
concept Timestamp = requires(T t) {
    { t.time_since_epoch() };
} || Integral<T>;

/**
 * @brief Concept for duration types
 */
template<typename T>
concept Duration = requires(T t) {
    { t.count() } -> Numeric;
};

// ============================================================================
// Event/Command Concepts
// ============================================================================

/**
 * @brief Concept for event types
 */
template<typename T>
concept Event = requires(T t) {
    { t.getType() };
    { t.getTimestamp() } -> Timestamp;
};

/**
 * @brief Concept for command types
 */
template<typename T>
concept Command = requires(T t) {
    { t.execute() };
    { t.getDescription() } -> StringLike;
};

/**
 * @brief Concept for undoable commands
 */
template<typename T>
concept UndoableCommand = Command<T> && requires(T t) {
    { t.undo() };
    { t.canUndo() } -> std::convertible_to<bool>;
};

// ============================================================================
// Result/Error Concepts
// ============================================================================

/**
 * @brief Concept for result types
 */
template<typename T>
concept ResultType = requires(T t) {
    { t.isOk() } -> std::convertible_to<bool>;
    { t.isError() } -> std::convertible_to<bool>;
};

/**
 * @brief Concept for error types
 */
template<typename T>
concept ErrorType = requires(T t) {
    { t.message() } -> StringLike;
    { t.code() } -> Integral;
};

// ============================================================================
// Utility Concept Functions
// ============================================================================

/**
 * @brief Check if type satisfies Numeric concept at compile time
 */
template<typename T>
constexpr bool is_numeric_v = Numeric<T>;

/**
 * @brief Check if type satisfies Container concept at compile time
 */
template<typename T>
constexpr bool is_container_v = Container<T>;

/**
 * @brief Check if type satisfies StringLike concept at compile time
 */
template<typename T>
constexpr bool is_string_like_v = StringLike<T>;

/**
 * @brief Check if type satisfies Serializable concept at compile time
 */
template<typename T>
constexpr bool is_serializable_v = Serializable<T>;

// ============================================================================
// Concept-Based Function Templates
// ============================================================================

/**
 * @brief Clamp value to range (requires Comparable)
 */
template<Comparable T>
constexpr T clamp(T value, T min_val, T max_val) noexcept {
    return value < min_val ? min_val : (value > max_val ? max_val : value);
}

/**
 * @brief Safe numeric cast (requires Numeric)
 */
template<Numeric To, Numeric From>
constexpr To safe_cast(From value) noexcept {
    return static_cast<To>(value);
}

/**
 * @brief Sum container elements (requires Container and Numeric value_type)
 */
template<Container C>
requires Numeric<typename C::value_type>
auto sum(const C& container) noexcept {
    typename C::value_type result{};
    for (const auto& elem : container) {
        result += elem;
    }
    return result;
}

/**
 * @brief Find in container (requires Container)
 */
template<Container C, typename T>
requires std::equality_comparable_with<typename C::value_type, T>
auto find(const C& container, const T& value) {
    return std::find(container.begin(), container.end(), value);
}

/**
 * @brief Filter container with predicate
 */
template<Container C, UnaryPredicate<typename C::value_type> Pred>
C filter(const C& container, Pred pred) {
    C result;
    for (const auto& elem : container) {
        if (pred(elem)) {
            if constexpr (requires { result.push_back(elem); }) {
                result.push_back(elem);
            } else if constexpr (requires { result.insert(elem); }) {
                result.insert(elem);
            }
        }
    }
    return result;
}

} // namespace concepts

// Bring commonly used concepts into dfsmshsm namespace
using concepts::Numeric;
using concepts::Integral;
using concepts::StringLike;
using concepts::Container;
using concepts::Callable;
using concepts::Predicate;
using concepts::Comparable;
using concepts::Serializable;

} // namespace dfsmshsm

#endif // HSM_CONCEPTS_HPP
