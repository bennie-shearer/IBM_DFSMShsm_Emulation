/*******************************************************************************
 * DFSMShsm Emulator - Report Generation System
 * @version 4.2.0
 * 
 * Provides comprehensive reporting capabilities for HSM operations including
 * activity reports, storage utilization, migration statistics, and trend analysis.
 * 
 * Features:
 * - Multiple report formats (Text, HTML, JSON, CSV)
 * - Activity reports (migration, recall, backup, recovery)
 * - Storage utilization reports
 * - Tape pool and volume reports
 * - Performance trending and analysis
 * - Scheduled report generation
 * - Report templates and customization
 * 
 * Author: DFSMShsm Emulator Team
 * License: MIT
 ******************************************************************************/

#ifndef HSM_REPORTS_HPP
#define HSM_REPORTS_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <functional>
#include <algorithm>
#include <numeric>

namespace dfsmshsm {
namespace reports {

// ============================================================================
// Report Format Types
// ============================================================================

enum class ReportFormat {
    TEXT,
    HTML,
    JSON,
    CSV,
    XML
};

inline std::string formatToString(ReportFormat fmt) {
    switch (fmt) {
        case ReportFormat::TEXT: return "text";
        case ReportFormat::HTML: return "html";
        case ReportFormat::JSON: return "json";
        case ReportFormat::CSV:  return "csv";
        case ReportFormat::XML:  return "xml";
        default: return "unknown";
    }
}

inline std::string formatExtension(ReportFormat fmt) {
    switch (fmt) {
        case ReportFormat::TEXT: return ".txt";
        case ReportFormat::HTML: return ".html";
        case ReportFormat::JSON: return ".json";
        case ReportFormat::CSV:  return ".csv";
        case ReportFormat::XML:  return ".xml";
        default: return ".txt";
    }
}

// ============================================================================
// Report Time Periods
// ============================================================================

enum class ReportPeriod {
    HOURLY,
    DAILY,
    WEEKLY,
    MONTHLY,
    YEARLY,
    CUSTOM
};

struct ReportTimeRange {
    std::chrono::system_clock::time_point start;
    std::chrono::system_clock::time_point end;
    ReportPeriod period;
    
    ReportTimeRange()
        : period(ReportPeriod::DAILY) {
        end = std::chrono::system_clock::now();
        start = end - std::chrono::hours(24);
    }
    
    static ReportTimeRange today() {
        ReportTimeRange r;
        r.period = ReportPeriod::DAILY;
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_buf);
#endif
        tm_buf.tm_hour = 0;
        tm_buf.tm_min = 0;
        tm_buf.tm_sec = 0;
        r.start = std::chrono::system_clock::from_time_t(mktime(&tm_buf));
        r.end = now;
        return r;
    }
    
    static ReportTimeRange lastNDays(int days) {
        ReportTimeRange r;
        r.period = ReportPeriod::CUSTOM;
        r.end = std::chrono::system_clock::now();
        r.start = r.end - std::chrono::hours(24 * days);
        return r;
    }
    
    static ReportTimeRange thisWeek() {
        return lastNDays(7);
    }
    
    static ReportTimeRange thisMonth() {
        return lastNDays(30);
    }
};

// ============================================================================
// Data Records for Reports
// ============================================================================

struct MigrationRecord {
    std::string dataset_name;
    std::string source_volume;
    std::string target_volume;
    size_t bytes_migrated;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds duration;
    bool compressed;
    double compression_ratio;
    bool success;
    std::string error_message;
    int migration_level;  // ML1 or ML2
};

struct RecallRecord {
    std::string dataset_name;
    std::string source_volume;
    size_t bytes_recalled;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds duration;
    std::chrono::milliseconds wait_time;  // Time waiting for tape mount
    bool success;
    std::string error_message;
};

struct BackupRecord {
    std::string dataset_name;
    std::string backup_volume;
    size_t bytes_backed_up;
    int version_number;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds duration;
    bool incremental;
    bool success;
    std::string error_message;
};

struct StorageUsageRecord {
    std::chrono::system_clock::time_point timestamp;
    std::string storage_class;
    size_t total_capacity;
    size_t used_capacity;
    size_t file_count;
    size_t ml1_bytes;
    size_t ml2_bytes;
};

// ============================================================================
// Report Data Collector
// ============================================================================

class ReportDataCollector {
public:
    // Add records
    void addMigration(const MigrationRecord& record) {
        migrations_.push_back(record);
    }
    
