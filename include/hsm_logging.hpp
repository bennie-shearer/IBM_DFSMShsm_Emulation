/*******************************************************************************
 * DFSMShsm Emulator - Enhanced Logging and Tracing System
 * @version 4.2.0
 * 
 * Provides structured logging with configurable levels, log rotation,
 * trace analysis, and performance tracking capabilities.
 * 
 * Features:
 * - Multiple log levels (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
 * - Structured JSON logging output
 * - Automatic log rotation by size and time
 * - Trace correlation with request IDs
 * - Performance timing integration
 * - Thread-safe logging operations
 * - Multiple output targets (file, console, callback)
 * 
 * Author: DFSMShsm Emulator Team
 * License: MIT
 ******************************************************************************/

#ifndef HSM_LOGGING_HPP
#define HSM_LOGGING_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <functional>
#include <thread>
#include <queue>
#include <condition_variable>
#include <filesystem>
#include <ctime>
#include <random>
#include <iostream>

namespace dfsmshsm {
namespace logging {

// ============================================================================
// Log Level Definitions
// ============================================================================

enum class LogLevel {
    TRACE = 0,    // Detailed trace information
    DEBUG = 1,    // Debug information
    INFO = 2,     // General information
    WARN = 3,     // Warning conditions
    ERROR = 4,    // Error conditions
    FATAL = 5,    // Fatal errors
    OFF = 6       // Logging disabled
};

inline std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        case LogLevel::OFF:   return "OFF";
        default: return "UNKNOWN";
    }
}

inline LogLevel stringToLogLevel(const std::string& str) {
    if (str == "TRACE") return LogLevel::TRACE;
    if (str == "DEBUG") return LogLevel::DEBUG;
    if (str == "INFO")  return LogLevel::INFO;
    if (str == "WARN")  return LogLevel::WARN;
    if (str == "ERROR") return LogLevel::ERROR;
    if (str == "FATAL") return LogLevel::FATAL;
    if (str == "OFF")   return LogLevel::OFF;
    return LogLevel::INFO;
}

// ============================================================================
// Log Entry Structure
// ============================================================================

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string logger_name;
    std::string message;
    std::string trace_id;
    std::string span_id;
    std::thread::id thread_id;
    std::string source_file;
    int source_line;
    std::string function_name;
    std::map<std::string, std::string> context;
    uint64_t sequence_number;
    
    LogEntry()
        : timestamp(std::chrono::system_clock::now())
        , level(LogLevel::INFO)
        , source_line(0)
        , sequence_number(0) {}
    
    // Format as plain text
    std::string formatPlainText() const {
        std::ostringstream oss;
        auto time_t_ts = std::chrono::system_clock::to_time_t(timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()) % 1000;
        
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t_ts);
#else
        localtime_r(&time_t_ts, &tm_buf);
#endif
        
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << " [" << logLevelToString(level) << "]"
            << " [" << logger_name << "]";
        
        if (!trace_id.empty()) {
            oss << " [trace:" << trace_id << "]";
        }
        
        oss << " " << message;
        
        if (!source_file.empty()) {
            oss << " (" << source_file << ":" << source_line << ")";
        }
        
        return oss.str();
    }
    
    // Format as JSON
    std::string formatJSON() const {
        std::ostringstream oss;
        auto time_t_ts = std::chrono::system_clock::to_time_t(timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()) % 1000;
        
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t_ts);
#else
        localtime_r(&time_t_ts, &tm_buf);
