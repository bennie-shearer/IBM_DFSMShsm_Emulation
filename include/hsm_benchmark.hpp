/*******************************************************************************
 * DFSMShsm Emulator - Performance Benchmarking System
 * @version 4.2.0
 * 
 * Provides built-in benchmarking tools for measuring and analyzing
 * performance of HSM operations including migration, recall, compression,
 * and I/O operations.
 * 
 * Features:
 * - High-resolution timing measurements
 * - Statistical analysis (mean, median, std dev, percentiles)
 * - Throughput and IOPS calculations
 * - Memory usage tracking
 * - Benchmark result export (JSON, CSV)
 * - Comparison between benchmark runs
 * - Automated performance regression detection
 * 
 * Author: DFSMShsm Emulator Team
 * License: MIT
 ******************************************************************************/

#ifndef HSM_BENCHMARK_HPP
#define HSM_BENCHMARK_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <functional>
#include <mutex>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace dfsmshsm {
namespace benchmark {

// ============================================================================
// Timing Utilities
// ============================================================================

using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;
using Duration = std::chrono::nanoseconds;

inline double toMilliseconds(Duration d) {
    return std::chrono::duration<double, std::milli>(d).count();
}

inline double toMicroseconds(Duration d) {
    return std::chrono::duration<double, std::micro>(d).count();
}

inline double toSeconds(Duration d) {
    return std::chrono::duration<double>(d).count();
}

// ============================================================================
// Benchmark Sample
// ============================================================================

struct BenchmarkSample {
    Duration elapsed_time;
    size_t bytes_processed;
    size_t operations_count;
    size_t memory_used;
    std::map<std::string, double> custom_metrics;
    
    BenchmarkSample()
        : elapsed_time(Duration::zero())
        , bytes_processed(0)
        , operations_count(1)
        , memory_used(0) {}
    
    double throughputMBps() const {
        double seconds = toSeconds(elapsed_time);
        if (seconds <= 0) return 0;
        return (bytes_processed / (1024.0 * 1024.0)) / seconds;
    }
    
    double operationsPerSecond() const {
        double seconds = toSeconds(elapsed_time);
        if (seconds <= 0) return 0;
        return operations_count / seconds;
    }
};

// ============================================================================
// Statistical Analysis
// ============================================================================

struct Statistics {
    size_t count;
    double min;
    double max;
    double mean;
    double median;
    double std_dev;
    double variance;
    double p50;
    double p75;
    double p90;
    double p95;
    double p99;
    double sum;
    
    Statistics()
        : count(0), min(0), max(0), mean(0), median(0)
        , std_dev(0), variance(0), p50(0), p75(0)
        , p90(0), p95(0), p99(0), sum(0) {}
    
    static Statistics calculate(std::vector<double> values) {
        Statistics stats;
        
        if (values.empty()) {
            return stats;
        }
        
        stats.count = values.size();
        std::sort(values.begin(), values.end());
        
        stats.min = values.front();
        stats.max = values.back();
        stats.sum = std::accumulate(values.begin(), values.end(), 0.0);
        stats.mean = stats.sum / stats.count;
        
        // Median
        if (stats.count % 2 == 0) {
            stats.median = (values[stats.count / 2 - 1] + values[stats.count / 2]) / 2.0;
        } else {
            stats.median = values[stats.count / 2];
        }
        
        // Variance and standard deviation
        double sq_sum = 0;
        for (double v : values) {
            sq_sum += (v - stats.mean) * (v - stats.mean);
        }
        stats.variance = sq_sum / stats.count;
        stats.std_dev = std::sqrt(stats.variance);
        
        // Percentiles
        stats.p50 = percentile(values, 0.50);
        stats.p75 = percentile(values, 0.75);
        stats.p90 = percentile(values, 0.90);
        stats.p95 = percentile(values, 0.95);
        stats.p99 = percentile(values, 0.99);
        
        return stats;
    }
    
private:
    static double percentile(const std::vector<double>& sorted, double p) {
        if (sorted.empty()) return 0;
        double idx = p * (sorted.size() - 1);
        size_t lower = static_cast<size_t>(std::floor(idx));
        size_t upper = static_cast<size_t>(std::ceil(idx));
        if (lower == upper) return sorted[lower];
        double frac = idx - lower;
        return sorted[lower] * (1 - frac) + sorted[upper] * frac;
    }
};

// ============================================================================
// Benchmark Result
// ============================================================================

struct BenchmarkResult {
    std::string name;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
    std::vector<BenchmarkSample> samples;
    
