/**
 * @file performance_monitor.hpp
 * @brief Performance monitoring for HSM operations
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_PERFORMANCE_MONITOR_HPP
#define DFSMSHSM_PERFORMANCE_MONITOR_HPP

#include "hsm_types.hpp"
#include <map>
#include <mutex>
#include <chrono>
#include <deque>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <limits>

namespace dfsmshsm {

class PerformanceMonitor {
public:
    struct Metrics {
        uint64_t count = 0;
        double total_ms = 0.0;
        double min_ms = std::numeric_limits<double>::max();
        double max_ms = 0.0;
        uint64_t total_bytes = 0;
    };

    static PerformanceMonitor& instance() { static PerformanceMonitor m; return m; }

    void recordOperation(OperationType type, double duration_ms, uint64_t bytes = 0) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto& m = metrics_[type];
        ++m.count; m.total_ms += duration_ms; m.total_bytes += bytes;
        if (duration_ms < m.min_ms) m.min_ms = duration_ms;
        if (duration_ms > m.max_ms) m.max_ms = duration_ms;
    }

    Metrics getMetrics(OperationType type) const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = metrics_.find(type);
        return (it != metrics_.end()) ? it->second : Metrics{};
    }

    double getAverageDuration(OperationType type) const {
        auto m = getMetrics(type);
        return m.count > 0 ? m.total_ms / static_cast<double>(m.count) : 0.0;
    }

    double getThroughput(OperationType type) const {
        auto m = getMetrics(type);
        return m.total_ms > 0 ? (static_cast<double>(m.total_bytes) / (m.total_ms / 1000.0)) : 0.0;
    }

    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        oss << "=== Performance Report v4.1.0 ===\n\n";
        for (const auto& [op, m] : metrics_) {
            if (m.count == 0) continue;
            double avg = m.total_ms / static_cast<double>(m.count);
            oss << operationToString(op) << ":\n"
                << "  Count: " << m.count << "\n"
                << "  Avg: " << std::fixed << std::setprecision(2) << avg << " ms\n"
                << "  Min: " << m.min_ms << " ms, Max: " << m.max_ms << " ms\n";
            if (m.total_bytes > 0) {
                double tp = static_cast<double>(m.total_bytes) / (m.total_ms / 1000.0) / 1048576.0;
                oss << "  Throughput: " << tp << " MB/s\n";
            }
            oss << "\n";
        }
        return oss.str();
    }

    void reset() { std::lock_guard<std::mutex> lock(mtx_); metrics_.clear(); }

private:
    PerformanceMonitor() = default;
    mutable std::mutex mtx_;
    std::map<OperationType, Metrics> metrics_;
};

class ScopedTimer {
public:
    ScopedTimer(OperationType type, uint64_t bytes = 0) : type_(type), bytes_(bytes), start_(std::chrono::high_resolution_clock::now()) {}
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start_).count();
        PerformanceMonitor::instance().recordOperation(type_, ms, bytes_);
    }
private:
    OperationType type_;
    uint64_t bytes_;
    std::chrono::high_resolution_clock::time_point start_;
};

} // namespace dfsmshsm
#endif