#endif
        
        oss << "{";
        oss << "\"timestamp\":\"";
        oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count() << "Z\"";
        oss << ",\"level\":\"" << logLevelToString(level) << "\"";
        oss << ",\"logger\":\"" << escapeJSON(logger_name) << "\"";
        oss << ",\"message\":\"" << escapeJSON(message) << "\"";
        oss << ",\"sequence\":" << sequence_number;
        
        if (!trace_id.empty()) {
            oss << ",\"trace_id\":\"" << trace_id << "\"";
        }
        if (!span_id.empty()) {
            oss << ",\"span_id\":\"" << span_id << "\"";
        }
        if (!source_file.empty()) {
            oss << ",\"source\":{\"file\":\"" << escapeJSON(source_file) 
                << "\",\"line\":" << source_line;
            if (!function_name.empty()) {
                oss << ",\"function\":\"" << escapeJSON(function_name) << "\"";
            }
            oss << "}";
        }
        if (!context.empty()) {
            oss << ",\"context\":{";
            bool first = true;
            for (const auto& [key, value] : context) {
                if (!first) oss << ",";
                oss << "\"" << escapeJSON(key) << "\":\"" << escapeJSON(value) << "\"";
                first = false;
            }
            oss << "}";
        }
        oss << "}";
        
        return oss.str();
    }
    
private:
    static std::string escapeJSON(const std::string& str) {
        std::ostringstream oss;
        for (char c : str) {
            switch (c) {
                case '"':  oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\b': oss << "\\b"; break;
                case '\f': oss << "\\f"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 32) {
                        oss << "\\u" << std::hex << std::setfill('0') 
                            << std::setw(4) << static_cast<int>(c);
                    } else {
                        oss << c;
                    }
            }
        }
        return oss.str();
    }
};

// ============================================================================
// Log Sink Interface
// ============================================================================

class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const LogEntry& entry) = 0;
    virtual void flush() = 0;
    virtual void close() = 0;
    
    void setMinLevel(LogLevel level) { min_level_ = level; }
    LogLevel getMinLevel() const { return min_level_; }
    
    void setUseJSON(bool use_json) { use_json_ = use_json; }
    bool getUseJSON() const { return use_json_; }
    
protected:
    LogLevel min_level_ = LogLevel::TRACE;
    bool use_json_ = false;
};

// ============================================================================
// Console Log Sink
// ============================================================================

class ConsoleSink : public LogSink {
public:
    ConsoleSink(bool use_stderr_for_errors = true)
        : use_stderr_for_errors_(use_stderr_for_errors) {}
    
    void write(const LogEntry& entry) override {
        if (entry.level < min_level_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        std::string formatted = use_json_ ? entry.formatJSON() : entry.formatPlainText();
        
        if (use_stderr_for_errors_ && entry.level >= LogLevel::ERROR) {
            std::cerr << formatted << std::endl;
        } else {
            std::cout << formatted << std::endl;
        }
    }
    
    void flush() override {
        std::cout.flush();
        std::cerr.flush();
    }
    
    void close() override {
        flush();
    }
    
private:
    bool use_stderr_for_errors_;
    std::mutex mutex_;
};

// ============================================================================
// File Log Sink with Rotation
// ============================================================================

struct RotationConfig {
    size_t max_file_size = 10 * 1024 * 1024;  // 10 MB
    int max_files = 5;
    bool rotate_on_open = false;
    bool compress_rotated = false;
    
    // Time-based rotation
    bool rotate_daily = false;
    int rotate_hour = 0;  // Hour to rotate (0-23)
};

class FileSink : public LogSink {
public:
    FileSink(const std::string& base_path, const RotationConfig& config = RotationConfig())
        : base_path_(base_path)
        , config_(config)
        , current_size_(0)
        , last_rotation_day_(0) {
        
        openFile();
        
        if (config_.rotate_on_open && std::filesystem::exists(base_path_)) {
            rotate();
        }
    }
    
    ~FileSink() override {
        close();
    }
    
    void write(const LogEntry& entry) override {
        if (entry.level < min_level_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check for time-based rotation
        if (config_.rotate_daily) {
            checkTimeRotation();
        }
        
        std::string formatted = use_json_ ? entry.formatJSON() : entry.formatPlainText();
        formatted += "\n";
        
        // Check for size-based rotation
        if (current_size_ + formatted.size() > config_.max_file_size) {
            rotate();
        }
        
        if (file_.is_open()) {
            file_ << formatted;
            current_size_ += formatted.size();
        }
    }
    
    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.flush();
        }
    }
    