    void addRecall(const RecallRecord& record) {
        recalls_.push_back(record);
    }
    
    void addBackup(const BackupRecord& record) {
        backups_.push_back(record);
    }
    
    void addStorageUsage(const StorageUsageRecord& record) {
        storage_usage_.push_back(record);
    }
    
    // Get records in time range
    std::vector<MigrationRecord> getMigrations(const ReportTimeRange& range) const {
        std::vector<MigrationRecord> result;
        for (const auto& r : migrations_) {
            if (r.timestamp >= range.start && r.timestamp <= range.end) {
                result.push_back(r);
            }
        }
        return result;
    }
    
    std::vector<RecallRecord> getRecalls(const ReportTimeRange& range) const {
        std::vector<RecallRecord> result;
        for (const auto& r : recalls_) {
            if (r.timestamp >= range.start && r.timestamp <= range.end) {
                result.push_back(r);
            }
        }
        return result;
    }
    
    std::vector<BackupRecord> getBackups(const ReportTimeRange& range) const {
        std::vector<BackupRecord> result;
        for (const auto& r : backups_) {
            if (r.timestamp >= range.start && r.timestamp <= range.end) {
                result.push_back(r);
            }
        }
        return result;
    }
    
    std::vector<StorageUsageRecord> getStorageUsage(const ReportTimeRange& range) const {
        std::vector<StorageUsageRecord> result;
        for (const auto& r : storage_usage_) {
            if (r.timestamp >= range.start && r.timestamp <= range.end) {
                result.push_back(r);
            }
        }
        return result;
    }
    
    // Clear old data
    void pruneOldData(std::chrono::system_clock::time_point before) {
        migrations_.erase(
            std::remove_if(migrations_.begin(), migrations_.end(),
                [&](const MigrationRecord& r) { return r.timestamp < before; }),
            migrations_.end());
        
        recalls_.erase(
            std::remove_if(recalls_.begin(), recalls_.end(),
                [&](const RecallRecord& r) { return r.timestamp < before; }),
            recalls_.end());
        
        backups_.erase(
            std::remove_if(backups_.begin(), backups_.end(),
                [&](const BackupRecord& r) { return r.timestamp < before; }),
            backups_.end());
    }
    
    void clear() {
        migrations_.clear();
        recalls_.clear();
        backups_.clear();
        storage_usage_.clear();
    }
    
private:
    std::vector<MigrationRecord> migrations_;
    std::vector<RecallRecord> recalls_;
    std::vector<BackupRecord> backups_;
    std::vector<StorageUsageRecord> storage_usage_;
};

// ============================================================================
// Report Statistics
// ============================================================================

struct MigrationStatistics {
    size_t total_count;
    size_t success_count;
    size_t failure_count;
    size_t total_bytes;
    double avg_bytes_per_migration;
    double avg_duration_ms;
    double avg_throughput_mbps;
    double compression_ratio_avg;
    size_t ml1_count;
    size_t ml2_count;
    
    MigrationStatistics() : total_count(0), success_count(0), failure_count(0),
        total_bytes(0), avg_bytes_per_migration(0), avg_duration_ms(0),
        avg_throughput_mbps(0), compression_ratio_avg(0), ml1_count(0), ml2_count(0) {}
    
