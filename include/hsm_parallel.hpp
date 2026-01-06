/*
 * DFSMShsm Emulator - Parallel Processing Support
 * @version 4.2.0
 *
 * Copyright (c) 2024 DFSMShsm Emulator Project
 * Licensed under the MIT License
 *
 * Provides comprehensive parallel processing capabilities including:
 * - PAV (Parallel Access Volumes) emulation
 * - HyperPAV (Dynamic PAV) emulation
 * - Concurrent I/O optimization
 * - Multi-path I/O management
 * - I/O priority queuing
 * - Work stealing schedulers
 * - Asynchronous I/O patterns
 * - Resource pool management
 */

#ifndef HSM_PARALLEL_HPP
#define HSM_PARALLEL_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <memory>
#include <chrono>
#include <functional>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <thread>
#include <future>
#include <queue>
#include <deque>
#include <optional>
#include <variant>
#include <random>

namespace hsm {
namespace parallel {

// ============================================================================
// Forward Declarations
// ============================================================================

class PAVManager;
class HyperPAVManager;
class IOScheduler;
class WorkStealingPool;
class AsyncIOManager;
class ParallelManager;

// ============================================================================
// Type Aliases
// ============================================================================

using Timestamp = std::chrono::system_clock::time_point;
using Duration = std::chrono::milliseconds;
using VolumeSerial = std::string;
using DeviceAddress = uint16_t;
using UCBAddress = uint32_t;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * Alias types for PAV
 */
enum class AliasType {
    BASE,           // Base device (primary UCB)
    PAV_ALIAS,      // Static PAV alias
    HYPERPAV_ALIAS  // Dynamic HyperPAV alias
};

/**
 * I/O operation types
 */
enum class IOOperationType {
    READ,
    WRITE,
    FORMAT,
    SEEK,
    SENSE,
    RESERVE,
    RELEASE,
    CONTROL
};

/**
 * I/O priority levels
 */
enum class IOPriority {
    CRITICAL = 0,    // Highest priority (system functions)
    HIGH = 1,        // High priority (real-time applications)
    NORMAL = 2,      // Normal priority (typical workload)
    LOW = 3,         // Low priority (batch jobs)
    BACKGROUND = 4   // Lowest priority (housekeeping)
};

/**
 * I/O completion status
 */
enum class IOStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED,
    TIMEOUT
};

/**
 * Alias allocation strategy
 */
enum class AllocationStrategy {
    ROUND_ROBIN,     // Simple round-robin
    LEAST_BUSY,      // Allocate to least busy alias
    AFFINITY,        // Maintain affinity to reduce switching
    RANDOM,          // Random selection
    WEIGHTED         // Weighted by capacity/performance
};

/**
 * Path status
 */
enum class PathStatus {
    ONLINE,
    OFFLINE,
    DEGRADED,
    FAILED,
    RESERVED
};

// ============================================================================
// UCB (Unit Control Block) Emulation
// ============================================================================

/**
 * Emulated z/OS UCB structure for device management
 */
struct UCB {
    UCBAddress address;             // UCB address (3 bytes on z/OS)
    DeviceAddress device_number;    // Device number (e.g., 0A80)
    VolumeSerial volser;            // Volume serial
    
    AliasType alias_type = AliasType::BASE;
    UCBAddress base_ucb_address = 0;    // For aliases, points to base
    
    bool is_online = true;
    bool is_allocated = false;
    std::string job_name;           // Job using this UCB
    std::string step_name;
    
    // Statistics
    uint64_t io_count = 0;
    uint64_t bytes_transferred = 0;
    std::chrono::microseconds total_service_time{0};
    std::chrono::microseconds total_queue_time{0};
    
    Timestamp last_used;
    
    UCB() = default;
    
    UCB(UCBAddress addr, DeviceAddress dev, const VolumeSerial& vol)
        : address(addr), device_number(dev), volser(vol) {}
    
    double getAverageServiceTimeMs() const {
        if (io_count == 0) return 0.0;
        return static_cast<double>(total_service_time.count()) / 
               (1000.0 * static_cast<double>(io_count));
    }
    
    double getAverageQueueTimeMs() const {
        if (io_count == 0) return 0.0;
        return static_cast<double>(total_queue_time.count()) / 
               (1000.0 * static_cast<double>(io_count));
    }
};

// ============================================================================
// I/O Request Structure
// ============================================================================

/**
 * I/O request for queuing and processing
 */
struct IORequest {
    uint64_t request_id;
    IOOperationType operation;
    IOPriority priority = IOPriority::NORMAL;
    
    VolumeSerial volser;
    uint64_t starting_block = 0;
    uint64_t block_count = 0;
    
    std::vector<uint8_t> data;
    
    std::string job_name;
    std::string step_name;
    std::string dataset_name;
    
    Timestamp submitted_time;
    Timestamp start_time;
    Timestamp completion_time;
    
    IOStatus status = IOStatus::PENDING;
    std::string error_message;
    
    // Completion handling
    std::promise<bool> completion_promise;
    std::function<void(const IORequest&)> completion_callback;
    
    // Affinity hints
    UCBAddress preferred_ucb = 0;
    bool require_affinity = false;
    
    IORequest() : request_id(0), operation(IOOperationType::READ) {
        submitted_time = std::chrono::system_clock::now();
    }
    
    Duration getQueueTime() const {
        return std::chrono::duration_cast<Duration>(start_time - submitted_time);
    }
    
    Duration getServiceTime() const {
        return std::chrono::duration_cast<Duration>(completion_time - start_time);
    }
    
    Duration getTotalTime() const {
        return std::chrono::duration_cast<Duration>(completion_time - submitted_time);
    }
};

// ============================================================================
// PAV (Parallel Access Volumes) Manager
// ============================================================================

/**
 * Static PAV configuration for a volume
 */
struct PAVConfiguration {
    VolumeSerial volser;
    DeviceAddress base_device;
    std::vector<DeviceAddress> alias_devices;
    
    uint8_t max_aliases = 8;
    AllocationStrategy strategy = AllocationStrategy::LEAST_BUSY;
    
    // Performance thresholds
    double busy_threshold = 0.7;      // When to prefer alternate aliases
    Duration max_queue_time{100};     // Max acceptable queue time
};

/**
 * PAV Manager - manages static PAV aliases
 */
class PAVManager {
public:
    struct Config {
        Config() = default;
        uint8_t default_aliases_per_volume = 4;
        AllocationStrategy default_strategy = AllocationStrategy::LEAST_BUSY;
        bool enable_statistics = true;
    };
    
private:
    Config config_;
    
    mutable std::shared_mutex mutex_;
    
    // UCB management
    std::map<UCBAddress, std::unique_ptr<UCB>> ucbs_;
    UCBAddress next_ucb_address_ = 0x00010000;
    
    // Volume to UCB mappings
    std::map<VolumeSerial, UCBAddress> base_ucb_map_;
    std::map<VolumeSerial, std::vector<UCBAddress>> alias_ucb_map_;
    
    // PAV configurations
    std::map<VolumeSerial, PAVConfiguration> pav_configs_;
    
    // Request ID generator
    std::atomic<uint64_t> next_request_id_{1};
    
public:
    PAVManager() : config_() {}
    explicit PAVManager(const Config& config) : config_(config) {}
    
