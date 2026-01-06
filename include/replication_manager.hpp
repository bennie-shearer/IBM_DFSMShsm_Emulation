/**
 * @file replication_manager.hpp
 * @brief Cross-tier data replication for reliability
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 * 
 * Implements data replication with:
 * - Synchronous and asynchronous replication
 * - Replica management
 * - Consistency verification
 */
#ifndef DFSMSHSM_REPLICATION_MANAGER_HPP
#define DFSMSHSM_REPLICATION_MANAGER_HPP

#include "hsm_types.hpp"
#include "event_system.hpp"
#include <map>
#include <set>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <sstream>
#include <iomanip>
#include <condition_variable>

namespace dfsmshsm {

/**
 * @struct ReplicaInfo
 * @brief Information about a data replica
 */
struct ReplicaInfo {
    std::string id;
    std::string dataset_name;
    std::string source_volume;
    std::string target_volume;
    StorageTier target_tier;
    uint64_t size = 0;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point last_sync;
    std::string content_hash;
    bool is_primary = false;
    bool is_consistent = true;
    uint32_t sync_failures = 0;
};

/**
 * @struct ReplicationJob
 * @brief Pending replication job
 */
struct ReplicationJob {
    std::string dataset_name;
    std::string source_volume;
    std::string target_volume;
    StorageTier target_tier;
    bool is_sync = false;
    std::chrono::system_clock::time_point queued;
};

/**
 * @struct ReplicationStatistics
 * @brief Replication statistics
 */
struct ReplicationStatistics {
    uint64_t total_replications = 0;
    uint64_t sync_replications = 0;
    uint64_t async_replications = 0;
    uint64_t bytes_replicated = 0;
    uint64_t failed_replications = 0;
    uint64_t consistency_checks = 0;
    uint64_t consistency_failures = 0;
    uint32_t active_replicas = 0;
    uint32_t pending_jobs = 0;
};

/**
 * @class ReplicationManager
 * @brief Manages data replication across storage tiers
 */
class ReplicationManager {
public:
    ReplicationManager() : running_(false), next_replica_id_(1) {}
    
    ~ReplicationManager() { stop(); }
    
    /**
     * @brief Start the replication manager
     */
    bool start() {
        if (running_) return false;
        running_ = true;
        
        // Start async replication worker
        worker_thread_ = std::thread([this] { processAsyncQueue(); });
        return true;
    }
    
    /**
     * @brief Stop the replication manager
     */
    void stop() {
        if (!running_) return;
        running_ = false;
        cv_.notify_all();
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    
    /**
     * @brief Check if manager is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Set replication policy for a dataset
     */
    void setPolicy(const std::string& dataset_name, const ReplicationPolicy& policy) {
        std::lock_guard<std::mutex> lock(mtx_);
        policies_[dataset_name] = policy;
    }
    
    /**
     * @brief Get replication policy for a dataset
     */
    std::optional<ReplicationPolicy> getPolicy(const std::string& dataset_name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = policies_.find(dataset_name);
        return it != policies_.end() ? std::optional<ReplicationPolicy>(it->second) : std::nullopt;
    }
    
    /**
     * @brief Create a replica of a dataset
     * @param dataset_name Dataset to replicate
     * @param source_volume Source volume
     * @param target_volume Target volume
     * @param target_tier Target storage tier
     * @param synchronous If true, wait for completion
     * @return Replica ID or empty if async
     */
    std::optional<std::string> createReplica(const std::string& dataset_name,
                                              const std::string& source_volume,
                                              const std::string& target_volume,
                                              StorageTier target_tier,
                                              bool synchronous = false) {
        if (synchronous) {
            return createReplicaSync(dataset_name, source_volume, target_volume, target_tier);
        } else {
            queueReplication(dataset_name, source_volume, target_volume, target_tier);
            return std::nullopt;
        }
    }
    
    /**
     * @brief Delete a replica
     */
    bool deleteReplica(const std::string& replica_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = replicas_.find(replica_id);
        if (it == replicas_.end()) return false;
        
        const std::string& dataset = it->second.dataset_name;
        
        // Remove from dataset's replica set
        auto ds_it = dataset_replicas_.find(dataset);
        if (ds_it != dataset_replicas_.end()) {
            ds_it->second.erase(replica_id);
            if (ds_it->second.empty()) {
                dataset_replicas_.erase(ds_it);
            }
        }
        
        replicas_.erase(it);
        --stats_.active_replicas;
        
        return true;
    }
    
    /**
     * @brief Get replica information
     */
    std::optional<ReplicaInfo> getReplica(const std::string& replica_id) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = replicas_.find(replica_id);
        return it != replicas_.end() ? std::optional<ReplicaInfo>(it->second) : std::nullopt;
    }
    
    /**
     * @brief Get all replicas for a dataset
     */
    std::vector<ReplicaInfo> getDatasetReplicas(const std::string& dataset_name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        std::vector<ReplicaInfo> result;
        auto it = dataset_replicas_.find(dataset_name);
        if (it != dataset_replicas_.end()) {
            for (const auto& id : it->second) {
                auto rep_it = replicas_.find(id);
                if (rep_it != replicas_.end()) {
                    result.push_back(rep_it->second);
                }
            }
        }
        return result;
    }
    
    /**
     * @brief Synchronize a replica with its source
     */
    bool syncReplica(const std::string& replica_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = replicas_.find(replica_id);
        if (it == replicas_.end()) return false;
        
        // Simulate sync
        it->second.last_sync = std::chrono::system_clock::now();
        it->second.is_consistent = true;
        
        EventSystem::instance().emit(
            EventType::REPLICATION_SYNC,
            "ReplicationManager",
            "Synchronized replica " + replica_id,
            it->second.dataset_name);
        
        return true;
    }
    
