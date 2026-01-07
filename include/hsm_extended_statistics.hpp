/*
 * DFSMShsm Emulator - Extended Statistics Support
 * @version 4.2.0
 *
 * Copyright (c) 2024 DFSMShsm Emulator Project
 * Licensed under the MIT License
 *
 * Provides comprehensive statistical analysis capabilities including:
 * - Detailed throughput metrics and performance tracking
 * - Time-series data collection and storage
 * - Trend analysis with statistical forecasting
 * - Capacity planning and growth projection
 * - Anomaly detection and alerting
 * - Historical data aggregation and reporting
 * - SLA compliance tracking
 */

#ifndef HSM_EXTENDED_STATISTICS_HPP
#define HSM_EXTENDED_STATISTICS_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <memory>
#include <chrono>
#include <functional>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <optional>
#include <variant>
#include <queue>
#include <deque>
#include <array>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace hsm {
namespace statistics {

// ============================================================================
// Forward Declarations
// ============================================================================

class MetricCollector;
class TimeSeriesStore;
class TrendAnalyzer;
class CapacityPlanner;
class AnomalyDetector;
class StatisticsManager;

// ============================================================================
// Type Aliases
// ============================================================================

using Timestamp = std::chrono::system_clock::time_point;
using Duration = std::chrono::milliseconds;
using MetricValue = double;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * Types of metrics collected
 */
enum class MetricType {
    // Throughput Metrics
    BYTES_MIGRATED_PER_SECOND,
    BYTES_RECALLED_PER_SECOND,
    BYTES_BACKED_UP_PER_SECOND,
    BYTES_RECOVERED_PER_SECOND,
    
    // Operation Counts
    MIGRATIONS_PER_HOUR,
    RECALLS_PER_HOUR,
    BACKUPS_PER_HOUR,
    RECOVERIES_PER_HOUR,
    
    // Latency Metrics
    MIGRATION_LATENCY_MS,
    RECALL_LATENCY_MS,
    BACKUP_LATENCY_MS,
    RECOVERY_LATENCY_MS,
    
    // Storage Metrics
    PRIMARY_UTILIZATION_PERCENT,
    ML1_UTILIZATION_PERCENT,
    ML2_UTILIZATION_PERCENT,
    TAPE_UTILIZATION_PERCENT,
    CLOUD_UTILIZATION_PERCENT,
    
    // Queue Metrics
    MIGRATION_QUEUE_DEPTH,
    RECALL_QUEUE_DEPTH,
    BACKUP_QUEUE_DEPTH,
    CRQ_QUEUE_DEPTH,
    
    // I/O Metrics
    DASD_IOPS,
    TAPE_IOPS,
    CLOUD_IOPS,
    CACHE_HIT_RATE,
    
    // Dataset Metrics
    TOTAL_DATASETS_MANAGED,
    DATASETS_ON_ML1,
    DATASETS_ON_ML2,
    DATASETS_EXPIRED,
    
    // Error Metrics
    MIGRATION_FAILURES,
    RECALL_FAILURES,
    BACKUP_FAILURES,
    IO_ERRORS,
    
    // Custom
    CUSTOM
};

/**
 * Aggregation methods for time series
 */
enum class AggregationMethod {
    SUM,
    AVERAGE,
    MINIMUM,
    MAXIMUM,
    PERCENTILE_50,
    PERCENTILE_90,
    PERCENTILE_95,
    PERCENTILE_99,
    COUNT,
    RATE,
    DELTA
};

/**
 * Time granularity for data points
 */
enum class TimeGranularity {
    SECOND,
    MINUTE,
    FIVE_MINUTES,
    FIFTEEN_MINUTES,
    HOUR,
    DAY,
    WEEK,
    MONTH
};

/**
 * Trend direction
 */
enum class TrendDirection {
    INCREASING,
    DECREASING,
    STABLE,
    VOLATILE,
    UNKNOWN
};

/**
 * Anomaly severity levels
 */
enum class AnomalySeverity {
    INFO,
    WARNING,
    CRITICAL,
    EMERGENCY
};

/**
 * Forecast confidence levels
 */
enum class ConfidenceLevel {
    LOW,        // 50% confidence
    MEDIUM,     // 75% confidence
    HIGH,       // 90% confidence
    VERY_HIGH   // 95% confidence
};

// ============================================================================
// Data Point Structure
// ============================================================================

/**
 * Individual metric data point
 */
struct DataPoint {
    Timestamp timestamp;
    MetricValue value;
    std::map<std::string, std::string> labels;  // Dimension labels
    
    DataPoint() : timestamp(std::chrono::system_clock::now()), value(0.0) {}
    
    DataPoint(MetricValue v) 
        : timestamp(std::chrono::system_clock::now()), value(v) {}
    
    DataPoint(Timestamp ts, MetricValue v) 
        : timestamp(ts), value(v) {}
    
    DataPoint(Timestamp ts, MetricValue v, std::map<std::string, std::string> l)
        : timestamp(ts), value(v), labels(std::move(l)) {}
    
    bool operator<(const DataPoint& other) const {
        return timestamp < other.timestamp;
    }
};

/**
 * Aggregated data bucket
 */
struct AggregatedBucket {
    Timestamp start_time;
    Timestamp end_time;
    TimeGranularity granularity;
    
    MetricValue sum = 0.0;
    MetricValue min_value = std::numeric_limits<MetricValue>::max();
    MetricValue max_value = std::numeric_limits<MetricValue>::lowest();
    MetricValue average = 0.0;
    size_t count = 0;
    
    std::vector<MetricValue> values;  // For percentile calculations
    
    void addValue(MetricValue v) {
        sum += v;
        min_value = std::min(min_value, v);
        max_value = std::max(max_value, v);
        count++;
        average = sum / static_cast<double>(count);
        values.push_back(v);
    }
    
    MetricValue getPercentile(double percentile) const {
        if (values.empty()) return 0.0;
        
        std::vector<MetricValue> sorted = values;
        std::sort(sorted.begin(), sorted.end());
        
        size_t index = static_cast<size_t>(percentile * (sorted.size() - 1));
        return sorted[index];
    }
};

// ============================================================================
// Time Series Definition
// ============================================================================

/**
 * Time series configuration and data
 */
class TimeSeries {
public:
    std::string name;
    MetricType type;
    std::string description;
    std::string unit;
    
    // Retention settings
    size_t max_raw_points = 86400;       // Keep 1 day of raw data
    size_t max_minute_points = 43200;    // Keep 30 days of minute data
    size_t max_hour_points = 8760;       // Keep 1 year of hourly data
    size_t max_day_points = 3650;        // Keep 10 years of daily data
    
private:
    mutable std::shared_mutex mutex_;
    std::deque<DataPoint> raw_data_;
    std::deque<AggregatedBucket> minute_data_;
    std::deque<AggregatedBucket> hour_data_;
    std::deque<AggregatedBucket> day_data_;
    
    Timestamp last_minute_aggregation_;
    Timestamp last_hour_aggregation_;
    Timestamp last_day_aggregation_;
    
public:
    TimeSeries(const std::string& n, MetricType t, 
               const std::string& desc = "", const std::string& u = "")
        : name(n), type(t), description(desc), unit(u) {
        auto now = std::chrono::system_clock::now();
        last_minute_aggregation_ = now;
        last_hour_aggregation_ = now;
        last_day_aggregation_ = now;
    }
    
    /**
     * Add a new data point
     */
    void addPoint(const DataPoint& point) {
        std::unique_lock lock(mutex_);
        raw_data_.push_back(point);
        
        // Enforce retention limit
        while (raw_data_.size() > max_raw_points) {
            raw_data_.pop_front();
        }
        
        // Check if we need to aggregate
        aggregateIfNeeded();
    }
    
    void addPoint(MetricValue value) {
        addPoint(DataPoint(std::chrono::system_clock::now(), value));
    }
    
    /**
     * Get raw data points within time range
     */
    std::vector<DataPoint> getRawData(Timestamp start, Timestamp end) const {
        std::shared_lock lock(mutex_);
        std::vector<DataPoint> result;
        
        for (const auto& point : raw_data_) {
            if (point.timestamp >= start && point.timestamp <= end) {
                result.push_back(point);
            }
        }
        return result;
    }
    