    // Computed statistics
    Statistics time_stats_ms;
    Statistics throughput_stats_mbps;
    Statistics ops_stats;
    
    // Metadata
    size_t iterations;
    size_t warmup_iterations;
    size_t total_bytes;
    size_t total_operations;
    Duration total_time;
    
    std::map<std::string, std::string> tags;
    std::map<std::string, Statistics> custom_metric_stats;
    
    BenchmarkResult()
        : timestamp(std::chrono::system_clock::now())
        , iterations(0)
        , warmup_iterations(0)
        , total_bytes(0)
        , total_operations(0)
        , total_time(Duration::zero()) {}
    
    void computeStatistics() {
        if (samples.empty()) return;
        
        std::vector<double> times_ms;
        std::vector<double> throughputs;
        std::vector<double> ops_per_sec;
        std::map<std::string, std::vector<double>> custom_values;
        
        total_bytes = 0;
        total_operations = 0;
        total_time = Duration::zero();
        
        for (const auto& sample : samples) {
            times_ms.push_back(toMilliseconds(sample.elapsed_time));
            throughputs.push_back(sample.throughputMBps());
            ops_per_sec.push_back(sample.operationsPerSecond());
            
            total_bytes += sample.bytes_processed;
            total_operations += sample.operations_count;
            total_time += sample.elapsed_time;
            
            for (const auto& [key, value] : sample.custom_metrics) {
                custom_values[key].push_back(value);
            }
        }
        
        time_stats_ms = Statistics::calculate(times_ms);
        throughput_stats_mbps = Statistics::calculate(throughputs);
        ops_stats = Statistics::calculate(ops_per_sec);
        
        for (const auto& [key, values] : custom_values) {
            custom_metric_stats[key] = Statistics::calculate(
                std::vector<double>(values.begin(), values.end()));
        }
        
        iterations = samples.size();
    }
    
    std::string toJSON() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        
        oss << "{\n";
        oss << "  \"name\": \"" << name << "\",\n";
        oss << "  \"description\": \"" << description << "\",\n";
        oss << "  \"iterations\": " << iterations << ",\n";
        oss << "  \"warmup_iterations\": " << warmup_iterations << ",\n";
        oss << "  \"total_bytes\": " << total_bytes << ",\n";
        oss << "  \"total_operations\": " << total_operations << ",\n";
        oss << "  \"total_time_ms\": " << toMilliseconds(total_time) << ",\n";
        
        oss << "  \"time_ms\": {\n";
        oss << "    \"min\": " << time_stats_ms.min << ",\n";
        oss << "    \"max\": " << time_stats_ms.max << ",\n";
        oss << "    \"mean\": " << time_stats_ms.mean << ",\n";
        oss << "    \"median\": " << time_stats_ms.median << ",\n";
        oss << "    \"std_dev\": " << time_stats_ms.std_dev << ",\n";
        oss << "    \"p95\": " << time_stats_ms.p95 << ",\n";
        oss << "    \"p99\": " << time_stats_ms.p99 << "\n";
        oss << "  },\n";
        
        oss << "  \"throughput_mbps\": {\n";
        oss << "    \"min\": " << throughput_stats_mbps.min << ",\n";
        oss << "    \"max\": " << throughput_stats_mbps.max << ",\n";
        oss << "    \"mean\": " << throughput_stats_mbps.mean << ",\n";
        oss << "    \"median\": " << throughput_stats_mbps.median << "\n";
        oss << "  },\n";
        