    /**
     * Define a volume with PAV support
     */
    bool defineVolume(const VolumeSerial& volser, DeviceAddress base_device,
                     uint8_t num_aliases = 0) {
        std::unique_lock lock(mutex_);
        
        if (base_ucb_map_.find(volser) != base_ucb_map_.end()) {
            return false;  // Already defined
        }
        
        // Create base UCB
        UCBAddress base_addr = next_ucb_address_++;
        auto base_ucb = std::make_unique<UCB>(base_addr, base_device, volser);
        base_ucb->alias_type = AliasType::BASE;
        
        base_ucb_map_[volser] = base_addr;
        ucbs_[base_addr] = std::move(base_ucb);
        
        // Create aliases
        uint8_t aliases = (num_aliases > 0) ? num_aliases : config_.default_aliases_per_volume;
        std::vector<UCBAddress> alias_addrs;
        
        PAVConfiguration pav_config;
        pav_config.volser = volser;
        pav_config.base_device = base_device;
        pav_config.max_aliases = aliases;
        pav_config.strategy = config_.default_strategy;
        
        for (uint8_t i = 0; i < aliases; ++i) {
            UCBAddress alias_addr = next_ucb_address_++;
            DeviceAddress alias_dev = base_device + i + 1;
            
            auto alias_ucb = std::make_unique<UCB>(alias_addr, alias_dev, volser);
            alias_ucb->alias_type = AliasType::PAV_ALIAS;
            alias_ucb->base_ucb_address = base_addr;
            
            pav_config.alias_devices.push_back(alias_dev);
            alias_addrs.push_back(alias_addr);
            ucbs_[alias_addr] = std::move(alias_ucb);
        }
        
        alias_ucb_map_[volser] = alias_addrs;
        pav_configs_[volser] = pav_config;
        
        return true;
    }
    
    /**
     * Allocate a UCB for I/O to a volume
     */
    std::optional<UCBAddress> allocateUCB(const VolumeSerial& volser,
                                          const std::string& job_name,
                                          UCBAddress preferred = 0) {
        std::unique_lock lock(mutex_);
        
        auto base_it = base_ucb_map_.find(volser);
        if (base_it == base_ucb_map_.end()) {
            return std::nullopt;  // Volume not defined
        }
        
        // Get configuration
        auto config_it = pav_configs_.find(volser);
        AllocationStrategy strategy = (config_it != pav_configs_.end()) ?
            config_it->second.strategy : config_.default_strategy;
        
        // Build list of available UCBs (base + aliases)
        std::vector<UCBAddress> candidates;
        candidates.push_back(base_it->second);
        
        auto alias_it = alias_ucb_map_.find(volser);
        if (alias_it != alias_ucb_map_.end()) {
            candidates.insert(candidates.end(), 
                            alias_it->second.begin(), 
                            alias_it->second.end());
        }
        
        // Apply allocation strategy
        UCBAddress selected = 0;
        
        switch (strategy) {
            case AllocationStrategy::ROUND_ROBIN:
                selected = selectRoundRobin(candidates);
                break;
                
            case AllocationStrategy::LEAST_BUSY:
                selected = selectLeastBusy(candidates);
                break;
                
            case AllocationStrategy::AFFINITY:
                selected = selectWithAffinity(candidates, preferred, job_name);
                break;
                
            case AllocationStrategy::RANDOM:
                selected = selectRandom(candidates);
                break;
                
            case AllocationStrategy::WEIGHTED:
                selected = selectWeighted(candidates);
                break;
                
            default:
                selected = selectLeastBusy(candidates);
        }
        
        if (selected != 0) {
            auto& ucb = ucbs_[selected];
            ucb->is_allocated = true;
            ucb->job_name = job_name;
            ucb->last_used = std::chrono::system_clock::now();
        }
        
        return selected;
    }
    
    /**
     * Release a UCB after I/O completion
     */
    void releaseUCB(UCBAddress address) {
        std::unique_lock lock(mutex_);
        
        auto it = ucbs_.find(address);
        if (it != ucbs_.end()) {
            it->second->is_allocated = false;
            it->second->job_name.clear();
            it->second->step_name.clear();
        }
    }
    
    /**
     * Record I/O completion statistics
     */
    void recordIOComplete(UCBAddress address, uint64_t bytes,
                         std::chrono::microseconds service_time,
                         std::chrono::microseconds queue_time) {
        std::unique_lock lock(mutex_);
        
        auto it = ucbs_.find(address);
        if (it != ucbs_.end()) {
            auto& ucb = it->second;
            ucb->io_count++;
            ucb->bytes_transferred += bytes;
            ucb->total_service_time += service_time;
            ucb->total_queue_time += queue_time;
        }
    }
    
    /**
     * Get UCB information
     */
    std::optional<UCB> getUCB(UCBAddress address) const {
        std::shared_lock lock(mutex_);
        auto it = ucbs_.find(address);
        if (it == ucbs_.end()) return std::nullopt;
        return *(it->second);
    }
    
    /**
     * Get all UCBs for a volume
     */
    std::vector<UCB> getVolumeUCBs(const VolumeSerial& volser) const {
        std::shared_lock lock(mutex_);
        std::vector<UCB> result;
        
        auto base_it = base_ucb_map_.find(volser);
        if (base_it != base_ucb_map_.end()) {
            auto ucb_it = ucbs_.find(base_it->second);
            if (ucb_it != ucbs_.end()) {
                result.push_back(*(ucb_it->second));
            }
        }
        
        auto alias_it = alias_ucb_map_.find(volser);
        if (alias_it != alias_ucb_map_.end()) {
            for (UCBAddress addr : alias_it->second) {
                auto ucb_it = ucbs_.find(addr);
                if (ucb_it != ucbs_.end()) {
                    result.push_back(*(ucb_it->second));
                }
            }
        }
        
        return result;
    }
    
    /**
     * Get PAV statistics for a volume
     */
    struct PAVStatistics {
        VolumeSerial volser;
        uint8_t total_ucbs = 0;
        uint8_t allocated_ucbs = 0;
        uint64_t total_ios = 0;
        uint64_t total_bytes = 0;
        double avg_service_time_ms = 0.0;
        double avg_queue_time_ms = 0.0;
        double utilization = 0.0;
    };
    
    PAVStatistics getStatistics(const VolumeSerial& volser) const {
        std::shared_lock lock(mutex_);
        PAVStatistics stats;
        stats.volser = volser;
        
        auto ucbs = getVolumeUCBs(volser);
        stats.total_ucbs = static_cast<uint8_t>(ucbs.size());
        
        for (const auto& ucb : ucbs) {
            if (ucb.is_allocated) stats.allocated_ucbs++;
            stats.total_ios += ucb.io_count;
            stats.total_bytes += ucb.bytes_transferred;
        }
        
        if (stats.total_ios > 0) {
            double total_service = 0, total_queue = 0;
            for (const auto& ucb : ucbs) {
                total_service += static_cast<double>(ucb.total_service_time.count());
                total_queue += static_cast<double>(ucb.total_queue_time.count());
            }
            stats.avg_service_time_ms = total_service / (1000.0 * stats.total_ios);
            stats.avg_queue_time_ms = total_queue / (1000.0 * stats.total_ios);
        }
        
        if (stats.total_ucbs > 0) {
            stats.utilization = static_cast<double>(stats.allocated_ucbs) / 
                               static_cast<double>(stats.total_ucbs);
        }
        
        return stats;
    }
    