    /**
     * Get aggregated data at specified granularity
     */
    std::vector<AggregatedBucket> getAggregatedData(
        TimeGranularity granularity,
        Timestamp start,
        Timestamp end) const {
        
        std::shared_lock lock(mutex_);
        std::vector<AggregatedBucket> result;
        
        const std::deque<AggregatedBucket>* source = nullptr;
        switch (granularity) {
            case TimeGranularity::MINUTE:
            case TimeGranularity::FIVE_MINUTES:
            case TimeGranularity::FIFTEEN_MINUTES:
                source = &minute_data_;
                break;
            case TimeGranularity::HOUR:
                source = &hour_data_;
                break;
            case TimeGranularity::DAY:
            case TimeGranularity::WEEK:
            case TimeGranularity::MONTH:
                source = &day_data_;
                break;
            default:
                return result;
        }
        
        for (const auto& bucket : *source) {
            if (bucket.start_time >= start && bucket.end_time <= end) {
                result.push_back(bucket);
            }
        }
        return result;
    }
    
    /**
     * Get latest value
     */
    std::optional<DataPoint> getLatest() const {
        std::shared_lock lock(mutex_);
        if (raw_data_.empty()) return std::nullopt;
        return raw_data_.back();
    }
    
    /**
     * Get statistics for time range
     */
    struct Statistics {
        MetricValue min = 0.0;
        MetricValue max = 0.0;
        MetricValue sum = 0.0;
        MetricValue average = 0.0;
        MetricValue stddev = 0.0;
        MetricValue p50 = 0.0;
        MetricValue p90 = 0.0;
        MetricValue p95 = 0.0;
        MetricValue p99 = 0.0;
        size_t count = 0;
    };
    
    Statistics getStatistics(Timestamp start, Timestamp end) const {
        auto data = getRawData(start, end);
        Statistics stats;
        
        if (data.empty()) return stats;
        
        stats.count = data.size();
        stats.min = std::numeric_limits<MetricValue>::max();
        stats.max = std::numeric_limits<MetricValue>::lowest();
        
        for (const auto& point : data) {
            stats.sum += point.value;
            stats.min = std::min(stats.min, point.value);
            stats.max = std::max(stats.max, point.value);
        }
        
        stats.average = stats.sum / static_cast<double>(stats.count);
        
        // Calculate standard deviation
        MetricValue variance = 0.0;
        for (const auto& point : data) {
            MetricValue diff = point.value - stats.average;
            variance += diff * diff;
        }
        stats.stddev = std::sqrt(variance / static_cast<double>(stats.count));
        
        // Calculate percentiles
        std::vector<MetricValue> values;
        values.reserve(data.size());
        for (const auto& point : data) {
            values.push_back(point.value);
        }
        std::sort(values.begin(), values.end());
        
        auto getPercentile = [&values](double p) -> MetricValue {
            size_t idx = static_cast<size_t>(p * (values.size() - 1));
            return values[idx];
        };
        
        stats.p50 = getPercentile(0.50);
        stats.p90 = getPercentile(0.90);
        stats.p95 = getPercentile(0.95);
        stats.p99 = getPercentile(0.99);
        
        return stats;
    }
    
    size_t getRawDataCount() const {
        std::shared_lock lock(mutex_);
        return raw_data_.size();
    }
    
private:
    void aggregateIfNeeded() {
        auto now = std::chrono::system_clock::now();
        
        // Aggregate to minutes
        auto minute_diff = std::chrono::duration_cast<std::chrono::minutes>(
            now - last_minute_aggregation_);
        if (minute_diff.count() >= 1) {
            aggregateToMinutes();
            last_minute_aggregation_ = now;
        }
        
        // Aggregate to hours
        auto hour_diff = std::chrono::duration_cast<std::chrono::hours>(
            now - last_hour_aggregation_);
        if (hour_diff.count() >= 1) {
            aggregateToHours();
            last_hour_aggregation_ = now;
        }
        
        // Aggregate to days
        auto day_diff = std::chrono::duration_cast<std::chrono::hours>(
            now - last_day_aggregation_);
        if (day_diff.count() >= 24) {
            aggregateToDays();
            last_day_aggregation_ = now;
        }
    }
    
    void aggregateToMinutes() {
        if (raw_data_.empty()) return;
        
        auto now = std::chrono::system_clock::now();
        auto minute_ago = now - std::chrono::minutes(1);
        
        AggregatedBucket bucket;
        bucket.start_time = minute_ago;
        bucket.end_time = now;
        bucket.granularity = TimeGranularity::MINUTE;
        
        for (const auto& point : raw_data_) {
            if (point.timestamp >= minute_ago && point.timestamp < now) {
                bucket.addValue(point.value);
            }
        }
        
        if (bucket.count > 0) {
            minute_data_.push_back(bucket);
            while (minute_data_.size() > max_minute_points) {
                minute_data_.pop_front();
            }
        }
    }
    
    void aggregateToHours() {
        if (minute_data_.empty()) return;
        
        auto now = std::chrono::system_clock::now();
        auto hour_ago = now - std::chrono::hours(1);
        
        AggregatedBucket bucket;
        bucket.start_time = hour_ago;
        bucket.end_time = now;
        bucket.granularity = TimeGranularity::HOUR;
        
        for (const auto& min_bucket : minute_data_) {
            if (min_bucket.start_time >= hour_ago && min_bucket.end_time <= now) {
                for (const auto& v : min_bucket.values) {
                    bucket.addValue(v);
                }
            }
        }
        
        if (bucket.count > 0) {
            hour_data_.push_back(bucket);
            while (hour_data_.size() > max_hour_points) {
                hour_data_.pop_front();
            }
        }
    }
    
    void aggregateToDays() {
        if (hour_data_.empty()) return;
        
        auto now = std::chrono::system_clock::now();
        auto day_ago = now - std::chrono::hours(24);
        
        AggregatedBucket bucket;
        bucket.start_time = day_ago;
        bucket.end_time = now;
        bucket.granularity = TimeGranularity::DAY;
        
        for (const auto& hour_bucket : hour_data_) {
            if (hour_bucket.start_time >= day_ago && hour_bucket.end_time <= now) {
                for (const auto& v : hour_bucket.values) {
                    bucket.addValue(v);
                }
            }
        }
        
        if (bucket.count > 0) {
            day_data_.push_back(bucket);
            while (day_data_.size() > max_day_points) {
                day_data_.pop_front();
            }
        }
    }
};

// ============================================================================
// Throughput Metrics
// ============================================================================

/**
 * Detailed throughput tracking for HSM operations
 */
class ThroughputMetrics {
public:
    struct OperationMetrics {
        std::atomic<uint64_t> total_bytes{0};
        std::atomic<uint64_t> total_operations{0};
        std::atomic<uint64_t> successful_operations{0};
        std::atomic<uint64_t> failed_operations{0};
        std::atomic<uint64_t> total_duration_ms{0};
        
        Timestamp window_start;
        std::atomic<uint64_t> window_bytes{0};
        std::atomic<uint64_t> window_operations{0};
        
        OperationMetrics() : window_start(std::chrono::system_clock::now()) {}
        
        void recordOperation(uint64_t bytes, uint64_t duration_ms, bool success) {
            total_bytes += bytes;
            total_operations++;
            total_duration_ms += duration_ms;
            
            if (success) {
                successful_operations++;
            } else {
                failed_operations++;
            }
            
            window_bytes += bytes;
            window_operations++;
        }
        
        double getAverageThroughputBytesPerSecond() const {
            if (total_duration_ms == 0) return 0.0;
            return static_cast<double>(total_bytes.load()) * 1000.0 / 
                   static_cast<double>(total_duration_ms.load());
        }
        
        double getAverageLatencyMs() const {
            if (total_operations == 0) return 0.0;
            return static_cast<double>(total_duration_ms.load()) / 
                   static_cast<double>(total_operations.load());
        }
        
        double getSuccessRate() const {
            if (total_operations == 0) return 1.0;
            return static_cast<double>(successful_operations.load()) / 
                   static_cast<double>(total_operations.load());
        }
        
        void resetWindow() {
            window_start = std::chrono::system_clock::now();
            window_bytes = 0;
            window_operations = 0;
        }
    };
    
private:
    OperationMetrics migration_metrics_;
    OperationMetrics recall_metrics_;
    OperationMetrics backup_metrics_;
    OperationMetrics recovery_metrics_;
    
    // Time series for detailed tracking
    std::unique_ptr<TimeSeries> migration_throughput_;
    std::unique_ptr<TimeSeries> recall_throughput_;
    std::unique_ptr<TimeSeries> backup_throughput_;
    std::unique_ptr<TimeSeries> recovery_throughput_;
    
public:
    ThroughputMetrics() {
        migration_throughput_ = std::make_unique<TimeSeries>(
            "migration_throughput", MetricType::BYTES_MIGRATED_PER_SECOND,
            "Migration throughput in bytes per second", "bytes/s");
        recall_throughput_ = std::make_unique<TimeSeries>(
            "recall_throughput", MetricType::BYTES_RECALLED_PER_SECOND,
            "Recall throughput in bytes per second", "bytes/s");
        backup_throughput_ = std::make_unique<TimeSeries>(
            "backup_throughput", MetricType::BYTES_BACKED_UP_PER_SECOND,
            "Backup throughput in bytes per second", "bytes/s");
        recovery_throughput_ = std::make_unique<TimeSeries>(
            "recovery_throughput", MetricType::BYTES_RECOVERED_PER_SECOND,
            "Recovery throughput in bytes per second", "bytes/s");
    }
    