        oss << "  \"ops_per_second\": {\n";
        oss << "    \"min\": " << ops_stats.min << ",\n";
        oss << "    \"max\": " << ops_stats.max << ",\n";
        oss << "    \"mean\": " << ops_stats.mean << ",\n";
        oss << "    \"median\": " << ops_stats.median << "\n";
        oss << "  }";
        
        if (!tags.empty()) {
            oss << ",\n  \"tags\": {";
            bool first = true;
            for (const auto& [key, value] : tags) {
                if (!first) oss << ",";
                oss << "\n    \"" << key << "\": \"" << value << "\"";
                first = false;
            }
            oss << "\n  }";
        }
        
        oss << "\n}";
        return oss.str();
    }
    
    std::string toCSVHeader() const {
        return "name,iterations,min_ms,max_ms,mean_ms,median_ms,std_dev_ms,p95_ms,p99_ms,"
               "throughput_mean_mbps,ops_mean_per_sec,total_bytes,total_time_ms";
    }
    
    std::string toCSVRow() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << name << ","
            << iterations << ","
            << time_stats_ms.min << ","
            << time_stats_ms.max << ","
            << time_stats_ms.mean << ","
            << time_stats_ms.median << ","
            << time_stats_ms.std_dev << ","
            << time_stats_ms.p95 << ","
            << time_stats_ms.p99 << ","
            << throughput_stats_mbps.mean << ","
            << ops_stats.mean << ","
            << total_bytes << ","
            << toMilliseconds(total_time);
        return oss.str();
    }
};

// ============================================================================
// Scoped Timer
// ============================================================================

class ScopedTimer {
public:
    ScopedTimer() : start_(Clock::now()), stopped_(false) {}
    
    void stop() {
        if (!stopped_) {
            end_ = Clock::now();
            stopped_ = true;
        }
    }
    
    Duration elapsed() const {
        if (stopped_) {
            return end_ - start_;
        }
        return Clock::now() - start_;
    }
    
    double elapsedMs() const {
        return toMilliseconds(elapsed());
    }
    
    double elapsedUs() const {
        return toMicroseconds(elapsed());
    }
    
    void reset() {
        start_ = Clock::now();
        stopped_ = false;
    }
    
private:
    TimePoint start_;
    TimePoint end_;
    bool stopped_;
};

// ============================================================================
// Benchmark Runner
// ============================================================================

class BenchmarkRunner {
public:
    using BenchmarkFunc = std::function<BenchmarkSample()>;
    using SetupFunc = std::function<void()>;
    using TeardownFunc = std::function<void()>;
    
    struct Config {
        size_t iterations = 100;
        size_t warmup_iterations = 10;
        bool enable_gc_between_runs = false;
        std::chrono::milliseconds min_duration{0};
        std::chrono::milliseconds max_duration{60000};
        bool stop_on_error = true;
    };
    
    BenchmarkRunner() = default;
    
    void setConfig(const Config& config) { config_ = config; }
    Config& config() { return config_; }
    
    BenchmarkResult run(const std::string& name,
                        BenchmarkFunc benchmark_func,
                        SetupFunc setup = nullptr,
                        TeardownFunc teardown = nullptr) {
        BenchmarkResult result;
        result.name = name;
        result.warmup_iterations = config_.warmup_iterations;
        
        // Warmup phase
        for (size_t i = 0; i < config_.warmup_iterations; ++i) {
            if (setup) setup();
            try {
                benchmark_func();
            } catch (...) {
                // Ignore warmup errors
            }
            if (teardown) teardown();
        }
        
        // Main benchmark phase
        auto start_time = Clock::now();
        
        for (size_t i = 0; i < config_.iterations; ++i) {
            // Check duration limits
            auto elapsed = Clock::now() - start_time;
            if (elapsed >= config_.max_duration) {
                break;
            }
            
            if (setup) setup();
            
            try {
                BenchmarkSample sample = benchmark_func();
                result.samples.push_back(sample);
            } catch (const std::exception& e) {
                if (config_.stop_on_error) {
                    throw;
                }
            }
            
            if (teardown) teardown();
        }
        
        result.computeStatistics();
        return result;
    }
    