    /**
     * Get number of aliases for a volume
     */
    size_t getAliasCount(const VolumeSerial& volser) const {
        std::shared_lock lock(mutex_);
        auto it = alias_ucb_map_.find(volser);
        return (it != alias_ucb_map_.end()) ? it->second.size() : 0;
    }
    
private:
    mutable size_t round_robin_index_ = 0;
    
    UCBAddress selectRoundRobin(const std::vector<UCBAddress>& candidates) {
        if (candidates.empty()) return 0;
        
        // Find first non-allocated
        for (size_t i = 0; i < candidates.size(); ++i) {
            size_t idx = (round_robin_index_ + i) % candidates.size();
            auto& ucb = ucbs_[candidates[idx]];
            if (!ucb->is_allocated) {
                round_robin_index_ = idx + 1;
                return candidates[idx];
            }
        }
        
        // All allocated, return round-robin anyway
        UCBAddress result = candidates[round_robin_index_ % candidates.size()];
        round_robin_index_++;
        return result;
    }
    
    UCBAddress selectLeastBusy(const std::vector<UCBAddress>& candidates) {
        if (candidates.empty()) return 0;
        
        UCBAddress best = candidates[0];
        uint64_t min_ios = std::numeric_limits<uint64_t>::max();
        bool found_free = false;
        
        for (UCBAddress addr : candidates) {
            auto& ucb = ucbs_[addr];
            
            // Prefer unallocated
            if (!ucb->is_allocated && !found_free) {
                best = addr;
                min_ios = ucb->io_count;
                found_free = true;
            } else if (found_free && !ucb->is_allocated) {
                // Among free UCBs, pick least used
                if (ucb->io_count < min_ios) {
                    best = addr;
                    min_ios = ucb->io_count;
                }
            } else if (!found_free) {
                // All busy, pick least used
                if (ucb->io_count < min_ios) {
                    best = addr;
                    min_ios = ucb->io_count;
                }
            }
        }
        
        return best;
    }
    
    UCBAddress selectWithAffinity(const std::vector<UCBAddress>& candidates,
                                  UCBAddress preferred,
                                  const std::string& job_name) {
        if (candidates.empty()) return 0;
        
        // Try preferred first
        if (preferred != 0) {
            auto it = ucbs_.find(preferred);
            if (it != ucbs_.end() && !it->second->is_allocated) {
                return preferred;
            }
        }
        
        // Try to find UCB previously used by this job
        for (UCBAddress addr : candidates) {
            auto& ucb = ucbs_[addr];
            if (!ucb->is_allocated && ucb->job_name == job_name) {
                return addr;
            }
        }
        
        // Fall back to least busy
        return selectLeastBusy(candidates);
    }
    
    UCBAddress selectRandom(const std::vector<UCBAddress>& candidates) {
        if (candidates.empty()) return 0;
        
        // Filter to non-allocated
        std::vector<UCBAddress> available;
        for (UCBAddress addr : candidates) {
            if (!ucbs_[addr]->is_allocated) {
                available.push_back(addr);
            }
        }
        
        if (available.empty()) {
            available = candidates;  // All busy, pick from all
        }
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dist(0, available.size() - 1);
        
        return available[dist(gen)];
    }
    
    UCBAddress selectWeighted(const std::vector<UCBAddress>& candidates) {
        if (candidates.empty()) return 0;
        
        // Weight by inverse of average service time
        std::vector<double> weights;
        double total_weight = 0;
        
        for (UCBAddress addr : candidates) {
            auto& ucb = ucbs_[addr];
            double avg_time = ucb->getAverageServiceTimeMs();
            double weight = (avg_time > 0) ? (1.0 / avg_time) : 1.0;
            
            // Penalty for allocated UCBs
            if (ucb->is_allocated) {
                weight *= 0.1;
            }
            
            weights.push_back(weight);
            total_weight += weight;
        }
        
        // Random weighted selection
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(0, total_weight);
        double random_val = dist(gen);
        
        double cumulative = 0;
        for (size_t i = 0; i < candidates.size(); ++i) {
            cumulative += weights[i];
            if (random_val <= cumulative) {
                return candidates[i];
            }
        }
        
        return candidates.back();
    }
};

// ============================================================================
// HyperPAV Manager
// ============================================================================

/**
 * HyperPAV pool configuration
 */
struct HyperPAVPool {
    std::string pool_name;
    std::vector<DeviceAddress> alias_devices;
    
    uint16_t min_aliases = 1;
    uint16_t max_aliases = 255;
    uint16_t current_aliases = 0;
    
    // Dynamic scaling parameters
    double scale_up_threshold = 0.8;    // Queue time threshold (ms)
    double scale_down_threshold = 0.3;
    Duration evaluation_interval{5000}; // 5 seconds
    
    bool auto_scale = true;
};

/**
 * HyperPAV Manager - manages dynamic PAV alias pools
 */
class HyperPAVManager {
public:
    struct Config {
        Config() = default;
        uint16_t default_pool_size = 16;
        Duration rebalance_interval{10000};  // 10 seconds
        bool enable_auto_scaling = true;
        double utilization_target = 0.7;
    };
    
private:
    Config config_;
    PAVManager& pav_manager_;  // Underlying PAV support
    
    mutable std::shared_mutex mutex_;
    
    // Alias pools
    std::map<std::string, HyperPAVPool> pools_;
    
    // Volume to pool assignments
    std::map<VolumeSerial, std::string> volume_pool_map_;
    
    // Dynamic alias assignments (alias device -> volume)
    std::map<DeviceAddress, VolumeSerial> alias_assignments_;
    
    // Pool statistics
    struct PoolStats {
        uint64_t allocations = 0;
        uint64_t releases = 0;
        uint64_t scale_ups = 0;
        uint64_t scale_downs = 0;
        Duration total_assignment_time{0};
    };
    std::map<std::string, PoolStats> pool_stats_;
    
    // Background thread for auto-scaling
    std::atomic<bool> running_{false};
    std::thread scaling_thread_;
    
public:
    explicit HyperPAVManager(PAVManager& pav_manager) 
        : config_(), pav_manager_(pav_manager) {}
    HyperPAVManager(PAVManager& pav_manager, const Config& config)
        : config_(config), pav_manager_(pav_manager) {}
    
    ~HyperPAVManager() {
        stop();
    }
    