    static MigrationStatistics calculate(const std::vector<MigrationRecord>& records) {
        MigrationStatistics stats;
        if (records.empty()) return stats;
        
        stats.total_count = records.size();
        double total_duration = 0;
        double total_ratio = 0;
        int compressed_count = 0;
        
        for (const auto& r : records) {
            if (r.success) stats.success_count++;
            else stats.failure_count++;
            
            stats.total_bytes += r.bytes_migrated;
            total_duration += r.duration.count();
            
            if (r.compressed) {
                total_ratio += r.compression_ratio;
                compressed_count++;
            }
            
            if (r.migration_level == 1) stats.ml1_count++;
            else if (r.migration_level == 2) stats.ml2_count++;
        }
        
        stats.avg_bytes_per_migration = static_cast<double>(stats.total_bytes) / stats.total_count;
        stats.avg_duration_ms = total_duration / stats.total_count;
        
        if (total_duration > 0) {
            stats.avg_throughput_mbps = (stats.total_bytes / (1024.0 * 1024.0)) / 
                                        (total_duration / 1000.0);
        }
        
        if (compressed_count > 0) {
            stats.compression_ratio_avg = total_ratio / compressed_count;
        }
        
        return stats;
    }
};

struct RecallStatistics {
    size_t total_count;
    size_t success_count;
    size_t failure_count;
    size_t total_bytes;
    double avg_bytes_per_recall;
    double avg_duration_ms;
    double avg_wait_time_ms;
    double avg_throughput_mbps;
    
    RecallStatistics() : total_count(0), success_count(0), failure_count(0),
        total_bytes(0), avg_bytes_per_recall(0), avg_duration_ms(0),
        avg_wait_time_ms(0), avg_throughput_mbps(0) {}
    
    static RecallStatistics calculate(const std::vector<RecallRecord>& records) {
        RecallStatistics stats;
        if (records.empty()) return stats;
        
        stats.total_count = records.size();
        double total_duration = 0;
        double total_wait = 0;
        
        for (const auto& r : records) {
            if (r.success) stats.success_count++;
            else stats.failure_count++;
            
            stats.total_bytes += r.bytes_recalled;
            total_duration += r.duration.count();
            total_wait += r.wait_time.count();
        }
        
        stats.avg_bytes_per_recall = static_cast<double>(stats.total_bytes) / stats.total_count;
        stats.avg_duration_ms = total_duration / stats.total_count;
        stats.avg_wait_time_ms = total_wait / stats.total_count;
        
        if (total_duration > 0) {
            stats.avg_throughput_mbps = (stats.total_bytes / (1024.0 * 1024.0)) / 
                                        (total_duration / 1000.0);
        }
        
        return stats;
    }
};

// ============================================================================
// Report Base Class
// ============================================================================

class Report {
public:
    virtual ~Report() = default;
    
    void setTitle(const std::string& title) { title_ = title; }
    void setTimeRange(const ReportTimeRange& range) { time_range_ = range; }
    void setFormat(ReportFormat format) { format_ = format; }
    
    std::string getTitle() const { return title_; }
    ReportFormat getFormat() const { return format_; }
    
    virtual std::string generate() = 0;
    
    void saveToFile(const std::string& path) {
        std::ofstream file(path);
        file << generate();
    }
    
protected:
    std::string title_;
    ReportTimeRange time_range_;
    ReportFormat format_ = ReportFormat::TEXT;
    
    std::string formatTimestamp(std::chrono::system_clock::time_point tp) const {
        auto time_t_tp = std::chrono::system_clock::to_time_t(tp);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t_tp);
#else
        localtime_r(&time_t_tp, &tm_buf);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
    
    std::string formatBytes(size_t bytes) const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        
        if (bytes >= 1024ULL * 1024 * 1024 * 1024) {
            oss << (bytes / (1024.0 * 1024 * 1024 * 1024)) << " TB";
        } else if (bytes >= 1024ULL * 1024 * 1024) {
            oss << (bytes / (1024.0 * 1024 * 1024)) << " GB";
        } else if (bytes >= 1024ULL * 1024) {
            oss << (bytes / (1024.0 * 1024)) << " MB";
        } else if (bytes >= 1024) {
            oss << (bytes / 1024.0) << " KB";
        } else {
            oss << bytes << " bytes";
        }
        
        return oss.str();
    }
    
    std::string escapeHTML(const std::string& str) const {
        std::ostringstream oss;
        for (char c : str) {
            switch (c) {
                case '<': oss << "&lt;"; break;
                case '>': oss << "&gt;"; break;
                case '&': oss << "&amp;"; break;
                case '"': oss << "&quot;"; break;
                default: oss << c;
            }
        }
        return oss.str();
    }
    
