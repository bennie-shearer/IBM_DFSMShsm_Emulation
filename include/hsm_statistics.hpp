/**
 * @file hsm_statistics.hpp
 * @brief HSM statistics collection and reporting
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_HSM_STATISTICS_HPP
#define DFSMSHSM_HSM_STATISTICS_HPP

#include "hsm_types.hpp"
#include <mutex>
#include <sstream>
#include <iomanip>
#include <atomic>

namespace dfsmshsm {

class HSMStatisticsManager {
public:
    static HSMStatisticsManager& instance() { static HSMStatisticsManager m; return m; }

    void recordMigration(uint64_t bytes) { ++stats_.total_migrations; stats_.bytes_migrated += bytes; }
    void recordRecall(uint64_t bytes) { ++stats_.total_recalls; stats_.bytes_recalled += bytes; }
    void recordBackup(uint64_t bytes) { ++stats_.total_backups; stats_.bytes_backed_up += bytes; }
    void recordCacheHit() { ++stats_.cache_hits; }
    void recordCacheMiss() { ++stats_.cache_misses; }
    void setDatasetCounts(uint64_t total, uint64_t active, uint64_t migrated, uint64_t backed) {
        stats_.total_datasets = total; stats_.active_datasets = active;
        stats_.migrated_datasets = migrated; stats_.backed_up_datasets = backed;
    }
    void setSpaceReclaimed(double pct) { stats_.space_reclaimed_percent = pct; }
    void addCompressionSavings(uint64_t bytes) { stats_.compression_savings += bytes; }

    HSMStatistics getStatistics() const { std::lock_guard<std::mutex> lock(mtx_); return stats_; }

    double calculateSpaceReclaimed() const {
        if (stats_.bytes_migrated == 0) return 0.0;
        return static_cast<double>(stats_.bytes_migrated);
    }

    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        oss << "=== HSM Statistics v4.1.0 ===\n\n"
            << "Datasets:\n"
            << "  Total: " << stats_.total_datasets << "\n"
            << "  Active: " << stats_.active_datasets << "\n"
            << "  Migrated: " << stats_.migrated_datasets << "\n"
            << "  Backed up: " << stats_.backed_up_datasets << "\n\n"
            << "Operations:\n"
            << "  Migrations: " << stats_.total_migrations << " (" << formatBytes(stats_.bytes_migrated) << ")\n"
            << "  Recalls: " << stats_.total_recalls << " (" << formatBytes(stats_.bytes_recalled) << ")\n"
            << "  Backups: " << stats_.total_backups << " (" << formatBytes(stats_.bytes_backed_up) << ")\n\n"
            << "Cache:\n"
            << "  Hits: " << stats_.cache_hits << "\n"
            << "  Misses: " << stats_.cache_misses << "\n"
            << "  Hit Ratio: " << std::fixed << std::setprecision(1) << cacheHitRatio() << "%\n\n"
            << "Compression Savings: " << formatBytes(stats_.compression_savings) << "\n";
        return oss.str();
    }

    void reset() { std::lock_guard<std::mutex> lock(mtx_); stats_ = HSMStatistics{}; }

private:
    HSMStatisticsManager() = default;

    double cacheHitRatio() const {
        uint64_t total = stats_.cache_hits + stats_.cache_misses;
        return total > 0 ? 100.0 * double(stats_.cache_hits) / double(total) : 0.0;
    }

    static std::string formatBytes(uint64_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int u = 0; double size = static_cast<double>(bytes);
        while (size >= 1024 && u < 4) { size /= 1024; ++u; }
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << units[u];
        return oss.str();
    }

    mutable std::mutex mtx_;
    HSMStatistics stats_;
};

} // namespace dfsmshsm
#endif