    // Simple timing benchmark
    BenchmarkResult runTimed(const std::string& name,
                             std::function<void()> func,
                             size_t bytes_per_iteration = 0) {
        return run(name, [&]() {
            ScopedTimer timer;
            func();
            timer.stop();
            
            BenchmarkSample sample;
            sample.elapsed_time = timer.elapsed();
            sample.bytes_processed = bytes_per_iteration;
            return sample;
        });
    }
    
private:
    Config config_;
};

// ============================================================================
// HSM Operation Benchmarks
// ============================================================================

class HSMBenchmarks {
public:
    struct MigrationBenchmarkConfig {
        size_t file_size = 1024 * 1024;  // 1 MB
        size_t file_count = 100;
        bool use_compression = true;
        std::string compression_algorithm = "LZ4";
        int compression_level = 3;
    };
    
    struct RecallBenchmarkConfig {
        size_t file_size = 1024 * 1024;
        size_t file_count = 100;
        bool sequential = true;
        size_t read_buffer_size = 64 * 1024;
    };
    
    struct CompressionBenchmarkConfig {
        size_t block_size = 64 * 1024;
        size_t iterations = 1000;
        std::string algorithm = "LZ4";
        int level = 3;
        std::string data_pattern = "mixed";  // "random", "zeros", "mixed", "text"
    };
    
    // Simulated migration benchmark
    static BenchmarkResult benchmarkMigration(
            const MigrationBenchmarkConfig& config,
            BenchmarkRunner::Config runner_config = {}) {
        
        BenchmarkRunner runner;
        runner.setConfig(runner_config);
        
        // Generate test data
        std::vector<char> test_data(config.file_size);
        generateTestData(test_data, "mixed");
        
        return runner.run("migration", [&]() {
            ScopedTimer timer;
            
            // Simulate migration: read + compress + write
            size_t total_bytes = 0;
            for (size_t i = 0; i < config.file_count; ++i) {
                // Simulate read from primary storage
                volatile size_t sum = 0;
                for (size_t j = 0; j < test_data.size(); j += 64) {
                    sum += test_data[j];
                }
                
                // Simulate compression (simplified)
                if (config.use_compression) {
                    simulateCompression(test_data);
                }
                
                // Simulate write to ML2
                simulateWrite(test_data.size());
                
                total_bytes += config.file_size;
            }
            
            timer.stop();
            
            BenchmarkSample sample;
            sample.elapsed_time = timer.elapsed();
            sample.bytes_processed = total_bytes;
            sample.operations_count = config.file_count;
            sample.custom_metrics["files_migrated"] = static_cast<double>(config.file_count);
            
            return sample;
        });
    }
    
    // Simulated recall benchmark
    static BenchmarkResult benchmarkRecall(
            const RecallBenchmarkConfig& config,
            BenchmarkRunner::Config runner_config = {}) {
        
        BenchmarkRunner runner;
        runner.setConfig(runner_config);
        
        std::vector<char> buffer(config.read_buffer_size);
        
        return runner.run("recall", [&]() {
            ScopedTimer timer;
            
            size_t total_bytes = 0;
            for (size_t i = 0; i < config.file_count; ++i) {
                // Simulate tape positioning
                if (!config.sequential && i > 0) {
                    simulateSeek();
                }
                
                // Simulate read from tape
                size_t remaining = config.file_size;
                while (remaining > 0) {
                    size_t to_read = std::min(remaining, config.read_buffer_size);
                    simulateRead(to_read);
                    remaining -= to_read;
                }
                
                // Simulate decompress + write to primary
                simulateDecompression(config.file_size);
                simulateWrite(config.file_size);
                
                total_bytes += config.file_size;
            }
            
            timer.stop();
            
            BenchmarkSample sample;
            sample.elapsed_time = timer.elapsed();
            sample.bytes_processed = total_bytes;
            sample.operations_count = config.file_count;
            sample.custom_metrics["files_recalled"] = static_cast<double>(config.file_count);
            
            return sample;
        });
    }
    