    std::string escapeJSON(const std::string& str) const {
        std::ostringstream oss;
        for (char c : str) {
            switch (c) {
                case '"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default: oss << c;
            }
        }
        return oss.str();
    }
};

// ============================================================================
// Migration Activity Report
// ============================================================================

class MigrationActivityReport : public Report {
public:
    MigrationActivityReport(const ReportDataCollector& collector)
        : collector_(collector) {
        title_ = "Migration Activity Report";
    }
    
    std::string generate() override {
        auto records = collector_.getMigrations(time_range_);
        auto stats = MigrationStatistics::calculate(records);
        
        switch (format_) {
            case ReportFormat::HTML: return generateHTML(records, stats);
            case ReportFormat::JSON: return generateJSON(records, stats);
            case ReportFormat::CSV: return generateCSV(records);
            default: return generateText(records, stats);
        }
    }
    
private:
    const ReportDataCollector& collector_;
    
    std::string generateText(const std::vector<MigrationRecord>& records,
                             const MigrationStatistics& stats) {
        std::ostringstream oss;
        oss << std::string(70, '=') << "\n";
        oss << "  " << title_ << "\n";
        oss << std::string(70, '=') << "\n\n";
        
        oss << "Report Period: " << formatTimestamp(time_range_.start) 
            << " to " << formatTimestamp(time_range_.end) << "\n\n";
        
        oss << "SUMMARY\n";
        oss << std::string(40, '-') << "\n";
        oss << "Total Migrations:    " << stats.total_count << "\n";
        oss << "Successful:          " << stats.success_count << "\n";
        oss << "Failed:              " << stats.failure_count << "\n";
        oss << "Total Data:          " << formatBytes(stats.total_bytes) << "\n";
        oss << "Avg per Migration:   " << formatBytes(static_cast<size_t>(stats.avg_bytes_per_migration)) << "\n";
        oss << std::fixed << std::setprecision(2);
        oss << "Avg Duration:        " << stats.avg_duration_ms << " ms\n";
        oss << "Avg Throughput:      " << stats.avg_throughput_mbps << " MB/s\n";
        oss << "Compression Ratio:   " << stats.compression_ratio_avg << ":1\n";
        oss << "ML1 Migrations:      " << stats.ml1_count << "\n";
        oss << "ML2 Migrations:      " << stats.ml2_count << "\n\n";
        
        if (!records.empty()) {
            oss << "RECENT ACTIVITY\n";
            oss << std::string(40, '-') << "\n";
            
            size_t display_count = std::min(records.size(), size_t(20));
            for (size_t i = 0; i < display_count; ++i) {
                const auto& r = records[records.size() - 1 - i];
                oss << formatTimestamp(r.timestamp) << " | "
                    << (r.success ? "OK" : "FAIL") << " | "
                    << std::setw(8) << formatBytes(r.bytes_migrated) << " | "
                    << r.dataset_name << "\n";
            }
        }
        
        return oss.str();
    }
    