    void close() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    void rotate() {
        if (file_.is_open()) {
            file_.close();
        }
        
        // Rotate existing files
        for (int i = config_.max_files - 1; i >= 1; --i) {
            std::string old_path = base_path_ + "." + std::to_string(i);
            std::string new_path = base_path_ + "." + std::to_string(i + 1);
            
            if (std::filesystem::exists(old_path)) {
                if (i == config_.max_files - 1) {
                    std::filesystem::remove(old_path);
                } else {
                    std::filesystem::rename(old_path, new_path);
                }
            }
        }
        
        // Rename current file to .1
        if (std::filesystem::exists(base_path_)) {
            std::filesystem::rename(base_path_, base_path_ + ".1");
        }
        
        openFile();
    }
    
    size_t getCurrentSize() const { return current_size_; }
    std::string getBasePath() const { return base_path_; }
    
private:
    void openFile() {
        file_.open(base_path_, std::ios::app);
        if (file_.is_open() && std::filesystem::exists(base_path_)) {
            current_size_ = std::filesystem::file_size(base_path_);
        } else {
            current_size_ = 0;
        }
        
        // Record current day for time-based rotation
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_buf);
#endif
        last_rotation_day_ = tm_buf.tm_yday;
    }
    
    void checkTimeRotation() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_buf);
#endif
        
        if (tm_buf.tm_yday != last_rotation_day_ && 
            tm_buf.tm_hour >= config_.rotate_hour) {
            rotate();
        }
    }
    
    std::string base_path_;
    RotationConfig config_;
    std::ofstream file_;
    size_t current_size_;
    int last_rotation_day_;
    std::mutex mutex_;
};

// ============================================================================
// Callback Log Sink
// ============================================================================

using LogCallback = std::function<void(const LogEntry&)>;

class CallbackSink : public LogSink {
public:
    explicit CallbackSink(LogCallback callback)
        : callback_(std::move(callback)) {}
    
    void write(const LogEntry& entry) override {
        if (entry.level < min_level_) return;
        if (callback_) {
            callback_(entry);
        }
    }
    
    void flush() override {}
    void close() override {}
    
private:
    LogCallback callback_;
};

// ============================================================================
// Async Log Sink Wrapper
// ============================================================================

class AsyncSink : public LogSink {
public:
    explicit AsyncSink(std::shared_ptr<LogSink> inner_sink, size_t queue_size = 8192)
        : inner_sink_(std::move(inner_sink))
        , max_queue_size_(queue_size)
        , running_(true) {
        worker_thread_ = std::thread(&AsyncSink::workerLoop, this);
    }
    
    ~AsyncSink() override {
        close();
    }
    
    void write(const LogEntry& entry) override {
        if (entry.level < min_level_) return;
        
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.size() < max_queue_size_) {
            queue_.push(entry);
            cv_.notify_one();
        }
        // Drop entries if queue is full
    }
    
    void flush() override {
        // Wait for queue to drain
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return queue_.empty(); });
        inner_sink_->flush();
    }
    
    void close() override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
            cv_.notify_all();
        }
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        inner_sink_->close();
    }
    
private:
    void workerLoop() {
        while (true) {
            LogEntry entry;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return !queue_.empty() || !running_; });
                
                if (!running_ && queue_.empty()) {
                    break;
                }
                
                if (!queue_.empty()) {
                    entry = std::move(queue_.front());
                    queue_.pop();
                } else {
                    continue;
                }
            }
            inner_sink_->write(entry);
        }
    }
    
    std::shared_ptr<LogSink> inner_sink_;
    size_t max_queue_size_;
    std::queue<LogEntry> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_;
    std::thread worker_thread_;
};

// ============================================================================
// Trace Context for Distributed Tracing
// ============================================================================

class TraceContext {
public:
    static std::string generateTraceId() {
        return generateId(16);
    }
    
    static std::string generateSpanId() {
        return generateId(8);
    }
    