    // Compression benchmark
    static BenchmarkResult benchmarkCompression(
            const CompressionBenchmarkConfig& config,
            BenchmarkRunner::Config runner_config = {}) {
        
        BenchmarkRunner runner;
        runner.setConfig(runner_config);
        
        std::vector<char> test_data(config.block_size);
        generateTestData(test_data, config.data_pattern);
        
        return runner.run("compression_" + config.algorithm, [&]() {
            ScopedTimer timer;
            
            size_t total_bytes = 0;
            size_t compressed_bytes = 0;
            
            for (size_t i = 0; i < config.iterations; ++i) {
                // Simulate compression
                size_t compressed_size = simulateCompression(test_data);
                total_bytes += config.block_size;
                compressed_bytes += compressed_size;
            }
            
            timer.stop();
            
            BenchmarkSample sample;
            sample.elapsed_time = timer.elapsed();
            sample.bytes_processed = total_bytes;
            sample.operations_count = config.iterations;
            sample.custom_metrics["compression_ratio"] = 
                static_cast<double>(total_bytes) / compressed_bytes;
            
            return sample;
        });
    }
    
    // I/O throughput benchmark
    static BenchmarkResult benchmarkIOThroughput(
            size_t block_size,
            size_t total_size,
            bool sequential,
            BenchmarkRunner::Config runner_config = {}) {
        
        BenchmarkRunner runner;
        runner.setConfig(runner_config);
        
        return runner.run(sequential ? "io_sequential" : "io_random", [&]() {
            ScopedTimer timer;
            
            size_t blocks = total_size / block_size;
            size_t bytes_processed = 0;
            
            for (size_t i = 0; i < blocks; ++i) {
                if (!sequential) {
                    simulateSeek();
                }
                simulateRead(block_size);
                bytes_processed += block_size;
            }
            
            timer.stop();
            
            BenchmarkSample sample;
            sample.elapsed_time = timer.elapsed();
            sample.bytes_processed = bytes_processed;
            sample.operations_count = blocks;
            
            return sample;
        });
    }
    
private:
    static void generateTestData(std::vector<char>& data, const std::string& pattern) {
        if (pattern == "zeros") {
            std::fill(data.begin(), data.end(), 0);
        } else if (pattern == "random") {
            for (size_t i = 0; i < data.size(); ++i) {
                data[i] = static_cast<char>(rand() % 256);
            }
        } else if (pattern == "text") {
            const char* text = "The quick brown fox jumps over the lazy dog. ";
            size_t text_len = strlen(text);
            for (size_t i = 0; i < data.size(); ++i) {
                data[i] = text[i % text_len];
            }
        } else {
            // Mixed pattern - some repetition, some random
            for (size_t i = 0; i < data.size(); ++i) {
                if (i % 4 == 0) {
                    data[i] = static_cast<char>(i % 256);
                } else {
                    data[i] = static_cast<char>((i * 17) % 256);
                }
            }
        }
    }
    
    static size_t simulateCompression(const std::vector<char>& data) {
        // Simulate compression CPU work
        volatile size_t checksum = 0;
        for (size_t i = 0; i < data.size(); i += 16) {
            checksum ^= data[i];
        }
        // Assume ~50% compression ratio on average
        return data.size() / 2;
    }
    
    static void simulateDecompression(size_t size) {
        volatile size_t work = 0;
        for (size_t i = 0; i < size / 32; ++i) {
            work += i;
        }
    }
    
    static void simulateRead(size_t size) {
        // Simulate read latency
        volatile size_t work = 0;
        for (size_t i = 0; i < size / 1024; ++i) {
            work += i;
        }
    }
    