    void recordMigration(uint64_t bytes, uint64_t duration_ms, bool success) {
        migration_metrics_.recordOperation(bytes, duration_ms, success);
        if (duration_ms > 0) {
            double throughput = static_cast<double>(bytes) * 1000.0 / 
                               static_cast<double>(duration_ms);
            migration_throughput_->addPoint(throughput);
        }
    }
    
    void recordRecall(uint64_t bytes, uint64_t duration_ms, bool success) {
        recall_metrics_.recordOperation(bytes, duration_ms, success);
        if (duration_ms > 0) {
            double throughput = static_cast<double>(bytes) * 1000.0 / 
                               static_cast<double>(duration_ms);
            recall_throughput_->addPoint(throughput);
        }
    }
    
    void recordBackup(uint64_t bytes, uint64_t duration_ms, bool success) {
        backup_metrics_.recordOperation(bytes, duration_ms, success);
        if (duration_ms > 0) {
            double throughput = static_cast<double>(bytes) * 1000.0 / 
                               static_cast<double>(duration_ms);
            backup_throughput_->addPoint(throughput);
        }
    }
    
    void recordRecovery(uint64_t bytes, uint64_t duration_ms, bool success) {
        recovery_metrics_.recordOperation(bytes, duration_ms, success);
        if (duration_ms > 0) {
            double throughput = static_cast<double>(bytes) * 1000.0 / 
                               static_cast<double>(duration_ms);
            recovery_throughput_->addPoint(throughput);
        }
    }
    
    const OperationMetrics& getMigrationMetrics() const { return migration_metrics_; }
    const OperationMetrics& getRecallMetrics() const { return recall_metrics_; }
    const OperationMetrics& getBackupMetrics() const { return backup_metrics_; }
    const OperationMetrics& getRecoveryMetrics() const { return recovery_metrics_; }
    
    TimeSeries* getMigrationThroughputSeries() { return migration_throughput_.get(); }
    TimeSeries* getRecallThroughputSeries() { return recall_throughput_.get(); }
    TimeSeries* getBackupThroughputSeries() { return backup_throughput_.get(); }
    TimeSeries* getRecoveryThroughputSeries() { return recovery_throughput_.get(); }
};

// ============================================================================
// Trend Analysis
// ============================================================================

/**
 * Trend analysis result
 */
struct TrendAnalysisResult {
    TrendDirection direction = TrendDirection::UNKNOWN;
    double slope = 0.0;                    // Rate of change per unit time
    double r_squared = 0.0;                // Coefficient of determination
    double confidence = 0.0;               // Confidence in the trend
    MetricValue predicted_next = 0.0;      // Predicted next value
    std::string description;
    
    bool isSignificant() const {
        return r_squared >= 0.7 && confidence >= 0.8;
    }
};

/**
 * Trend analyzer using linear regression and statistical analysis
 */
class TrendAnalyzer {
public:
    struct Config {
        Config() = default;
        size_t min_data_points = 10;           // Minimum points for analysis
        double significance_threshold = 0.05;  // p-value threshold
        double slope_threshold = 0.01;         // Min slope to consider trend
    };
    
private:
    Config config_;
    
public:
    TrendAnalyzer() : config_() {}
    explicit TrendAnalyzer(const Config& config) : config_(config) {}
    
    /**
     * Analyze trend in time series data
     */
    TrendAnalysisResult analyze(const std::vector<DataPoint>& data) const {
        TrendAnalysisResult result;
        
        if (data.size() < config_.min_data_points) {
            result.description = "Insufficient data points for trend analysis";
            return result;
        }
        
        // Convert timestamps to numeric values (seconds since first point)
        auto first_time = data.front().timestamp;
        std::vector<double> x_values, y_values;
        x_values.reserve(data.size());
        y_values.reserve(data.size());
        
        for (const auto& point : data) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                point.timestamp - first_time);
            x_values.push_back(static_cast<double>(duration.count()));
            y_values.push_back(point.value);
        }
        
        // Calculate linear regression using least squares
        double n = static_cast<double>(data.size());
        double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
        
        for (size_t i = 0; i < data.size(); ++i) {
            sum_x += x_values[i];
            sum_y += y_values[i];
            sum_xy += x_values[i] * y_values[i];
            sum_x2 += x_values[i] * x_values[i];
            sum_y2 += y_values[i] * y_values[i];
        }
        
        double denominator = n * sum_x2 - sum_x * sum_x;
        if (std::abs(denominator) < 1e-10) {
            result.description = "Cannot determine trend (constant x values)";
            return result;
        }
        
        result.slope = (n * sum_xy - sum_x * sum_y) / denominator;
        double intercept = (sum_y - result.slope * sum_x) / n;
        
        // Calculate R-squared
        double y_mean = sum_y / n;
        double ss_tot = 0, ss_res = 0;
        
        for (size_t i = 0; i < data.size(); ++i) {
            double predicted = result.slope * x_values[i] + intercept;
            ss_res += (y_values[i] - predicted) * (y_values[i] - predicted);
            ss_tot += (y_values[i] - y_mean) * (y_values[i] - y_mean);
        }
        
        result.r_squared = (ss_tot > 0) ? 1.0 - (ss_res / ss_tot) : 0.0;
        
        // Determine trend direction
        if (std::abs(result.slope) < config_.slope_threshold) {
            result.direction = TrendDirection::STABLE;
            result.description = "Trend is stable with minimal change";
        } else if (result.slope > 0) {
            result.direction = TrendDirection::INCREASING;
            result.description = "Trend is increasing at " + 
                std::to_string(result.slope * 3600) + " units per hour";
        } else {
            result.direction = TrendDirection::DECREASING;
            result.description = "Trend is decreasing at " + 
                std::to_string(std::abs(result.slope) * 3600) + " units per hour";
        }
        
        // Check for volatility
        double variance = ss_res / n;
        double stddev = std::sqrt(variance);
        double cv = (y_mean != 0) ? stddev / std::abs(y_mean) : 0;
        
        if (cv > 0.5 && result.r_squared < 0.5) {
            result.direction = TrendDirection::VOLATILE;
            result.description = "Data is highly volatile with coefficient of variation: " +
                std::to_string(cv * 100) + "%";
        }
        
        // Calculate confidence based on R-squared and sample size
        result.confidence = result.r_squared * (1.0 - 1.0 / n);
        
        // Predict next value
        double next_x = x_values.back() + 60.0;  // Predict 1 minute ahead
        result.predicted_next = result.slope * next_x + intercept;
        
        return result;
    }
    
    /**
     * Detect seasonality in data
     */
    struct SeasonalityResult {
        bool has_seasonality = false;
        Duration period;
        double strength = 0.0;
        std::string description;
    };
    
    SeasonalityResult detectSeasonality(const std::vector<DataPoint>& data) const {
        SeasonalityResult result;
        
        if (data.size() < 48) {  // Need at least 2 days of hourly data
            result.description = "Insufficient data for seasonality detection";
            return result;
        }
        
        // Simple autocorrelation analysis
        std::vector<double> values;
        values.reserve(data.size());
        for (const auto& p : data) {
            values.push_back(p.value);
        }
        
        double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        
        // Test common periods: hourly (60), daily (1440), weekly (10080) in minutes
        std::vector<size_t> test_lags = {60, 1440, 10080};
        
        for (size_t lag : test_lags) {
            if (lag >= values.size() / 2) continue;
            
            double autocorr = calculateAutocorrelation(values, mean, lag);
            
            if (autocorr > 0.7) {
                result.has_seasonality = true;
                result.period = Duration(lag * 60 * 1000);  // Convert to ms
                result.strength = autocorr;
                
                if (lag == 60) {
                    result.description = "Hourly seasonality detected";
                } else if (lag == 1440) {
                    result.description = "Daily seasonality detected";
                } else if (lag == 10080) {
                    result.description = "Weekly seasonality detected";
                }
                break;
            }
        }
        
        if (!result.has_seasonality) {
            result.description = "No significant seasonality detected";
        }
        
        return result;
    }
    
private:
    double calculateAutocorrelation(const std::vector<double>& values, 
                                   double mean, size_t lag) const {
        double numerator = 0, denominator = 0;
        
        for (size_t i = 0; i < values.size() - lag; ++i) {
            numerator += (values[i] - mean) * (values[i + lag] - mean);
        }
        
        for (const auto& v : values) {
            denominator += (v - mean) * (v - mean);
        }
        
        return (denominator > 0) ? numerator / denominator : 0;
    }
};

