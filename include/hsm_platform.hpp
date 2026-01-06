/**
 * @file hsm_platform.hpp
 * @brief Platform detection, compiler features, and cross-platform compatibility utilities
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - Platform detection macros (Windows, Linux, macOS)
 * - Compiler detection macros (GCC, Clang, MSVC)
 * - C++ standard feature detection
 * - Cross-platform utility macros
 * - Performance hint macros
 * - Static assertion helpers
 */

#ifndef HSM_PLATFORM_HPP
#define HSM_PLATFORM_HPP

#include <cstdint>
#include <type_traits>

namespace dfsmshsm {
namespace platform {

// ============================================================================
// Platform Detection
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define HSM_PLATFORM_WINDOWS 1
    #define HSM_PLATFORM_NAME "Windows"
    #if defined(_WIN64)
        #define HSM_PLATFORM_64BIT 1
    #else
        #define HSM_PLATFORM_32BIT 1
    #endif
#elif defined(__APPLE__) && defined(__MACH__)
    #define HSM_PLATFORM_MACOS 1
    #define HSM_PLATFORM_POSIX 1
    #define HSM_PLATFORM_NAME "macOS"
    #define HSM_PLATFORM_64BIT 1
#elif defined(__linux__)
    #define HSM_PLATFORM_LINUX 1
    #define HSM_PLATFORM_POSIX 1
    #define HSM_PLATFORM_NAME "Linux"
    #if defined(__x86_64__) || defined(__aarch64__)
        #define HSM_PLATFORM_64BIT 1
    #else
        #define HSM_PLATFORM_32BIT 1
    #endif
#elif defined(__unix__)
    #define HSM_PLATFORM_UNIX 1
    #define HSM_PLATFORM_POSIX 1
    #define HSM_PLATFORM_NAME "Unix"
#else
    #define HSM_PLATFORM_UNKNOWN 1
    #define HSM_PLATFORM_NAME "Unknown"
#endif

// ============================================================================
// Compiler Detection
// ============================================================================

#if defined(_MSC_VER)
    #define HSM_COMPILER_MSVC 1
    #define HSM_COMPILER_NAME "MSVC"
    #define HSM_COMPILER_VERSION _MSC_VER
    #define HSM_MSVC_VERSION _MSC_VER
#elif defined(__clang__)
    #define HSM_COMPILER_CLANG 1
    #define HSM_COMPILER_NAME "Clang"
    #define HSM_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
    #define HSM_CLANG_VERSION HSM_COMPILER_VERSION
#elif defined(__GNUC__)
    #define HSM_COMPILER_GCC 1
    #define HSM_COMPILER_NAME "GCC"
    #define HSM_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
    #define HSM_GCC_VERSION HSM_COMPILER_VERSION
#else
    #define HSM_COMPILER_UNKNOWN 1
    #define HSM_COMPILER_NAME "Unknown"
    #define HSM_COMPILER_VERSION 0
#endif

// ============================================================================
// C++ Standard Detection
// ============================================================================

#if defined(_MSVC_LANG)
    #define HSM_CPP_VERSION _MSVC_LANG
#elif defined(__cplusplus)
    #define HSM_CPP_VERSION __cplusplus
#else
    #define HSM_CPP_VERSION 0
#endif

#define HSM_CPP11 (HSM_CPP_VERSION >= 201103L)
#define HSM_CPP14 (HSM_CPP_VERSION >= 201402L)
#define HSM_CPP17 (HSM_CPP_VERSION >= 201703L)
#define HSM_CPP20 (HSM_CPP_VERSION >= 202002L)
#define HSM_CPP23 (HSM_CPP_VERSION >= 202302L)

// Static assertion for C++20 requirement
static_assert(HSM_CPP20, "DFSMShsm requires C++20 or later");

// ============================================================================
// Compiler Feature Macros
// ============================================================================

// NODISCARD - warn if return value is ignored
#if HSM_CPP17
    #define HSM_NODISCARD [[nodiscard]]
    #define HSM_NODISCARD_MSG(msg) [[nodiscard(msg)]]
#else
    #define HSM_NODISCARD
    #define HSM_NODISCARD_MSG(msg)
#endif

// MAYBE_UNUSED - suppress unused variable/parameter warnings
#if HSM_CPP17
    #define HSM_MAYBE_UNUSED [[maybe_unused]]
#else
    #define HSM_MAYBE_UNUSED
#endif

// FALLTHROUGH - explicit fallthrough in switch statements
#if HSM_CPP17
    #define HSM_FALLTHROUGH [[fallthrough]]
#else
    #define HSM_FALLTHROUGH
#endif

// DEPRECATED - mark as deprecated
#if HSM_CPP14
    #define HSM_DEPRECATED [[deprecated]]
    #define HSM_DEPRECATED_MSG(msg) [[deprecated(msg)]]
#else
    #define HSM_DEPRECATED
    #define HSM_DEPRECATED_MSG(msg)
#endif

// LIKELY/UNLIKELY - branch prediction hints (C++20)
#if HSM_CPP20
    #define HSM_LIKELY [[likely]]
    #define HSM_UNLIKELY [[unlikely]]
#else
    #define HSM_LIKELY
    #define HSM_UNLIKELY
#endif

// NO_UNIQUE_ADDRESS - empty member optimization (C++20)
#if HSM_CPP20
    #define HSM_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
    #define HSM_NO_UNIQUE_ADDRESS
#endif

// ============================================================================
// Compiler-Specific Attributes
// ============================================================================

// Force inline
#if defined(HSM_COMPILER_MSVC)
    #define HSM_FORCE_INLINE __forceinline
#elif defined(HSM_COMPILER_GCC) || defined(HSM_COMPILER_CLANG)
    #define HSM_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define HSM_FORCE_INLINE inline
#endif

// No inline
#if defined(HSM_COMPILER_MSVC)
    #define HSM_NO_INLINE __declspec(noinline)
#elif defined(HSM_COMPILER_GCC) || defined(HSM_COMPILER_CLANG)
    #define HSM_NO_INLINE __attribute__((noinline))
#else
    #define HSM_NO_INLINE
#endif

// Restrict (pointer aliasing hint)
#if defined(HSM_COMPILER_MSVC)
    #define HSM_RESTRICT __restrict
#elif defined(HSM_COMPILER_GCC) || defined(HSM_COMPILER_CLANG)
    #define HSM_RESTRICT __restrict__
#else
    #define HSM_RESTRICT
#endif

// Aligned allocation
#if defined(HSM_COMPILER_MSVC)
    #define HSM_ALIGN(n) __declspec(align(n))
#elif defined(HSM_COMPILER_GCC) || defined(HSM_COMPILER_CLANG)
    #define HSM_ALIGN(n) __attribute__((aligned(n)))
#else
    #define HSM_ALIGN(n) alignas(n)
#endif

// ============================================================================
// Diagnostic Control
// ============================================================================

#if defined(HSM_COMPILER_GCC) || defined(HSM_COMPILER_CLANG)
    #define HSM_DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")
    #define HSM_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")
    #define HSM_DIAGNOSTIC_IGNORE_SIGN_COMPARE _Pragma("GCC diagnostic ignored \"-Wsign-compare\"")
    #define HSM_DIAGNOSTIC_IGNORE_UNUSED_RESULT _Pragma("GCC diagnostic ignored \"-Wunused-result\"")
    #define HSM_DIAGNOSTIC_IGNORE_DEPRECATED _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#elif defined(HSM_COMPILER_MSVC)
    #define HSM_DIAGNOSTIC_PUSH __pragma(warning(push))
    #define HSM_DIAGNOSTIC_POP __pragma(warning(pop))
    #define HSM_DIAGNOSTIC_IGNORE_SIGN_COMPARE __pragma(warning(disable: 4018 4389))
    #define HSM_DIAGNOSTIC_IGNORE_UNUSED_RESULT
    #define HSM_DIAGNOSTIC_IGNORE_DEPRECATED __pragma(warning(disable: 4996))
#else
    #define HSM_DIAGNOSTIC_PUSH
    #define HSM_DIAGNOSTIC_POP
    #define HSM_DIAGNOSTIC_IGNORE_SIGN_COMPARE
    #define HSM_DIAGNOSTIC_IGNORE_UNUSED_RESULT
    #define HSM_DIAGNOSTIC_IGNORE_DEPRECATED
#endif

// ============================================================================
// Debug/Release Detection
// ============================================================================

#if defined(NDEBUG) || defined(_NDEBUG)
    #define HSM_RELEASE 1
    #define HSM_DEBUG 0
#else
    #define HSM_RELEASE 0
    #define HSM_DEBUG 1
#endif

// Debug-only code macro
#if HSM_DEBUG
    #define HSM_DEBUG_ONLY(code) code
#else
    #define HSM_DEBUG_ONLY(code)
#endif

// ============================================================================
// Static Assertion Helpers
// ============================================================================

// Compile-time size check
#define HSM_STATIC_ASSERT_SIZE(type, expected_size) \
    static_assert(sizeof(type) == expected_size, \
        "Size of " #type " must be " #expected_size " bytes")

// Compile-time alignment check
#define HSM_STATIC_ASSERT_ALIGN(type, expected_align) \
    static_assert(alignof(type) == expected_align, \
        "Alignment of " #type " must be " #expected_align " bytes")

// Compile-time type trait checks
#define HSM_STATIC_ASSERT_TRIVIAL(type) \
    static_assert(std::is_trivial_v<type>, #type " must be trivial")

#define HSM_STATIC_ASSERT_STANDARD_LAYOUT(type) \
    static_assert(std::is_standard_layout_v<type>, #type " must be standard layout")

#define HSM_STATIC_ASSERT_POD(type) \
    static_assert(std::is_trivial_v<type> && std::is_standard_layout_v<type>, \
        #type " must be POD (trivial and standard layout)")

// ============================================================================
// Utility Macros
// ============================================================================

// Stringify
#define HSM_STRINGIFY_IMPL(x) #x
#define HSM_STRINGIFY(x) HSM_STRINGIFY_IMPL(x)

// Concatenate
#define HSM_CONCAT_IMPL(a, b) a##b
#define HSM_CONCAT(a, b) HSM_CONCAT_IMPL(a, b)

// Unique identifier generator
#define HSM_UNIQUE_ID HSM_CONCAT(_hsm_uid_, __LINE__)

// Array size (compile-time)
template<typename T, std::size_t N>
constexpr std::size_t arraySize(const T (&)[N]) noexcept {
    return N;
}

// ============================================================================
// Platform Information Functions
// ============================================================================

/**
 * @brief Get platform name string
 * @return Platform name (Windows, Linux, macOS, Unix, Unknown)
 */
constexpr const char* getPlatformName() noexcept {
    return HSM_PLATFORM_NAME;
}

/**
 * @brief Get compiler name string
 * @return Compiler name (MSVC, GCC, Clang, Unknown)
 */
constexpr const char* getCompilerName() noexcept {
    return HSM_COMPILER_NAME;
}

/**
 * @brief Get compiler version as integer
 * @return Compiler version (format varies by compiler)
 */
constexpr int getCompilerVersion() noexcept {
    return HSM_COMPILER_VERSION;
}

/**
 * @brief Check if running on Windows
 * @return true if Windows platform
 */
constexpr bool isWindows() noexcept {
#ifdef HSM_PLATFORM_WINDOWS
    return true;
#else
    return false;
#endif
}

/**
 * @brief Check if running on Linux
 * @return true if Linux platform
 */
constexpr bool isLinux() noexcept {
#ifdef HSM_PLATFORM_LINUX
    return true;
#else
    return false;
#endif
}

/**
 * @brief Check if running on macOS
 * @return true if macOS platform
 */
constexpr bool isMacOS() noexcept {
#ifdef HSM_PLATFORM_MACOS
    return true;
#else
    return false;
#endif
}

/**
 * @brief Check if running on POSIX-compliant system
 * @return true if POSIX platform (Linux, macOS, Unix)
 */
constexpr bool isPosix() noexcept {
#ifdef HSM_PLATFORM_POSIX
    return true;
#else
    return false;
#endif
}

/**
 * @brief Check if 64-bit platform
 * @return true if 64-bit
 */
constexpr bool is64Bit() noexcept {
#ifdef HSM_PLATFORM_64BIT
    return true;
#else
    return false;
#endif
}

/**
 * @brief Check if debug build
 * @return true if debug build
 */
constexpr bool isDebugBuild() noexcept {
    return HSM_DEBUG;
}

/**
 * @brief Get C++ standard version
 * @return C++ version (e.g., 202002 for C++20)
 */
constexpr long getCppVersion() noexcept {
    return HSM_CPP_VERSION;
}

// ============================================================================
// Endianness Detection (Runtime)
// ============================================================================

/**
 * @brief Check if system is little-endian
 * @return true if little-endian
 */
inline bool isLittleEndian() noexcept {
    const uint32_t test = 0x01020304;
    return *reinterpret_cast<const uint8_t*>(&test) == 0x04;
}

/**
 * @brief Check if system is big-endian
 * @return true if big-endian
 */
inline bool isBigEndian() noexcept {
    return !isLittleEndian();
}

// ============================================================================
// Byte Swap Utilities
// ============================================================================

/**
 * @brief Swap bytes in 16-bit value
 * @param value Input value
 * @return Byte-swapped value
 */
constexpr uint16_t byteSwap16(uint16_t value) noexcept {
    return static_cast<uint16_t>((value >> 8) | (value << 8));
}

/**
 * @brief Swap bytes in 32-bit value
 * @param value Input value
 * @return Byte-swapped value
 */
constexpr uint32_t byteSwap32(uint32_t value) noexcept {
    return ((value >> 24) & 0x000000FF) |
           ((value >> 8)  & 0x0000FF00) |
           ((value << 8)  & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
}

/**
 * @brief Swap bytes in 64-bit value
 * @param value Input value
 * @return Byte-swapped value
 */
constexpr uint64_t byteSwap64(uint64_t value) noexcept {
    return ((value >> 56) & 0x00000000000000FF) |
           ((value >> 40) & 0x000000000000FF00) |
           ((value >> 24) & 0x0000000000FF0000) |
           ((value >> 8)  & 0x00000000FF000000) |
           ((value << 8)  & 0x000000FF00000000) |
           ((value << 24) & 0x0000FF0000000000) |
           ((value << 40) & 0x00FF000000000000) |
           ((value << 56) & 0xFF00000000000000);
}

} // namespace platform
} // namespace dfsmshsm

#endif // HSM_PLATFORM_HPP
