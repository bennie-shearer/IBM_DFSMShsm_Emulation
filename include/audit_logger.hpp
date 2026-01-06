/**
 * @file audit_logger.hpp
 * @brief Comprehensive audit logging for HSM operations
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_AUDIT_LOGGER_HPP
#define DFSMSHSM_AUDIT_LOGGER_HPP

#include "hsm_types.hpp"
#include <fstream>
#include <mutex>
#include <deque>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <map>

namespace dfsmshsm {

class AuditLogger {
public:
    static AuditLogger& instance() { static AuditLogger l; return l; }

    bool initialize(const std::filesystem::path& dir, size_t max = 10000) {
        std::lock_guard<std::mutex> lock(mtx_);
        dir_ = dir; max_ = max;
        try { if (!std::filesystem::exists(dir_)) std::filesystem::create_directories(dir_); openFile(); init_ = true; return true; }
        catch (...) { return false; }
    }

    void log(const AuditRecord& r) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!init_) return;
        recs_.push_back(r); while (recs_.size() > max_) recs_.pop_front();
        writeRecord(r); if (fsize_ >= 10485760) openFile();
    }

    void logOperation(OperationType op, const std::string& target, OperationStatus status, const std::string& details = "", const std::string& user = "SYSTEM") {
        AuditRecord r;
        r.operation_id = generateOperationId();
        r.user = user;
        r.target = target;
        r.details = details;
        r.source_ip = "127.0.0.1";
        r.operation = op;
        r.status = status;
        r.timestamp = std::chrono::system_clock::now();
        log(r);
    }

    std::vector<AuditRecord> getRecentRecords(size_t n = 100) const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<AuditRecord> result;
        size_t start = (recs_.size() > n) ? recs_.size() - n : 0;
        for (size_t i = start; i < recs_.size(); ++i) result.push_back(recs_[i]);
        return result;
    }

    std::vector<AuditRecord> queryByOperation(OperationType op, size_t limit = 1000) const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<AuditRecord> result;
        for (const auto& r : recs_) if (r.operation == op) { result.push_back(r); if (result.size() >= limit) break; }
        return result;
    }

    std::string generateReport(size_t hours = 24) const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(hours);
        std::map<OperationType, size_t> ops; std::map<OperationStatus, size_t> sts; size_t total = 0;
        for (const auto& r : recs_) if (r.timestamp >= cutoff) { ++ops[r.operation]; ++sts[r.status]; ++total; }
        std::ostringstream oss;
        oss << "=== HSM Audit Report (Last " << hours << "h) ===\nTotal: " << total << "\n\nBy Type:\n";
        for (const auto& [o, c] : ops) oss << "  " << operationToString(o) << ": " << c << "\n";
        oss << "\nBy Status:\n";
        for (const auto& [s, c] : sts) oss << "  " << statusToString(s) << ": " << c << "\n";
        return oss.str();
    }

    size_t recordCount() const { std::lock_guard<std::mutex> lock(mtx_); return recs_.size(); }
    void flush() { std::lock_guard<std::mutex> lock(mtx_); if (file_.is_open()) file_.flush(); }
    
    void shutdown() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
        init_ = false;
    }

private:
    AuditLogger() = default;
    ~AuditLogger() { if (file_.is_open()) file_.close(); }
    AuditLogger(const AuditLogger&) = delete;
    AuditLogger& operator=(const AuditLogger&) = delete;

    void openFile() {
        if (file_.is_open()) file_.close();
        auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm{}; 
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream fn; fn << "audit_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".log";
        file_.open(dir_ / fn.str(), std::ios::app); fsize_ = 0;
        if (file_) file_ << "# HSM Audit Log v4.1.0\n";
    }

    void writeRecord(const AuditRecord& r) {
        if (!file_) return;
        auto t = std::chrono::system_clock::to_time_t(r.timestamp);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream line;
        line << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "|" << r.operation_id << "|" << operationToString(r.operation) << "|" << r.user << "|" << r.target << "|" << statusToString(r.status) << "|" << r.details << "\n";
        std::string s = line.str(); file_ << s; fsize_ += s.size();
    }

    mutable std::mutex mtx_;
    std::deque<AuditRecord> recs_;
    std::filesystem::path dir_;
    std::ofstream file_;
    size_t fsize_ = 0, max_ = 10000;
    bool init_ = false;
};

} // namespace dfsmshsm
#endif
