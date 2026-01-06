/**
 * @file hsm_source_location.hpp
 * @brief Source location utilities for debugging and logging
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - Source location capture
 * - Call stack information
 * - Debug assertion macros
 * - Function signature utilities
 */

#ifndef HSM_SOURCE_LOCATION_HPP
#define HSM_SOURCE_LOCATION_HPP

#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <cstdint>

// Check for C++20 source_location support
#if __has_include(<source_location>)
    #include <source_location>
    #define HSM_HAS_SOURCE_LOCATION 1
#else
    #define HSM_HAS_SOURCE_LOCATION 0
#endif

namespace dfsmshsm {
namespace debug {

// ============================================================================
// Source Location
// ============================================================================

#if HSM_HAS_SOURCE_LOCATION

using SourceLocation = std::source_location;

/**
 * @brief Get current source location
 * @return Source location at call site
 */
[[nodiscard]] inline SourceLocation here(
    SourceLocation loc = SourceLocation::current()) noexcept {
    return loc;
}

#else

/**
 * @brief Fallback source location for pre-C++20 compilers
 */
class SourceLocation {
public:
    constexpr SourceLocation() noexcept = default;
    
    constexpr SourceLocation(const char* file, const char* func, 
                            uint32_t line, uint32_t col = 0) noexcept
        : file_(file), func_(func), line_(line), col_(col) {}
    
    [[nodiscard]] constexpr const char* file_name() const noexcept { return file_; }
    [[nodiscard]] constexpr const char* function_name() const noexcept { return func_; }
    [[nodiscard]] constexpr uint32_t line() const noexcept { return line_; }
    [[nodiscard]] constexpr uint32_t column() const noexcept { return col_; }
    
private:
    const char* file_ = "";
    const char* func_ = "";
    uint32_t line_ = 0;
    uint32_t col_ = 0;
};

// Macro to create source location
#define HSM_SOURCE_LOCATION() \
    dfsmshsm::debug::SourceLocation(__FILE__, __func__, __LINE__)

/**
 * @brief Get current source location (fallback)
 */
[[nodiscard]] inline SourceLocation here() noexcept {
    return SourceLocation();
}

#endif

// ============================================================================
// Location Formatting
// ============================================================================

/**
 * @brief Format source location as string
 * @param loc Source location
 * @return Formatted string "file:line"
 */
[[nodiscard]] inline std::string formatLocation(const SourceLocation& loc) {
    std::ostringstream oss;
    oss << loc.file_name() << ":" << loc.line();
    return oss.str();
}

/**
 * @brief Format source location with function name
 * @param loc Source location
 * @return Formatted string "file:line in function"
 */
[[nodiscard]] inline std::string formatLocationFull(const SourceLocation& loc) {
    std::ostringstream oss;
    oss << loc.file_name() << ":" << loc.line() 
        << " in " << loc.function_name();
    return oss.str();
}

/**
 * @brief Extract just the filename from full path
 * @param path Full file path
 * @return Just the filename
 */
[[nodiscard]] inline std::string_view extractFilename(std::string_view path) noexcept {
    auto pos = path.find_last_of("/\\");
    return pos == std::string_view::npos ? path : path.substr(pos + 1);
}

/**
 * @brief Format location with short filename
 * @param loc Source location
 * @return Formatted string "filename:line"
 */
[[nodiscard]] inline std::string formatLocationShort(const SourceLocation& loc) {
    std::ostringstream oss;
    oss << extractFilename(loc.file_name()) << ":" << loc.line();
    return oss.str();
}

// ============================================================================
// Call Frame Information
// ============================================================================

/**
 * @brief Represents a single call frame
 */
struct CallFrame {
    std::string file;
    std::string function;
    uint32_t line = 0;
    uint32_t column = 0;
    
    /**
     * @brief Create from source location
     */
    static CallFrame fromLocation(const SourceLocation& loc) {
        return CallFrame{
            loc.file_name(),
            loc.function_name(),
            loc.line(),
            loc.column()
        };
    }
    
    /**
     * @brief Format as string
     */
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << extractFilename(file) << ":" << line << " " << function;
        return oss.str();
    }
};

/**
 * @brief Simple manual call stack (for explicit tracking)
 */
class CallStack {
public:
    /**
     * @brief Push a frame onto the stack
     */
    void push(const SourceLocation& loc) {
        frames_.push_back(CallFrame::fromLocation(loc));
    }
    
    /**
     * @brief Push a frame with custom info
     */
    void push(std::string_view file, std::string_view func, uint32_t line) {
        frames_.push_back(CallFrame{std::string(file), std::string(func), line, 0});
    }
    
    /**
     * @brief Pop the top frame
     */
    void pop() {
        if (!frames_.empty()) {
            frames_.pop_back();
        }
    }
    