    // Thread-local trace context
    static void setCurrentTraceId(const std::string& trace_id) {
        current_trace_id() = trace_id;
    }
    
    static void setCurrentSpanId(const std::string& span_id) {
        current_span_id() = span_id;
    }
    
    static std::string getCurrentTraceId() {
        return current_trace_id();
    }
    
    static std::string getCurrentSpanId() {
        return current_span_id();
    }
    
    static void clear() {
        current_trace_id().clear();
        current_span_id().clear();
    }
    
private:
    static std::string generateId(size_t bytes) {
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 gen(rd());
        static thread_local std::uniform_int_distribution<uint64_t> dis;
        
        std::ostringstream oss;
        for (size_t i = 0; i < bytes; i += 8) {
            uint64_t val = dis(gen);
            size_t remaining = std::min(size_t(8), bytes - i);
            for (size_t j = 0; j < remaining; ++j) {
                oss << std::hex << std::setfill('0') << std::setw(2)
                    << ((val >> (j * 8)) & 0xFF);
            }
        }
        return oss.str();
    }
    
    static std::string& current_trace_id() {
        static thread_local std::string trace_id;
        return trace_id;
    }
    
    static std::string& current_span_id() {
        static thread_local std::string span_id;
        return span_id;
    }
};

// ============================================================================
// Scoped Trace Span
// ============================================================================

class TraceSpan {
public:
    TraceSpan(const std::string& name, const std::string& trace_id = "")
        : name_(name)
        , start_time_(std::chrono::steady_clock::now())
        , parent_span_id_(TraceContext::getCurrentSpanId()) {
        
        if (trace_id.empty()) {
            if (TraceContext::getCurrentTraceId().empty()) {
                TraceContext::setCurrentTraceId(TraceContext::generateTraceId());
            }
        } else {
            TraceContext::setCurrentTraceId(trace_id);
        }
        
        span_id_ = TraceContext::generateSpanId();
        TraceContext::setCurrentSpanId(span_id_);
    }
    
    ~TraceSpan() {
        TraceContext::setCurrentSpanId(parent_span_id_);
    }
    
    std::string getSpanId() const { return span_id_; }
    std::string getName() const { return name_; }
    
    std::chrono::nanoseconds elapsed() const {
        return std::chrono::steady_clock::now() - start_time_;
    }
    
    double elapsedMs() const {
        return std::chrono::duration<double, std::milli>(elapsed()).count();
    }
    
    void addTag(const std::string& key, const std::string& value) {
        tags_[key] = value;
    }
    
    const std::map<std::string, std::string>& getTags() const {
        return tags_;
    }
    
private:
    std::string name_;
    std::chrono::steady_clock::time_point start_time_;
    std::string parent_span_id_;
    std::string span_id_;
    std::map<std::string, std::string> tags_;
};

// ============================================================================
// Logger Class
// ============================================================================

class Logger {
public:
    explicit Logger(const std::string& name)
        : name_(name)
        , level_(LogLevel::INFO) {}
    
    void setLevel(LogLevel level) { level_ = level; }
    LogLevel getLevel() const { return level_; }
    
    void addSink(std::shared_ptr<LogSink> sink) {
        std::lock_guard<std::mutex> lock(mutex_);
        sinks_.push_back(std::move(sink));
    }
    
    void removeSink(std::shared_ptr<LogSink> sink) {
        std::lock_guard<std::mutex> lock(mutex_);
        sinks_.erase(
            std::remove(sinks_.begin(), sinks_.end(), sink),
            sinks_.end());
    }
    
    void clearSinks() {
        std::lock_guard<std::mutex> lock(mutex_);
        sinks_.clear();
    }
    
