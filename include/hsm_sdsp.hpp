/*
 * DFSMShsm Emulator - SDSP (System Data Set Placement) Support
 * @version 4.2.0
 *
 * Copyright (c) 2024 DFSMShsm Emulator Project
 * Licensed under the MIT License
 *
 * Provides comprehensive System Data Set Placement (SDSP) emulation including:
 * - Dataset placement optimization algorithms
 * - Migration Level (ML) allocation strategies
 * - Volume selection and load balancing
 * - Space management policies
 * - Affinity group management
 * - Performance-based placement
 * - Tiered storage optimization
 */

#ifndef HSM_SDSP_HPP
#define HSM_SDSP_HPP

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
#include <optional>
#include <variant>
#include <queue>
#include <random>

namespace hsm {
namespace sdsp {

// ============================================================================
// Forward Declarations
// ============================================================================

class PlacementPolicy;
class VolumeSelector;
class AffinityManager;
class LoadBalancer;
class SDSPManager;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * Migration levels for HSM storage hierarchy
 */
enum class MigrationLevel {
    ML0,        // Primary DASD (Level 0)
    ML1,        // Migration Level 1 (DASD)
    ML2_DASD,   // Migration Level 2 on DASD
    ML2_TAPE,   // Migration Level 2 on Tape
    ML2_CLOUD   // Migration Level 2 on Cloud storage
};

/**
 * Placement optimization strategies
 */
enum class PlacementStrategy {
    ROUND_ROBIN,        // Simple round-robin across volumes
    LEAST_USED,         // Place on volume with most free space
    BEST_FIT,           // Best fit algorithm for space
    PERFORMANCE,        // Optimize for I/O performance
    AFFINITY,           // Keep related datasets together
    BALANCED,           // Balance across all criteria
    COST_OPTIMIZED,     // Minimize storage cost
    CUSTOM              // User-defined placement function
};

/**
 * Volume selection criteria
 */
enum class SelectionCriteria {
    SPACE_AVAILABLE,    // Most space available
    SPACE_PERCENT,      // Lowest utilization percentage
    IO_PERFORMANCE,     // Best I/O throughput
    RESPONSE_TIME,      // Lowest response time
    PROXIMITY,          // Closest to related data
    DEVICE_TYPE,        // Specific device type preference
    STORAGE_GROUP       // Storage group preference
};

/**
 * Affinity types for dataset grouping
 */
enum class AffinityType {
    SAME_VOLUME,        // Must be on same volume
    SAME_POOL,          // Must be in same storage pool
    SEPARATE_VOLUMES,   // Must be on different volumes (for HA)
    PREFERRED_TOGETHER, // Prefer same volume but not required
    NO_AFFINITY         // No placement constraints
};

/**
 * Load balancing algorithms
 */
enum class BalancingAlgorithm {
    STATIC_RATIO,       // Fixed distribution ratio
    DYNAMIC_LOAD,       // Based on current load
    WEIGHTED_ROUND_ROBIN,
    ADAPTIVE,           // Machine learning based
    THRESHOLD_BASED     // Trigger on threshold breach
};

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Volume performance metrics for placement decisions
 */
struct VolumeMetrics {
    std::string volume_serial;
    double iops_current{0.0};
    double iops_capacity{0.0};
    double throughput_mbps{0.0};
    double avg_response_time_ms{0.0};
    double queue_depth{0.0};
    uint64_t bytes_available{0};
    uint64_t bytes_total{0};
    double utilization_percent{0.0};
    uint32_t dataset_count{0};
    std::chrono::system_clock::time_point last_updated;
    
    double getSpacePercent() const {
        return bytes_total > 0 ? (static_cast<double>(bytes_available) / bytes_total) * 100.0 : 0.0;
    }
    
    double getLoadScore() const {
        // Lower is better - composite score
        double io_score = (iops_capacity > 0) ? (iops_current / iops_capacity) : 1.0;
        double space_score = 1.0 - (getSpacePercent() / 100.0);
        double response_score = avg_response_time_ms / 10.0;  // Normalize to ~0-1
        return (io_score * 0.4) + (space_score * 0.4) + (response_score * 0.2);
    }
};

/**
 * Storage pool definition for ML levels
 */
struct StoragePool {
    std::string pool_name;
    MigrationLevel level{MigrationLevel::ML1};
    std::vector<std::string> volumes;
    uint64_t total_capacity{0};
    uint64_t used_capacity{0};
    double high_threshold{85.0};
    double low_threshold{70.0};
    bool auto_migrate{true};
    std::string device_type;
    uint32_t priority{5};  // 1-10, higher = more preferred
    
    double getUtilization() const {
        return total_capacity > 0 ? (static_cast<double>(used_capacity) / total_capacity) * 100.0 : 0.0;
    }
    
    bool isAboveHighThreshold() const {
        return getUtilization() > high_threshold;
    }
    
    bool isBelowLowThreshold() const {
        return getUtilization() < low_threshold;
    }
    
    uint64_t getAvailableSpace() const {
        return total_capacity > used_capacity ? total_capacity - used_capacity : 0;
    }
};

/**
 * Dataset placement request
 */
struct PlacementRequest {
    std::string dataset_name;
    uint64_t size_bytes{0};
    uint64_t estimated_iops{0};
    std::string preferred_volume;
    std::string preferred_pool;
    std::string storage_class;
    std::string management_class;
    std::string data_class;
    AffinityType affinity{AffinityType::NO_AFFINITY};
    std::vector<std::string> affinity_datasets;  // Related datasets
    bool high_availability{false};
    bool performance_critical{false};
    std::chrono::system_clock::time_point deadline;
    std::map<std::string, std::string> attributes;
};

/**
 * Placement decision result
 */
struct PlacementDecision {
    bool success{false};
    std::string volume_serial;
    std::string pool_name;
    MigrationLevel level{MigrationLevel::ML1};
    uint64_t allocated_space{0};
    double confidence_score{0.0};  // 0-1, how confident in the decision
    std::string reason;
    std::vector<std::string> alternative_volumes;
    std::chrono::system_clock::time_point decision_time;
    std::map<std::string, double> scoring_breakdown;
};

/**
 * Affinity group definition
 */
struct AffinityGroup {
    std::string group_id;
    std::string group_name;
    AffinityType affinity_type{AffinityType::PREFERRED_TOGETHER};
    std::vector<std::string> dataset_patterns;  // Wildcards supported
    std::vector<std::string> member_datasets;
    std::string preferred_volume;
    std::string preferred_pool;
    bool enforce_strictly{false};
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point last_modified;
};

/**
 * Migration policy for SDSP
 */
struct MigrationPolicy {
    std::string policy_name;
    uint32_t days_unused_threshold{30};
    uint64_t size_threshold_bytes{0};  // 0 = no size limit
    MigrationLevel source_level{MigrationLevel::ML0};
    MigrationLevel target_level{MigrationLevel::ML1};
    std::vector<std::string> include_patterns;
    std::vector<std::string> exclude_patterns;
    std::chrono::hours quiet_period{24};  // Don't migrate if accessed within
    bool compress_on_migrate{true};
    uint32_t priority{5};
    bool enabled{true};
    