    std::string generateHTML(const std::vector<MigrationRecord>& records,
                             const MigrationStatistics& stats) {
        std::ostringstream oss;
        oss << "<!DOCTYPE html>\n<html>\n<head>\n";
        oss << "<title>" << escapeHTML(title_) << "</title>\n";
        oss << "<style>\n";
        oss << "body { font-family: Arial, sans-serif; margin: 20px; }\n";
        oss << "h1 { color: #333; }\n";
        oss << "table { border-collapse: collapse; width: 100%; }\n";
        oss << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
        oss << "th { background-color: #4CAF50; color: white; }\n";
        oss << "tr:nth-child(even) { background-color: #f2f2f2; }\n";
        oss << ".success { color: green; } .failure { color: red; }\n";
        oss << ".summary { background: #f9f9f9; padding: 15px; margin: 10px 0; }\n";
        oss << "</style>\n</head>\n<body>\n";
        
        oss << "<h1>" << escapeHTML(title_) << "</h1>\n";
        oss << "<p>Period: " << formatTimestamp(time_range_.start) 
            << " to " << formatTimestamp(time_range_.end) << "</p>\n";
        
        oss << "<div class='summary'>\n";
        oss << "<h2>Summary</h2>\n";
        oss << "<table>\n";
        oss << "<tr><td>Total Migrations</td><td>" << stats.total_count << "</td></tr>\n";
        oss << "<tr><td>Successful</td><td class='success'>" << stats.success_count << "</td></tr>\n";
        oss << "<tr><td>Failed</td><td class='failure'>" << stats.failure_count << "</td></tr>\n";
        oss << "<tr><td>Total Data</td><td>" << formatBytes(stats.total_bytes) << "</td></tr>\n";
        oss << std::fixed << std::setprecision(2);
        oss << "<tr><td>Avg Throughput</td><td>" << stats.avg_throughput_mbps << " MB/s</td></tr>\n";
        oss << "</table>\n</div>\n";
        
        if (!records.empty()) {
            oss << "<h2>Recent Activity</h2>\n";
            oss << "<table>\n";
            oss << "<tr><th>Time</th><th>Status</th><th>Size</th><th>Dataset</th><th>Duration</th></tr>\n";
            
            size_t display_count = std::min(records.size(), size_t(50));
            for (size_t i = 0; i < display_count; ++i) {
                const auto& r = records[records.size() - 1 - i];
                oss << "<tr>";
                oss << "<td>" << formatTimestamp(r.timestamp) << "</td>";
                oss << "<td class='" << (r.success ? "success'>OK" : "failure'>FAIL") << "</td>";
                oss << "<td>" << formatBytes(r.bytes_migrated) << "</td>";
                oss << "<td>" << escapeHTML(r.dataset_name) << "</td>";
                oss << "<td>" << r.duration.count() << " ms</td>";
                oss << "</tr>\n";
            }
            oss << "</table>\n";
        }
        
        oss << "</body>\n</html>";
        return oss.str();
    }
    
    std::string generateJSON(const std::vector<MigrationRecord>& records,
                             const MigrationStatistics& stats) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        
        oss << "{\n";
        oss << "  \"title\": \"" << escapeJSON(title_) << "\",\n";
        oss << "  \"period\": {\n";
        oss << "    \"start\": \"" << formatTimestamp(time_range_.start) << "\",\n";
        oss << "    \"end\": \"" << formatTimestamp(time_range_.end) << "\"\n";
        oss << "  },\n";
        
        oss << "  \"statistics\": {\n";
        oss << "    \"total_count\": " << stats.total_count << ",\n";
        oss << "    \"success_count\": " << stats.success_count << ",\n";
        oss << "    \"failure_count\": " << stats.failure_count << ",\n";
        oss << "    \"total_bytes\": " << stats.total_bytes << ",\n";
        oss << "    \"avg_bytes_per_migration\": " << stats.avg_bytes_per_migration << ",\n";
        oss << "    \"avg_duration_ms\": " << stats.avg_duration_ms << ",\n";
        oss << "    \"avg_throughput_mbps\": " << stats.avg_throughput_mbps << ",\n";
        oss << "    \"compression_ratio_avg\": " << stats.compression_ratio_avg << "\n";
        oss << "  },\n";
        
        oss << "  \"records\": [\n";
        for (size_t i = 0; i < records.size(); ++i) {
            const auto& r = records[i];
            oss << "    {\n";
            oss << "      \"timestamp\": \"" << formatTimestamp(r.timestamp) << "\",\n";
            oss << "      \"dataset\": \"" << escapeJSON(r.dataset_name) << "\",\n";
            oss << "      \"bytes\": " << r.bytes_migrated << ",\n";
            oss << "      \"duration_ms\": " << r.duration.count() << ",\n";
            oss << "      \"success\": " << (r.success ? "true" : "false") << "\n";
            oss << "    }" << (i < records.size() - 1 ? "," : "") << "\n";
        }
        oss << "  ]\n";
        oss << "}";
        
        return oss.str();
    }
    
    std::string generateCSV(const std::vector<MigrationRecord>& records) {
        std::ostringstream oss;
        oss << "timestamp,dataset,source_volume,target_volume,bytes,duration_ms,success,compression_ratio\n";
        
        for (const auto& r : records) {
            oss << formatTimestamp(r.timestamp) << ","
                << "\"" << r.dataset_name << "\","
                << r.source_volume << ","
                << r.target_volume << ","
                << r.bytes_migrated << ","
                << r.duration.count() << ","
                << (r.success ? "true" : "false") << ","
                << std::fixed << std::setprecision(2) << r.compression_ratio << "\n";
        }
        
        return oss.str();
    }
};