    void log(LogLevel level, const std::string& message,
             const std::string& file = "", int line = 0,
             const std::string& func = "") {
        if (level < level_) return;
        
        LogEntry entry;
        entry.level = level;
        entry.logger_name = name_;
        entry.message = message;
        entry.trace_id = TraceContext::getCurrentTraceId();
        entry.span_id = TraceContext::getCurrentSpanId();
        entry.thread_id = std::this_thread::get_id();
        entry.source_file = file;
        entry.source_line = line;
        entry.function_name = func;
        entry.sequence_number = next_sequence_++;
        
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& sink : sinks_) {
            sink->write(entry);
        }
    }
    
    void logWithContext(LogLevel level, const std::string& message,
                        const std::map<std::string, std::string>& context,
                        const std::string& file = "", int line = 0,
                        const std::string& func = "") {
        if (level < level_) return;
        
        LogEntry entry;
        entry.level = level;
        entry.logger_name = name_;
        entry.message = message;
        entry.trace_id = TraceContext::getCurrentTraceId();
        entry.span_id = TraceContext::getCurrentSpanId();
        entry.thread_id = std::this_thread::get_id();
        entry.source_file = file;
        entry.source_line = line;
        entry.function_name = func;
        entry.context = context;
        entry.sequence_number = next_sequence_++;
        
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& sink : sinks_) {
            sink->write(entry);
        }
    }
    
    void trace(const std::string& msg) { log(LogLevel::TRACE, msg); }
    void debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
    void info(const std::string& msg) { log(LogLevel::INFO, msg); }
    void warn(const std::string& msg) { log(LogLevel::WARN, msg); }
    void error(const std::string& msg) { log(LogLevel::ERROR, msg); }
    void fatal(const std::string& msg) { log(LogLevel::FATAL, msg); }
    
    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& sink : sinks_) {
            sink->flush();
        }
    }
    
    const std::string& getName() const { return name_; }
    
private:
    std::string name_;
    std::atomic<LogLevel> level_;
    std::vector<std::shared_ptr<LogSink>> sinks_;
    std::mutex mutex_;
    static std::atomic<uint64_t> next_sequence_;
};

inline std::atomic<uint64_t> Logger::next_sequence_{0};

// ============================================================================
// Log Manager (Registry)
// ============================================================================

class LogManager {
public:
    static LogManager& instance() {
        static LogManager instance;
        return instance;
    }
    
    std::shared_ptr<Logger> getLogger(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = loggers_.find(name);
        if (it != loggers_.end()) {
            return it->second;
        }
        
        auto logger = std::make_shared<Logger>(name);
        logger->setLevel(default_level_);
        for (auto& sink : default_sinks_) {
            logger->addSink(sink);
        }
        loggers_[name] = logger;
        return logger;
    }
    
    void setDefaultLevel(LogLevel level) {
        default_level_ = level;
    }
    
    void addDefaultSink(std::shared_ptr<LogSink> sink) {
        std::lock_guard<std::mutex> lock(mutex_);
        default_sinks_.push_back(std::move(sink));
    }
    
    void setGlobalLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        default_level_ = level;
        for (auto& [name, logger] : loggers_) {
            logger->setLevel(level);
        }
    }
    
    void flushAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [name, logger] : loggers_) {
            logger->flush();
        }
    }
    
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [name, logger] : loggers_) {
            logger->flush();
            logger->clearSinks();
        }
        loggers_.clear();
        default_sinks_.clear();
    }
    
private:
    LogManager() = default;
    
    std::map<std::string, std::shared_ptr<Logger>> loggers_;
    std::vector<std::shared_ptr<LogSink>> default_sinks_;
    LogLevel default_level_ = LogLevel::INFO;
    std::mutex mutex_;
};

// ============================================================================
// Trace Analyzer
// ============================================================================

struct TraceRecord {
    std::string trace_id;
    std::string span_id;
    std::string parent_span_id;
    std::string operation;
    std::chrono::system_clock::time_point start_time;
    std::chrono::nanoseconds duration;
    std::map<std::string, std::string> tags;
    LogLevel status;
};

class TraceAnalyzer {
public:
    void recordTrace(const TraceRecord& record) {
        std::lock_guard<std::mutex> lock(mutex_);
        traces_.push_back(record);
        
        // Keep only recent traces
        while (traces_.size() > max_traces_) {
            traces_.erase(traces_.begin());
        }
    }
    