    /**
     * Start background auto-scaling
     */
    void start() {
        if (running_.exchange(true)) return;
        
        scaling_thread_ = std::thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(config_.rebalance_interval);
                if (running_ && config_.enable_auto_scaling) {
                    rebalanceAliases();
                }
            }
        });
    }
    
    /**
     * Stop background thread
     */
    void stop() {
        running_ = false;
        if (scaling_thread_.joinable()) {
            scaling_thread_.join();
        }
    }
    
    /**
     * Define a HyperPAV alias pool
     */
    bool definePool(const std::string& pool_name,
                   const std::vector<DeviceAddress>& devices,
                   uint16_t min_aliases = 1,
                   uint16_t max_aliases = 255) {
        std::unique_lock lock(mutex_);
        
        if (pools_.find(pool_name) != pools_.end()) {
            return false;  // Already exists
        }
        
        HyperPAVPool pool;
        pool.pool_name = pool_name;
        pool.alias_devices = devices;
        pool.min_aliases = min_aliases;
        pool.max_aliases = std::min(max_aliases, 
                                   static_cast<uint16_t>(devices.size()));
        pool.current_aliases = min_aliases;
        
        pools_[pool_name] = pool;
        pool_stats_[pool_name] = PoolStats{};
        
        return true;
    }
    
    /**
     * Assign a volume to a HyperPAV pool
     */
    bool assignVolumeToPool(const VolumeSerial& volser, 
                           const std::string& pool_name) {
        std::unique_lock lock(mutex_);
        
        if (pools_.find(pool_name) == pools_.end()) {
            return false;  // Pool doesn't exist
        }
        
        volume_pool_map_[volser] = pool_name;
        return true;
    }
    
    /**
     * Dynamically allocate an alias for a volume
     */
    std::optional<DeviceAddress> allocateAlias(const VolumeSerial& volser,
                                               const std::string& job_name) {
        std::unique_lock lock(mutex_);
        
        // Find pool for this volume
        auto pool_it = volume_pool_map_.find(volser);
        if (pool_it == volume_pool_map_.end()) {
            return std::nullopt;  // Volume not in any pool
        }
        
        auto& pool = pools_[pool_it->second];
        auto& stats = pool_stats_[pool_it->second];
        
        // Find available alias
        for (DeviceAddress dev : pool.alias_devices) {
            auto assign_it = alias_assignments_.find(dev);
            
            // Unassigned or assigned to same volume
            if (assign_it == alias_assignments_.end()) {
                alias_assignments_[dev] = volser;
                stats.allocations++;
                return dev;
            }
            
            if (assign_it->second == volser) {
                // Already assigned to this volume, can share
                stats.allocations++;
                return dev;
            }
        }
        
        // Pool exhausted - try to reassign least recently used
        // (simplified - would track LRU in production)
        return std::nullopt;
    }
    
    /**
     * Release an alias assignment
     */
    void releaseAlias(DeviceAddress device, const VolumeSerial& volser) {
        std::unique_lock lock(mutex_);
        
        auto it = alias_assignments_.find(device);
        if (it != alias_assignments_.end() && it->second == volser) {
            alias_assignments_.erase(it);
            
            // Update stats
            auto pool_it = volume_pool_map_.find(volser);
            if (pool_it != volume_pool_map_.end()) {
                pool_stats_[pool_it->second].releases++;
            }
        }
    }
    
    /**
     * Get pool statistics
     */
    struct PoolStatistics {
        std::string pool_name;
        uint16_t total_aliases;
        uint16_t assigned_aliases;
        uint16_t volumes_served;
        uint64_t total_allocations;
        double utilization;
    };
    
    PoolStatistics getPoolStatistics(const std::string& pool_name) const {
        std::shared_lock lock(mutex_);
        PoolStatistics stats;
        stats.pool_name = pool_name;
        
        auto pool_it = pools_.find(pool_name);
        if (pool_it == pools_.end()) return stats;
        
        const auto& pool = pool_it->second;
        stats.total_aliases = static_cast<uint16_t>(pool.alias_devices.size());
        
        // Count assigned aliases
        std::set<VolumeSerial> volumes;
        for (DeviceAddress dev : pool.alias_devices) {
            auto assign_it = alias_assignments_.find(dev);
            if (assign_it != alias_assignments_.end()) {
                stats.assigned_aliases++;
                volumes.insert(assign_it->second);
            }
        }
        
        stats.volumes_served = static_cast<uint16_t>(volumes.size());
        
        auto stat_it = pool_stats_.find(pool_name);
        if (stat_it != pool_stats_.end()) {
            stats.total_allocations = stat_it->second.allocations;
        }
        
        stats.utilization = (stats.total_aliases > 0) ?
            static_cast<double>(stats.assigned_aliases) / stats.total_aliases : 0.0;
        
        return stats;
    }
    
    /**
     * Get all pool names
     */
    std::vector<std::string> getPoolNames() const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> names;
        for (const auto& [name, _] : pools_) {
            names.push_back(name);
        }
        return names;
    }
    
private:
    void rebalanceAliases() {
        std::unique_lock lock(mutex_);
        
        // For each pool, evaluate if rebalancing is needed
        for (auto& [pool_name, pool] : pools_) {
            if (!pool.auto_scale) continue;
            
            // Count current utilization
            size_t assigned = 0;
            for (DeviceAddress dev : pool.alias_devices) {
                if (alias_assignments_.find(dev) != alias_assignments_.end()) {
                    assigned++;
                }
            }
            
            double utilization = static_cast<double>(assigned) / pool.alias_devices.size();
            
            // Scale up if high utilization
            if (utilization > pool.scale_up_threshold && 
                pool.current_aliases < pool.max_aliases) {
                pool.current_aliases = std::min(
                    static_cast<uint16_t>(pool.current_aliases + 1),
                    pool.max_aliases);
                pool_stats_[pool_name].scale_ups++;
            }
            
            // Scale down if low utilization
            if (utilization < pool.scale_down_threshold && 
                pool.current_aliases > pool.min_aliases) {
                pool.current_aliases = std::max(
                    static_cast<uint16_t>(pool.current_aliases - 1),
                    pool.min_aliases);
                pool_stats_[pool_name].scale_downs++;
            }
        }
    }
};

// ============================================================================
// I/O Priority Queue
// ============================================================================

/**
 * Priority queue for I/O requests
 */
class IOPriorityQueue {
private:
    struct QueueEntry {
        std::shared_ptr<IORequest> request;
        
        bool operator<(const QueueEntry& other) const {
            // Lower priority value = higher priority
            if (request->priority != other.request->priority) {
                return static_cast<int>(request->priority) > 
                       static_cast<int>(other.request->priority);
            }
            // Same priority: FIFO by submission time
            return request->submitted_time > other.request->submitted_time;
        }
    };
    
    std::priority_queue<QueueEntry> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    std::atomic<size_t> size_{0};
    std::atomic<bool> closed_{false};
    
    // Statistics
    std::array<std::atomic<uint64_t>, 5> enqueue_counts_;  // Per priority
    std::array<std::atomic<uint64_t>, 5> dequeue_counts_;
    
public:
    IOPriorityQueue() {
        for (auto& count : enqueue_counts_) count = 0;
        for (auto& count : dequeue_counts_) count = 0;
    }
    
    /**
     * Enqueue a request
     */
    void enqueue(std::shared_ptr<IORequest> request) {
        {
            std::lock_guard lock(mutex_);
            queue_.push({request});
            size_++;
            enqueue_counts_[static_cast<size_t>(request->priority)]++;
        }
        cv_.notify_one();
    }
    