// ============================================================================
// Capacity Planning
// ============================================================================

/**
 * Capacity forecast result
 */
struct CapacityForecast {
    Timestamp forecast_date;
    MetricValue predicted_usage;
    MetricValue lower_bound;
    MetricValue upper_bound;
    ConfidenceLevel confidence;
    
    bool willExceedThreshold(MetricValue threshold) const {
        return predicted_usage > threshold;
    }
};

/**
 * Storage capacity summary
 */
struct CapacitySummary {
    std::string storage_tier;
    uint64_t total_capacity_bytes = 0;
    uint64_t used_capacity_bytes = 0;
    uint64_t free_capacity_bytes = 0;
    double utilization_percent = 0.0;
    
    // Growth metrics
    double daily_growth_bytes = 0.0;
    double weekly_growth_bytes = 0.0;
    double monthly_growth_bytes = 0.0;
    
    // Projections
    int days_until_80_percent = -1;
    int days_until_90_percent = -1;
    int days_until_full = -1;
    
    Timestamp last_updated;
};

/**
 * Capacity planner for storage forecasting
 */
class CapacityPlanner {
public:
    struct Config {
        Config() = default;
        std::vector<double> warning_thresholds = {0.70, 0.80, 0.90, 0.95};
        size_t forecast_days = 90;
        double growth_buffer_percent = 10.0;
    };
    
private:
    Config config_;
    TrendAnalyzer trend_analyzer_;
    
    mutable std::shared_mutex mutex_;
    std::map<std::string, std::unique_ptr<TimeSeries>> capacity_series_;
    std::map<std::string, CapacitySummary> capacity_summaries_;
    
public:
    CapacityPlanner() : config_() {}
    explicit CapacityPlanner(const Config& config) : config_(config) {}
    
    /**
     * Record storage capacity data point
     */
    void recordCapacity(const std::string& tier, uint64_t total, uint64_t used) {
        std::unique_lock lock(mutex_);
        
        // Get or create time series
        auto it = capacity_series_.find(tier);
        if (it == capacity_series_.end()) {
            auto series = std::make_unique<TimeSeries>(
                tier + "_capacity", MetricType::PRIMARY_UTILIZATION_PERCENT,
                "Capacity utilization for " + tier, "%");
            it = capacity_series_.emplace(tier, std::move(series)).first;
        }
        
        double utilization = (total > 0) ? 
            static_cast<double>(used) * 100.0 / static_cast<double>(total) : 0.0;
        it->second->addPoint(utilization);
        
        // Update summary
        auto& summary = capacity_summaries_[tier];
        summary.storage_tier = tier;
        summary.total_capacity_bytes = total;
        summary.used_capacity_bytes = used;
        summary.free_capacity_bytes = total - used;
        summary.utilization_percent = utilization;
        summary.last_updated = std::chrono::system_clock::now();
    }
    
    /**
     * Get capacity summary for a tier
     */
    std::optional<CapacitySummary> getCapacitySummary(const std::string& tier) const {
        std::shared_lock lock(mutex_);
        auto it = capacity_summaries_.find(tier);
        if (it == capacity_summaries_.end()) return std::nullopt;
        return it->second;
    }
    
    /**
     * Generate capacity forecast
     */
    std::vector<CapacityForecast> generateForecast(
        const std::string& tier,
        size_t days = 0) const {
        
        std::shared_lock lock(mutex_);
        std::vector<CapacityForecast> forecasts;
        
        size_t forecast_days = (days > 0) ? days : config_.forecast_days;
        
        auto series_it = capacity_series_.find(tier);
        if (series_it == capacity_series_.end()) return forecasts;
        
        auto summary_it = capacity_summaries_.find(tier);
        if (summary_it == capacity_summaries_.end()) return forecasts;
        
        // Get historical data
        auto now = std::chrono::system_clock::now();
        auto month_ago = now - std::chrono::hours(24 * 30);
        auto data = series_it->second->getRawData(month_ago, now);
        
        if (data.size() < 7) {
            // Not enough data - use simple projection
            for (size_t day = 1; day <= forecast_days; ++day) {
                CapacityForecast forecast;
                forecast.forecast_date = now + std::chrono::hours(24 * day);
                forecast.predicted_usage = summary_it->second.utilization_percent;
                forecast.lower_bound = forecast.predicted_usage * 0.95;
                forecast.upper_bound = forecast.predicted_usage * 1.05;
                forecast.confidence = ConfidenceLevel::LOW;
                forecasts.push_back(forecast);
            }
            return forecasts;
        }
        
        // Analyze trend
        auto trend = trend_analyzer_.analyze(data);
        
        // Generate forecasts
        double current = summary_it->second.utilization_percent;
        double slope_per_day = trend.slope * 86400;  // Convert to daily rate
        
        for (size_t day = 1; day <= forecast_days; ++day) {
            CapacityForecast forecast;
            forecast.forecast_date = now + std::chrono::hours(24 * day);
            
            forecast.predicted_usage = current + (slope_per_day * day);
            
            // Calculate confidence bounds based on R-squared
            double uncertainty = (1.0 - trend.r_squared) * day * 0.5;
            forecast.lower_bound = forecast.predicted_usage - uncertainty;
            forecast.upper_bound = forecast.predicted_usage + uncertainty;
            
            // Assign confidence level
            if (trend.r_squared >= 0.9 && day <= 7) {
                forecast.confidence = ConfidenceLevel::VERY_HIGH;
            } else if (trend.r_squared >= 0.7 && day <= 30) {
                forecast.confidence = ConfidenceLevel::HIGH;
            } else if (trend.r_squared >= 0.5 && day <= 60) {
                forecast.confidence = ConfidenceLevel::MEDIUM;
            } else {
                forecast.confidence = ConfidenceLevel::LOW;
            }
            
            forecasts.push_back(forecast);
        }
        
        return forecasts;
    }
    
    /**
     * Calculate days until threshold is reached
     */
    int daysUntilThreshold(const std::string& tier, double threshold_percent) const {
        std::shared_lock lock(mutex_);
        
        auto summary_it = capacity_summaries_.find(tier);
        if (summary_it == capacity_summaries_.end()) return -1;
        
        double current = summary_it->second.utilization_percent;
        if (current >= threshold_percent) return 0;
        
        auto series_it = capacity_series_.find(tier);
        if (series_it == capacity_series_.end()) return -1;
        
        auto now = std::chrono::system_clock::now();
        auto week_ago = now - std::chrono::hours(24 * 7);
        auto data = series_it->second->getRawData(week_ago, now);
        
        if (data.size() < 2) return -1;
        
        auto trend = trend_analyzer_.analyze(data);
        if (trend.slope <= 0) return -1;  // Not growing
        
        double remaining = threshold_percent - current;
        double slope_per_day = trend.slope * 86400;
        
        if (slope_per_day <= 0) return -1;
        
        return static_cast<int>(remaining / slope_per_day);
    }
    
    /**
     * Get capacity planning recommendations
     */
    struct Recommendation {
        std::string tier;
        std::string message;
        std::string action;
        AnomalySeverity severity;
    };
    
    std::vector<Recommendation> getRecommendations() const {
        std::shared_lock lock(mutex_);
        std::vector<Recommendation> recommendations;
        
        for (const auto& [tier, summary] : capacity_summaries_) {
            // Check current utilization
            for (size_t i = 0; i < config_.warning_thresholds.size(); ++i) {
                double threshold = config_.warning_thresholds[i] * 100;
                if (summary.utilization_percent >= threshold) {
                    Recommendation rec;
                    rec.tier = tier;
                    rec.message = tier + " utilization at " + 
                        std::to_string(static_cast<int>(summary.utilization_percent)) + "%";
                    
                    if (i >= 3) {
                        rec.severity = AnomalySeverity::EMERGENCY;
                        rec.action = "Immediate action required: Add capacity or migrate data";
                    } else if (i >= 2) {
                        rec.severity = AnomalySeverity::CRITICAL;
                        rec.action = "Plan capacity expansion within 1 week";
                    } else if (i >= 1) {
                        rec.severity = AnomalySeverity::WARNING;
                        rec.action = "Monitor closely; plan expansion within 1 month";
                    } else {
                        rec.severity = AnomalySeverity::INFO;
                        rec.action = "Consider planning future capacity expansion";
                    }
                    
                    recommendations.push_back(rec);
                    break;
                }
            }
            
            // Check growth rate
            int days_to_90 = daysUntilThreshold(tier, 90.0);
            if (days_to_90 >= 0 && days_to_90 <= 30) {
                Recommendation rec;
                rec.tier = tier;
                rec.message = tier + " projected to reach 90% in " + 
                    std::to_string(days_to_90) + " days";
                rec.severity = (days_to_90 <= 7) ? 
                    AnomalySeverity::CRITICAL : AnomalySeverity::WARNING;
                rec.action = "Begin capacity planning immediately";
                recommendations.push_back(rec);
            }
        }
        
        return recommendations;
    }
};