    static void simulateWrite(size_t size) {
        // Simulate write latency
        volatile size_t work = 0;
        for (size_t i = 0; i < size / 1024; ++i) {
            work += i;
        }
    }
    
    static void simulateSeek() {
        // Simulate tape seek time
        volatile size_t work = 0;
        for (size_t i = 0; i < 10000; ++i) {
            work += i * 31;
        }
    }
};

// ============================================================================
// Benchmark Comparison
// ============================================================================

struct ComparisonResult {
    std::string metric_name;
    double baseline_value;
    double current_value;
    double absolute_diff;
    double percent_diff;
    bool is_regression;
    double threshold;
    
    std::string toString() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << metric_name << ": "
            << baseline_value << " -> " << current_value
            << " (" << (percent_diff >= 0 ? "+" : "") << percent_diff << "%)";
        if (is_regression) {
            oss << " [REGRESSION]";
        }
        return oss.str();
    }
};

class BenchmarkComparator {
public:
    BenchmarkComparator(double regression_threshold_percent = 10.0)
        : threshold_(regression_threshold_percent) {}
    
    std::vector<ComparisonResult> compare(
            const BenchmarkResult& baseline,
            const BenchmarkResult& current) {
        
        std::vector<ComparisonResult> results;
        
        // Compare timing (lower is better)
        results.push_back(compareMetric(
            "mean_time_ms",
            baseline.time_stats_ms.mean,
            current.time_stats_ms.mean,
            true));  // true = lower is better
        
        results.push_back(compareMetric(
            "p95_time_ms",
            baseline.time_stats_ms.p95,
            current.time_stats_ms.p95,
            true));
        
        // Compare throughput (higher is better)
        results.push_back(compareMetric(
            "throughput_mbps",
            baseline.throughput_stats_mbps.mean,
            current.throughput_stats_mbps.mean,
            false));  // false = higher is better
        
        // Compare ops/sec (higher is better)
        results.push_back(compareMetric(
            "ops_per_second",
            baseline.ops_stats.mean,
            current.ops_stats.mean,
            false));
        
        return results;
    }
    
    bool hasRegression(const std::vector<ComparisonResult>& results) const {
        for (const auto& r : results) {
            if (r.is_regression) return true;
        }
        return false;
    }
    
private:
    ComparisonResult compareMetric(
            const std::string& name,
            double baseline,
            double current,
            bool lower_is_better) {
        
        ComparisonResult result;
        result.metric_name = name;
        result.baseline_value = baseline;
        result.current_value = current;
        result.absolute_diff = current - baseline;
        result.threshold = threshold_;
        
        if (baseline != 0) {
            result.percent_diff = ((current - baseline) / baseline) * 100.0;
        } else {
            result.percent_diff = 0;
        }
        
        // Determine if this is a regression
        if (lower_is_better) {
            result.is_regression = result.percent_diff > threshold_;
        } else {
            result.is_regression = result.percent_diff < -threshold_;
        }
        
        return result;
    }
    
    double threshold_;
};

// ============================================================================
// Benchmark Suite
// ============================================================================

class BenchmarkSuite {
public:
    using BenchmarkFactory = std::function<BenchmarkResult()>;
    
    void addBenchmark(const std::string& name, BenchmarkFactory factory) {
        benchmarks_[name] = std::move(factory);
    }
    
    void removeBenchmark(const std::string& name) {
        benchmarks_.erase(name);
    }
    
    std::map<std::string, BenchmarkResult> runAll() {
        std::map<std::string, BenchmarkResult> results;
        
        for (const auto& [name, factory] : benchmarks_) {
            try {
                results[name] = factory();
            } catch (const std::exception& e) {
                BenchmarkResult failed;
                failed.name = name;
                failed.description = std::string("FAILED: ") + e.what();
                results[name] = failed;
            }
        }
        
        return results;
    }
    
    BenchmarkResult run(const std::string& name) {
        auto it = benchmarks_.find(name);
        if (it == benchmarks_.end()) {
            throw std::runtime_error("Benchmark not found: " + name);
        }
        return it->second();
    }
    