    /**
     * Dequeue the highest priority request (blocking)
     */
    std::shared_ptr<IORequest> dequeue() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this]() { return !queue_.empty() || closed_; });
        
        if (queue_.empty()) {
            return nullptr;
        }
        
        auto entry = queue_.top();
        queue_.pop();
        size_--;
        dequeue_counts_[static_cast<size_t>(entry.request->priority)]++;
        
        return entry.request;
    }
    
    /**
     * Try to dequeue (non-blocking)
     */
    std::shared_ptr<IORequest> tryDequeue() {
        std::lock_guard lock(mutex_);
        
        if (queue_.empty()) {
            return nullptr;
        }
        
        auto entry = queue_.top();
        queue_.pop();
        size_--;
        dequeue_counts_[static_cast<size_t>(entry.request->priority)]++;
        
        return entry.request;
    }
    
    /**
     * Dequeue with timeout
     */
    std::shared_ptr<IORequest> dequeue(Duration timeout) {
        std::unique_lock lock(mutex_);
        
        if (!cv_.wait_for(lock, timeout, [this]() { 
            return !queue_.empty() || closed_; 
        })) {
            return nullptr;  // Timeout
        }
        
        if (queue_.empty()) {
            return nullptr;
        }
        
        auto entry = queue_.top();
        queue_.pop();
        size_--;
        dequeue_counts_[static_cast<size_t>(entry.request->priority)]++;
        
        return entry.request;
    }
    
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    
    void close() {
        closed_ = true;
        cv_.notify_all();
    }
    
    struct QueueStatistics {
        size_t current_size;
        std::array<uint64_t, 5> enqueued_by_priority;
        std::array<uint64_t, 5> dequeued_by_priority;
    };
    
    QueueStatistics getStatistics() const {
        QueueStatistics stats;
        stats.current_size = size_;
        for (size_t i = 0; i < 5; ++i) {
            stats.enqueued_by_priority[i] = enqueue_counts_[i];
            stats.dequeued_by_priority[i] = dequeue_counts_[i];
        }
        return stats;
    }
};

// ============================================================================
// Work Stealing Thread Pool
// ============================================================================

/**
 * Work stealing thread pool for parallel I/O processing
 */
class WorkStealingPool {
public:
    using Task = std::function<void()>;
    
    struct Config {
        Config() : num_threads(std::thread::hardware_concurrency()) {}
        size_t num_threads;
        size_t queue_size = 1024;
        bool enable_work_stealing = true;
    };
    
private:
    Config config_;
    
    std::vector<std::thread> threads_;
    std::vector<std::deque<Task>> local_queues_;
    std::vector<std::mutex> queue_mutexes_;
    
    std::atomic<bool> running_{false};
    std::atomic<size_t> task_count_{0};
    std::atomic<size_t> completed_count_{0};
    
    // Statistics per thread
    struct ThreadStats {
        std::atomic<uint64_t> tasks_executed{0};
        std::atomic<uint64_t> tasks_stolen{0};
        std::atomic<uint64_t> steal_attempts{0};
    };
    std::vector<ThreadStats> thread_stats_;
    
public:
    WorkStealingPool() 
        : config_(),
          local_queues_(config_.num_threads),
          queue_mutexes_(config_.num_threads),
          thread_stats_(config_.num_threads) {}
    
    explicit WorkStealingPool(const Config& config) 
        : config_(config),
          local_queues_(config.num_threads),
          queue_mutexes_(config.num_threads),
          thread_stats_(config.num_threads) {}
    
    ~WorkStealingPool() {
        stop();
    }
    
    /**
     * Start the thread pool
     */
    void start() {
        if (running_.exchange(true)) return;
        
        for (size_t i = 0; i < config_.num_threads; ++i) {
            threads_.emplace_back([this, i]() {
                workerLoop(i);
            });
        }
    }
    
    /**
     * Stop the thread pool
     */
    void stop() {
        running_ = false;
        
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        threads_.clear();
    }
    
    /**
     * Submit a task to the pool
     */
    void submit(Task task) {
        // Distribute to thread with smallest queue
        size_t min_idx = 0;
        size_t min_size = std::numeric_limits<size_t>::max();
        
        for (size_t i = 0; i < config_.num_threads; ++i) {
            std::lock_guard lock(queue_mutexes_[i]);
            if (local_queues_[i].size() < min_size) {
                min_size = local_queues_[i].size();
                min_idx = i;
            }
        }
        
        {
            std::lock_guard lock(queue_mutexes_[min_idx]);
            local_queues_[min_idx].push_back(std::move(task));
        }
        
        task_count_++;
    }
    