    // Get traces for a specific trace ID
    std::vector<TraceRecord> getTrace(const std::string& trace_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TraceRecord> result;
        for (const auto& trace : traces_) {
            if (trace.trace_id == trace_id) {
                result.push_back(trace);
            }
        }
        return result;
    }
    
    // Get slow traces above threshold
    std::vector<TraceRecord> getSlowTraces(std::chrono::nanoseconds threshold) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TraceRecord> result;
        for (const auto& trace : traces_) {
            if (trace.duration >= threshold) {
                result.push_back(trace);
            }
        }
        return result;
    }
    
    // Get error traces
    std::vector<TraceRecord> getErrorTraces() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TraceRecord> result;
        for (const auto& trace : traces_) {
            if (trace.status >= LogLevel::ERROR) {
                result.push_back(trace);
            }
        }
        return result;
    }
    
    // Calculate statistics
    struct TraceStats {
        size_t total_traces;
        double avg_duration_ms;
        double max_duration_ms;
        double min_duration_ms;
        double p50_duration_ms;
        double p95_duration_ms;
        double p99_duration_ms;
        size_t error_count;
    };
    
    TraceStats calculateStats(const std::string& operation = "") const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<double> durations;
        size_t error_count = 0;
        
        for (const auto& trace : traces_) {
            if (!operation.empty() && trace.operation != operation) {
                continue;
            }
            durations.push_back(
                std::chrono::duration<double, std::milli>(trace.duration).count());
            if (trace.status >= LogLevel::ERROR) {
                ++error_count;
            }
        }
        
        TraceStats stats{};
        stats.total_traces = durations.size();
        stats.error_count = error_count;
        
        if (durations.empty()) {
            return stats;
        }
        
        std::sort(durations.begin(), durations.end());
        
        double sum = 0;
        for (double d : durations) sum += d;
        
        stats.avg_duration_ms = sum / durations.size();
        stats.min_duration_ms = durations.front();
        stats.max_duration_ms = durations.back();
        stats.p50_duration_ms = percentile(durations, 0.50);
        stats.p95_duration_ms = percentile(durations, 0.95);
        stats.p99_duration_ms = percentile(durations, 0.99);
        
        return stats;
    }
    
    void setMaxTraces(size_t max) { max_traces_ = max; }
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        traces_.clear();
    }
    
private:
    static double percentile(const std::vector<double>& sorted_data, double p) {
        if (sorted_data.empty()) return 0;
        size_t idx = static_cast<size_t>(p * (sorted_data.size() - 1));
        return sorted_data[idx];
    }
    
    std::vector<TraceRecord> traces_;
    size_t max_traces_ = 10000;
    mutable std::mutex mutex_;
};

// ============================================================================
// Convenience Macros
// ============================================================================

#define HSM_LOG(logger, level, msg) \
    (logger)->log(level, msg, __FILE__, __LINE__, __func__)

#define HSM_LOG_TRACE(logger, msg) HSM_LOG(logger, dfsmshsm::logging::LogLevel::TRACE, msg)
#define HSM_LOG_DEBUG(logger, msg) HSM_LOG(logger, dfsmshsm::logging::LogLevel::DEBUG, msg)
#define HSM_LOG_INFO(logger, msg)  HSM_LOG(logger, dfsmshsm::logging::LogLevel::INFO, msg)
#define HSM_LOG_WARN(logger, msg)  HSM_LOG(logger, dfsmshsm::logging::LogLevel::WARN, msg)
#define HSM_LOG_ERROR(logger, msg) HSM_LOG(logger, dfsmshsm::logging::LogLevel::ERROR, msg)
#define HSM_LOG_FATAL(logger, msg) HSM_LOG(logger, dfsmshsm::logging::LogLevel::FATAL, msg)

// Get default HSM logger
inline std::shared_ptr<Logger> getHSMLogger() {
    return LogManager::instance().getLogger("hsm");
}

} // namespace logging
} // namespace dfsmshsm

#endif // HSM_LOGGING_HPP