// ============================================================================
// Anomaly Detection
// ============================================================================

/**
 * Anomaly detection result
 */
struct Anomaly {
    Timestamp detected_at;
    std::string metric_name;
    MetricValue actual_value;
    MetricValue expected_value;
    double deviation_score;
    AnomalySeverity severity;
    std::string description;
    bool is_acknowledged = false;
    
    std::string getSeverityString() const {
        switch (severity) {
            case AnomalySeverity::INFO: return "INFO";
            case AnomalySeverity::WARNING: return "WARNING";
            case AnomalySeverity::CRITICAL: return "CRITICAL";
            case AnomalySeverity::EMERGENCY: return "EMERGENCY";
            default: return "UNKNOWN";
        }
    }
};

/**
 * Anomaly detector using statistical methods
 */
class AnomalyDetector {
public:
    struct Config {
        double warning_stddev = 2.0;     // Deviations for warning
        double critical_stddev = 3.0;    // Deviations for critical
        double emergency_stddev = 4.0;   // Deviations for emergency
        size_t baseline_window = 1000;   // Points for baseline calculation
        size_t min_baseline_points = 100;
    };
    
    using AnomalyCallback = std::function<void(const Anomaly&)>;
    
private:
    Config config_;
    AnomalyCallback callback_;
    
    mutable std::shared_mutex mutex_;
    std::deque<Anomaly> detected_anomalies_;
    size_t max_anomaly_history_ = 10000;
    
    // Baseline statistics per metric
    struct BaselineStats {
        double mean = 0.0;
        double stddev = 0.0;
        size_t sample_count = 0;
        std::deque<double> recent_values;
    };
    std::map<std::string, BaselineStats> baselines_;
    
public:
    AnomalyDetector() : config_() {}
    explicit AnomalyDetector(const Config& config) : config_(config) {}
    
    void setCallback(AnomalyCallback callback) {
        callback_ = std::move(callback);
    }
    
    /**
     * Check value against baseline and detect anomalies
     */
    std::optional<Anomaly> checkValue(
        const std::string& metric_name,
        MetricValue value) {
        
        std::unique_lock lock(mutex_);
        
        auto& baseline = baselines_[metric_name];
        
        // Add to baseline window
        baseline.recent_values.push_back(value);
        while (baseline.recent_values.size() > config_.baseline_window) {
            baseline.recent_values.pop_front();
        }
        
        // Update baseline statistics
        if (baseline.recent_values.size() >= config_.min_baseline_points) {
            updateBaseline(baseline);
            
            // Check for anomaly
            if (baseline.stddev > 0) {
                double z_score = std::abs(value - baseline.mean) / baseline.stddev;
                
                if (z_score >= config_.warning_stddev) {
                    Anomaly anomaly;
                    anomaly.detected_at = std::chrono::system_clock::now();
                    anomaly.metric_name = metric_name;
                    anomaly.actual_value = value;
                    anomaly.expected_value = baseline.mean;
                    anomaly.deviation_score = z_score;
                    
                    if (z_score >= config_.emergency_stddev) {
                        anomaly.severity = AnomalySeverity::EMERGENCY;
                    } else if (z_score >= config_.critical_stddev) {
                        anomaly.severity = AnomalySeverity::CRITICAL;
                    } else {
                        anomaly.severity = AnomalySeverity::WARNING;
                    }
                    
                    anomaly.description = metric_name + " value " + 
                        std::to_string(value) + " is " + 
                        std::to_string(z_score) + " standard deviations from mean " +
                        std::to_string(baseline.mean);
                    
                    detected_anomalies_.push_back(anomaly);
                    while (detected_anomalies_.size() > max_anomaly_history_) {
                        detected_anomalies_.pop_front();
                    }
                    
                    if (callback_) {
                        lock.unlock();
                        callback_(anomaly);
                    }
                    
                    return anomaly;
                }
            }
        }
        
        return std::nullopt;
    }
    
    /**
     * Get recent anomalies
     */
    std::vector<Anomaly> getRecentAnomalies(size_t count = 100) const {
        std::shared_lock lock(mutex_);
        std::vector<Anomaly> result;
        
        size_t start = (detected_anomalies_.size() > count) ? 
            detected_anomalies_.size() - count : 0;
        
        for (size_t i = start; i < detected_anomalies_.size(); ++i) {
            result.push_back(detected_anomalies_[i]);
        }
        
        return result;
    }
    
    /**
     * Get anomalies by severity
     */
    std::vector<Anomaly> getAnomaliesBySeverity(AnomalySeverity min_severity) const {
        std::shared_lock lock(mutex_);
        std::vector<Anomaly> result;
        
        for (const auto& anomaly : detected_anomalies_) {
            if (anomaly.severity >= min_severity) {
                result.push_back(anomaly);
            }
        }
        
        return result;
    }
    
    /**
     * Acknowledge an anomaly
     */
    void acknowledgeAnomaly(size_t index) {
        std::unique_lock lock(mutex_);
        if (index < detected_anomalies_.size()) {
            detected_anomalies_[index].is_acknowledged = true;
        }
    }
    
    /**
     * Get unacknowledged anomaly count
     */
    size_t getUnacknowledgedCount() const {
        std::shared_lock lock(mutex_);
        return std::count_if(detected_anomalies_.begin(), detected_anomalies_.end(),
            [](const Anomaly& a) { return !a.is_acknowledged; });
    }
    
private:
    void updateBaseline(BaselineStats& baseline) {
        if (baseline.recent_values.empty()) return;
        
        double sum = 0;
        for (double v : baseline.recent_values) {
            sum += v;
        }
        baseline.mean = sum / baseline.recent_values.size();
        
        double variance = 0;
        for (double v : baseline.recent_values) {
            double diff = v - baseline.mean;
            variance += diff * diff;
        }
        baseline.stddev = std::sqrt(variance / baseline.recent_values.size());
        baseline.sample_count = baseline.recent_values.size();
    }
};

// ============================================================================
// SLA Tracking
// ============================================================================

/**
 * SLA definition
 */
struct SLADefinition {
    std::string name;
    std::string metric_name;
    MetricValue target_value;
    bool is_maximum;  // true = target is max (latency), false = target is min (throughput)
    double required_compliance_percent = 99.0;
    
    SLADefinition() = default;
    
    SLADefinition(const std::string& n, const std::string& m, 
                  MetricValue target, bool is_max, double compliance = 99.0)
        : name(n), metric_name(m), target_value(target), 
          is_maximum(is_max), required_compliance_percent(compliance) {}
};

/**
 * SLA compliance result
 */
struct SLAComplianceResult {
    std::string sla_name;
    double compliance_percent = 0.0;
    size_t total_samples = 0;
    size_t compliant_samples = 0;
    size_t violation_samples = 0;
    MetricValue worst_violation;
    Timestamp worst_violation_time;
    bool meets_sla = false;
    
    std::string getStatusString() const {
        return meets_sla ? "COMPLIANT" : "VIOLATION";
    }
};

/**
 * SLA compliance tracker
 */
class SLATracker {
private:
    mutable std::shared_mutex mutex_;
    std::map<std::string, SLADefinition> sla_definitions_;
    std::map<std::string, std::deque<std::pair<Timestamp, MetricValue>>> sla_samples_;
    size_t max_samples_per_sla_ = 86400;  // 24 hours at 1 sample/second
    
public:
    /**
     * Define an SLA
     */
    void defineSLA(const SLADefinition& sla) {
        std::unique_lock lock(mutex_);
        sla_definitions_[sla.name] = sla;
    }
    
    /**
     * Record a metric sample for SLA tracking
     */
    void recordSample(const std::string& metric_name, MetricValue value) {
        std::unique_lock lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        
        // Find all SLAs that track this metric
        for (const auto& [sla_name, sla] : sla_definitions_) {
            if (sla.metric_name == metric_name) {
                auto& samples = sla_samples_[sla_name];
                samples.emplace_back(now, value);
                
                while (samples.size() > max_samples_per_sla_) {
                    samples.pop_front();
                }
            }
        }
    }
    