    struct Schedule {
        std::vector<uint8_t> days_of_week;  // 0=Sunday
        uint8_t start_hour{0};
        uint8_t end_hour{24};
        bool active{true};
    } schedule;
};

/**
 * Space management threshold configuration
 */
struct SpaceThresholds {
    double ml0_high_threshold{85.0};
    double ml0_low_threshold{70.0};
    double ml1_high_threshold{90.0};
    double ml1_low_threshold{75.0};
    double ml2_high_threshold{95.0};
    double ml2_low_threshold{80.0};
    uint64_t minimum_free_space{1024ULL * 1024 * 1024};  // 1GB minimum
    bool auto_expand_pools{false};
    bool auto_delete_aged{false};
    uint32_t delete_after_days{365};
};

/**
 * Placement statistics
 */
struct PlacementStatistics {
    uint64_t total_placements{0};
    uint64_t successful_placements{0};
    uint64_t failed_placements{0};
    uint64_t affinity_satisfied{0};
    uint64_t affinity_violated{0};
    double avg_confidence_score{0.0};
    std::map<std::string, uint64_t> placements_by_volume;
    std::map<std::string, uint64_t> placements_by_pool;
    std::map<MigrationLevel, uint64_t> placements_by_level;
    std::chrono::system_clock::time_point stats_since;
    std::chrono::system_clock::time_point last_placement;
    
    double getSuccessRate() const {
        return total_placements > 0 ? 
            (static_cast<double>(successful_placements) / total_placements) * 100.0 : 0.0;
    }
};

// ============================================================================
// Placement Policy Implementation
// ============================================================================

/**
 * Customizable placement policy
 */
class PlacementPolicy {
public:
    using ScoringFunction = std::function<double(const VolumeMetrics&, const PlacementRequest&)>;
    
    explicit PlacementPolicy(const std::string& name, PlacementStrategy strategy = PlacementStrategy::BALANCED)
        : policy_name_(name), strategy_(strategy) {
        initializeDefaultWeights();
    }
    
    /**
     * Set weighting factors for different criteria
     */
    void setWeights(double space_weight, double performance_weight, 
                   double affinity_weight, double cost_weight) {
        weights_.space = space_weight;
        weights_.performance = performance_weight;
        weights_.affinity = affinity_weight;
        weights_.cost = cost_weight;
        normalizeWeights();
    }
    
    /**
     * Set custom scoring function
     */
    void setCustomScorer(ScoringFunction scorer) {
        custom_scorer_ = std::move(scorer);
    }
    
    /**
     * Calculate placement score for a volume
     */
    double scoreVolume(const VolumeMetrics& metrics, const PlacementRequest& request,
                       const std::optional<AffinityGroup>& affinity = std::nullopt) const {
        
        if (strategy_ == PlacementStrategy::CUSTOM && custom_scorer_) {
            return custom_scorer_(metrics, request);
        }
        
        double score = 0.0;
        
        // Space score (higher free space = better)
        double space_score = metrics.getSpacePercent() / 100.0;
        
        // Performance score (lower response time = better)
        double perf_score = 1.0 - std::min(metrics.avg_response_time_ms / 20.0, 1.0);
        
        // Affinity score
        double affinity_score = calculateAffinityScore(metrics, request, affinity);
        
        // Cost score (simplified - based on device tier)
        double cost_score = calculateCostScore(metrics);
        
        switch (strategy_) {
            case PlacementStrategy::ROUND_ROBIN:
                // All volumes equal, use small random variation
                score = 0.5 + (std::rand() % 100) / 1000.0;
                break;
                
            case PlacementStrategy::LEAST_USED:
                score = space_score;
                break;
                
            case PlacementStrategy::PERFORMANCE:
                score = perf_score * 0.7 + space_score * 0.3;
                break;
                
            case PlacementStrategy::AFFINITY:
                score = affinity_score * 0.6 + space_score * 0.4;
                break;
                
            case PlacementStrategy::COST_OPTIMIZED:
                score = cost_score * 0.5 + space_score * 0.3 + perf_score * 0.2;
                break;
                
            case PlacementStrategy::BALANCED:
            default:
                score = (space_score * weights_.space) +
                        (perf_score * weights_.performance) +
                        (affinity_score * weights_.affinity) +
                        (cost_score * weights_.cost);
                break;
        }
        
        // Penalty for insufficient space
        if (metrics.bytes_available < request.size_bytes) {
            score = 0.0;
        }
        
        return std::clamp(score, 0.0, 1.0);
    }
    
    const std::string& getName() const { return policy_name_; }
    PlacementStrategy getStrategy() const { return strategy_; }
    
private:
    std::string policy_name_;
    PlacementStrategy strategy_;
    ScoringFunction custom_scorer_;
    
    struct Weights {
        double space{0.35};
        double performance{0.30};
        double affinity{0.20};
        double cost{0.15};
    } weights_;
    
    void initializeDefaultWeights() {
        weights_ = {0.35, 0.30, 0.20, 0.15};
    }
    
    void normalizeWeights() {
        double total = weights_.space + weights_.performance + 
                      weights_.affinity + weights_.cost;
        if (total > 0) {
            weights_.space /= total;
            weights_.performance /= total;
            weights_.affinity /= total;
            weights_.cost /= total;
        }
    }
    