    /**
     * Submit with future for result
     */
    template<typename F, typename R = std::invoke_result_t<F>>
    std::future<R> submitWithFuture(F&& func) {
        auto promise = std::make_shared<std::promise<R>>();
        auto future = promise->get_future();
        
        submit([promise, func = std::forward<F>(func)]() {
            try {
                if constexpr (std::is_void_v<R>) {
                    func();
                    promise->set_value();
                } else {
                    promise->set_value(func());
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });
        
        return future;
    }
    
    /**
     * Wait for all tasks to complete
     */
    void waitAll() {
        while (task_count_ > completed_count_) {
            std::this_thread::yield();
        }
    }
    
    /**
     * Get pool statistics
     */
    struct PoolStatistics {
        size_t num_threads;
        size_t pending_tasks;
        uint64_t completed_tasks;
        std::vector<uint64_t> tasks_per_thread;
        std::vector<uint64_t> steals_per_thread;
        double steal_rate;
    };
    
    PoolStatistics getStatistics() const {
        PoolStatistics stats;
        stats.num_threads = config_.num_threads;
        stats.pending_tasks = task_count_ - completed_count_;
        stats.completed_tasks = completed_count_;
        
        uint64_t total_tasks = 0;
        uint64_t total_steals = 0;
        
        for (size_t i = 0; i < config_.num_threads; ++i) {
            uint64_t tasks = thread_stats_[i].tasks_executed;
            uint64_t steals = thread_stats_[i].tasks_stolen;
            
            stats.tasks_per_thread.push_back(tasks);
            stats.steals_per_thread.push_back(steals);
            
            total_tasks += tasks;
            total_steals += steals;
        }
        
        stats.steal_rate = (total_tasks > 0) ?
            static_cast<double>(total_steals) / total_tasks : 0.0;
        
        return stats;
    }
    
    size_t getThreadCount() const { return config_.num_threads; }
    
private:
    void workerLoop(size_t thread_id) {
        while (running_) {
            Task task;
            
            // Try local queue first
            {
                std::lock_guard lock(queue_mutexes_[thread_id]);
                if (!local_queues_[thread_id].empty()) {
                    task = std::move(local_queues_[thread_id].front());
                    local_queues_[thread_id].pop_front();
                }
            }
            
            // Try work stealing
            if (!task && config_.enable_work_stealing) {
                task = trySteal(thread_id);
                if (task) {
                    thread_stats_[thread_id].tasks_stolen++;
                }
            }
            
            // Execute task
            if (task) {
                task();
                thread_stats_[thread_id].tasks_executed++;
                completed_count_++;
            } else {
                // No work available, yield
                std::this_thread::yield();
            }
        }
    }
    
    Task trySteal(size_t thread_id) {
        // Random starting point for stealing
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        
        std::uniform_int_distribution<size_t> dist(0, config_.num_threads - 1);
        
        for (size_t attempts = 0; attempts < config_.num_threads; ++attempts) {
            size_t victim = dist(gen);
            if (victim == thread_id) continue;
            
            thread_stats_[thread_id].steal_attempts++;
            
            std::lock_guard lock(queue_mutexes_[victim]);
            if (!local_queues_[victim].empty()) {
                Task task = std::move(local_queues_[victim].back());
                local_queues_[victim].pop_back();
                return task;
            }
        }
        
        return nullptr;
    }
};

// ============================================================================
// Asynchronous I/O Manager
// ============================================================================

/**
 * Async I/O operation result
 */
struct AsyncIOResult {
    uint64_t request_id;
    IOStatus status;
    uint64_t bytes_transferred = 0;
    Duration elapsed_time;
    std::string error_message;
    std::vector<uint8_t> data;  // For read operations
};

/**
 * Asynchronous I/O Manager
 */
class AsyncIOManager {
public:
    using CompletionCallback = std::function<void(const AsyncIOResult&)>;
    
    struct Config {
        Config() = default;
        size_t max_outstanding = 256;
        size_t io_threads = 4;
        Duration default_timeout{30000};  // 30 seconds
        bool enable_batching = true;
        size_t batch_size = 16;
    };
    
private:
    Config config_;
    WorkStealingPool thread_pool_;
    IOPriorityQueue request_queue_;
    
    mutable std::shared_mutex mutex_;
    std::map<uint64_t, std::shared_ptr<IORequest>> pending_requests_;
    
    std::atomic<uint64_t> next_request_id_{1};
    std::atomic<size_t> outstanding_count_{0};
    
    // Statistics
    std::atomic<uint64_t> total_submitted_{0};
    std::atomic<uint64_t> total_completed_{0};
    std::atomic<uint64_t> total_failed_{0};
    std::atomic<uint64_t> total_bytes_{0};
    
    std::atomic<bool> running_{false};
    std::vector<std::thread> io_threads_;
    
public:
    AsyncIOManager()
        : config_(),
          thread_pool_() {}
    
    explicit AsyncIOManager(const Config& config)
        : config_(config),
          thread_pool_(makePoolConfig(config.io_threads)) {}
    
    ~AsyncIOManager() {
        stop();
    }
    
private:
    static WorkStealingPool::Config makePoolConfig(size_t threads) {
        WorkStealingPool::Config cfg;
        cfg.num_threads = threads;
        return cfg;
    }
    
public:
    
    /**
     * Start the async I/O system
     */
    void start() {
        if (running_.exchange(true)) return;
        
        thread_pool_.start();
        
        // Start I/O processing threads
        for (size_t i = 0; i < config_.io_threads; ++i) {
            io_threads_.emplace_back([this]() {
                processIOLoop();
            });
        }
    }
    
    /**
     * Stop the async I/O system
     */
    void stop() {
        running_ = false;
        request_queue_.close();
        
        for (auto& thread : io_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        io_threads_.clear();
        
        thread_pool_.stop();
    }
    
    /**
     * Submit an async read operation
     */
    std::future<AsyncIOResult> asyncRead(
        const VolumeSerial& volser,
        uint64_t starting_block,
        uint64_t block_count,
        IOPriority priority = IOPriority::NORMAL) {
        
        auto request = std::make_shared<IORequest>();
        request->request_id = next_request_id_++;
        request->operation = IOOperationType::READ;
        request->priority = priority;
        request->volser = volser;
        request->starting_block = starting_block;
        request->block_count = block_count;
        
        return submitRequest(request);
    }
    
    /**
     * Submit an async write operation
     */
    std::future<AsyncIOResult> asyncWrite(
        const VolumeSerial& volser,
        uint64_t starting_block,
        const std::vector<uint8_t>& data,
        IOPriority priority = IOPriority::NORMAL) {
        
        auto request = std::make_shared<IORequest>();
        request->request_id = next_request_id_++;
        request->operation = IOOperationType::WRITE;
        request->priority = priority;
        request->volser = volser;
        request->starting_block = starting_block;
        request->block_count = (data.size() + 4095) / 4096;  // Assume 4K blocks
        request->data = data;
        
        return submitRequest(request);
    }
    
    /**
     * Submit a generic I/O request
     */
    std::future<AsyncIOResult> submitRequest(std::shared_ptr<IORequest> request) {
        if (outstanding_count_ >= config_.max_outstanding) {
            // Return immediately with error
            std::promise<AsyncIOResult> promise;
            AsyncIOResult result;
            result.request_id = request->request_id;
            result.status = IOStatus::FAILED;
            result.error_message = "Maximum outstanding requests exceeded";
            promise.set_value(result);
            return promise.get_future();
        }
        
        auto future = request->completion_promise.get_future();
        
        {
            std::unique_lock lock(mutex_);
            pending_requests_[request->request_id] = request;
        }
        
        outstanding_count_++;
        total_submitted_++;
        
        request_queue_.enqueue(request);
        
        // Convert promise<bool> to future<AsyncIOResult>
        return thread_pool_.submitWithFuture([this, request]() -> AsyncIOResult {
            try {
                request->completion_promise.get_future().get();
            } catch (...) {
                // Ignore - we'll check status
            }
            
            AsyncIOResult result;
            result.request_id = request->request_id;
            result.status = request->status;
            result.bytes_transferred = request->block_count * 4096;
            result.elapsed_time = request->getTotalTime();
            result.error_message = request->error_message;
            result.data = std::move(request->data);
            
            return result;
        });
    }
    
    /**
     * Cancel a pending request
     */
    bool cancelRequest(uint64_t request_id) {
        std::unique_lock lock(mutex_);
        
        auto it = pending_requests_.find(request_id);
        if (it == pending_requests_.end()) {
            return false;
        }
        
        it->second->status = IOStatus::CANCELLED;
        it->second->completion_promise.set_value(false);
        pending_requests_.erase(it);
        outstanding_count_--;
        
        return true;
    }
    
    /**
     * Wait for all pending operations
     */
    void waitAll(Duration timeout = Duration::max()) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        
        while (outstanding_count_ > 0) {
            if (std::chrono::steady_clock::now() >= deadline) {
                break;
            }
            std::this_thread::sleep_for(Duration(10));
        }
    }
    
    /**
     * Get I/O statistics
     */
    struct IOStatistics {
        uint64_t total_submitted;
        uint64_t total_completed;
        uint64_t total_failed;
        uint64_t total_bytes;
        size_t outstanding;
        size_t queue_depth;
    };
    
    IOStatistics getStatistics() const {
        IOStatistics stats;
        stats.total_submitted = total_submitted_;
        stats.total_completed = total_completed_;
        stats.total_failed = total_failed_;
        stats.total_bytes = total_bytes_;
        stats.outstanding = outstanding_count_;
        stats.queue_depth = request_queue_.size();
        return stats;
    }
    
private:
    void processIOLoop() {
        while (running_) {
            auto request = request_queue_.dequeue(Duration(100));
            
            if (!request) continue;
            
            processRequest(request);
        }
    }
    
    void processRequest(std::shared_ptr<IORequest> request) {
        request->status = IOStatus::RUNNING;
        request->start_time = std::chrono::system_clock::now();
        
        try {
            // Simulate I/O operation
            simulateIO(request);
            
            request->status = IOStatus::COMPLETED;
            request->completion_time = std::chrono::system_clock::now();
            request->completion_promise.set_value(true);
            
            total_completed_++;
            total_bytes_ += request->block_count * 4096;
            
        } catch (const std::exception& e) {
            request->status = IOStatus::FAILED;
            request->error_message = e.what();
            request->completion_time = std::chrono::system_clock::now();
            request->completion_promise.set_value(false);
            
            total_failed_++;
        }
        
        // Remove from pending
        {
            std::unique_lock lock(mutex_);
            pending_requests_.erase(request->request_id);
        }
        outstanding_count_--;
        
        // Call completion callback if set
        if (request->completion_callback) {
            AsyncIOResult result;
            result.request_id = request->request_id;
            result.status = request->status;
            result.bytes_transferred = request->block_count * 4096;
            result.elapsed_time = request->getServiceTime();
            result.error_message = request->error_message;
            
            request->completion_callback(*request);
        }
    }
    
    void simulateIO(std::shared_ptr<IORequest> request) {
        // Simulate I/O latency based on operation type and size
        Duration base_latency(1);  // 1ms base
        
        switch (request->operation) {
            case IOOperationType::READ:
                base_latency = Duration(2);
                // Simulate reading data
                request->data.resize(request->block_count * 4096);
                break;
                
            case IOOperationType::WRITE:
                base_latency = Duration(3);
                break;
                
            case IOOperationType::SEEK:
                base_latency = Duration(5);
                break;
                
            default:
                base_latency = Duration(1);
        }
        
        // Add size-dependent latency
        Duration size_latency(request->block_count / 100);  // ~10 blocks per ms
        
        std::this_thread::sleep_for(base_latency + size_latency);
    }
};

// ============================================================================
// Multi-Path I/O Manager
// ============================================================================

/**
 * I/O Path definition
 */
struct IOPath {
    std::string path_id;
    DeviceAddress chpid;           // Channel Path ID
    DeviceAddress device;
    PathStatus status = PathStatus::ONLINE;
    
    // Performance characteristics
    double bandwidth_mbps = 100.0;
    Duration typical_latency{2};
    double error_rate = 0.0;
    
    // Statistics
    uint64_t io_count = 0;
    uint64_t byte_count = 0;
    uint64_t error_count = 0;
    
    Timestamp last_used;
    Timestamp last_error;
};

/**
 * Multi-path I/O manager
 */
class MultiPathManager {
public:
    struct Config {
        Config() = default;
        size_t paths_per_device = 4;
        Duration path_check_interval{5000};
        double error_threshold = 0.01;  // 1% error rate triggers path offline
        bool enable_load_balancing = true;
    };
    
private:
    Config config_;
    
    mutable std::shared_mutex mutex_;
    std::map<VolumeSerial, std::vector<IOPath>> volume_paths_;
    
    std::atomic<bool> running_{false};
    std::thread health_check_thread_;
    
public:
    MultiPathManager() : config_() {}
    explicit MultiPathManager(const Config& config) : config_(config) {}
    
    ~MultiPathManager() {
        stop();
    }
    
    void start() {
        if (running_.exchange(true)) return;
        
        health_check_thread_ = std::thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(config_.path_check_interval);
                checkPathHealth();
            }
        });
    }
    
    void stop() {
        running_ = false;
        if (health_check_thread_.joinable()) {
            health_check_thread_.join();
        }
    }
    
    /**
     * Define paths for a volume
     */
    void definePaths(const VolumeSerial& volser, 
                    const std::vector<IOPath>& paths) {
        std::unique_lock lock(mutex_);
        volume_paths_[volser] = paths;
    }
    
    /**
     * Select best path for I/O
     */
    std::optional<IOPath> selectPath(const VolumeSerial& volser) {
        std::shared_lock lock(mutex_);
        
        auto it = volume_paths_.find(volser);
        if (it == volume_paths_.end() || it->second.empty()) {
            return std::nullopt;
        }
        
        // Find best online path
        const IOPath* best = nullptr;
        double best_score = std::numeric_limits<double>::max();
        
        for (const auto& path : it->second) {
            if (path.status != PathStatus::ONLINE) continue;
            
            // Score: lower is better (latency + load factor)
            double load = static_cast<double>(path.io_count % 1000) / 1000.0;
            double score = static_cast<double>(path.typical_latency.count()) + 
                          (load * 10.0);
            
            if (score < best_score) {
                best_score = score;
                best = &path;
            }
        }
        
        if (best) {
            return *best;
        }
        return std::nullopt;
    }
    
    /**
     * Record I/O on a path
     */
    void recordIO(const VolumeSerial& volser, const std::string& path_id,
                 uint64_t bytes, bool success) {
        std::unique_lock lock(mutex_);
        
        auto it = volume_paths_.find(volser);
        if (it == volume_paths_.end()) return;
        
        for (auto& path : it->second) {
            if (path.path_id == path_id) {
                path.io_count++;
                path.byte_count += bytes;
                path.last_used = std::chrono::system_clock::now();
                
                if (!success) {
                    path.error_count++;
                    path.last_error = std::chrono::system_clock::now();
                }
                break;
            }
        }
    }
    
    /**
     * Get paths for a volume
     */
    std::vector<IOPath> getPaths(const VolumeSerial& volser) const {
        std::shared_lock lock(mutex_);
        auto it = volume_paths_.find(volser);
        return (it != volume_paths_.end()) ? it->second : std::vector<IOPath>{};
    }
    
    /**
     * Set path status
     */
    void setPathStatus(const VolumeSerial& volser, const std::string& path_id,
                      PathStatus status) {
        std::unique_lock lock(mutex_);
        
        auto it = volume_paths_.find(volser);
        if (it == volume_paths_.end()) return;
        
        for (auto& path : it->second) {
            if (path.path_id == path_id) {
                path.status = status;
                break;
            }
        }
    }
    
