/**
 * @file snapshot_manager.hpp
 * @brief Point-in-time snapshot management
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 * 
 * Implements snapshot functionality:
 * - Point-in-time dataset snapshots
 * - Snapshot retention policies
 * - Snapshot-based recovery
 */
#ifndef DFSMSHSM_SNAPSHOT_MANAGER_HPP
#define DFSMSHSM_SNAPSHOT_MANAGER_HPP

#include "hsm_types.hpp"
#include "event_system.hpp"
#include <map>
#include <mutex>
#include <atomic>
#include <sstream>
#include <iomanip>

namespace dfsmshsm {

/**
 * @struct SnapshotPolicy
 * @brief Policy for automatic snapshot creation
 */
struct SnapshotPolicy {
    uint32_t hourly_keep = 24;      ///< Keep last 24 hourly snapshots
    uint32_t daily_keep = 7;        ///< Keep last 7 daily snapshots
    uint32_t weekly_keep = 4;       ///< Keep last 4 weekly snapshots
    uint32_t monthly_keep = 12;     ///< Keep last 12 monthly snapshots
    bool enabled = false;
};

/**
 * @struct SnapshotStatistics
 * @brief Snapshot statistics
 */
struct SnapshotStatistics {
    uint64_t total_snapshots = 0;
    uint64_t active_snapshots = 0;
    uint64_t deleted_snapshots = 0;
    uint64_t bytes_used = 0;
    uint64_t bytes_saved_by_dedupe = 0;
    uint64_t restores_performed = 0;
};

/**
 * @class SnapshotManager
 * @brief Manages point-in-time snapshots
 */
class SnapshotManager {
public:
    SnapshotManager() : running_(false), next_snapshot_id_(1) {}
    
    /**
     * @brief Start the snapshot manager
     */
    bool start() {
        if (running_) return false;
        running_ = true;
        return true;
    }
    
    /**
     * @brief Stop the snapshot manager
     */
    void stop() { running_ = false; }
    
    /**
     * @brief Check if manager is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Create a snapshot of a dataset
     * @param dataset_name Name of the dataset
     * @param description Optional description
     * @param retention_days Days to keep snapshot (0 = indefinite)
     * @return Snapshot ID
     */
    std::optional<std::string> createSnapshot(const std::string& dataset_name,
                                               const std::string& description = "",
                                               uint32_t retention_days = 0) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto now = std::chrono::system_clock::now();
        
        SnapshotInfo snap;
        snap.id = "SNAP-" + std::to_string(next_snapshot_id_++);
        snap.dataset_name = dataset_name;
        snap.description = description;
        snap.created = now;
        snap.is_consistent = true;
        
        if (retention_days > 0) {
            snap.expiration = now + std::chrono::hours(24 * retention_days);
        } else {
            snap.expiration = std::chrono::system_clock::time_point::max();
        }
        
        // Determine version number
        auto& versions = dataset_snapshots_[dataset_name];
        snap.version = versions.empty() ? 1 : versions.rbegin()->second.version + 1;
        
        // In real implementation, would copy data here
        // For emulation, just track metadata
        snap.size = 0;  // Would be actual snapshot size
        
        snapshots_[snap.id] = snap;
        versions[snap.id] = snap;
        
        ++stats_.total_snapshots;
        ++stats_.active_snapshots;
        
        EventSystem::instance().emit(
            EventType::DATASET_SNAPSHOT,
            "SnapshotManager",
            "Created snapshot " + snap.id + " for " + dataset_name,
            dataset_name);
        
        return snap.id;
    }
    