    double calculateAffinityScore(const VolumeMetrics& metrics,
                                  const PlacementRequest& request,
                                  const std::optional<AffinityGroup>& affinity) const {
        if (!affinity || affinity->affinity_type == AffinityType::NO_AFFINITY) {
            return 0.5;  // Neutral score
        }
        
        bool on_preferred = (metrics.volume_serial == affinity->preferred_volume);
        
        switch (affinity->affinity_type) {
            case AffinityType::SAME_VOLUME:
                return on_preferred ? 1.0 : 0.0;
                
            case AffinityType::SEPARATE_VOLUMES:
                return on_preferred ? 0.0 : 1.0;
                
            case AffinityType::PREFERRED_TOGETHER:
                return on_preferred ? 0.8 : 0.4;
                
            case AffinityType::SAME_POOL:
                return 0.6;  // Handled at pool level
                
            default:
                return 0.5;
        }
    }
    
    double calculateCostScore(const VolumeMetrics& /*metrics*/) const {
        // Simplified cost model - would be based on device tier in production
        return 0.7;
    }
};

// ============================================================================
// Volume Selector
// ============================================================================

/**
 * Intelligent volume selection with multiple criteria support
 */
class VolumeSelector {
public:
    using VolumeFilter = std::function<bool(const VolumeMetrics&)>;
    
    VolumeSelector() = default;
    
    /**
     * Add a volume to the selection pool
     */
    void addVolume(const VolumeMetrics& metrics) {
        std::unique_lock lock(mutex_);
        volumes_[metrics.volume_serial] = metrics;
    }
    
    /**
     * Update metrics for existing volume
     */
    void updateMetrics(const std::string& volume_serial, const VolumeMetrics& metrics) {
        std::unique_lock lock(mutex_);
        if (volumes_.find(volume_serial) != volumes_.end()) {
            volumes_[volume_serial] = metrics;
        }
    }
    
    /**
     * Remove a volume from selection
     */
    void removeVolume(const std::string& volume_serial) {
        std::unique_lock lock(mutex_);
        volumes_.erase(volume_serial);
    }
    