    /**
     * @brief Verify replica consistency
     */
    bool verifyConsistency(const std::string& replica_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = replicas_.find(replica_id);
        if (it == replicas_.end()) return false;
        
        ++stats_.consistency_checks;
        
        // In real implementation, would compare checksums
        bool consistent = true;  // Simulated
        
        it->second.is_consistent = consistent;
        
        if (!consistent) {
            ++stats_.consistency_failures;
            it->second.sync_failures++;
        }
        
        return consistent;
    }
    
    /**
     * @brief Get replica count for a dataset
     */
    size_t getReplicaCount(const std::string& dataset_name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = dataset_replicas_.find(dataset_name);
        return it != dataset_replicas_.end() ? it->second.size() : 0;
    }
    
    /**
     * @brief Get pending job count
     */
    size_t getPendingJobCount() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return pending_jobs_.size();
    }
    
    /**
     * @brief Get replication statistics
     */
    ReplicationStatistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto stats = stats_;
        stats.pending_jobs = static_cast<uint32_t>(pending_jobs_.size());
        return stats;
    }
    
    /**
     * @brief Generate replication report
     */
    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        
        oss << "=== Replication Statistics ===\n"
            << "Total Replications: " << stats_.total_replications << "\n"
            << "Sync: " << stats_.sync_replications << "\n"
            << "Async: " << stats_.async_replications << "\n"
            << "Bytes Replicated: " << formatBytes(stats_.bytes_replicated) << "\n"
            << "Failed: " << stats_.failed_replications << "\n"
            << "Consistency Checks: " << stats_.consistency_checks << "\n"
            << "Consistency Failures: " << stats_.consistency_failures << "\n"
            << "Active Replicas: " << stats_.active_replicas << "\n"
            << "Pending Jobs: " << pending_jobs_.size() << "\n\n"
            << "Replicated Datasets: " << dataset_replicas_.size() << "\n";
        
        for (const auto& [ds, reps] : dataset_replicas_) {
            oss << "  " << ds << ": " << reps.size() << " replicas\n";
        }
        
        return oss.str();
    }

private:
    std::optional<std::string> createReplicaSync(const std::string& dataset_name,
                                                   const std::string& source_volume,
                                                   const std::string& target_volume,
                                                   StorageTier target_tier) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        ReplicaInfo replica;
        replica.id = "REP-" + std::to_string(next_replica_id_++);
        replica.dataset_name = dataset_name;
        replica.source_volume = source_volume;
        replica.target_volume = target_volume;
        replica.target_tier = target_tier;
        replica.created = std::chrono::system_clock::now();
        replica.last_sync = replica.created;
        replica.is_consistent = true;
        replica.is_primary = false;
        
        replicas_[replica.id] = replica;
        dataset_replicas_[dataset_name].insert(replica.id);
        
        ++stats_.total_replications;
        ++stats_.sync_replications;
        ++stats_.active_replicas;
        
        EventSystem::instance().emit(
            EventType::DATASET_REPLICATED,
            "ReplicationManager",
            "Created replica " + replica.id + " for " + dataset_name,
            dataset_name);
        
        return replica.id;
    }
    
    void queueReplication(const std::string& dataset_name,
                          const std::string& source_volume,
                          const std::string& target_volume,
                          StorageTier target_tier) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        ReplicationJob job;
        job.dataset_name = dataset_name;
        job.source_volume = source_volume;
        job.target_volume = target_volume;
        job.target_tier = target_tier;
        job.is_sync = false;
        job.queued = std::chrono::system_clock::now();
        
        pending_jobs_.push(job);
        cv_.notify_one();
    }
    
    void processAsyncQueue() {
        while (running_) {
            std::unique_lock<std::mutex> lock(mtx_);
            
            cv_.wait(lock, [this] { 
                return !running_ || !pending_jobs_.empty(); 
            });
            
            if (!running_) break;
            
            while (!pending_jobs_.empty()) {
                auto job = pending_jobs_.front();
                pending_jobs_.pop();
                lock.unlock();
                
                // Process the job
                ReplicaInfo replica;
                replica.id = "REP-" + std::to_string(next_replica_id_++);
                replica.dataset_name = job.dataset_name;
                replica.source_volume = job.source_volume;
                replica.target_volume = job.target_volume;
                replica.target_tier = job.target_tier;
                replica.created = std::chrono::system_clock::now();
                replica.last_sync = replica.created;
                replica.is_consistent = true;
                
                lock.lock();
                replicas_[replica.id] = replica;
                dataset_replicas_[job.dataset_name].insert(replica.id);
                
                ++stats_.total_replications;
                ++stats_.async_replications;
                ++stats_.active_replicas;
            }
        }
    }
    
    std::atomic<bool> running_;
    std::atomic<uint64_t> next_replica_id_;
    std::thread worker_thread_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    
    std::map<std::string, ReplicaInfo> replicas_;  // replica_id -> info
    std::map<std::string, std::set<std::string>> dataset_replicas_;  // dataset -> replica_ids
    std::map<std::string, ReplicationPolicy> policies_;  // dataset -> policy
    std::queue<ReplicationJob> pending_jobs_;
    ReplicationStatistics stats_;
};

} // namespace dfsmshsm
#endif // DFSMSHSM_REPLICATION_MANAGER_HPP