private:
    void checkPathHealth() {
        std::unique_lock lock(mutex_);
        
        for (auto& [volser, paths] : volume_paths_) {
            for (auto& path : paths) {
                if (path.io_count > 0) {
                    path.error_rate = static_cast<double>(path.error_count) / 
                                     static_cast<double>(path.io_count);
                    
                    // Mark degraded if error rate exceeds threshold
                    if (path.error_rate > config_.error_threshold &&
                        path.status == PathStatus::ONLINE) {
                        path.status = PathStatus::DEGRADED;
                    }
                }
            }
        }
    }
};

// ============================================================================
// Parallel Manager - Main Entry Point
// ============================================================================

/**
 * Central parallel processing management
 */
class ParallelManager {
public:
    struct Config {
        Config() = default;
        PAVManager::Config pav_config;
        HyperPAVManager::Config hyperpav_config;
        AsyncIOManager::Config async_config;
        MultiPathManager::Config multipath_config;
        WorkStealingPool::Config pool_config;
        
        bool enable_pav = true;
        bool enable_hyperpav = true;
        bool enable_multipath = true;
    };
    
private:
    Config config_;
    
    std::unique_ptr<PAVManager> pav_manager_;
    std::unique_ptr<HyperPAVManager> hyperpav_manager_;
    std::unique_ptr<AsyncIOManager> async_io_manager_;
    std::unique_ptr<MultiPathManager> multipath_manager_;
    std::unique_ptr<WorkStealingPool> work_pool_;
    
    bool running_ = false;
    
public:
    ParallelManager() : config_() {
        pav_manager_ = std::make_unique<PAVManager>();
        hyperpav_manager_ = std::make_unique<HyperPAVManager>(*pav_manager_);
        async_io_manager_ = std::make_unique<AsyncIOManager>();
        multipath_manager_ = std::make_unique<MultiPathManager>();
        work_pool_ = std::make_unique<WorkStealingPool>();
    }
    
