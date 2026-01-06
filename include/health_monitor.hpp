/**
 * @file health_monitor.hpp
 * @brief System health monitoring and diagnostics
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_HEALTH_MONITOR_HPP
#define DFSMSHSM_HEALTH_MONITOR_HPP

#include "hsm_types.hpp"
#include "event_system.hpp"
#include <thread>
#include <atomic>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/statvfs.h>
#endif

namespace dfsmshsm {

class HealthMonitor {
public:
    struct Thresholds { double space_warn = 80.0, space_crit = 95.0, mem_warn = 80.0, mem_crit = 95.0; uint32_t queue_warn = 100, queue_crit = 500, fail_warn = 10, fail_crit = 50; };

    explicit HealthMonitor(const std::filesystem::path& path) : path_(path), running_(false) {}
    ~HealthMonitor() { stop(); }

    bool start(uint32_t interval = 60) {
        if (running_) return false;
        running_ = true; interval_ = std::chrono::seconds(interval);
        thread_ = std::thread([this]{ while (running_) { checkHealth(); auto end = std::chrono::steady_clock::now() + interval_; while (running_ && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(std::chrono::milliseconds(100)); } });
        return true;
    }

    void stop() { running_ = false; if (thread_.joinable()) thread_.join(); }
    bool isRunning() const { return running_; }

    HealthReport checkHealth() {
        HealthReport r; r.timestamp = std::chrono::system_clock::now();
        r.primary_space_percent = getDiskUsage(); r.memory_usage_percent = getMemoryUsage();
        r.pending_operations = pending_; r.failed_operations_24h = countFailures();
        r.overall_status = determineStatus(r); generateAlerts(r);
        std::lock_guard<std::mutex> lock(mtx_); reports_.push_back(r); while (reports_.size() > 1440) reports_.pop_front();
        return r;
    }

    std::optional<HealthReport> getLatestReport() const { std::lock_guard<std::mutex> lock(mtx_); return reports_.empty() ? std::nullopt : std::optional<HealthReport>(reports_.back()); }
    std::vector<HealthReport> getReportHistory(size_t n = 100) const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<HealthReport> result; size_t start = (reports_.size() > n) ? reports_.size() - n : 0;
        for (size_t i = start; i < reports_.size(); ++i) result.push_back(reports_[i]);
        return result;
    }

    void setThresholds(const Thresholds& t) { thresh_ = t; }
    Thresholds getThresholds() const { return thresh_; }
    void recordOperation(bool success) { std::lock_guard<std::mutex> lock(mtx_); ops_.push_back({std::chrono::system_clock::now(), success}); auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24); while (!ops_.empty() && ops_.front().ts < cutoff) ops_.pop_front(); }
    void setPendingOperations(uint32_t n) { pending_ = n; }

    std::string generateStatusString() const {
        auto r = getLatestReport(); if (!r) return "No health data";
        std::ostringstream oss;
        oss << "=== System Health ===\nStatus: " << healthToString(r->overall_status) << "\nStorage: " << std::fixed << std::setprecision(1) << r->primary_space_percent << "% used\nMemory: " << r->memory_usage_percent << "% used\nPending: " << r->pending_operations << "\nFailed (24h): " << r->failed_operations_24h << "\n";
        if (!r->warnings.empty()) { oss << "\nWarnings:\n"; for (const auto& w : r->warnings) oss << "  - " << w << "\n"; }
        if (!r->errors.empty()) { oss << "\nErrors:\n"; for (const auto& e : r->errors) oss << "  - " << e << "\n"; }
        return oss.str();
    }

private:
    struct OpRec { std::chrono::system_clock::time_point ts; bool success; };

    double getDiskUsage() {
#ifdef _WIN32
        ULARGE_INTEGER fb, tb; if (GetDiskFreeSpaceExW(path_.c_str(), &fb, &tb, nullptr) && tb.QuadPart > 0) return 100.0 * double(tb.QuadPart - fb.QuadPart) / double(tb.QuadPart);
#else
        struct statvfs st; if (statvfs(path_.c_str(), &st) == 0) { uint64_t t = uint64_t(st.f_blocks) * st.f_frsize, f = uint64_t(st.f_bavail) * st.f_frsize; if (t > 0) return 100.0 * double(t - f) / double(t); }
#endif
        return 0.0;
    }

    double getMemoryUsage() {
#ifdef _WIN32
        MEMORYSTATUSEX s; s.dwLength = sizeof(s); if (GlobalMemoryStatusEx(&s)) return double(s.dwMemoryLoad);
#else
        std::ifstream f("/proc/meminfo"); if (f) { std::string l; uint64_t t = 0, a = 0; while (std::getline(f, l)) { if (l.find("MemTotal:") == 0) t = std::stoull(l.substr(10)); else if (l.find("MemAvailable:") == 0) a = std::stoull(l.substr(14)); } if (t > 0) return 100.0 * double(t - a) / double(t); }
#endif
        return 0.0;
    }

    uint32_t countFailures() { std::lock_guard<std::mutex> lock(mtx_); uint32_t c = 0; for (const auto& o : ops_) if (!o.success) ++c; return c; }

    HealthStatus determineStatus(const HealthReport& r) {
        if (r.primary_space_percent >= thresh_.space_crit || r.memory_usage_percent >= thresh_.mem_crit || r.pending_operations >= thresh_.queue_crit || r.failed_operations_24h >= thresh_.fail_crit) return HealthStatus::CRITICAL;
        if (r.primary_space_percent >= thresh_.space_warn || r.memory_usage_percent >= thresh_.mem_warn || r.pending_operations >= thresh_.queue_warn || r.failed_operations_24h >= thresh_.fail_warn) return HealthStatus::WARNING;
        return HealthStatus::HEALTHY;
    }

    void generateAlerts(HealthReport& r) {
        auto pct = [](double v) { return std::to_string(int(v)); };
        if (r.primary_space_percent >= thresh_.space_crit) { r.errors.push_back("Storage critical: " + pct(r.primary_space_percent) + "%"); EventSystem::instance().emit(EventType::HEALTH_CRITICAL, "HealthMonitor", "Storage critically low"); }
        else if (r.primary_space_percent >= thresh_.space_warn) { r.warnings.push_back("Storage low: " + pct(r.primary_space_percent) + "%"); EventSystem::instance().emit(EventType::HEALTH_WARNING, "HealthMonitor", "Storage low"); }
        if (r.memory_usage_percent >= thresh_.mem_crit) r.errors.push_back("Memory critical: " + pct(r.memory_usage_percent) + "%");
        else if (r.memory_usage_percent >= thresh_.mem_warn) r.warnings.push_back("Memory high: " + pct(r.memory_usage_percent) + "%");
        if (r.pending_operations >= thresh_.queue_crit) r.errors.push_back("Queue critical: " + std::to_string(r.pending_operations));
        else if (r.pending_operations >= thresh_.queue_warn) r.warnings.push_back("Queue building: " + std::to_string(r.pending_operations));
        if (r.failed_operations_24h >= thresh_.fail_crit) r.errors.push_back("High failures: " + std::to_string(r.failed_operations_24h));
        else if (r.failed_operations_24h >= thresh_.fail_warn) r.warnings.push_back("Elevated failures: " + std::to_string(r.failed_operations_24h));
    }

    std::filesystem::path path_;
    std::atomic<bool> running_;
    std::thread thread_;
    std::chrono::seconds interval_{60};
    mutable std::mutex mtx_;
    std::deque<HealthReport> reports_;
    std::deque<OpRec> ops_;
    Thresholds thresh_;
    std::atomic<uint32_t> pending_{0};
};

} // namespace dfsmshsm
#endif