// ============================================================================
// Recall Activity Report
// ============================================================================

class RecallActivityReport : public Report {
public:
    RecallActivityReport(const ReportDataCollector& collector)
        : collector_(collector) {
        title_ = "Recall Activity Report";
    }
    
    std::string generate() override {
        auto records = collector_.getRecalls(time_range_);
        auto stats = RecallStatistics::calculate(records);
        
        switch (format_) {
            case ReportFormat::JSON: return generateJSON(records, stats);
            case ReportFormat::CSV: return generateCSV(records);
            default: return generateText(records, stats);
        }
    }
    
private:
    const ReportDataCollector& collector_;
    
    std::string generateText([[maybe_unused]] const std::vector<RecallRecord>& records,
                             const RecallStatistics& stats) {
        std::ostringstream oss;
        oss << std::string(70, '=') << "\n";
        oss << "  " << title_ << "\n";
        oss << std::string(70, '=') << "\n\n";
        
        oss << "Report Period: " << formatTimestamp(time_range_.start) 
            << " to " << formatTimestamp(time_range_.end) << "\n\n";
        
        oss << "SUMMARY\n";
        oss << std::string(40, '-') << "\n";
        oss << "Total Recalls:       " << stats.total_count << "\n";
        oss << "Successful:          " << stats.success_count << "\n";
        oss << "Failed:              " << stats.failure_count << "\n";
        oss << "Total Data:          " << formatBytes(stats.total_bytes) << "\n";
        oss << std::fixed << std::setprecision(2);
        oss << "Avg Duration:        " << stats.avg_duration_ms << " ms\n";
        oss << "Avg Wait Time:       " << stats.avg_wait_time_ms << " ms\n";
        oss << "Avg Throughput:      " << stats.avg_throughput_mbps << " MB/s\n\n";
        
        return oss.str();
    }
    
    std::string generateJSON([[maybe_unused]] const std::vector<RecallRecord>& records,
                             const RecallStatistics& stats) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        
        oss << "{\n";
        oss << "  \"title\": \"" << escapeJSON(title_) << "\",\n";
        oss << "  \"statistics\": {\n";
        oss << "    \"total_count\": " << stats.total_count << ",\n";
        oss << "    \"success_count\": " << stats.success_count << ",\n";
        oss << "    \"total_bytes\": " << stats.total_bytes << ",\n";
        oss << "    \"avg_wait_time_ms\": " << stats.avg_wait_time_ms << "\n";
        oss << "  }\n";
        oss << "}";
        
        return oss.str();
    }
    
    std::string generateCSV(const std::vector<RecallRecord>& records) {
        std::ostringstream oss;
        oss << "timestamp,dataset,source_volume,bytes,duration_ms,wait_time_ms,success\n";
        
        for (const auto& r : records) {
            oss << formatTimestamp(r.timestamp) << ","
                << "\"" << r.dataset_name << "\","
                << r.source_volume << ","
                << r.bytes_recalled << ","
                << r.duration.count() << ","
                << r.wait_time.count() << ","
                << (r.success ? "true" : "false") << "\n";
        }
        
        return oss.str();
    }
};

// ============================================================================
// Storage Utilization Report
// ============================================================================

class StorageUtilizationReport : public Report {
public:
    StorageUtilizationReport(const ReportDataCollector& collector)
        : collector_(collector) {
        title_ = "Storage Utilization Report";
    }
    
    std::string generate() override {
        auto records = collector_.getStorageUsage(time_range_);
        
        switch (format_) {
            case ReportFormat::JSON: return generateJSON(records);
            default: return generateText(records);
        }
    }
    
private:
    const ReportDataCollector& collector_;
    