    /**
     * Calculate SLA compliance
     */
    SLAComplianceResult calculateCompliance(const std::string& sla_name) const {
        std::shared_lock lock(mutex_);
        SLAComplianceResult result;
        result.sla_name = sla_name;
        
        auto sla_it = sla_definitions_.find(sla_name);
        if (sla_it == sla_definitions_.end()) {
            return result;
        }
        
        const auto& sla = sla_it->second;
        
        auto samples_it = sla_samples_.find(sla_name);
        if (samples_it == sla_samples_.end() || samples_it->second.empty()) {
            return result;
        }
        
        const auto& samples = samples_it->second;
        result.total_samples = samples.size();
        result.worst_violation = sla.is_maximum ? 
            std::numeric_limits<MetricValue>::lowest() :
            std::numeric_limits<MetricValue>::max();
        
        for (const auto& [timestamp, value] : samples) {
            bool compliant = sla.is_maximum ? 
                (value <= sla.target_value) : (value >= sla.target_value);
            
            if (compliant) {
                result.compliant_samples++;
            } else {
                result.violation_samples++;
                
                // Track worst violation
                bool is_worse = sla.is_maximum ?
                    (value > result.worst_violation) :
                    (value < result.worst_violation);
                
                if (is_worse) {
                    result.worst_violation = value;
                    result.worst_violation_time = timestamp;
                }
            }
        }
        
        result.compliance_percent = 
            static_cast<double>(result.compliant_samples) * 100.0 / 
            static_cast<double>(result.total_samples);
        
        result.meets_sla = result.compliance_percent >= sla.required_compliance_percent;
        
        return result;
    }
    
    /**
     * Get all SLA compliance results
     */
    std::vector<SLAComplianceResult> getAllCompliance() const {
        std::shared_lock lock(mutex_);
        std::vector<SLAComplianceResult> results;
        
        for (const auto& [sla_name, _] : sla_definitions_) {
            lock.unlock();
            results.push_back(calculateCompliance(sla_name));
            lock.lock();
        }
        
        return results;
    }
    
    /**
     * Get SLA definitions
     */
    std::vector<SLADefinition> getSLADefinitions() const {
        std::shared_lock lock(mutex_);
        std::vector<SLADefinition> result;
        for (const auto& [_, sla] : sla_definitions_) {
            result.push_back(sla);
        }
        return result;
    }
};

// ============================================================================
// Report Generator
// ============================================================================

/**
 * Statistical report types
 */
enum class ReportType {
    DAILY_SUMMARY,
    WEEKLY_SUMMARY,
    MONTHLY_SUMMARY,
    CAPACITY_FORECAST,
    PERFORMANCE_ANALYSIS,
    SLA_COMPLIANCE,
    ANOMALY_REPORT,
    TREND_ANALYSIS,
    EXECUTIVE_DASHBOARD
};

/**
 * Report generation options
 */
struct ReportOptions {
    ReportType type = ReportType::DAILY_SUMMARY;
    Timestamp start_time;
    Timestamp end_time;
    std::vector<std::string> include_metrics;
    std::vector<std::string> include_tiers;
    bool include_charts = false;
    std::string format = "text";  // text, json, html, csv
};

/**
 * Report generator
 */
class ReportGenerator {
private:
    ThroughputMetrics* throughput_metrics_;
    CapacityPlanner* capacity_planner_;
    AnomalyDetector* anomaly_detector_;
    SLATracker* sla_tracker_;
    TrendAnalyzer trend_analyzer_;
    
public:
    ReportGenerator(ThroughputMetrics* throughput,
                   CapacityPlanner* capacity,
                   AnomalyDetector* anomaly,
                   SLATracker* sla)
        : throughput_metrics_(throughput),
          capacity_planner_(capacity),
          anomaly_detector_(anomaly),
          sla_tracker_(sla) {}
    
    /**
     * Generate report
     */
    std::string generateReport(const ReportOptions& options) const {
        std::ostringstream report;
        
        switch (options.type) {
            case ReportType::DAILY_SUMMARY:
                generateDailySummary(report, options);
                break;
            case ReportType::CAPACITY_FORECAST:
                generateCapacityForecast(report, options);
                break;
            case ReportType::SLA_COMPLIANCE:
                generateSLACompliance(report, options);
                break;
            case ReportType::ANOMALY_REPORT:
                generateAnomalyReport(report, options);
                break;
            case ReportType::PERFORMANCE_ANALYSIS:
                generatePerformanceAnalysis(report, options);
                break;
            default:
                generateDailySummary(report, options);
        }
        
        return report.str();
    }
    
private:
    void generateDailySummary(std::ostringstream& report, 
                             [[maybe_unused]] const ReportOptions& options) const {
        report << "========================================\n";
        report << "         HSM Daily Summary Report\n";
        report << "========================================\n\n";
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        report << "Generated: " << std::ctime(&time_t) << "\n";
        
        // Throughput Summary
        if (throughput_metrics_) {
            report << "\n-- Throughput Summary --\n\n";
            
            const auto& mig = throughput_metrics_->getMigrationMetrics();
            report << "Migrations:\n";
            report << "  Total Operations: " << mig.total_operations.load() << "\n";
            report << "  Total Bytes: " << formatBytes(mig.total_bytes.load()) << "\n";
            report << "  Avg Throughput: " << formatBytes(
                static_cast<uint64_t>(mig.getAverageThroughputBytesPerSecond())) << "/s\n";
            report << "  Avg Latency: " << mig.getAverageLatencyMs() << " ms\n";
            report << "  Success Rate: " << (mig.getSuccessRate() * 100) << "%\n\n";
            
            const auto& rec = throughput_metrics_->getRecallMetrics();
            report << "Recalls:\n";
            report << "  Total Operations: " << rec.total_operations.load() << "\n";
            report << "  Total Bytes: " << formatBytes(rec.total_bytes.load()) << "\n";
            report << "  Avg Throughput: " << formatBytes(
                static_cast<uint64_t>(rec.getAverageThroughputBytesPerSecond())) << "/s\n";
            report << "  Avg Latency: " << rec.getAverageLatencyMs() << " ms\n";
            report << "  Success Rate: " << (rec.getSuccessRate() * 100) << "%\n\n";
            
            const auto& bak = throughput_metrics_->getBackupMetrics();
            report << "Backups:\n";
            report << "  Total Operations: " << bak.total_operations.load() << "\n";
            report << "  Total Bytes: " << formatBytes(bak.total_bytes.load()) << "\n";
            report << "  Avg Throughput: " << formatBytes(
                static_cast<uint64_t>(bak.getAverageThroughputBytesPerSecond())) << "/s\n";
            report << "  Success Rate: " << (bak.getSuccessRate() * 100) << "%\n";
        }
        
        // Capacity Summary
        if (capacity_planner_) {
            report << "\n-- Capacity Summary --\n\n";
            auto recommendations = capacity_planner_->getRecommendations();
            for (const auto& rec : recommendations) {
                report << "  [" << getSeverityString(rec.severity) << "] " 
                       << rec.message << "\n";
                report << "    Action: " << rec.action << "\n";
            }
            if (recommendations.empty()) {
                report << "  All storage tiers operating within normal parameters.\n";
            }
        }
        
        // Anomaly Summary
        if (anomaly_detector_) {
            report << "\n-- Anomaly Summary --\n\n";
            size_t unack = anomaly_detector_->getUnacknowledgedCount();
            report << "  Unacknowledged anomalies: " << unack << "\n";
            
            auto critical = anomaly_detector_->getAnomaliesBySeverity(
                AnomalySeverity::CRITICAL);
            if (!critical.empty()) {
                report << "  Critical/Emergency anomalies: " << critical.size() << "\n";
                for (size_t i = 0; i < std::min(critical.size(), size_t(5)); ++i) {
                    report << "    - " << critical[i].description << "\n";
                }
            }
        }
    }
    
    void generateCapacityForecast(std::ostringstream& report,
                                  const ReportOptions& options) const {
        report << "========================================\n";
        report << "       Capacity Forecast Report\n";
        report << "========================================\n\n";
        
        if (!capacity_planner_) {
            report << "No capacity data available.\n";
            return;
        }
        
        for (const auto& tier : options.include_tiers) {
            auto summary = capacity_planner_->getCapacitySummary(tier);
            if (!summary) continue;
            
            report << "-- " << tier << " --\n\n";
            report << "Current Utilization: " 
                   << std::fixed << std::setprecision(1) 
                   << summary->utilization_percent << "%\n";
            report << "Used: " << formatBytes(summary->used_capacity_bytes) 
                   << " / " << formatBytes(summary->total_capacity_bytes) << "\n\n";
            
            auto forecasts = capacity_planner_->generateForecast(tier, 30);
            report << "30-Day Forecast:\n";
            for (size_t i = 0; i < forecasts.size(); i += 7) {
                const auto& f = forecasts[i];
                auto time_t = std::chrono::system_clock::to_time_t(f.forecast_date);
                std::tm* tm = std::localtime(&time_t);
                char buf[32];
                std::strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
                
                report << "  " << buf << ": " 
                       << std::fixed << std::setprecision(1) << f.predicted_usage << "%"
                       << " (" << f.lower_bound << "% - " << f.upper_bound << "%)\n";
            }
            report << "\n";
        }
    }
    
