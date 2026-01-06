/**
 * @file hsm_time_utils.hpp
 * @brief Time and date utilities for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - Time point manipulation
 * - Duration calculations
 * - Date/time formatting
 * - Timestamp utilities
 * - z/OS time format conversions
 */

#ifndef HSM_TIME_UTILS_HPP
#define HSM_TIME_UTILS_HPP

#include <chrono>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <optional>

namespace dfsmshsm {
namespace time_utils {

// Type aliases for convenience
using Clock = std::chrono::system_clock;
using TimePoint = std::chrono::system_clock::time_point;
using Duration = std::chrono::system_clock::duration;
using Seconds = std::chrono::seconds;
using Milliseconds = std::chrono::milliseconds;
using Microseconds = std::chrono::microseconds;
using Minutes = std::chrono::minutes;
using Hours = std::chrono::hours;
using Days = std::chrono::duration<int, std::ratio<86400>>;

// ============================================================================
// Current Time Functions
// ============================================================================

/**
 * @brief Get current time point
 * @return Current system time
 */
[[nodiscard]] inline TimePoint now() noexcept {
    return Clock::now();
}

/**
 * @brief Get current Unix timestamp (seconds since epoch)
 * @return Unix timestamp
 */
[[nodiscard]] inline int64_t unixTimestamp() noexcept {
    return std::chrono::duration_cast<Seconds>(now().time_since_epoch()).count();
}

/**
 * @brief Get current Unix timestamp in milliseconds
 * @return Unix timestamp in milliseconds
 */
[[nodiscard]] inline int64_t unixTimestampMs() noexcept {
    return std::chrono::duration_cast<Milliseconds>(now().time_since_epoch()).count();
}

/**
 * @brief Get current Unix timestamp in microseconds
 * @return Unix timestamp in microseconds
 */
[[nodiscard]] inline int64_t unixTimestampUs() noexcept {
    return std::chrono::duration_cast<Microseconds>(now().time_since_epoch()).count();
}

// ============================================================================
// Time Point Conversions
// ============================================================================

/**
 * @brief Convert Unix timestamp to time point
 * @param timestamp Unix timestamp (seconds)
 * @return Time point
 */
[[nodiscard]] inline TimePoint fromUnixTimestamp(int64_t timestamp) noexcept {
    return TimePoint(Seconds(timestamp));
}

/**
 * @brief Convert Unix timestamp (ms) to time point
 * @param timestampMs Unix timestamp in milliseconds
 * @return Time point
 */
[[nodiscard]] inline TimePoint fromUnixTimestampMs(int64_t timestampMs) noexcept {
    return TimePoint(Milliseconds(timestampMs));
}

/**
 * @brief Convert time_t to time point
 * @param t time_t value
 * @return Time point
 */
[[nodiscard]] inline TimePoint fromTimeT(std::time_t t) noexcept {
    return Clock::from_time_t(t);
}

/**
 * @brief Convert time point to time_t
 * @param tp Time point
 * @return time_t value
 */
[[nodiscard]] inline std::time_t toTimeT(TimePoint tp) noexcept {
    return Clock::to_time_t(tp);
}

// ============================================================================
// Formatting Functions
// ============================================================================

/**
 * @brief Format time point as ISO 8601 string
 * @param tp Time point (default: now)
 * @return ISO 8601 formatted string (YYYY-MM-DDTHH:MM:SSZ)
 */
[[nodiscard]] inline std::string toISO8601(TimePoint tp = now()) noexcept {
    auto t = toTimeT(tp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

/**
 * @brief Format time point as date string
 * @param tp Time point (default: now)
 * @return Date string (YYYY-MM-DD)
 */
[[nodiscard]] inline std::string toDateString(TimePoint tp = now()) noexcept {
    auto t = toTimeT(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

/**
 * @brief Format time point as time string
 * @param tp Time point (default: now)
 * @return Time string (HH:MM:SS)
 */
[[nodiscard]] inline std::string toTimeString(TimePoint tp = now()) noexcept {
    auto t = toTimeT(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

/**
 * @brief Format time point as datetime string
 * @param tp Time point (default: now)
 * @return Datetime string (YYYY-MM-DD HH:MM:SS)
 */
[[nodiscard]] inline std::string toDateTimeString(TimePoint tp = now()) noexcept {
    auto t = toTimeT(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/**
 * @brief Format time point with custom format
 * @param tp Time point
 * @param format strftime format string
 * @return Formatted string
 */
[[nodiscard]] inline std::string format(TimePoint tp, const char* format) noexcept {
    auto t = toTimeT(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, format);
    return oss.str();
}

// ============================================================================
// z/OS Time Formats
// ============================================================================

/**
 * @brief Format as z/OS timestamp (YYYYDDD.HHMMSS)
 * @param tp Time point (default: now)
 * @return z/OS formatted timestamp
 */
[[nodiscard]] inline std::string toZOSTimestamp(TimePoint tp = now()) noexcept {
    auto t = toTimeT(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%j.%H%M%S");
    return oss.str();
}

/**
 * @brief Format as z/OS Julian date (YYYYDDD)
 * @param tp Time point (default: now)
 * @return Julian date string
 */
[[nodiscard]] inline std::string toJulianDate(TimePoint tp = now()) noexcept {
    auto t = toTimeT(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%j");
    return oss.str();
}

/**
 * @brief Format as SMF timestamp (binary format representation)
 * @param tp Time point (default: now)
 * @return SMF timestamp as hex string
 */
[[nodiscard]] inline std::string toSMFTimestamp(TimePoint tp = now()) noexcept {
    auto ms = std::chrono::duration_cast<Milliseconds>(tp.time_since_epoch()).count();
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0') << std::setw(16) << ms;
    return oss.str();
}

// ============================================================================
// Duration Functions
// ============================================================================

/**
 * @brief Calculate duration between two time points
 * @param start Start time
 * @param end End time (default: now)
 * @return Duration in seconds
 */
[[nodiscard]] inline double durationSeconds(TimePoint start, TimePoint end = now()) noexcept {
    return std::chrono::duration<double>(end - start).count();
}

/**
 * @brief Calculate duration between two time points in milliseconds
 * @param start Start time
 * @param end End time (default: now)
 * @return Duration in milliseconds
 */
[[nodiscard]] inline int64_t durationMs(TimePoint start, TimePoint end = now()) noexcept {
    return std::chrono::duration_cast<Milliseconds>(end - start).count();
}

/**
 * @brief Calculate duration between two time points in microseconds
 * @param start Start time
 * @param end End time (default: now)
 * @return Duration in microseconds
 */
[[nodiscard]] inline int64_t durationUs(TimePoint start, TimePoint end = now()) noexcept {
    return std::chrono::duration_cast<Microseconds>(end - start).count();
}

/**
 * @brief Format duration as human-readable string
 * @param seconds Duration in seconds
 * @return Human-readable string (e.g., "2h 30m 15s")
 */
[[nodiscard]] inline std::string formatDuration(double seconds) noexcept {
    std::ostringstream oss;
    
    if (seconds < 0) {
        oss << "-";
        seconds = -seconds;
    }
    
    int64_t totalSeconds = static_cast<int64_t>(seconds);
    int64_t days = totalSeconds / 86400;
    int64_t hours = (totalSeconds % 86400) / 3600;
    int64_t mins = (totalSeconds % 3600) / 60;
    int64_t secs = totalSeconds % 60;
    
    if (days > 0) {
        oss << days << "d ";
    }
    if (hours > 0 || days > 0) {
        oss << hours << "h ";
    }
    if (mins > 0 || hours > 0 || days > 0) {
        oss << mins << "m ";
    }
    oss << secs << "s";
    
    return oss.str();
}

/**
 * @brief Format duration in milliseconds as human-readable
 * @param ms Duration in milliseconds
 * @return Human-readable string
 */
[[nodiscard]] inline std::string formatDurationMs(int64_t ms) noexcept {
    if (ms < 1000) {
        return std::to_string(ms) + "ms";
    }
    return formatDuration(static_cast<double>(ms) / 1000.0);
}

// ============================================================================
// Time Arithmetic
// ============================================================================

/**
 * @brief Add seconds to time point
 * @param tp Base time point
 * @param secs Seconds to add
 * @return New time point
 */
[[nodiscard]] inline TimePoint addSeconds(TimePoint tp, int64_t secs) noexcept {
    return tp + Seconds(secs);
}

/**
 * @brief Add minutes to time point
 * @param tp Base time point
 * @param mins Minutes to add
 * @return New time point
 */
[[nodiscard]] inline TimePoint addMinutes(TimePoint tp, int64_t mins) noexcept {
    return tp + Minutes(mins);
}

/**
 * @brief Add hours to time point
 * @param tp Base time point
 * @param hrs Hours to add
 * @return New time point
 */
[[nodiscard]] inline TimePoint addHours(TimePoint tp, int64_t hrs) noexcept {
    return tp + Hours(hrs);
}

/**
 * @brief Add days to time point
 * @param tp Base time point
 * @param days Days to add
 * @return New time point
 */
[[nodiscard]] inline TimePoint addDays(TimePoint tp, int64_t days) noexcept {
    return tp + Days(static_cast<int>(days));
}

// ============================================================================
// Comparison Functions
// ============================================================================

/**
 * @brief Check if time point is in the past
 * @param tp Time point to check
 * @return true if in the past
 */
[[nodiscard]] inline bool isPast(TimePoint tp) noexcept {
    return tp < now();
}

/**
 * @brief Check if time point is in the future
 * @param tp Time point to check
 * @return true if in the future
 */
[[nodiscard]] inline bool isFuture(TimePoint tp) noexcept {
    return tp > now();
}

/**
 * @brief Check if time point is within duration of now
 * @param tp Time point to check
 * @param seconds Duration in seconds
 * @return true if within duration
 */
[[nodiscard]] inline bool isWithin(TimePoint tp, int64_t seconds) noexcept {
    auto diff = std::chrono::duration_cast<Seconds>(now() - tp).count();
    return std::abs(diff) <= seconds;
}

/**
 * @brief Get age of time point in seconds
 * @param tp Time point
 * @return Age in seconds (positive if past, negative if future)
 */
[[nodiscard]] inline int64_t ageSeconds(TimePoint tp) noexcept {
    return std::chrono::duration_cast<Seconds>(now() - tp).count();
}

// ============================================================================
// Date Components
// ============================================================================

/**
 * @brief Date components structure
 */
struct DateComponents {
    int year;
    int month;    // 1-12
    int day;      // 1-31
    int hour;     // 0-23
    int minute;   // 0-59
    int second;   // 0-59
    int dayOfWeek; // 0-6 (Sunday = 0)
    int dayOfYear; // 1-366
};

/**
 * @brief Get date components from time point
 * @param tp Time point (default: now)
 * @return DateComponents structure
 */
[[nodiscard]] inline DateComponents getComponents(TimePoint tp = now()) noexcept {
    auto t = toTimeT(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    
    return DateComponents{
        tm.tm_year + 1900,
        tm.tm_mon + 1,
        tm.tm_mday,
        tm.tm_hour,
        tm.tm_min,
        tm.tm_sec,
        tm.tm_wday,
        tm.tm_yday + 1
    };
}

/**
 * @brief Create time point from date components
 * @param year Year
 * @param month Month (1-12)
 * @param day Day (1-31)
 * @param hour Hour (0-23, default: 0)
 * @param minute Minute (0-59, default: 0)
 * @param second Second (0-59, default: 0)
 * @return Time point
 */
[[nodiscard]] inline TimePoint fromComponents(int year, int month, int day,
                                              int hour = 0, int minute = 0, 
                                              int second = 0) noexcept {
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    tm.tm_isdst = -1;
    
    return fromTimeT(std::mktime(&tm));
}

// ============================================================================
// Timer Utility Class
// ============================================================================

/**
 * @brief Simple stopwatch timer
 */
class Timer {
public:
    /**
     * @brief Start the timer
     */
    void start() noexcept {
        start_ = now();
        running_ = true;
    }
    
    /**
     * @brief Stop the timer
     */
    void stop() noexcept {
        if (running_) {
            end_ = now();
            running_ = false;
        }
    }
    
    /**
     * @brief Reset the timer
     */
    void reset() noexcept {
        start_ = TimePoint{};
        end_ = TimePoint{};
        running_ = false;
    }
    
    /**
     * @brief Get elapsed time in seconds
     * @return Elapsed seconds
     */
    [[nodiscard]] double elapsedSeconds() const noexcept {
        auto endTime = running_ ? now() : end_;
        return durationSeconds(start_, endTime);
    }
    
    /**
     * @brief Get elapsed time in milliseconds
     * @return Elapsed milliseconds
     */
    [[nodiscard]] int64_t elapsedMs() const noexcept {
        auto endTime = running_ ? now() : end_;
        return durationMs(start_, endTime);
    }
    
    /**
     * @brief Get elapsed time in microseconds
     * @return Elapsed microseconds
     */
    [[nodiscard]] int64_t elapsedUs() const noexcept {
        auto endTime = running_ ? now() : end_;
        return durationUs(start_, endTime);
    }
    
    /**
     * @brief Check if timer is running
     * @return true if running
     */
    [[nodiscard]] bool isRunning() const noexcept {
        return running_;
    }
    
    /**
     * @brief Get formatted elapsed time
     * @return Human-readable elapsed time
     */
    [[nodiscard]] std::string toString() const noexcept {
        return formatDuration(elapsedSeconds());
    }

private:
    TimePoint start_;
    TimePoint end_;
    bool running_ = false;
};

/**
 * @brief RAII-based scoped timer for automatic timing
 */
class ScopedTimer {
public:
    /**
     * @brief Constructor - starts timing
     * @param name Timer name for output
     * @param output Output stream (default: none)
     */
    explicit ScopedTimer(std::string name = "Timer", std::ostream* output = nullptr) noexcept
        : name_(std::move(name)), output_(output) {
        timer_.start();
    }
    
    /**
     * @brief Destructor - stops timing and optionally outputs result
     */
    ~ScopedTimer() {
        timer_.stop();
        if (output_) {
            *output_ << name_ << ": " << timer_.toString() << "\n";
        }
    }
    
    /**
     * @brief Get elapsed time in milliseconds
     * @return Elapsed milliseconds
     */
    [[nodiscard]] int64_t elapsedMs() const noexcept {
        return timer_.elapsedMs();
    }
    
    /**
     * @brief Get elapsed time in seconds
     * @return Elapsed seconds
     */
    [[nodiscard]] double elapsedSeconds() const noexcept {
        return timer_.elapsedSeconds();
    }

private:
    std::string name_;
    std::ostream* output_;
    Timer timer_;
};

} // namespace time_utils
} // namespace dfsmshsm

#endif // HSM_TIME_UTILS_HPP
