/**
 * @file hsm_logger.hpp
 * @brief Logging system for HSM operations
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_HSM_LOGGER_HPP
#define DFSMSHSM_HSM_LOGGER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace dfsmshsm {

// Use LOG_ prefix to avoid Windows macro conflicts (ERROR is defined in WinGDI.h)
enum class LogLevel { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_FATAL };

class HSMLogger {
public:
    static HSMLogger& instance() { static HSMLogger l; return l; }

    bool initialize(const std::filesystem::path& dir, LogLevel level = LogLevel::LOG_INFO) {
        std::lock_guard<std::mutex> lock(mtx_);
        dir_ = dir; level_ = level;
        try {
            if (!std::filesystem::exists(dir_)) std::filesystem::create_directories(dir_);
            openFile();
            init_ = true;
            return true;
        } catch (...) { return false; }
    }

    void setLevel(LogLevel level) { level_ = level; }
    LogLevel getLevel() const { return level_; }
    void setConsoleOutput(bool enabled) { console_ = enabled; }

    void log(LogLevel level, const std::string& component, const std::string& message) {
        if (level < level_) return;
        std::lock_guard<std::mutex> lock(mtx_);
        std::string line = formatLine(level, component, message);
        if (init_ && file_.is_open()) { file_ << line << "\n"; file_.flush(); }
        if (console_) std::cout << line << "\n";
    }

    void trace(const std::string& comp, const std::string& msg) { log(LogLevel::LOG_TRACE, comp, msg); }
    void debug(const std::string& comp, const std::string& msg) { log(LogLevel::LOG_DEBUG, comp, msg); }
    void info(const std::string& comp, const std::string& msg) { log(LogLevel::LOG_INFO, comp, msg); }
    void warning(const std::string& comp, const std::string& msg) { log(LogLevel::LOG_WARNING, comp, msg); }
    void error(const std::string& comp, const std::string& msg) { log(LogLevel::LOG_ERROR, comp, msg); }
    void fatal(const std::string& comp, const std::string& msg) { log(LogLevel::LOG_FATAL, comp, msg); }

    void rotate() { std::lock_guard<std::mutex> lock(mtx_); openFile(); }
    
    void shutdown() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
        init_ = false;
    }

private:
    HSMLogger() = default;
    ~HSMLogger() { if (file_.is_open()) file_.close(); }
    HSMLogger(const HSMLogger&) = delete;
    HSMLogger& operator=(const HSMLogger&) = delete;

    void openFile() {
        if (file_.is_open()) file_.close();
        auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream fn;
        fn << "hsm_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".log";
        file_.open(dir_ / fn.str(), std::ios::app);
    }

    std::string formatLine(LogLevel level, const std::string& comp, const std::string& msg) {
        static const char* levels[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
        auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " [" << levels[static_cast<int>(level)] << "] [" << comp << "] " << msg;
        return oss.str();
    }

    mutable std::mutex mtx_;
    std::filesystem::path dir_;
    std::ofstream file_;
    LogLevel level_ = LogLevel::LOG_INFO;
    bool init_ = false, console_ = false;
};

#define LOG_TRACE(comp, msg) dfsmshsm::HSMLogger::instance().trace(comp, msg)
#define LOG_DEBUG(comp, msg) dfsmshsm::HSMLogger::instance().debug(comp, msg)
#define LOG_INFO(comp, msg) dfsmshsm::HSMLogger::instance().info(comp, msg)
#define LOG_WARNING(comp, msg) dfsmshsm::HSMLogger::instance().warning(comp, msg)
#define LOG_ERROR(comp, msg) dfsmshsm::HSMLogger::instance().error(comp, msg)
#define LOG_FATAL(comp, msg) dfsmshsm::HSMLogger::instance().fatal(comp, msg)

} // namespace dfsmshsm
#endif