    void generateSLACompliance(std::ostringstream& report,
                              [[maybe_unused]] const ReportOptions& options) const {
        report << "========================================\n";
        report << "       SLA Compliance Report\n";
        report << "========================================\n\n";
        
        if (!sla_tracker_) {
            report << "No SLA data available.\n";
            return;
        }
        
        auto results = sla_tracker_->getAllCompliance();
        
        size_t compliant = std::count_if(results.begin(), results.end(),
            [](const auto& r) { return r.meets_sla; });
        
        report << "Overall Status: " << compliant << "/" << results.size() 
               << " SLAs meeting target\n\n";
        
        for (const auto& result : results) {
            report << "SLA: " << result.sla_name << "\n";
            report << "  Status: " << result.getStatusString() << "\n";
            report << "  Compliance: " << std::fixed << std::setprecision(2) 
                   << result.compliance_percent << "%\n";
            report << "  Samples: " << result.total_samples 
                   << " (Compliant: " << result.compliant_samples 
                   << ", Violations: " << result.violation_samples << ")\n";
            if (result.violation_samples > 0) {
                report << "  Worst Violation: " << result.worst_violation << "\n";
            }
            report << "\n";
        }
    }
    
    void generateAnomalyReport(std::ostringstream& report,
                              [[maybe_unused]] const ReportOptions& options) const {
        report << "========================================\n";
        report << "          Anomaly Report\n";
        report << "========================================\n\n";
        
        if (!anomaly_detector_) {
            report << "No anomaly data available.\n";
            return;
        }
        
        auto anomalies = anomaly_detector_->getRecentAnomalies(100);
        
        // Group by severity
        std::map<AnomalySeverity, std::vector<const Anomaly*>> by_severity;
        for (const auto& a : anomalies) {
            by_severity[a.severity].push_back(&a);
        }
        
        for (auto severity : {AnomalySeverity::EMERGENCY, AnomalySeverity::CRITICAL,
                              AnomalySeverity::WARNING, AnomalySeverity::INFO}) {
            auto it = by_severity.find(severity);
            if (it == by_severity.end() || it->second.empty()) continue;
            
            report << "-- " << getSeverityString(severity) << " --\n\n";
            
            for (const auto* a : it->second) {
                auto time_t = std::chrono::system_clock::to_time_t(a->detected_at);
                report << "  [" << std::ctime(&time_t);
                report.seekp(-1, std::ios_base::cur);  // Remove newline from ctime
                report << "] " << a->description << "\n";
                report << "    Acknowledged: " << (a->is_acknowledged ? "Yes" : "No") << "\n";
            }
            report << "\n";
        }
    }
    
    void generatePerformanceAnalysis(std::ostringstream& report,
                                    [[maybe_unused]] const ReportOptions& options) const {
        report << "========================================\n";
        report << "      Performance Analysis Report\n";
        report << "========================================\n\n";
        
        if (!throughput_metrics_) {
            report << "No throughput data available.\n";
            return;
        }
        
        auto now = std::chrono::system_clock::now();
        auto day_ago = now - std::chrono::hours(24);
        
        // Analyze each operation type
        struct AnalysisTarget {
            std::string name;
            TimeSeries* series;
        };
        
        std::vector<AnalysisTarget> targets = {
            {"Migration", throughput_metrics_->getMigrationThroughputSeries()},
            {"Recall", throughput_metrics_->getRecallThroughputSeries()},
            {"Backup", throughput_metrics_->getBackupThroughputSeries()},
            {"Recovery", throughput_metrics_->getRecoveryThroughputSeries()}
        };
        
        for (const auto& target : targets) {
            if (!target.series) continue;
            
            auto data = target.series->getRawData(day_ago, now);
            if (data.empty()) continue;
            
            auto stats = target.series->getStatistics(day_ago, now);
            auto trend = trend_analyzer_.analyze(data);
            
            report << "-- " << target.name << " Throughput --\n\n";
            report << "  Sample Count: " << stats.count << "\n";
            report << "  Average: " << formatBytes(static_cast<uint64_t>(stats.average)) << "/s\n";
            report << "  Minimum: " << formatBytes(static_cast<uint64_t>(stats.min)) << "/s\n";
            report << "  Maximum: " << formatBytes(static_cast<uint64_t>(stats.max)) << "/s\n";
            report << "  Std Dev: " << formatBytes(static_cast<uint64_t>(stats.stddev)) << "/s\n";
            report << "  P50: " << formatBytes(static_cast<uint64_t>(stats.p50)) << "/s\n";
            report << "  P90: " << formatBytes(static_cast<uint64_t>(stats.p90)) << "/s\n";
            report << "  P99: " << formatBytes(static_cast<uint64_t>(stats.p99)) << "/s\n\n";
            
            report << "  Trend: " << trend.description << "\n";
            report << "  R-squared: " << trend.r_squared << "\n\n";
        }
    }
    
    std::string formatBytes(uint64_t bytes) const {
        const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
        int unit_idx = 0;
        double value = static_cast<double>(bytes);
        
        while (value >= 1024 && unit_idx < 5) {
            value /= 1024;
            unit_idx++;
        }
        
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << value << " " << units[unit_idx];
        return ss.str();
    }
    
    std::string getSeverityString(AnomalySeverity severity) const {
        switch (severity) {
            case AnomalySeverity::INFO: return "INFO";
            case AnomalySeverity::WARNING: return "WARNING";
            case AnomalySeverity::CRITICAL: return "CRITICAL";
            case AnomalySeverity::EMERGENCY: return "EMERGENCY";
            default: return "UNKNOWN";
        }
    }
};

// ============================================================================
// Statistics Manager - Main Entry Point
// ============================================================================

/**
 * Central statistics management class
 */
class StatisticsManager {
public:
    struct Config {
        Config() = default;
        TrendAnalyzer::Config trend_config;
        CapacityPlanner::Config capacity_config;
        AnomalyDetector::Config anomaly_config;
        
        bool enable_auto_aggregation = true;
        bool enable_anomaly_detection = true;
        bool enable_sla_tracking = true;
        
        size_t metric_retention_days = 30;
    };
    
private:
    Config config_;
    
    std::unique_ptr<ThroughputMetrics> throughput_metrics_;
    std::unique_ptr<TrendAnalyzer> trend_analyzer_;
    std::unique_ptr<CapacityPlanner> capacity_planner_;
    std::unique_ptr<AnomalyDetector> anomaly_detector_;
    std::unique_ptr<SLATracker> sla_tracker_;
    std::unique_ptr<ReportGenerator> report_generator_;
    
    mutable std::shared_mutex mutex_;
    std::map<std::string, std::unique_ptr<TimeSeries>> custom_metrics_;
    
    bool running_ = false;
    
public:
    StatisticsManager() : config_() {
        throughput_metrics_ = std::make_unique<ThroughputMetrics>();
        trend_analyzer_ = std::make_unique<TrendAnalyzer>();
        capacity_planner_ = std::make_unique<CapacityPlanner>();
        anomaly_detector_ = std::make_unique<AnomalyDetector>();
        sla_tracker_ = std::make_unique<SLATracker>();
        report_generator_ = std::make_unique<ReportGenerator>(
            throughput_metrics_.get(),
            capacity_planner_.get(),
            anomaly_detector_.get(),
            sla_tracker_.get()
        );
    }
    
    explicit StatisticsManager(const Config& config) 
        : config_(config) {
        
        throughput_metrics_ = std::make_unique<ThroughputMetrics>();
        trend_analyzer_ = std::make_unique<TrendAnalyzer>(config.trend_config);
        capacity_planner_ = std::make_unique<CapacityPlanner>(config.capacity_config);
        
        if (config.enable_anomaly_detection) {
            anomaly_detector_ = std::make_unique<AnomalyDetector>(config.anomaly_config);
        }
        
        if (config.enable_sla_tracking) {
            sla_tracker_ = std::make_unique<SLATracker>();
        }
        
        report_generator_ = std::make_unique<ReportGenerator>(
            throughput_metrics_.get(),
            capacity_planner_.get(),
            anomaly_detector_.get(),
            sla_tracker_.get());
    }
    
    // -- Throughput Tracking --
    