    /**
     * Select best volume based on policy
     */
    std::optional<VolumeMetrics> selectVolume(
        const PlacementRequest& request,
        const PlacementPolicy& policy,
        const std::optional<AffinityGroup>& affinity = std::nullopt,
        VolumeFilter filter = nullptr) const {
        
        std::shared_lock lock(mutex_);
        
        if (volumes_.empty()) {
            return std::nullopt;
        }
        
        std::vector<std::pair<std::string, double>> scored_volumes;
        
        for (const auto& [serial, metrics] : volumes_) {
            // Apply filter if provided
            if (filter && !filter(metrics)) {
                continue;
            }
            
            // Skip volumes with insufficient space
            if (metrics.bytes_available < request.size_bytes) {
                continue;
            }
            
            double score = policy.scoreVolume(metrics, request, affinity);
            scored_volumes.emplace_back(serial, score);
        }
        
        if (scored_volumes.empty()) {
            return std::nullopt;
        }
        
        // Sort by score descending
        std::sort(scored_volumes.begin(), scored_volumes.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        return volumes_.at(scored_volumes.front().first);
    }
    
    /**
     * Select top N volumes
     */
    std::vector<VolumeMetrics> selectTopVolumes(
        const PlacementRequest& request,
        const PlacementPolicy& policy,
        size_t count,
        VolumeFilter filter = nullptr) const {
        
        std::shared_lock lock(mutex_);
        
        std::vector<std::pair<std::string, double>> scored_volumes;
        
        for (const auto& [serial, metrics] : volumes_) {
            if (filter && !filter(metrics)) {
                continue;
            }
            
            if (metrics.bytes_available < request.size_bytes) {
                continue;
            }
            
            double score = policy.scoreVolume(metrics, request, std::nullopt);
            scored_volumes.emplace_back(serial, score);
        }
        
        std::sort(scored_volumes.begin(), scored_volumes.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        std::vector<VolumeMetrics> result;
        for (size_t i = 0; i < std::min(count, scored_volumes.size()); ++i) {
            result.push_back(volumes_.at(scored_volumes[i].first));
        }
        
        return result;
    }
    
    /**
     * Get volumes in a specific storage pool
     */
    std::vector<VolumeMetrics> getPoolVolumes(const std::vector<std::string>& pool_volumes) const {
        std::shared_lock lock(mutex_);
        
        std::vector<VolumeMetrics> result;
        for (const auto& serial : pool_volumes) {
            auto it = volumes_.find(serial);
            if (it != volumes_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }
    
    /**
     * Get all volumes
     */
    std::vector<VolumeMetrics> getAllVolumes() const {
        std::shared_lock lock(mutex_);
        
        std::vector<VolumeMetrics> result;
        result.reserve(volumes_.size());
        for (const auto& [_, metrics] : volumes_) {
            result.push_back(metrics);
        }
        return result;
    }
    
    size_t getVolumeCount() const {
        std::shared_lock lock(mutex_);
        return volumes_.size();
    }
    
private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, VolumeMetrics> volumes_;
};

// ============================================================================
// Affinity Manager
// ============================================================================

/**
 * Manages dataset affinity groups and placement constraints
 */
class AffinityManager {
public:
    AffinityManager() = default;
    
    /**
     * Create a new affinity group
     */
    std::string createGroup(const std::string& name, AffinityType type,
                           const std::vector<std::string>& patterns = {}) {
        std::unique_lock lock(mutex_);
        
        std::string group_id = generateGroupId();
        
        AffinityGroup group;
        group.group_id = group_id;
        group.group_name = name;
        group.affinity_type = type;
        group.dataset_patterns = patterns;
        group.created = std::chrono::system_clock::now();
        group.last_modified = group.created;
        
        groups_[group_id] = std::move(group);
        
        return group_id;
    }
    
    /**
     * Add dataset to affinity group
     */
    bool addDatasetToGroup(const std::string& group_id, const std::string& dataset_name) {
        std::unique_lock lock(mutex_);
        
        auto it = groups_.find(group_id);
        if (it == groups_.end()) {
            return false;
        }
        
        it->second.member_datasets.push_back(dataset_name);
        it->second.last_modified = std::chrono::system_clock::now();
        
        // Update dataset-to-group mapping
        dataset_groups_[dataset_name] = group_id;
        
        return true;
    }
    
    /**
     * Remove dataset from affinity group
     */
    bool removeDatasetFromGroup(const std::string& group_id, const std::string& dataset_name) {
        std::unique_lock lock(mutex_);
        
        auto it = groups_.find(group_id);
        if (it == groups_.end()) {
            return false;
        }
        
        auto& members = it->second.member_datasets;
        members.erase(
            std::remove(members.begin(), members.end(), dataset_name),
            members.end()
        );
        
        it->second.last_modified = std::chrono::system_clock::now();
        dataset_groups_.erase(dataset_name);
        
        return true;
    }
    
    /**
     * Get affinity group for a dataset
     */
    std::optional<AffinityGroup> getGroupForDataset(const std::string& dataset_name) const {
        std::shared_lock lock(mutex_);
        
        // Check direct membership
        auto it = dataset_groups_.find(dataset_name);
        if (it != dataset_groups_.end()) {
            auto group_it = groups_.find(it->second);
            if (group_it != groups_.end()) {
                return group_it->second;
            }
        }
        
        // Check pattern matching
        for (const auto& [_, group] : groups_) {
            for (const auto& pattern : group.dataset_patterns) {
                if (matchPattern(dataset_name, pattern)) {
                    return group;
                }
            }
        }
        
        return std::nullopt;
    }
    
    /**
     * Set preferred volume for affinity group
     */
    bool setPreferredVolume(const std::string& group_id, const std::string& volume) {
        std::unique_lock lock(mutex_);
        
        auto it = groups_.find(group_id);
        if (it == groups_.end()) {
            return false;
        }
        
        it->second.preferred_volume = volume;
        it->second.last_modified = std::chrono::system_clock::now();
        return true;
    }
    
    /**
     * Delete an affinity group
     */
    bool deleteGroup(const std::string& group_id) {
        std::unique_lock lock(mutex_);
        
        auto it = groups_.find(group_id);
        if (it == groups_.end()) {
            return false;
        }
        
        // Remove dataset mappings
        for (const auto& dataset : it->second.member_datasets) {
            dataset_groups_.erase(dataset);
        }
        
        groups_.erase(it);
        return true;
    }
    
    /**
     * Get all affinity groups
     */
    std::vector<AffinityGroup> getAllGroups() const {
        std::shared_lock lock(mutex_);
        
        std::vector<AffinityGroup> result;
        result.reserve(groups_.size());
        for (const auto& [_, group] : groups_) {
            result.push_back(group);
        }
        return result;
    }
    
    /**
     * Check if placement satisfies affinity constraints
     */
    bool validatePlacement(const std::string& dataset_name, const std::string& volume,
                          std::string& violation_reason) const {
        std::shared_lock lock(mutex_);
        
        auto group_opt = getGroupForDataset(dataset_name);
        if (!group_opt) {
            return true;  // No affinity constraints
        }
        
        const auto& group = *group_opt;
        
        if (!group.enforce_strictly) {
            return true;  // Soft affinity - always valid
        }
        
        switch (group.affinity_type) {
            case AffinityType::SAME_VOLUME:
                if (!group.preferred_volume.empty() && volume != group.preferred_volume) {
                    violation_reason = "Dataset must be on volume " + group.preferred_volume;
                    return false;
                }
                break;
                
            case AffinityType::SEPARATE_VOLUMES:
                if (!group.preferred_volume.empty() && volume == group.preferred_volume) {
                    violation_reason = "Dataset must NOT be on volume " + group.preferred_volume;
                    return false;
                }
                break;
                
            default:
                break;
        }
        
        return true;
    }
    
private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, AffinityGroup> groups_;
    std::unordered_map<std::string, std::string> dataset_groups_;  // dataset -> group_id
    uint64_t next_group_id_{1};
    
    std::string generateGroupId() {
        return "AG" + std::to_string(next_group_id_++);
    }
    
    static bool matchPattern(const std::string& name, const std::string& pattern) {
        // Simple wildcard matching (* and ?)
        size_t n = name.length();
        size_t p = pattern.length();
        
        std::vector<std::vector<bool>> dp(n + 1, std::vector<bool>(p + 1, false));
        dp[0][0] = true;
        
        for (size_t j = 1; j <= p; ++j) {
            if (pattern[j-1] == '*') {
                dp[0][j] = dp[0][j-1];
            }
        }
        
        for (size_t i = 1; i <= n; ++i) {
            for (size_t j = 1; j <= p; ++j) {
                if (pattern[j-1] == '*') {
                    dp[i][j] = dp[i-1][j] || dp[i][j-1];
                } else if (pattern[j-1] == '?' || name[i-1] == pattern[j-1]) {
                    dp[i][j] = dp[i-1][j-1];
                }
            }
        }
        
        return dp[n][p];
    }
};

// ============================================================================
// Load Balancer
// ============================================================================

/**
 * Intelligent load balancing across storage pools and volumes
 */
class LoadBalancer {
public:
    explicit LoadBalancer(BalancingAlgorithm algorithm = BalancingAlgorithm::DYNAMIC_LOAD)
        : algorithm_(algorithm) {}
    
    /**
     * Register a storage pool
     */
    void registerPool(const StoragePool& pool) {
        std::unique_lock lock(mutex_);
        pools_[pool.pool_name] = pool;
    }
    
    /**
     * Update pool metrics
     */
    void updatePoolMetrics(const std::string& pool_name, uint64_t used, uint64_t total) {
        std::unique_lock lock(mutex_);
        
        auto it = pools_.find(pool_name);
        if (it != pools_.end()) {
            it->second.used_capacity = used;
            it->second.total_capacity = total;
        }
    }
    
    /**
     * Select optimal pool for migration level
     */
    std::optional<StoragePool> selectPool(MigrationLevel level, uint64_t required_space) const {
        std::shared_lock lock(mutex_);
        
        std::vector<const StoragePool*> candidates;
        
        for (const auto& [_, pool] : pools_) {
            if (pool.level == level && pool.getAvailableSpace() >= required_space) {
                candidates.push_back(&pool);
            }
        }
        
        if (candidates.empty()) {
            return std::nullopt;
        }
        
        // Sort by criteria based on algorithm
        switch (algorithm_) {
            case BalancingAlgorithm::STATIC_RATIO:
                std::sort(candidates.begin(), candidates.end(),
                    [](const auto* a, const auto* b) {
                        return a->priority > b->priority;
                    });
                break;
                
            case BalancingAlgorithm::DYNAMIC_LOAD:
            case BalancingAlgorithm::ADAPTIVE:
                std::sort(candidates.begin(), candidates.end(),
                    [](const auto* a, const auto* b) {
                        return a->getUtilization() < b->getUtilization();
                    });
                break;
                
            case BalancingAlgorithm::THRESHOLD_BASED:
                // Prefer pools below low threshold
                std::sort(candidates.begin(), candidates.end(),
                    [](const auto* a, const auto* b) {
                        bool a_below = a->isBelowLowThreshold();
                        bool b_below = b->isBelowLowThreshold();
                        if (a_below != b_below) return a_below;
                        return a->getUtilization() < b->getUtilization();
                    });
                break;
                
            default:
                break;
        }
        
        return *candidates.front();
    }
    
    /**
     * Get pools that need migration (above high threshold)
     */
    std::vector<StoragePool> getPoolsNeedingMigration() const {
        std::shared_lock lock(mutex_);
        
        std::vector<StoragePool> result;
        for (const auto& [_, pool] : pools_) {
            if (pool.auto_migrate && pool.isAboveHighThreshold()) {
                result.push_back(pool);
            }
        }
        
        // Sort by utilization descending (most urgent first)
        std::sort(result.begin(), result.end(),
            [](const auto& a, const auto& b) {
                return a.getUtilization() > b.getUtilization();
            });
        
        return result;
    }
    
    /**
     * Calculate rebalancing recommendations
     */
    struct RebalanceRecommendation {
        std::string source_pool;
        std::string target_pool;
        uint64_t bytes_to_move{0};
        double priority{0.0};
        std::string reason;
    };
    
    std::vector<RebalanceRecommendation> getRebalanceRecommendations() const {
        std::shared_lock lock(mutex_);
        
        std::vector<RebalanceRecommendation> recommendations;
        
        // Group pools by migration level
        std::map<MigrationLevel, std::vector<const StoragePool*>> pools_by_level;
        for (const auto& [_, pool] : pools_) {
            pools_by_level[pool.level].push_back(&pool);
        }
        
        // Check for imbalances within each level
        for (const auto& [level, level_pools] : pools_by_level) {
            if (level_pools.size() < 2) continue;
            
            double avg_util = 0.0;
            for (const auto* pool : level_pools) {
                avg_util += pool->getUtilization();
            }
            avg_util /= level_pools.size();
            
            for (const auto* high_pool : level_pools) {
                if (high_pool->getUtilization() <= avg_util + 10.0) continue;
                
                for (const auto* low_pool : level_pools) {
                    if (low_pool->getUtilization() >= avg_util - 10.0) continue;
                    
                    double diff = high_pool->getUtilization() - low_pool->getUtilization();
                    if (diff > 15.0) {  // Significant imbalance
                        RebalanceRecommendation rec;
                        rec.source_pool = high_pool->pool_name;
                        rec.target_pool = low_pool->pool_name;
                        rec.bytes_to_move = static_cast<uint64_t>(
                            high_pool->total_capacity * (diff / 200.0)  // Move half the difference
                        );
                        rec.priority = diff / 100.0;
                        rec.reason = "Utilization imbalance: " + 
                                    std::to_string(static_cast<int>(high_pool->getUtilization())) + 
                                    "% vs " + 
                                    std::to_string(static_cast<int>(low_pool->getUtilization())) + "%";
                        recommendations.push_back(rec);
                    }
                }
            }
        }
        
        // Sort by priority
        std::sort(recommendations.begin(), recommendations.end(),
            [](const auto& a, const auto& b) { return a.priority > b.priority; });
        
        return recommendations;
    }
    
    /**
     * Get pool utilization summary
     */
    struct PoolUtilizationSummary {
        std::string pool_name;
        MigrationLevel level;
        double utilization_percent;
        uint64_t available_bytes;
        std::string status;  // "OK", "WARNING", "CRITICAL"
    };
    
    std::vector<PoolUtilizationSummary> getUtilizationSummary() const {
        std::shared_lock lock(mutex_);
        
        std::vector<PoolUtilizationSummary> summary;
        
        for (const auto& [_, pool] : pools_) {
            PoolUtilizationSummary s;
            s.pool_name = pool.pool_name;
            s.level = pool.level;
            s.utilization_percent = pool.getUtilization();
            s.available_bytes = pool.getAvailableSpace();
            
            if (pool.isAboveHighThreshold()) {
                s.status = "CRITICAL";
            } else if (s.utilization_percent > pool.low_threshold) {
                s.status = "WARNING";
            } else {
                s.status = "OK";
            }
            
            summary.push_back(s);
        }
        
        return summary;
    }
    
    void setAlgorithm(BalancingAlgorithm alg) {
        std::unique_lock lock(mutex_);
        algorithm_ = alg;
    }
    
private:
    mutable std::shared_mutex mutex_;
    BalancingAlgorithm algorithm_;
    std::unordered_map<std::string, StoragePool> pools_;
};

// ============================================================================
// ML Allocation Strategy
// ============================================================================

/**
 * Migration Level allocation strategies
 */
class MLAllocator {
public:
    MLAllocator() = default;
    
    /**
     * Determine optimal migration level for a dataset
     */
    MigrationLevel determineLevel(const PlacementRequest& request,
                                  const std::vector<MigrationPolicy>& policies) const {
        // Check policies in priority order
        std::vector<const MigrationPolicy*> sorted_policies;
        for (const auto& p : policies) {
            if (p.enabled) {
                sorted_policies.push_back(&p);
            }
        }
        
        std::sort(sorted_policies.begin(), sorted_policies.end(),
            [](const auto* a, const auto* b) { return a->priority > b->priority; });
        
        for (const auto* policy : sorted_policies) {
            if (matchesPolicy(request.dataset_name, *policy)) {
                return policy->target_level;
            }
        }
        
        // Default based on dataset characteristics
        if (request.performance_critical) {
            return MigrationLevel::ML0;
        }
        
        if (request.size_bytes > 10ULL * 1024 * 1024 * 1024) {  // > 10GB
            return MigrationLevel::ML2_DASD;
        }
        
        return MigrationLevel::ML1;
    }
    
    /**
     * Check if dataset should be migrated based on access patterns
     */
    struct MigrationRecommendation {
        bool should_migrate{false};
        MigrationLevel current_level{MigrationLevel::ML0};
        MigrationLevel recommended_level{MigrationLevel::ML0};
        std::string reason;
        uint32_t days_since_access{0};
    };
    
    MigrationRecommendation checkMigration(
        const std::string& dataset_name,
        MigrationLevel current_level,
        uint32_t days_since_access,
        uint64_t dataset_size,
        const std::vector<MigrationPolicy>& policies) const {
        
        MigrationRecommendation rec;
        rec.current_level = current_level;
        rec.recommended_level = current_level;
        rec.days_since_access = days_since_access;
        
        // Find applicable policy
        for (const auto& policy : policies) {
            if (!policy.enabled) continue;
            if (policy.source_level != current_level) continue;
            if (!matchesPolicy(dataset_name, policy)) continue;
            
            // Check days threshold
            if (days_since_access >= policy.days_unused_threshold) {
                // Check size threshold
                if (policy.size_threshold_bytes == 0 || 
                    dataset_size >= policy.size_threshold_bytes) {
                    rec.should_migrate = true;
                    rec.recommended_level = policy.target_level;
                    rec.reason = "Dataset unused for " + std::to_string(days_since_access) + 
                                " days (threshold: " + std::to_string(policy.days_unused_threshold) + ")";
                    return rec;
                }
            }
        }
        
        // Default migration rules
        if (current_level == MigrationLevel::ML0 && days_since_access > 30) {
            rec.should_migrate = true;
            rec.recommended_level = MigrationLevel::ML1;
            rec.reason = "Default ML0->ML1 migration after 30 days inactivity";
        } else if (current_level == MigrationLevel::ML1 && days_since_access > 90) {
            rec.should_migrate = true;
            rec.recommended_level = MigrationLevel::ML2_DASD;
            rec.reason = "Default ML1->ML2 migration after 90 days inactivity";
        }
        
        return rec;
    }
    
private:
    bool matchesPolicy(const std::string& dataset_name, const MigrationPolicy& policy) const {
        // Check exclude patterns first
        for (const auto& pattern : policy.exclude_patterns) {
            if (matchWildcard(dataset_name, pattern)) {
                return false;
            }
        }
        
        // If no include patterns, matches all
        if (policy.include_patterns.empty()) {
            return true;
        }
        
        // Check include patterns
        for (const auto& pattern : policy.include_patterns) {
            if (matchWildcard(dataset_name, pattern)) {
                return true;
            }
        }
        
        return false;
    }
    
    static bool matchWildcard(const std::string& str, const std::string& pattern) {
        // Simple wildcard matching
        if (pattern == "*") return true;
        
        size_t pos = pattern.find('*');
        if (pos == std::string::npos) {
            return str == pattern;
        }
        
        std::string prefix = pattern.substr(0, pos);
        std::string suffix = pattern.substr(pos + 1);
        
        if (!prefix.empty() && str.substr(0, prefix.length()) != prefix) {
            return false;
        }
        
        if (!suffix.empty() && 
            (str.length() < suffix.length() || 
             str.substr(str.length() - suffix.length()) != suffix)) {
            return false;
        }
        
        return true;
    }
};

// ============================================================================
// SDSP Manager - Main Interface
// ============================================================================

/**
 * Central SDSP management component
 */
class SDSPManager {
public:
    SDSPManager() 
        : volume_selector_(std::make_unique<VolumeSelector>()),
          affinity_manager_(std::make_unique<AffinityManager>()),
          load_balancer_(std::make_unique<LoadBalancer>()),
          ml_allocator_(std::make_unique<MLAllocator>()) {
        statistics_.stats_since = std::chrono::system_clock::now();
    }
    
    // ========================================================================
    // Volume Management
    // ========================================================================
    
    /**
     * Register a volume with the SDSP system
     */
    void registerVolume(const VolumeMetrics& metrics) {
        volume_selector_->addVolume(metrics);
    }
    
    /**
     * Update volume metrics
     */
    void updateVolumeMetrics(const std::string& volume_serial, const VolumeMetrics& metrics) {
        volume_selector_->updateMetrics(volume_serial, metrics);
    }
    
    /**
     * Remove a volume
     */
    void removeVolume(const std::string& volume_serial) {
        volume_selector_->removeVolume(volume_serial);
    }
    
    // ========================================================================
    // Pool Management
    // ========================================================================
    
    /**
     * Register a storage pool
     */
    void registerPool(const StoragePool& pool) {
        std::unique_lock lock(mutex_);
        pools_[pool.pool_name] = pool;
        load_balancer_->registerPool(pool);
    }
    
    /**
     * Update pool capacity
     */
    void updatePoolCapacity(const std::string& pool_name, uint64_t used, uint64_t total) {
        std::unique_lock lock(mutex_);
        
        auto it = pools_.find(pool_name);
        if (it != pools_.end()) {
            it->second.used_capacity = used;
            it->second.total_capacity = total;
            load_balancer_->updatePoolMetrics(pool_name, used, total);
        }
    }
    
    // ========================================================================
    // Policy Management
    // ========================================================================
    
    /**
     * Add a placement policy
     */
    void addPolicy(std::shared_ptr<PlacementPolicy> policy) {
        std::unique_lock lock(mutex_);
        policies_[policy->getName()] = std::move(policy);
    }
    
    /**
     * Set default placement policy
     */
    void setDefaultPolicy(const std::string& policy_name) {
        std::unique_lock lock(mutex_);
        default_policy_ = policy_name;
    }
    
    /**
     * Add migration policy
     */
    void addMigrationPolicy(const MigrationPolicy& policy) {
        std::unique_lock lock(mutex_);
        migration_policies_.push_back(policy);
    }
    
    // ========================================================================
    // Affinity Management
    // ========================================================================
    
    /**
     * Create affinity group
     */
    std::string createAffinityGroup(const std::string& name, AffinityType type,
                                   const std::vector<std::string>& patterns = {}) {
        return affinity_manager_->createGroup(name, type, patterns);
    }
    
    /**
     * Add dataset to affinity group
     */
    bool addToAffinityGroup(const std::string& group_id, const std::string& dataset_name) {
        return affinity_manager_->addDatasetToGroup(group_id, dataset_name);
    }
    
    // ========================================================================
    // Placement Operations
    // ========================================================================
    
    /**
     * Request dataset placement
     */
    PlacementDecision requestPlacement(const PlacementRequest& request) {
        auto start_time = std::chrono::system_clock::now();
        PlacementDecision decision;
        decision.decision_time = start_time;
        
        std::shared_lock lock(mutex_);
        
        // Get applicable policy
        std::shared_ptr<PlacementPolicy> policy;
        if (!request.storage_class.empty()) {
            auto it = policies_.find(request.storage_class);
            if (it != policies_.end()) {
                policy = it->second;
            }
        }
        
        if (!policy && !default_policy_.empty()) {
            auto it = policies_.find(default_policy_);
            if (it != policies_.end()) {
                policy = it->second;
            }
        }
        
        if (!policy) {
            policy = std::make_shared<PlacementPolicy>("default", PlacementStrategy::BALANCED);
        }
        
        // Get affinity constraints
        auto affinity = affinity_manager_->getGroupForDataset(request.dataset_name);
        
        // Determine migration level
        MigrationLevel target_level = ml_allocator_->determineLevel(request, migration_policies_);
        
        // Select appropriate pool
        std::optional<StoragePool> pool_opt = load_balancer_->selectPool(target_level, request.size_bytes);
        
        if (!pool_opt) {
            // Try next level
            if (target_level == MigrationLevel::ML1) {
                pool_opt = load_balancer_->selectPool(MigrationLevel::ML2_DASD, request.size_bytes);
                if (pool_opt) {
                    target_level = MigrationLevel::ML2_DASD;
                }
            }
        }
        
        if (!pool_opt) {
            decision.success = false;
            decision.reason = "No available pool with sufficient space";
            updateStatistics(decision);
            return decision;
        }
        
        // Build volume filter for the selected pool
        std::set<std::string> pool_volume_set(pool_opt->volumes.begin(), pool_opt->volumes.end());
        auto volume_filter = [&pool_volume_set](const VolumeMetrics& m) {
            return pool_volume_set.find(m.volume_serial) != pool_volume_set.end();
        };
        
        // Select best volume
        auto volume_opt = volume_selector_->selectVolume(request, *policy, affinity, volume_filter);
        
        if (!volume_opt) {
            // Try without pool constraint
            volume_opt = volume_selector_->selectVolume(request, *policy, affinity);
        }
        
        if (!volume_opt) {
            decision.success = false;
            decision.reason = "No suitable volume found";
            updateStatistics(decision);
            return decision;
        }
        
        // Validate affinity constraints
        std::string violation_reason;
        if (!affinity_manager_->validatePlacement(request.dataset_name, 
                                                   volume_opt->volume_serial,
                                                   violation_reason)) {
            decision.success = false;
            decision.reason = "Affinity violation: " + violation_reason;
            updateStatistics(decision);
            return decision;
        }
        
        // Build successful decision
        decision.success = true;
        decision.volume_serial = volume_opt->volume_serial;
        decision.pool_name = pool_opt->pool_name;
        decision.level = target_level;
        decision.allocated_space = request.size_bytes;
        decision.confidence_score = policy->scoreVolume(*volume_opt, request, affinity);
        decision.reason = "Placed by " + policy->getName() + " policy";
        
        // Get alternatives
        auto alternatives = volume_selector_->selectTopVolumes(request, *policy, 3);
        for (const auto& alt : alternatives) {
            if (alt.volume_serial != volume_opt->volume_serial) {
                decision.alternative_volumes.push_back(alt.volume_serial);
            }
        }
        
        // Record scoring breakdown
        decision.scoring_breakdown["space"] = volume_opt->getSpacePercent() / 100.0;
        decision.scoring_breakdown["performance"] = 
            1.0 - std::min(volume_opt->avg_response_time_ms / 20.0, 1.0);
        decision.scoring_breakdown["load"] = 1.0 - volume_opt->getLoadScore();
        
        updateStatistics(decision);
        return decision;
    }
    
    /**
     * Check if dataset should be migrated
     */
    MLAllocator::MigrationRecommendation checkMigration(
        const std::string& dataset_name,
        MigrationLevel current_level,
        uint32_t days_since_access,
        uint64_t dataset_size) const {
        
        std::shared_lock lock(mutex_);
        return ml_allocator_->checkMigration(dataset_name, current_level, 
                                             days_since_access, dataset_size,
                                             migration_policies_);
    }
    
    // ========================================================================
    // Reporting and Statistics
    // ========================================================================
    
    /**
     * Get placement statistics
     */
    PlacementStatistics getStatistics() const {
        std::shared_lock lock(mutex_);
        return statistics_;
    }
    
    /**
     * Get load balancing recommendations
     */
    std::vector<LoadBalancer::RebalanceRecommendation> getRebalanceRecommendations() const {
        return load_balancer_->getRebalanceRecommendations();
    }
    
    /**
     * Get pool utilization summary
     */
    std::vector<LoadBalancer::PoolUtilizationSummary> getPoolUtilization() const {
        return load_balancer_->getUtilizationSummary();
    }
    
    /**
     * Get pools needing migration
     */
    std::vector<StoragePool> getPoolsNeedingMigration() const {
        return load_balancer_->getPoolsNeedingMigration();
    }
    
    /**
     * Set space thresholds
     */
    void setSpaceThresholds(const SpaceThresholds& thresholds) {
        std::unique_lock lock(mutex_);
        thresholds_ = thresholds;
    }
    
    /**
     * Export configuration as text
     */
    std::string exportConfiguration() const {
        std::shared_lock lock(mutex_);
        
        std::string output;
        output += "SDSP Configuration Export\n";
        output += std::string(60, '=') + "\n\n";
        
        // Pools
        output += "Storage Pools:\n";
        output += std::string(40, '-') + "\n";
        for (const auto& [name, pool] : pools_) {
            output += "  Pool: " + name + "\n";
            output += "    Level: ML" + std::to_string(static_cast<int>(pool.level)) + "\n";
            output += "    Volumes: " + std::to_string(pool.volumes.size()) + "\n";
            output += "    Capacity: " + std::to_string(pool.total_capacity / (1024*1024*1024)) + " GB\n";
            output += "    Utilization: " + std::to_string(static_cast<int>(pool.getUtilization())) + "%\n";
            output += "    High Threshold: " + std::to_string(static_cast<int>(pool.high_threshold)) + "%\n";
            output += "    Low Threshold: " + std::to_string(static_cast<int>(pool.low_threshold)) + "%\n\n";
        }
        
        // Policies
        output += "\nPlacement Policies:\n";
        output += std::string(40, '-') + "\n";
        for (const auto& [name, policy] : policies_) {
            output += "  Policy: " + name + "\n";
            output += "    Strategy: " + strategyToString(policy->getStrategy()) + "\n\n";
        }
        
        // Migration Policies
        output += "\nMigration Policies:\n";
        output += std::string(40, '-') + "\n";
        for (const auto& policy : migration_policies_) {
            output += "  Policy: " + policy.policy_name + "\n";
            output += "    Enabled: " + std::string(policy.enabled ? "Yes" : "No") + "\n";
            output += "    Days Threshold: " + std::to_string(policy.days_unused_threshold) + "\n";
            output += "    Priority: " + std::to_string(policy.priority) + "\n\n";
        }
        
        // Affinity Groups
        auto groups = affinity_manager_->getAllGroups();
        output += "\nAffinity Groups:\n";
        output += std::string(40, '-') + "\n";
        for (const auto& group : groups) {
            output += "  Group: " + group.group_name + " (" + group.group_id + ")\n";
            output += "    Type: " + affinityTypeToString(group.affinity_type) + "\n";
            output += "    Members: " + std::to_string(group.member_datasets.size()) + "\n";
            if (!group.preferred_volume.empty()) {
                output += "    Preferred Volume: " + group.preferred_volume + "\n";
            }
            output += "\n";
        }
        
        // Statistics
        output += "\nStatistics:\n";
        output += std::string(40, '-') + "\n";
        output += "  Total Placements: " + std::to_string(statistics_.total_placements) + "\n";
        output += "  Successful: " + std::to_string(statistics_.successful_placements) + "\n";
        output += "  Failed: " + std::to_string(statistics_.failed_placements) + "\n";
        output += "  Success Rate: " + std::to_string(static_cast<int>(statistics_.getSuccessRate())) + "%\n";
        output += "  Avg Confidence: " + std::to_string(statistics_.avg_confidence_score) + "\n";
        
        return output;
    }
    
private:
    mutable std::shared_mutex mutex_;
    
    std::unique_ptr<VolumeSelector> volume_selector_;
    std::unique_ptr<AffinityManager> affinity_manager_;
    std::unique_ptr<LoadBalancer> load_balancer_;
    std::unique_ptr<MLAllocator> ml_allocator_;
    
    std::unordered_map<std::string, StoragePool> pools_;
    std::unordered_map<std::string, std::shared_ptr<PlacementPolicy>> policies_;
    std::vector<MigrationPolicy> migration_policies_;
    std::string default_policy_;
    SpaceThresholds thresholds_;
    PlacementStatistics statistics_;
    
    void updateStatistics(const PlacementDecision& decision) {
        // Note: Caller must hold lock
        statistics_.total_placements++;
        statistics_.last_placement = decision.decision_time;
        
        if (decision.success) {
            statistics_.successful_placements++;
            statistics_.placements_by_volume[decision.volume_serial]++;
            statistics_.placements_by_pool[decision.pool_name]++;
            statistics_.placements_by_level[decision.level]++;
            
            // Update running average of confidence
            double n = static_cast<double>(statistics_.successful_placements);
            statistics_.avg_confidence_score = 
                ((statistics_.avg_confidence_score * (n - 1)) + decision.confidence_score) / n;
        } else {
            statistics_.failed_placements++;
        }
    }
    
    static std::string strategyToString(PlacementStrategy strategy) {
        switch (strategy) {
            case PlacementStrategy::ROUND_ROBIN: return "Round Robin";
            case PlacementStrategy::LEAST_USED: return "Least Used";
            case PlacementStrategy::BEST_FIT: return "Best Fit";
            case PlacementStrategy::PERFORMANCE: return "Performance";
            case PlacementStrategy::AFFINITY: return "Affinity";
            case PlacementStrategy::BALANCED: return "Balanced";
            case PlacementStrategy::COST_OPTIMIZED: return "Cost Optimized";
            case PlacementStrategy::CUSTOM: return "Custom";
            default: return "Unknown";
        }
    }
    
    static std::string affinityTypeToString(AffinityType type) {
        switch (type) {
            case AffinityType::SAME_VOLUME: return "Same Volume";
            case AffinityType::SAME_POOL: return "Same Pool";
            case AffinityType::SEPARATE_VOLUMES: return "Separate Volumes";
            case AffinityType::PREFERRED_TOGETHER: return "Preferred Together";
            case AffinityType::NO_AFFINITY: return "No Affinity";
            default: return "Unknown";
        }
    }
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Convert migration level to string
 */
inline std::string migrationLevelToString(MigrationLevel level) {
    switch (level) {
        case MigrationLevel::ML0: return "ML0";
        case MigrationLevel::ML1: return "ML1";
        case MigrationLevel::ML2_DASD: return "ML2-DASD";
        case MigrationLevel::ML2_TAPE: return "ML2-TAPE";
        case MigrationLevel::ML2_CLOUD: return "ML2-CLOUD";
        default: return "UNKNOWN";
    }
}

/**
 * Parse migration level from string
 */
inline std::optional<MigrationLevel> parseMigrationLevel(const std::string& str) {
    if (str == "ML0" || str == "0") return MigrationLevel::ML0;
    if (str == "ML1" || str == "1") return MigrationLevel::ML1;
    if (str == "ML2-DASD" || str == "ML2" || str == "2") return MigrationLevel::ML2_DASD;
    if (str == "ML2-TAPE") return MigrationLevel::ML2_TAPE;
    if (str == "ML2-CLOUD") return MigrationLevel::ML2_CLOUD;
    return std::nullopt;
}

/**
 * Format bytes for display
 */
inline std::string formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < 5) {
        size /= 1024.0;
        unit_index++;
    }
    
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
    return std::string(buffer);
}

}  // namespace sdsp
}  // namespace hsm

#endif // HSM_SDSP_HPP