    explicit ParallelManager(const Config& config) 
        : config_(config) {
        
        if (config.enable_pav) {
            pav_manager_ = std::make_unique<PAVManager>(config.pav_config);
        }
        
        if (config.enable_hyperpav && pav_manager_) {
            hyperpav_manager_ = std::make_unique<HyperPAVManager>(
                *pav_manager_, config.hyperpav_config);
        }
        
        async_io_manager_ = std::make_unique<AsyncIOManager>(config.async_config);
        
        if (config.enable_multipath) {
            multipath_manager_ = std::make_unique<MultiPathManager>(
                config.multipath_config);
        }
        
        work_pool_ = std::make_unique<WorkStealingPool>(config.pool_config);
    }
    
    /**
     * Start all parallel processing components
     */
    void start() {
        if (running_) return;
        running_ = true;
        
        if (hyperpav_manager_) {
            hyperpav_manager_->start();
        }
        
        async_io_manager_->start();
        
        if (multipath_manager_) {
            multipath_manager_->start();
        }
        
        work_pool_->start();
    }
    
    /**
     * Stop all components
     */
    void stop() {
        if (!running_) return;
        running_ = false;
        
        work_pool_->stop();
        
        if (multipath_manager_) {
            multipath_manager_->stop();
        }
        
        async_io_manager_->stop();
        
        if (hyperpav_manager_) {
            hyperpav_manager_->stop();
        }
    }
    
    // -- Volume Management --
    
    bool defineVolume(const VolumeSerial& volser, DeviceAddress base_device,
                     uint8_t num_aliases = 4) {
        if (!pav_manager_) return false;
        return pav_manager_->defineVolume(volser, base_device, num_aliases);
    }
    
    // -- PAV Operations --
    
    std::optional<UCBAddress> allocateUCB(const VolumeSerial& volser,
                                          const std::string& job_name) {
        if (!pav_manager_) return std::nullopt;
        return pav_manager_->allocateUCB(volser, job_name);
    }
    
    void releaseUCB(UCBAddress address) {
        if (pav_manager_) {
            pav_manager_->releaseUCB(address);
        }
    }
    
    PAVManager::PAVStatistics getPAVStatistics(const VolumeSerial& volser) const {
        if (!pav_manager_) return PAVManager::PAVStatistics{};
        return pav_manager_->getStatistics(volser);
    }
    
    // -- HyperPAV Operations --
    
    bool defineHyperPAVPool(const std::string& pool_name,
                           const std::vector<DeviceAddress>& devices) {
        if (!hyperpav_manager_) return false;
        return hyperpav_manager_->definePool(pool_name, devices);
    }
    
    bool assignVolumeToPool(const VolumeSerial& volser,
                           const std::string& pool_name) {
        if (!hyperpav_manager_) return false;
        return hyperpav_manager_->assignVolumeToPool(volser, pool_name);
    }
    
    // -- Async I/O Operations --
    
    std::future<AsyncIOResult> asyncRead(const VolumeSerial& volser,
                                        uint64_t starting_block,
                                        uint64_t block_count,
                                        IOPriority priority = IOPriority::NORMAL) {
        return async_io_manager_->asyncRead(volser, starting_block, 
                                           block_count, priority);
    }
    
    std::future<AsyncIOResult> asyncWrite(const VolumeSerial& volser,
                                         uint64_t starting_block,
                                         const std::vector<uint8_t>& data,
                                         IOPriority priority = IOPriority::NORMAL) {
        return async_io_manager_->asyncWrite(volser, starting_block, 
                                            data, priority);
    }
    
    AsyncIOManager::IOStatistics getIOStatistics() const {
        return async_io_manager_->getStatistics();
    }
    
    // -- Multi-Path Operations --
    
    void definePaths(const VolumeSerial& volser,
                    const std::vector<IOPath>& paths) {
        if (multipath_manager_) {
            multipath_manager_->definePaths(volser, paths);
        }
    }
    
    std::optional<IOPath> selectPath(const VolumeSerial& volser) {
        if (!multipath_manager_) return std::nullopt;
        return multipath_manager_->selectPath(volser);
    }
    
    // -- Work Pool Operations --
    
    template<typename F>
    void submitTask(F&& task) {
        work_pool_->submit(std::forward<F>(task));
    }
    
    template<typename F, typename R = std::invoke_result_t<F>>
    std::future<R> submitTaskWithResult(F&& func) {
        return work_pool_->submitWithFuture(std::forward<F>(func));
    }
    
    void waitAllTasks() {
        work_pool_->waitAll();
    }
    
    WorkStealingPool::PoolStatistics getPoolStatistics() const {
        return work_pool_->getStatistics();
    }
    
    // -- Component Access --
    
    PAVManager* getPAVManager() { return pav_manager_.get(); }
    HyperPAVManager* getHyperPAVManager() { return hyperpav_manager_.get(); }
    AsyncIOManager* getAsyncIOManager() { return async_io_manager_.get(); }
    MultiPathManager* getMultiPathManager() { return multipath_manager_.get(); }
    WorkStealingPool* getWorkPool() { return work_pool_.get(); }
    
    // -- Summary Report --
    
    std::string getSummaryReport() const {
        std::ostringstream report;
        report << "========================================\n";
        report << "      Parallel Processing Summary\n";
        report << "========================================\n\n";
        
        // PAV Statistics
        if (pav_manager_) {
            report << "-- PAV Status --\n";
            report << "  PAV Manager: Active\n\n";
        }
        
        // HyperPAV Statistics
        if (hyperpav_manager_) {
            report << "-- HyperPAV Pools --\n";
            for (const auto& name : hyperpav_manager_->getPoolNames()) {
                auto stats = hyperpav_manager_->getPoolStatistics(name);
                report << "  Pool: " << name << "\n";
                report << "    Aliases: " << stats.assigned_aliases 
                       << "/" << stats.total_aliases << "\n";
                report << "    Volumes: " << stats.volumes_served << "\n";
                report << "    Utilization: " << (stats.utilization * 100) << "%\n";
            }
            report << "\n";
        }
        
        // I/O Statistics
        auto io_stats = async_io_manager_->getStatistics();
        report << "-- Async I/O --\n";
        report << "  Submitted: " << io_stats.total_submitted << "\n";
        report << "  Completed: " << io_stats.total_completed << "\n";
        report << "  Failed: " << io_stats.total_failed << "\n";
        report << "  Outstanding: " << io_stats.outstanding << "\n";
        report << "  Queue Depth: " << io_stats.queue_depth << "\n\n";
        
        // Work Pool Statistics
        auto pool_stats = work_pool_->getStatistics();
        report << "-- Work Pool --\n";
        report << "  Threads: " << pool_stats.num_threads << "\n";
        report << "  Pending: " << pool_stats.pending_tasks << "\n";
        report << "  Completed: " << pool_stats.completed_tasks << "\n";
        report << "  Steal Rate: " << (pool_stats.steal_rate * 100) << "%\n";
        
        return report.str();
    }
};

} // namespace parallel
} // namespace hsm

#endif // HSM_PARALLEL_HPP