    void recordMigration(uint64_t bytes, uint64_t duration_ms, bool success = true) {
        throughput_metrics_->recordMigration(bytes, duration_ms, success);
        
        if (anomaly_detector_ && duration_ms > 0) {
            double latency = static_cast<double>(duration_ms);
            anomaly_detector_->checkValue("migration_latency", latency);
        }
        
        if (sla_tracker_ && duration_ms > 0) {
            sla_tracker_->recordSample("migration_latency", 
                static_cast<double>(duration_ms));
        }
    }
    
    void recordRecall(uint64_t bytes, uint64_t duration_ms, bool success = true) {
        throughput_metrics_->recordRecall(bytes, duration_ms, success);
        
        if (anomaly_detector_ && duration_ms > 0) {
            anomaly_detector_->checkValue("recall_latency", 
                static_cast<double>(duration_ms));
        }
        
        if (sla_tracker_ && duration_ms > 0) {
            sla_tracker_->recordSample("recall_latency", 
                static_cast<double>(duration_ms));
        }
    }
    
    void recordBackup(uint64_t bytes, uint64_t duration_ms, bool success = true) {
        throughput_metrics_->recordBackup(bytes, duration_ms, success);
    }
    
    void recordRecovery(uint64_t bytes, uint64_t duration_ms, bool success = true) {
        throughput_metrics_->recordRecovery(bytes, duration_ms, success);
    }
    
    // -- Capacity Tracking --
    
    void recordCapacity(const std::string& tier, uint64_t total, uint64_t used) {
        capacity_planner_->recordCapacity(tier, total, used);
        
        if (anomaly_detector_) {
            double utilization = (total > 0) ? 
                static_cast<double>(used) * 100.0 / static_cast<double>(total) : 0.0;
            anomaly_detector_->checkValue(tier + "_utilization", utilization);
        }
    }
    
    // -- Custom Metrics --
    
    void recordMetric(const std::string& name, MetricValue value) {
        std::unique_lock lock(mutex_);
        
        auto it = custom_metrics_.find(name);
        if (it == custom_metrics_.end()) {
            auto series = std::make_unique<TimeSeries>(
                name, MetricType::CUSTOM, "", "");
            it = custom_metrics_.emplace(name, std::move(series)).first;
        }
        it->second->addPoint(value);
        
        if (anomaly_detector_) {
            lock.unlock();
            anomaly_detector_->checkValue(name, value);
        }
    }
    
    TimeSeries* getCustomMetric(const std::string& name) {
        std::shared_lock lock(mutex_);
        auto it = custom_metrics_.find(name);
        return (it != custom_metrics_.end()) ? it->second.get() : nullptr;
    }
    
    // -- SLA Management --
    
    void defineSLA(const SLADefinition& sla) {
        if (sla_tracker_) {
            sla_tracker_->defineSLA(sla);
        }
    }
    
    std::optional<SLAComplianceResult> getSLACompliance(const std::string& name) const {
        if (!sla_tracker_) return std::nullopt;
        return sla_tracker_->calculateCompliance(name);
    }
    
    std::vector<SLAComplianceResult> getAllSLACompliance() const {
        if (!sla_tracker_) return {};
        return sla_tracker_->getAllCompliance();
    }
    
    // -- Trend Analysis --
    
    TrendAnalysisResult analyzeTrend(const std::string& metric_name) const {
        std::shared_lock lock(mutex_);
        
        auto it = custom_metrics_.find(metric_name);
        if (it == custom_metrics_.end()) {
            return TrendAnalysisResult{};
        }
        
        auto now = std::chrono::system_clock::now();
        auto week_ago = now - std::chrono::hours(24 * 7);
        auto data = it->second->getRawData(week_ago, now);
        
        return trend_analyzer_->analyze(data);
    }
    
    // -- Capacity Planning --
    
    std::optional<CapacitySummary> getCapacitySummary(const std::string& tier) const {
        return capacity_planner_->getCapacitySummary(tier);
    }
    
    std::vector<CapacityForecast> getCapacityForecast(
        const std::string& tier, size_t days = 90) const {
        return capacity_planner_->generateForecast(tier, days);
    }
    
    std::vector<CapacityPlanner::Recommendation> getCapacityRecommendations() const {
        return capacity_planner_->getRecommendations();
    }
    
    // -- Anomaly Detection --
    
    void setAnomalyCallback(AnomalyDetector::AnomalyCallback callback) {
        if (anomaly_detector_) {
            anomaly_detector_->setCallback(std::move(callback));
        }
    }
    
    std::vector<Anomaly> getRecentAnomalies(size_t count = 100) const {
        if (!anomaly_detector_) return {};
        return anomaly_detector_->getRecentAnomalies(count);
    }
    
    size_t getUnacknowledgedAnomalyCount() const {
        if (!anomaly_detector_) return 0;
        return anomaly_detector_->getUnacknowledgedCount();
    }
    
    // -- Reporting --
    
    std::string generateReport(const ReportOptions& options) const {
        return report_generator_->generateReport(options);
    }
    
    std::string generateDailyReport() const {
        ReportOptions options;
        options.type = ReportType::DAILY_SUMMARY;
        return generateReport(options);
    }
    
    std::string generateCapacityReport(const std::vector<std::string>& tiers) const {
        ReportOptions options;
        options.type = ReportType::CAPACITY_FORECAST;
        options.include_tiers = tiers;
        return generateReport(options);
    }
    
    // -- Access to Components --
    
    ThroughputMetrics* getThroughputMetrics() { return throughput_metrics_.get(); }
    CapacityPlanner* getCapacityPlanner() { return capacity_planner_.get(); }
    AnomalyDetector* getAnomalyDetector() { return anomaly_detector_.get(); }
    SLATracker* getSLATracker() { return sla_tracker_.get(); }
    TrendAnalyzer* getTrendAnalyzer() { return trend_analyzer_.get(); }
    
    // -- Statistics Export --
    
    struct ExportData {
        std::map<std::string, std::vector<DataPoint>> metrics;
        std::vector<SLAComplianceResult> sla_results;
        std::vector<Anomaly> anomalies;
        std::vector<CapacityPlanner::Recommendation> recommendations;
    };
    
    ExportData exportData() const {
        ExportData data;
        
        // Export custom metrics
        {
            std::shared_lock lock(mutex_);
            auto now = std::chrono::system_clock::now();
            auto day_ago = now - std::chrono::hours(24);
            
            for (const auto& [name, series] : custom_metrics_) {
                data.metrics[name] = series->getRawData(day_ago, now);
            }
        }
        
        // Export SLA results
        data.sla_results = getAllSLACompliance();
        
        // Export anomalies
        data.anomalies = getRecentAnomalies();
        
        // Export recommendations
        data.recommendations = getCapacityRecommendations();
        
        return data;
    }
    
    /**
     * Export to JSON format
     */
    std::string exportToJSON() const {
        std::ostringstream json;
        json << "{\n";
        
        // Throughput summary
        json << "  \"throughput\": {\n";
        const auto& mig = throughput_metrics_->getMigrationMetrics();
        json << "    \"migration\": {\n";
        json << "      \"total_operations\": " << mig.total_operations.load() << ",\n";
        json << "      \"total_bytes\": " << mig.total_bytes.load() << ",\n";
        json << "      \"success_rate\": " << mig.getSuccessRate() << ",\n";
        json << "      \"avg_latency_ms\": " << mig.getAverageLatencyMs() << "\n";
        json << "    },\n";
        
        const auto& rec = throughput_metrics_->getRecallMetrics();
        json << "    \"recall\": {\n";
        json << "      \"total_operations\": " << rec.total_operations.load() << ",\n";
        json << "      \"total_bytes\": " << rec.total_bytes.load() << ",\n";
        json << "      \"success_rate\": " << rec.getSuccessRate() << ",\n";
        json << "      \"avg_latency_ms\": " << rec.getAverageLatencyMs() << "\n";
        json << "    }\n";
        json << "  },\n";
        
        // Anomaly count
        json << "  \"anomalies\": {\n";
        json << "    \"unacknowledged\": " << getUnacknowledgedAnomalyCount() << "\n";
        json << "  },\n";
        
        // SLA summary
        json << "  \"sla_compliance\": [\n";
        auto sla_results = getAllSLACompliance();
        for (size_t i = 0; i < sla_results.size(); ++i) {
            const auto& r = sla_results[i];
            json << "    {\n";
            json << "      \"name\": \"" << r.sla_name << "\",\n";
            json << "      \"compliant\": " << (r.meets_sla ? "true" : "false") << ",\n";
            json << "      \"compliance_percent\": " << r.compliance_percent << "\n";
            json << "    }" << (i + 1 < sla_results.size() ? "," : "") << "\n";
        }
        json << "  ]\n";
        
        json << "}\n";
        return json.str();
    }
};

} // namespace statistics
} // namespace hsm

#endif // HSM_EXTENDED_STATISTICS_HPP