    std::string generateText(const std::vector<StorageUsageRecord>& records) {
        std::ostringstream oss;
        oss << std::string(70, '=') << "\n";
        oss << "  " << title_ << "\n";
        oss << std::string(70, '=') << "\n\n";
        
        if (records.empty()) {
            oss << "No storage usage data available for this period.\n";
            return oss.str();
        }
        
        // Group by storage class
        std::map<std::string, std::vector<StorageUsageRecord>> by_class;
        for (const auto& r : records) {
            by_class[r.storage_class].push_back(r);
        }
        
        for (const auto& [storage_class, class_records] : by_class) {
            const auto& latest = class_records.back();
            double utilization = (latest.total_capacity > 0) ?
                (100.0 * latest.used_capacity / latest.total_capacity) : 0;
            
            oss << "Storage Class: " << storage_class << "\n";
            oss << std::string(40, '-') << "\n";
            oss << "Total Capacity:  " << formatBytes(latest.total_capacity) << "\n";
            oss << "Used Capacity:   " << formatBytes(latest.used_capacity) << "\n";
            oss << std::fixed << std::setprecision(1);
            oss << "Utilization:     " << utilization << "%\n";
            oss << "File Count:      " << latest.file_count << "\n";
            oss << "ML1 Data:        " << formatBytes(latest.ml1_bytes) << "\n";
            oss << "ML2 Data:        " << formatBytes(latest.ml2_bytes) << "\n\n";
        }
        
        return oss.str();
    }
    
    std::string generateJSON(const std::vector<StorageUsageRecord>& records) {
        std::ostringstream oss;
        oss << "{\n  \"storage_classes\": [\n";
        
        std::map<std::string, StorageUsageRecord> latest_by_class;
        for (const auto& r : records) {
            latest_by_class[r.storage_class] = r;
        }
        
        bool first = true;
        for (const auto& [sc, r] : latest_by_class) {
            if (!first) oss << ",\n";
            oss << "    {\n";
            oss << "      \"name\": \"" << sc << "\",\n";
            oss << "      \"total_capacity\": " << r.total_capacity << ",\n";
            oss << "      \"used_capacity\": " << r.used_capacity << ",\n";
            oss << "      \"file_count\": " << r.file_count << "\n";
            oss << "    }";
            first = false;
        }
        
        oss << "\n  ]\n}";
        return oss.str();
    }
};

// ============================================================================
// Summary Dashboard Report
// ============================================================================

class DashboardReport : public Report {
public:
    DashboardReport(const ReportDataCollector& collector)
        : collector_(collector) {
        title_ = "HSM Dashboard Summary";
    }
    
    std::string generate() override {
        auto migrations = collector_.getMigrations(time_range_);
        auto recalls = collector_.getRecalls(time_range_);
        auto backups = collector_.getBackups(time_range_);
        
        auto mig_stats = MigrationStatistics::calculate(migrations);
        auto recall_stats = RecallStatistics::calculate(recalls);
        
        std::ostringstream oss;
        
        if (format_ == ReportFormat::HTML) {
            return generateHTML(mig_stats, recall_stats, backups.size());
        }
        
        oss << std::string(70, '=') << "\n";
        oss << "  " << title_ << "\n";
        oss << std::string(70, '=') << "\n\n";
        
        oss << "Period: " << formatTimestamp(time_range_.start) 
            << " to " << formatTimestamp(time_range_.end) << "\n\n";
        
        oss << "MIGRATION ACTIVITY\n";
        oss << "  Total: " << mig_stats.total_count 
            << " (" << mig_stats.success_count << " ok, " 
            << mig_stats.failure_count << " failed)\n";
        oss << "  Data: " << formatBytes(mig_stats.total_bytes) << "\n";
        oss << std::fixed << std::setprecision(2);
        oss << "  Throughput: " << mig_stats.avg_throughput_mbps << " MB/s\n\n";
        
        oss << "RECALL ACTIVITY\n";
        oss << "  Total: " << recall_stats.total_count 
            << " (" << recall_stats.success_count << " ok)\n";
        oss << "  Data: " << formatBytes(recall_stats.total_bytes) << "\n";
        oss << "  Avg Wait: " << recall_stats.avg_wait_time_ms << " ms\n\n";
        
        oss << "BACKUP ACTIVITY\n";
        oss << "  Total Backups: " << backups.size() << "\n\n";
        
        return oss.str();
    }
    
private:
    const ReportDataCollector& collector_;
    