    /**
     * @brief Get number of frames
     */
    [[nodiscard]] std::size_t depth() const noexcept {
        return frames_.size();
    }
    
    /**
     * @brief Check if stack is empty
     */
    [[nodiscard]] bool empty() const noexcept {
        return frames_.empty();
    }
    
    /**
     * @brief Clear all frames
     */
    void clear() noexcept {
        frames_.clear();
    }
    
    /**
     * @brief Get all frames
     */
    [[nodiscard]] const std::vector<CallFrame>& frames() const noexcept {
        return frames_;
    }
    
    /**
     * @brief Get top frame
     */
    [[nodiscard]] const CallFrame* top() const noexcept {
        return frames_.empty() ? nullptr : &frames_.back();
    }
    
    /**
     * @brief Format as string
     */
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        for (std::size_t i = 0; i < frames_.size(); ++i) {
            oss << "#" << i << " " << frames_[i].toString() << "\n";
        }
        return oss.str();
    }

private:
    std::vector<CallFrame> frames_;
};

// ============================================================================
// RAII Scope Guard for Call Stack
// ============================================================================

/**
 * @brief RAII guard for call stack tracking
 */
class ScopeGuard {
public:
    /**
     * @brief Push frame on construction
     */
#if HSM_HAS_SOURCE_LOCATION
    explicit ScopeGuard(CallStack& stack, 
                       SourceLocation loc = SourceLocation::current())
        : stack_(stack) {
        stack_.push(loc);
    }
#else
    explicit ScopeGuard(CallStack& stack, const char* file, const char* func, 
                       uint32_t line)
        : stack_(stack) {
        stack_.push(file, func, line);
    }
#endif
    
    /**
     * @brief Pop frame on destruction
     */
    ~ScopeGuard() {
        stack_.pop();
    }
    
    // Non-copyable, non-movable
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard&&) = delete;
    ScopeGuard& operator=(ScopeGuard&&) = delete;

private:
    CallStack& stack_;
};

// ============================================================================
// Debug Context
// ============================================================================

/**
 * @brief Debug context with additional information
 */
struct DebugContext {
    SourceLocation location;
    std::string message;
    std::string category;
    
    /**
     * @brief Create debug context
     */
#if HSM_HAS_SOURCE_LOCATION
    static DebugContext create(std::string_view msg = "", 
                               std::string_view cat = "",
                               SourceLocation loc = SourceLocation::current()) {
        return DebugContext{loc, std::string(msg), std::string(cat)};
    }
#else
    static DebugContext create(std::string_view msg, std::string_view cat,
                               const char* file, const char* func, uint32_t line) {
        return DebugContext{
            SourceLocation(file, func, line),
            std::string(msg),
            std::string(cat)
        };
    }
#endif
    
    /**
     * @brief Format as string
     */
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << formatLocationShort(location);
        if (!category.empty()) {
            oss << " [" << category << "]";
        }
        if (!message.empty()) {
            oss << ": " << message;
        }
        return oss.str();
    }
};

// ============================================================================
// Function Signature Utilities
// ============================================================================

/**
 * @brief Extract function name from decorated signature
 * @param signature Full function signature (e.g., from __PRETTY_FUNCTION__)
 * @return Simplified function name
 */
[[nodiscard]] inline std::string simplifyFunctionName(std::string_view signature) {
    // Find function name by looking for '('
    auto parenPos = signature.find('(');
    if (parenPos == std::string_view::npos) {
        return std::string(signature);
    }
    
    // Find the start of the function name
    auto nameStart = signature.rfind(' ', parenPos);
    if (nameStart == std::string_view::npos) {
        nameStart = 0;
    } else {
        ++nameStart;
    }
    
    return std::string(signature.substr(nameStart, parenPos - nameStart));
}

/**
 * @brief Extract class name from method signature
 * @param signature Full function signature
 * @return Class name or empty string
 */
[[nodiscard]] inline std::string extractClassName(std::string_view signature) {
    auto colonPos = signature.find("::");
    if (colonPos == std::string_view::npos) {
        return "";
    }
    
    auto nameStart = signature.rfind(' ', colonPos);
    if (nameStart == std::string_view::npos) {
        nameStart = 0;
    } else {
        ++nameStart;
    }
    
    return std::string(signature.substr(nameStart, colonPos - nameStart));
}

// ============================================================================
// Debug Macros
// ============================================================================

#if HSM_HAS_SOURCE_LOCATION

// Use C++20 source_location
#define HSM_HERE() std::source_location::current()
#define HSM_LOCATION_STRING() dfsmshsm::debug::formatLocation(std::source_location::current())
#define HSM_SCOPE_GUARD(stack) dfsmshsm::debug::ScopeGuard _hsm_guard_(stack)
#define HSM_DEBUG_CONTEXT(msg, cat) dfsmshsm::debug::DebugContext::create(msg, cat)

#else