    void exportJSON(const std::map<std::string, BenchmarkResult>& results,
                    const std::string& filepath) {
        std::ofstream file(filepath);
        file << "{\n  \"benchmarks\": [\n";
        
        bool first = true;
        for (const auto& [name, result] : results) {
            if (!first) file << ",\n";
            file << "    " << result.toJSON();
            first = false;
        }
        
        file << "\n  ]\n}";
    }
    
    void exportCSV(const std::map<std::string, BenchmarkResult>& results,
                   const std::string& filepath) {
        std::ofstream file(filepath);
        
        if (!results.empty()) {
            file << results.begin()->second.toCSVHeader() << "\n";
            for (const auto& [name, result] : results) {
                file << result.toCSVRow() << "\n";
            }
        }
    }
    
    std::vector<std::string> getBenchmarkNames() const {
        std::vector<std::string> names;
        for (const auto& [name, factory] : benchmarks_) {
            names.push_back(name);
        }
        return names;
    }
    
private:
    std::map<std::string, BenchmarkFactory> benchmarks_;
};

// ============================================================================
// Default HSM Benchmark Suite
// ============================================================================

inline BenchmarkSuite createDefaultHSMBenchmarkSuite() {
    BenchmarkSuite suite;
    
    BenchmarkRunner::Config fast_config;
    fast_config.iterations = 10;
    fast_config.warmup_iterations = 2;
    
    BenchmarkRunner::Config standard_config;
    standard_config.iterations = 50;
    standard_config.warmup_iterations = 5;
    
    // Migration benchmarks
    suite.addBenchmark("migration_small", []() {
        HSMBenchmarks::MigrationBenchmarkConfig config;
        config.file_size = 64 * 1024;  // 64 KB
        config.file_count = 100;
        
        BenchmarkRunner::Config rc;
        rc.iterations = 20;
        return HSMBenchmarks::benchmarkMigration(config, rc);
    });
    
    suite.addBenchmark("migration_large", []() {
        HSMBenchmarks::MigrationBenchmarkConfig config;
        config.file_size = 10 * 1024 * 1024;  // 10 MB
        config.file_count = 10;
        
        BenchmarkRunner::Config rc;
        rc.iterations = 10;
        return HSMBenchmarks::benchmarkMigration(config, rc);
    });
    
    // Recall benchmarks
    suite.addBenchmark("recall_sequential", []() {
        HSMBenchmarks::RecallBenchmarkConfig config;
        config.sequential = true;
        
        BenchmarkRunner::Config rc;
        rc.iterations = 20;
        return HSMBenchmarks::benchmarkRecall(config, rc);
    });
    
    suite.addBenchmark("recall_random", []() {
        HSMBenchmarks::RecallBenchmarkConfig config;
        config.sequential = false;
        
        BenchmarkRunner::Config rc;
        rc.iterations = 20;
        return HSMBenchmarks::benchmarkRecall(config, rc);
    });
    
    // Compression benchmarks
    suite.addBenchmark("compression_lz4", []() {
        HSMBenchmarks::CompressionBenchmarkConfig config;
        config.algorithm = "LZ4";
        config.iterations = 100;
        
        BenchmarkRunner::Config rc;
        rc.iterations = 10;
        return HSMBenchmarks::benchmarkCompression(config, rc);
    });
    
    // I/O benchmarks
    suite.addBenchmark("io_sequential_4k", []() {
        BenchmarkRunner::Config rc;
        rc.iterations = 20;
        return HSMBenchmarks::benchmarkIOThroughput(4096, 1024 * 1024, true, rc);
    });
    
    suite.addBenchmark("io_sequential_64k", []() {
        BenchmarkRunner::Config rc;
        rc.iterations = 20;
        return HSMBenchmarks::benchmarkIOThroughput(65536, 10 * 1024 * 1024, true, rc);
    });
    
    return suite;
}

} // namespace benchmark
} // namespace dfsmshsm

#endif // HSM_BENCHMARK_HPP