    std::string generateHTML(const MigrationStatistics& mig_stats,
                             const RecallStatistics& recall_stats,
                             size_t backup_count) {
        std::ostringstream oss;
        oss << "<!DOCTYPE html>\n<html>\n<head>\n";
        oss << "<title>HSM Dashboard</title>\n";
        oss << "<style>\n";
        oss << "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }\n";
        oss << ".dashboard { display: flex; flex-wrap: wrap; gap: 20px; }\n";
        oss << ".card { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); min-width: 250px; }\n";
        oss << ".card h3 { margin-top: 0; color: #333; }\n";
        oss << ".metric { font-size: 24px; font-weight: bold; color: #4CAF50; }\n";
        oss << ".label { color: #666; font-size: 12px; }\n";
        oss << "</style>\n</head>\n<body>\n";
        
        oss << "<h1>HSM Dashboard</h1>\n";
        oss << "<div class='dashboard'>\n";
        
        oss << "<div class='card'>\n";
        oss << "  <h3>Migrations</h3>\n";
        oss << "  <div class='metric'>" << mig_stats.total_count << "</div>\n";
        oss << "  <div class='label'>Total migrations</div>\n";
        oss << "  <p>" << formatBytes(mig_stats.total_bytes) << " migrated</p>\n";
        oss << "</div>\n";
        
        oss << "<div class='card'>\n";
        oss << "  <h3>Recalls</h3>\n";
        oss << "  <div class='metric'>" << recall_stats.total_count << "</div>\n";
        oss << "  <div class='label'>Total recalls</div>\n";
        oss << "  <p>" << formatBytes(recall_stats.total_bytes) << " recalled</p>\n";
        oss << "</div>\n";
        
        oss << "<div class='card'>\n";
        oss << "  <h3>Backups</h3>\n";
        oss << "  <div class='metric'>" << backup_count << "</div>\n";
        oss << "  <div class='label'>Total backups</div>\n";
        oss << "</div>\n";
        
        oss << std::fixed << std::setprecision(1);
        oss << "<div class='card'>\n";
        oss << "  <h3>Performance</h3>\n";
        oss << "  <div class='metric'>" << mig_stats.avg_throughput_mbps << "</div>\n";
        oss << "  <div class='label'>MB/s avg migration</div>\n";
        oss << "</div>\n";
        
        oss << "</div>\n</body>\n</html>";
        return oss.str();
    }
};

// ============================================================================
// Report Generator Factory
// ============================================================================

class ReportGenerator {
public:
    ReportGenerator(std::shared_ptr<ReportDataCollector> collector)
        : collector_(std::move(collector)) {}
    
    std::unique_ptr<Report> createMigrationReport() {
        return std::make_unique<MigrationActivityReport>(*collector_);
    }
    
    std::unique_ptr<Report> createRecallReport() {
        return std::make_unique<RecallActivityReport>(*collector_);
    }
    
    std::unique_ptr<Report> createStorageReport() {
        return std::make_unique<StorageUtilizationReport>(*collector_);
    }
    
    std::unique_ptr<Report> createDashboard() {
        return std::make_unique<DashboardReport>(*collector_);
    }
    
    // Generate all standard reports
    std::map<std::string, std::string> generateAllReports(
            const ReportTimeRange& range,
            ReportFormat format = ReportFormat::TEXT) {
        
        std::map<std::string, std::string> reports;
        
        auto mig = createMigrationReport();
        mig->setTimeRange(range);
        mig->setFormat(format);
        reports["migration"] = mig->generate();
        
        auto recall = createRecallReport();
        recall->setTimeRange(range);
        recall->setFormat(format);
        reports["recall"] = recall->generate();
        
        auto storage = createStorageReport();
        storage->setTimeRange(range);
        storage->setFormat(format);
        reports["storage"] = storage->generate();
        
        auto dash = createDashboard();
        dash->setTimeRange(range);
        dash->setFormat(format);
        reports["dashboard"] = dash->generate();
        
        return reports;
    }
    
private:
    std::shared_ptr<ReportDataCollector> collector_;
};

} // namespace reports
} // namespace dfsmshsm

#endif // HSM_REPORTS_HPP