// Fallback macros
#define HSM_HERE() HSM_SOURCE_LOCATION()
#define HSM_LOCATION_STRING() (std::string(__FILE__) + ":" + std::to_string(__LINE__))
#define HSM_SCOPE_GUARD(stack) dfsmshsm::debug::ScopeGuard _hsm_guard_(stack, __FILE__, __func__, __LINE__)
#define HSM_DEBUG_CONTEXT(msg, cat) dfsmshsm::debug::DebugContext::create(msg, cat, __FILE__, __func__, __LINE__)

#endif

// Common debug macros (always available)
#define HSM_FUNCTION_NAME __func__

#ifdef __GNUC__
#define HSM_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define HSM_PRETTY_FUNCTION __FUNCSIG__
#else
#define HSM_PRETTY_FUNCTION __func__
#endif

// ============================================================================
// Debug Assertions
// ============================================================================

/**
 * @brief Assertion result
 */
struct AssertionResult {
    bool passed;
    std::string expression;
    std::string message;
    SourceLocation location;
    
    [[nodiscard]] explicit operator bool() const noexcept { return passed; }
};

/**
 * @brief Create assertion result
 */
#if HSM_HAS_SOURCE_LOCATION
inline AssertionResult makeAssertion(bool condition, std::string_view expr,
                                     std::string_view msg = "",
                                     SourceLocation loc = SourceLocation::current()) {
    return AssertionResult{condition, std::string(expr), std::string(msg), loc};
}
#else
inline AssertionResult makeAssertion(bool condition, std::string_view expr,
                                     std::string_view msg,
                                     const char* file, const char* func, uint32_t line) {
    return AssertionResult{
        condition, 
        std::string(expr), 
        std::string(msg),
        SourceLocation(file, func, line)
    };
}
#endif

/**
 * @brief Format assertion failure
 */
[[nodiscard]] inline std::string formatAssertionFailure(const AssertionResult& result) {
    std::ostringstream oss;
    oss << "Assertion failed: " << result.expression << "\n"
        << "  at " << formatLocationFull(result.location);
    if (!result.message.empty()) {
        oss << "\n  message: " << result.message;
    }
    return oss.str();
}

// Assertion macros
#if HSM_HAS_SOURCE_LOCATION
#define HSM_ASSERT(cond) dfsmshsm::debug::makeAssertion((cond), #cond)
#define HSM_ASSERT_MSG(cond, msg) dfsmshsm::debug::makeAssertion((cond), #cond, msg)
#else
#define HSM_ASSERT(cond) dfsmshsm::debug::makeAssertion((cond), #cond, "", __FILE__, __func__, __LINE__)
#define HSM_ASSERT_MSG(cond, msg) dfsmshsm::debug::makeAssertion((cond), #cond, msg, __FILE__, __func__, __LINE__)
#endif

// ============================================================================
// Trace Logging Helper
// ============================================================================

/**
 * @brief Trace entry for logging function calls
 */
class TraceEntry {
public:
#if HSM_HAS_SOURCE_LOCATION
    explicit TraceEntry(std::string_view name = "",
                       SourceLocation loc = SourceLocation::current())
        : name_(name.empty() ? loc.function_name() : std::string(name))
        , location_(loc)
        , entryTime_(std::chrono::steady_clock::now()) {}
#else
    explicit TraceEntry(std::string_view name, const char* file, 
                       const char* func, uint32_t line)
        : name_(name.empty() ? func : std::string(name))
        , location_(file, func, line)
        , entryTime_(std::chrono::steady_clock::now()) {}
#endif
    
    /**
     * @brief Get entry name
     */
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    
    /**
     * @brief Get source location
     */
    [[nodiscard]] const SourceLocation& location() const noexcept { return location_; }
    
    /**
     * @brief Get elapsed time in microseconds
     */
    [[nodiscard]] int64_t elapsedMicros() const noexcept {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now - entryTime_).count();
    }
    
    /**
     * @brief Format entry information
     */
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << name_ << " at " << formatLocationShort(location_);
        return oss.str();
    }

private:
    std::string name_;
    SourceLocation location_;
    std::chrono::steady_clock::time_point entryTime_;
};

#if HSM_HAS_SOURCE_LOCATION
#define HSM_TRACE() dfsmshsm::debug::TraceEntry _hsm_trace_("")
#define HSM_TRACE_NAMED(name) dfsmshsm::debug::TraceEntry _hsm_trace_(name)
#else
#define HSM_TRACE() dfsmshsm::debug::TraceEntry _hsm_trace_("", __FILE__, __func__, __LINE__)
#define HSM_TRACE_NAMED(name) dfsmshsm::debug::TraceEntry _hsm_trace_(name, __FILE__, __func__, __LINE__)
#endif

} // namespace debug
} // namespace dfsmshsm

#endif // HSM_SOURCE_LOCATION_HPP
