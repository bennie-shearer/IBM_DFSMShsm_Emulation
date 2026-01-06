/**
 * @file metrics_exporter.hpp
 * @brief Prometheus/Grafana metrics export
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_METRICS_EXPORTER_HPP
#define DFSMSHSM_METRICS_EXPORTER_HPP

#include "hsm_types.hpp"
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <mutex>
#include <chrono>

namespace dfsmshsm {

enum class MetricType { COUNTER, GAUGE, HISTOGRAM };

struct MetricLabel { std::string name; std::string value; };

class MetricsRegistry {
public:
    static MetricsRegistry& instance() { static MetricsRegistry inst; return inst; }

    void registerCounter(const std::string& name, const std::string& help) {
        std::lock_guard<std::mutex> lock(mutex_);
        meta_[name] = {name, help, MetricType::COUNTER};
        counters_[name] = 0;
    }

    void incrementCounter(const std::string& name, double value = 1.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        counters_[name] += value;
    }

    void incrementCounter(const std::string& name, const std::vector<MetricLabel>& labels, double value = 1.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        labeled_counters_[makeKey(name, labels)] += value;
    }

    void registerGauge(const std::string& name, const std::string& help) {
        std::lock_guard<std::mutex> lock(mutex_);
        meta_[name] = {name, help, MetricType::GAUGE};
        gauges_[name] = 0;
    }

    void setGauge(const std::string& name, double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        gauges_[name] = value;
    }

    void setGauge(const std::string& name, const std::vector<MetricLabel>& labels, double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        labeled_gauges_[makeKey(name, labels)] = value;
    }

    void incrementGauge(const std::string& name, double value = 1.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        gauges_[name] += value;
    }

    void decrementGauge(const std::string& name, double value = 1.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        gauges_[name] -= value;
    }

    void registerHistogram(const std::string& name, const std::string& help, const std::vector<double>& buckets) {
        std::lock_guard<std::mutex> lock(mutex_);
        meta_[name] = {name, help, MetricType::HISTOGRAM};
        histogram_buckets_[name] = buckets;
        histogram_counts_[name].resize(buckets.size() + 1, 0);
        histogram_sum_[name] = 0;
        histogram_count_[name] = 0;
    }

    void observeHistogram(const std::string& name, double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = histogram_buckets_.find(name);
        if (it == histogram_buckets_.end()) return;
        const auto& buckets = it->second;
        auto& counts = histogram_counts_[name];
        for (size_t i = 0; i < buckets.size(); ++i) if (value <= buckets[i]) counts[i]++;
        counts[buckets.size()]++;
        histogram_sum_[name] += value;
        histogram_count_[name]++;
    }

    std::string exportPrometheus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        for (const auto& [name, value] : counters_) {
            auto it = meta_.find(name);
            if (it != meta_.end()) {
                oss << "# HELP " << name << " " << it->second.help << "\n";
                oss << "# TYPE " << name << " counter\n";
            }
            oss << name << " " << value << "\n";
        }
        for (const auto& [name, value] : gauges_) {
            auto it = meta_.find(name);
            if (it != meta_.end()) {
                oss << "# HELP " << name << " " << it->second.help << "\n";
                oss << "# TYPE " << name << " gauge\n";
            }
            oss << name << " " << value << "\n";
        }
        for (const auto& [name, buckets] : histogram_buckets_) {
            auto it = meta_.find(name);
            if (it != meta_.end()) {
                oss << "# HELP " << name << " " << it->second.help << "\n";
                oss << "# TYPE " << name << " histogram\n";
            }
            const auto& counts = histogram_counts_.at(name);
            uint64_t cumulative = 0;
            for (size_t i = 0; i < buckets.size(); ++i) {
                cumulative += counts[i];
                oss << name << "_bucket{le=\"" << buckets[i] << "\"} " << cumulative << "\n";
            }
            cumulative += counts[buckets.size()];
            oss << name << "_bucket{le=\"+Inf\"} " << cumulative << "\n";
            oss << name << "_sum " << histogram_sum_.at(name) << "\n";
            oss << name << "_count " << histogram_count_.at(name) << "\n";
        }
        return oss.str();
    }

    std::string exportJSON() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << "{\n  \"metrics\": [\n";
        bool first = true;
        for (const auto& [name, value] : counters_) {
            if (!first) oss << ",\n";
            first = false;
            oss << "    {\"name\": \"" << name << "\", \"type\": \"counter\", \"value\": " << value << "}";
        }
        for (const auto& [name, value] : gauges_) {
            if (!first) oss << ",\n";
            first = false;
            oss << "    {\"name\": \"" << name << "\", \"type\": \"gauge\", \"value\": " << value << "}";
        }
        oss << "\n  ]\n}";
        return oss.str();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        counters_.clear(); gauges_.clear(); labeled_counters_.clear(); labeled_gauges_.clear();
        histogram_buckets_.clear(); histogram_counts_.clear(); histogram_sum_.clear(); histogram_count_.clear();
        meta_.clear();
    }

private:
    MetricsRegistry() = default;
    struct MetricMeta { std::string name; std::string help; MetricType type; };
    std::string makeKey(const std::string& name, const std::vector<MetricLabel>& labels) const {
        std::ostringstream oss;
        oss << name;
        for (const auto& l : labels) oss << "|" << l.name << "=" << l.value;
        return oss.str();
    }
    mutable std::mutex mutex_;
    std::map<std::string, double> counters_, gauges_, labeled_counters_, labeled_gauges_;
    std::map<std::string, std::vector<double>> histogram_buckets_;
    std::map<std::string, std::vector<uint64_t>> histogram_counts_;
    std::map<std::string, double> histogram_sum_;
    std::map<std::string, uint64_t> histogram_count_;
    std::map<std::string, MetricMeta> meta_;
};

class HSMMetrics {
public:
    static HSMMetrics& instance() { static HSMMetrics inst; return inst; }

    void initialize() {
        auto& r = MetricsRegistry::instance();
        r.registerCounter("dfsmshsm_datasets_created_total", "Total datasets created");
        r.registerGauge("dfsmshsm_datasets_active", "Active datasets");
        r.registerCounter("dfsmshsm_migrations_total", "Total migrations");
        r.registerCounter("dfsmshsm_recalls_total", "Total recalls");
        r.registerCounter("dfsmshsm_backups_total", "Total backups");
        r.registerGauge("dfsmshsm_storage_bytes_total", "Total storage");
        r.registerGauge("dfsmshsm_storage_bytes_used", "Used storage");
        r.registerGauge("dfsmshsm_dedupe_ratio", "Dedup ratio");
        r.registerGauge("dfsmshsm_compression_ratio", "Compression ratio");
        r.registerGauge("dfsmshsm_health_status", "Health (1=ok)");
        r.registerGauge("dfsmshsm_uptime_seconds", "Uptime");
        r.registerHistogram("dfsmshsm_operation_duration_seconds", "Op duration", {0.01, 0.1, 0.5, 1.0, 5.0});
    }

    void recordDatasetCreated() { MetricsRegistry::instance().incrementCounter("dfsmshsm_datasets_created_total"); MetricsRegistry::instance().incrementGauge("dfsmshsm_datasets_active"); }
    void recordDatasetDeleted() { MetricsRegistry::instance().decrementGauge("dfsmshsm_datasets_active"); }
    void recordMigration(uint64_t) { MetricsRegistry::instance().incrementCounter("dfsmshsm_migrations_total"); }
    void recordRecall() { MetricsRegistry::instance().incrementCounter("dfsmshsm_recalls_total"); }
    void recordBackup(uint64_t) { MetricsRegistry::instance().incrementCounter("dfsmshsm_backups_total"); }
    void updateStorageMetrics(uint64_t t, uint64_t u) { auto& r = MetricsRegistry::instance(); r.setGauge("dfsmshsm_storage_bytes_total", double(t)); r.setGauge("dfsmshsm_storage_bytes_used", double(u)); }
    void updateTierMetrics(StorageTier, size_t, uint64_t) {}
    void updateDedupeRatio(double v) { MetricsRegistry::instance().setGauge("dfsmshsm_dedupe_ratio", v); }
    void updateCompressionRatio(double v) { MetricsRegistry::instance().setGauge("dfsmshsm_compression_ratio", v); }
    void updateHealth(bool h) { MetricsRegistry::instance().setGauge("dfsmshsm_health_status", h ? 1.0 : 0.0); }
    void updateUptime(double s) { MetricsRegistry::instance().setGauge("dfsmshsm_uptime_seconds", s); }
    void updateQuotaUsage(const std::string&, double) {}
    std::string exportPrometheus() const { return MetricsRegistry::instance().exportPrometheus(); }
    std::string exportJSON() const { return MetricsRegistry::instance().exportJSON(); }
private:
    HSMMetrics() = default;
};

} // namespace dfsmshsm
#endif