    /**
     * @brief Delete a snapshot
     */
    bool deleteSnapshot(const std::string& snapshot_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = snapshots_.find(snapshot_id);
        if (it == snapshots_.end()) return false;
        
        const std::string& dataset_name = it->second.dataset_name;
        
        // Remove from dataset's snapshot list
        auto ds_it = dataset_snapshots_.find(dataset_name);
        if (ds_it != dataset_snapshots_.end()) {
            ds_it->second.erase(snapshot_id);
            if (ds_it->second.empty()) {
                dataset_snapshots_.erase(ds_it);
            }
        }
        
        stats_.bytes_used -= it->second.size;
        snapshots_.erase(it);
        
        ++stats_.deleted_snapshots;
        --stats_.active_snapshots;
        
        return true;
    }
    
    /**
     * @brief Get snapshot information
     */
    std::optional<SnapshotInfo> getSnapshot(const std::string& snapshot_id) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = snapshots_.find(snapshot_id);
        return it != snapshots_.end() ? std::optional<SnapshotInfo>(it->second) : std::nullopt;
    }
    
    /**
     * @brief Get all snapshots for a dataset
     */
    std::vector<SnapshotInfo> getDatasetSnapshots(const std::string& dataset_name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        std::vector<SnapshotInfo> result;
        auto it = dataset_snapshots_.find(dataset_name);
        if (it != dataset_snapshots_.end()) {
            for (const auto& [id, snap] : it->second) {
                result.push_back(snap);
            }
        }
        return result;
    }
    
    /**
     * @brief Get latest snapshot for a dataset
     */
    std::optional<SnapshotInfo> getLatestSnapshot(const std::string& dataset_name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = dataset_snapshots_.find(dataset_name);
        if (it == dataset_snapshots_.end() || it->second.empty()) {
            return std::nullopt;
        }
        
        return it->second.rbegin()->second;
    }
    
    /**
     * @brief Restore dataset from snapshot
     * @param snapshot_id Snapshot to restore from
     * @return True if successful
     */
    bool restoreFromSnapshot(const std::string& snapshot_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = snapshots_.find(snapshot_id);
        if (it == snapshots_.end()) return false;
        
        // In real implementation, would restore data here
        ++stats_.restores_performed;
        
        return true;
    }
    
    /**
     * @brief Clean up expired snapshots
     * @return Number of snapshots deleted
     */
    size_t cleanupExpired() {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto now = std::chrono::system_clock::now();
        std::vector<std::string> to_delete;
        
        for (const auto& [id, snap] : snapshots_) {
            if (snap.expiration < now) {
                to_delete.push_back(id);
            }
        }
        
        size_t deleted = 0;
        for (const auto& id : to_delete) {
            auto it = snapshots_.find(id);
            if (it != snapshots_.end()) {
                const std::string& dataset_name = it->second.dataset_name;
                
                auto ds_it = dataset_snapshots_.find(dataset_name);
                if (ds_it != dataset_snapshots_.end()) {
                    ds_it->second.erase(id);
                }
                
                stats_.bytes_used -= it->second.size;
                snapshots_.erase(it);
                ++deleted;
            }
        }
        
        stats_.deleted_snapshots += deleted;
        stats_.active_snapshots -= deleted;
        
        return deleted;
    }
    
    /**
     * @brief Set snapshot policy for a dataset
     */
    void setPolicy(const std::string& dataset_name, const SnapshotPolicy& policy) {
        std::lock_guard<std::mutex> lock(mtx_);
        policies_[dataset_name] = policy;
    }
    
    /**
     * @brief Get snapshot policy for a dataset
     */
    std::optional<SnapshotPolicy> getPolicy(const std::string& dataset_name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = policies_.find(dataset_name);
        return it != policies_.end() ? std::optional<SnapshotPolicy>(it->second) : std::nullopt;
    }
    
    /**
     * @brief Apply retention policy
     * @param dataset_name Dataset to apply policy to
     * @return Number of snapshots deleted
     */
    size_t applyRetentionPolicy(const std::string& dataset_name) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto policy_it = policies_.find(dataset_name);
        if (policy_it == policies_.end() || !policy_it->second.enabled) {
            return 0;
        }
        
        const auto& policy = policy_it->second;
        auto ds_it = dataset_snapshots_.find(dataset_name);
        if (ds_it == dataset_snapshots_.end()) return 0;
        
        // For simplicity, just keep the most recent N snapshots based on total
        uint32_t max_keep = policy.hourly_keep + policy.daily_keep + 
                           policy.weekly_keep + policy.monthly_keep;
        
        std::vector<std::string> sorted_ids;
        for (const auto& [id, snap] : ds_it->second) {
            sorted_ids.push_back(id);
        }
        
        // Sort by creation time (newest first based on ID order)
        std::sort(sorted_ids.rbegin(), sorted_ids.rend());
        
        size_t deleted = 0;
        for (size_t i = max_keep; i < sorted_ids.size(); ++i) {
            const std::string& id = sorted_ids[i];
            auto snap_it = snapshots_.find(id);
            if (snap_it != snapshots_.end()) {
                stats_.bytes_used -= snap_it->second.size;
                snapshots_.erase(snap_it);
                ds_it->second.erase(id);
                ++deleted;
            }
        }
        
        stats_.deleted_snapshots += deleted;
        stats_.active_snapshots -= deleted;
        
        return deleted;
    }
    
    /**
     * @brief Compare two snapshots
     * @return Description of differences
     */
    std::string compareSnapshots(const std::string& snap1_id, 
                                  const std::string& snap2_id) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it1 = snapshots_.find(snap1_id);
        auto it2 = snapshots_.find(snap2_id);
        
        if (it1 == snapshots_.end() || it2 == snapshots_.end()) {
            return "Snapshot not found";
        }
        
        std::ostringstream oss;
        oss << "Comparison: " << snap1_id << " vs " << snap2_id << "\n"
            << "  Dataset: " << it1->second.dataset_name << "\n"
            << "  Version: " << it1->second.version << " vs " << it2->second.version << "\n"
            << "  Size: " << formatBytes(it1->second.size) << " vs " 
            << formatBytes(it2->second.size) << "\n";
        
        return oss.str();
    }
    
    /**
     * @brief Get snapshot statistics
     */
    SnapshotStatistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return stats_;
    }
    
    /**
     * @brief Get snapshot count for a dataset
     */
    size_t getSnapshotCount(const std::string& dataset_name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = dataset_snapshots_.find(dataset_name);
        return it != dataset_snapshots_.end() ? it->second.size() : 0;
    }
    
    /**
     * @brief Get total snapshot count
     */
    size_t getTotalSnapshotCount() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return snapshots_.size();
    }
    
    /**
     * @brief Generate snapshot report
     */
    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        
        oss << "=== Snapshot Statistics ===\n"
            << "Total Created: " << stats_.total_snapshots << "\n"
            << "Active: " << stats_.active_snapshots << "\n"
            << "Deleted: " << stats_.deleted_snapshots << "\n"
            << "Bytes Used: " << formatBytes(stats_.bytes_used) << "\n"
            << "Restores: " << stats_.restores_performed << "\n\n"
            << "Datasets with Snapshots: " << dataset_snapshots_.size() << "\n";
        
        for (const auto& [ds, snaps] : dataset_snapshots_) {
            oss << "  " << ds << ": " << snaps.size() << " snapshots\n";
        }
        
        return oss.str();
    }

private:
    std::atomic<bool> running_;
    std::atomic<uint64_t> next_snapshot_id_;
    mutable std::mutex mtx_;
    
    std::map<std::string, SnapshotInfo> snapshots_;  // snapshot_id -> info
    std::map<std::string, std::map<std::string, SnapshotInfo>> dataset_snapshots_;  // dataset -> snapshots
    std::map<std::string, SnapshotPolicy> policies_;  // dataset -> policy
    SnapshotStatistics stats_;
};

} // namespace dfsmshsm
#endif // DFSMSHSM_SNAPSHOT_MANAGER_HPP
