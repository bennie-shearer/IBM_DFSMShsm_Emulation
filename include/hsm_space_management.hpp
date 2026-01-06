/*
 * DFSMShsm Emulator - Space Management
 * @version 4.2.0
 * 
 * Emulates HSM automatic space management:
 * - Primary/secondary space management thresholds
 * - High/low watermark processing
 * - Age-based migration triggers
 * - Volume space monitoring
 * - Automatic migration/deletion
 * - Space reclamation
 */

#ifndef HSM_SPACE_MANAGEMENT_HPP
#define HSM_SPACE_MANAGEMENT_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <functional>
#include <optional>
#include <queue>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <thread>

namespace hsm {
namespace space {

// =============================================================================
// Threshold Types
// =============================================================================

enum class ThresholdType {
    HIGH_THRESHOLD,      // Start migration when exceeded
    LOW_THRESHOLD,       // Stop migration when reached
    CRITICAL_THRESHOLD,  // Emergency action required
    WARNING_THRESHOLD    // Alert only
};

enum class SpaceAction {
    NONE,
    MIGRATE_PRIMARY,     // Migrate from primary to ML1
    MIGRATE_ML1_TO_ML2,  // Migrate from ML1 to ML2
    DELETE_EXPIRED,      // Delete expired datasets
    DELETE_MIGRATED,     // Delete migrated copies
    RECALL,              // Recall datasets
    COMPRESS,            // Compress in place
    DEFRAGMENT,          // Defragment volume
    EXTEND,              // Extend volume (if possible)
    ALERT                // Alert only
};

enum class SelectionCriteria {
    OLDEST_REFERENCE,    // Least recently referenced
    OLDEST_CREATION,     // Oldest creation date
    LARGEST_SIZE,        // Largest datasets first
    SMALLEST_SIZE,       // Smallest datasets first
    LOWEST_PRIORITY,     // Lowest priority first
    MOST_INACTIVE,       // Most days without access
    MANAGEMENT_CLASS     // By management class policy
};

// =============================================================================
// Volume Space Information
// =============================================================================

struct VolumeSpaceInfo {
    std::string volser;
    std::string deviceType;
    std::string storageGroup;
    
    uint64_t totalCapacity{0};        // Total capacity in bytes
    uint64_t allocatedSpace{0};       // Allocated space
    uint64_t usedSpace{0};            // Actually used
    uint64_t freeSpace{0};            // Free space
    uint64_t fragmentedSpace{0};      // Fragmented free space
    uint64_t largestFreeExtent{0};    // Largest contiguous free
    
    uint32_t datasetCount{0};         // Number of datasets
    uint32_t fragmentCount{0};        // Number of free extents
    
    double utilizationPercent{0.0};   // Space utilization %
    double fragmentationPercent{0.0}; // Fragmentation %
    
    std::chrono::system_clock::time_point lastUpdate;
    std::chrono::system_clock::time_point lastMigration;
    
    bool isOnline{true};
    bool isThresholdExceeded{false};
    bool isCritical{false};
    
    void recalculate() {
        if (totalCapacity > 0) {
            utilizationPercent = (static_cast<double>(allocatedSpace) / totalCapacity) * 100.0;
        }
        if (freeSpace > 0 && fragmentCount > 0) {
            fragmentationPercent = (1.0 - (static_cast<double>(largestFreeExtent) / freeSpace)) * 100.0;
        }
    }
};

// =============================================================================
// Dataset Space Information
// =============================================================================

struct DatasetSpaceInfo {
    std::string datasetName;
    std::string volser;
    std::string storageClass;
    std::string managementClass;
    
    uint64_t allocatedSpace{0};       // Allocated space
    uint64_t usedSpace{0};            // Actually used
    uint32_t blockSize{0};
    uint32_t recordLength{0};
    
    std::chrono::system_clock::time_point creationDate;
    std::chrono::system_clock::time_point lastReferenceDate;
    std::chrono::system_clock::time_point expirationDate;
    
    int32_t daysNonUsage{0};          // Days since last use
    int32_t priority{5};              // Dataset priority 1-10
    
    bool isExpired{false};
    bool isMigrated{false};
    bool isBackedUp{false};
    bool isCompressed{false};
    bool isEligibleForMigration{true};
    bool isEligibleForDelete{false};
    
    void updateDaysNonUsage() {
        auto now = std::chrono::system_clock::now();
        auto duration = now - lastReferenceDate;
        daysNonUsage = std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24;
    }
};

// =============================================================================
// Space Thresholds
// =============================================================================

struct SpaceThreshold {
    std::string name;
    ThresholdType type{ThresholdType::HIGH_THRESHOLD};
    double percent{0.0};             // Threshold percentage (0-100)
    SpaceAction action{SpaceAction::MIGRATE_PRIMARY};
    SelectionCriteria criteria{SelectionCriteria::OLDEST_REFERENCE};
    int priority{5};                  // Action priority
    bool enabled{true};
    
    std::string storageGroup;         // Apply to specific storage group
    std::string volumePattern;        // Apply to matching volumes
    
    int migrateDays{0};               // Migrate if not used for N days
    int deleteAfterDays{0};           // Delete migrated after N days
    int targetPercent{0};             // Target utilization after action
};

// =============================================================================
// Space Management Policy
// =============================================================================

struct SpacePolicy {
    std::string name;
    std::string description;
    
    std::vector<SpaceThreshold> thresholds;
    
    bool autoMigrate{true};
    bool autoDelete{false};
    bool autoReclaim{true};
    
    int primaryMigrateDays{30};       // Default days for primary migration
    int ml1MigrateDays{90};           // Default days for ML1 to ML2
    int deleteAfterDays{365};         // Default days before delete
    
    int minDatasetsPerCycle{10};      // Minimum datasets per cycle
    int maxDatasetsPerCycle{1000};    // Maximum datasets per cycle
    
    uint64_t minDatasetSize{0};       // Minimum size to migrate
    uint64_t maxDatasetSize{0};       // Maximum size (0 = unlimited)
    
    std::vector<std::string> excludePatterns;  // Patterns to exclude
    std::vector<std::string> includePatterns;  // Patterns to include (if set)
    
    std::chrono::hours interval{1};   // Check interval
    
    bool matchesDataset(const std::string& dsn) const {
        // Check excludes first
        for (const auto& pattern : excludePatterns) {
            if (wildcardMatch(dsn, pattern)) {
                return false;
            }
        }
        
        // If includes specified, must match one
        if (!includePatterns.empty()) {
            for (const auto& pattern : includePatterns) {
                if (wildcardMatch(dsn, pattern)) {
                    return true;
                }
            }
            return false;
        }
        
        return true;
    }
    
private:
    static bool wildcardMatch(const std::string& str, const std::string& pattern) {
        size_t s = 0, p = 0;
        size_t starIdx = std::string::npos, matchIdx = 0;
        
        while (s < str.size()) {
            if (p < pattern.size() && (pattern[p] == '?' || 
                std::toupper(pattern[p]) == std::toupper(str[s]))) {
                s++;
                p++;
            } else if (p < pattern.size() && pattern[p] == '*') {
                starIdx = p++;
                matchIdx = s;
            } else if (starIdx != std::string::npos) {
                p = starIdx + 1;
                s = ++matchIdx;
            } else {
                return false;
            }
        }
        while (p < pattern.size() && pattern[p] == '*') p++;
        return p == pattern.size();
    }
};

// =============================================================================
// Migration Candidate
// =============================================================================

struct MigrationCandidate {
    DatasetSpaceInfo dataset;
    double score{0.0};                // Selection score (higher = migrate first)
    SpaceAction recommendedAction{SpaceAction::MIGRATE_PRIMARY};
    std::string reason;
    
    bool operator<(const MigrationCandidate& other) const {
        return score < other.score;  // Priority queue needs less-than
    }
};

// =============================================================================
// Space Event
// =============================================================================

struct SpaceEvent {
    enum class Type {
        THRESHOLD_EXCEEDED,
        THRESHOLD_CLEARED,
        MIGRATION_STARTED,
        MIGRATION_COMPLETED,
        DELETE_COMPLETED,
        RECLAIM_COMPLETED,
        CRITICAL_SPACE,
        VOLUME_OFFLINE,
        VOLUME_ONLINE
    };
    
    Type type;
    std::string volser;
    std::string message;
    double percent{0.0};
    uint64_t spaceAffected{0};
    std::chrono::system_clock::time_point timestamp;
};

// =============================================================================
// Space Manager
// =============================================================================

class SpaceManager {
public:
    struct Config {
        std::chrono::seconds monitorInterval{300};  // 5 minutes
        size_t maxCandidatesPerCycle{1000};
        bool enableAutoMigration{true};
        bool enableAutoDelete{false};
        double defaultHighThreshold{85.0};
        double defaultLowThreshold{70.0};
        double defaultCriticalThreshold{95.0};
        
        Config() = default;
    };
    
    using EventCallback = std::function<void(const SpaceEvent&)>;
    using MigrationCallback = std::function<bool(const DatasetSpaceInfo&)>;
    using DeleteCallback = std::function<bool(const DatasetSpaceInfo&)>;
    
private:
    Config config_;
    mutable std::shared_mutex mutex_;
    
    std::map<std::string, VolumeSpaceInfo> volumes_;      // By volser
    std::map<std::string, SpacePolicy> policies_;         // By name
    std::multimap<std::string, DatasetSpaceInfo> volumeDatasets_;  // volser -> datasets
    
    std::vector<EventCallback> eventCallbacks_;
    MigrationCallback migrationCallback_;
    DeleteCallback deleteCallback_;
    
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> totalMigrated_{0};
    std::atomic<uint64_t> totalDeleted_{0};
    std::atomic<uint64_t> totalReclaimed_{0};
    
    // Statistics
    struct Statistics {
        uint64_t cyclesCompleted{0};
        uint64_t datasetsMigrated{0};
        uint64_t datasetsDeleted{0};
        uint64_t bytesMigrated{0};
        uint64_t bytesDeleted{0};
        uint64_t bytesReclaimed{0};
        uint32_t thresholdExceededCount{0};
        uint32_t criticalEventCount{0};
        std::chrono::system_clock::time_point lastCycleTime;
    } stats_;
    
public:
    SpaceManager() : config_() {}
    explicit SpaceManager(const Config& config) : config_(config) {}
    
    // =========================================================================
    // Volume Management
    // =========================================================================
    
    void registerVolume(const VolumeSpaceInfo& volume) {
        std::unique_lock lock(mutex_);
        volumes_[volume.volser] = volume;
    }
    
    void updateVolume(const VolumeSpaceInfo& volume) {
        std::unique_lock lock(mutex_);
        
        auto it = volumes_.find(volume.volser);
        if (it != volumes_.end()) {
            bool wasExceeded = it->second.isThresholdExceeded;
            it->second = volume;
            it->second.recalculate();
            
            // Check threshold changes
            checkThresholds(it->second, wasExceeded);
        }
    }
    
    void removeVolume(const std::string& volser) {
        std::unique_lock lock(mutex_);
        volumes_.erase(volser);
        volumeDatasets_.erase(volser);
    }
    
    std::optional<VolumeSpaceInfo> getVolume(const std::string& volser) const {
        std::shared_lock lock(mutex_);
        auto it = volumes_.find(volser);
        if (it != volumes_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<VolumeSpaceInfo> getAllVolumes() const {
        std::shared_lock lock(mutex_);
        std::vector<VolumeSpaceInfo> result;
        for (const auto& [volser, info] : volumes_) {
            result.push_back(info);
        }
        return result;
    }
    
    std::vector<VolumeSpaceInfo> getVolumesAboveThreshold(double threshold) const {
        std::shared_lock lock(mutex_);
        std::vector<VolumeSpaceInfo> result;
        for (const auto& [volser, info] : volumes_) {
            if (info.utilizationPercent >= threshold) {
                result.push_back(info);
            }
        }
        return result;
    }
    
    // =========================================================================
    // Dataset Management
    // =========================================================================
    
    void registerDataset(const DatasetSpaceInfo& dataset) {
        std::unique_lock lock(mutex_);
        volumeDatasets_.emplace(dataset.volser, dataset);
    }
    
    void updateDataset(const DatasetSpaceInfo& dataset) {
        std::unique_lock lock(mutex_);
        
        auto range = volumeDatasets_.equal_range(dataset.volser);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second.datasetName == dataset.datasetName) {
                it->second = dataset;
                return;
            }
        }
        // Not found, add it
        volumeDatasets_.emplace(dataset.volser, dataset);
    }
    
    void removeDataset(const std::string& volser, const std::string& datasetName) {
        std::unique_lock lock(mutex_);
        
        auto range = volumeDatasets_.equal_range(volser);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second.datasetName == datasetName) {
                volumeDatasets_.erase(it);
                return;
            }
        }
    }
    
    std::vector<DatasetSpaceInfo> getDatasetsOnVolume(const std::string& volser) const {
        std::shared_lock lock(mutex_);
        
        std::vector<DatasetSpaceInfo> result;
        auto range = volumeDatasets_.equal_range(volser);
        for (auto it = range.first; it != range.second; ++it) {
            result.push_back(it->second);
        }
        return result;
    }
    
    // =========================================================================
    // Policy Management
    // =========================================================================
    
    void addPolicy(const SpacePolicy& policy) {
        std::unique_lock lock(mutex_);
        policies_[policy.name] = policy;
    }
    
    void removePolicy(const std::string& name) {
        std::unique_lock lock(mutex_);
        policies_.erase(name);
    }
    
    std::optional<SpacePolicy> getPolicy(const std::string& name) const {
        std::shared_lock lock(mutex_);
        auto it = policies_.find(name);
        if (it != policies_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<SpacePolicy> getAllPolicies() const {
        std::shared_lock lock(mutex_);
        std::vector<SpacePolicy> result;
        for (const auto& [name, policy] : policies_) {
            result.push_back(policy);
        }
        return result;
    }
    
    // =========================================================================
    // Threshold Management
    // =========================================================================
    
    void setDefaultThresholds(const std::string& volser,
                              double high, double low, double critical) {
        std::unique_lock lock(mutex_);
        
        SpacePolicy defaultPolicy;
        defaultPolicy.name = "DEFAULT_" + volser;
        
        SpaceThreshold highThresh;
        highThresh.name = "HIGH";
        highThresh.type = ThresholdType::HIGH_THRESHOLD;
        highThresh.percent = high;
        highThresh.action = SpaceAction::MIGRATE_PRIMARY;
        defaultPolicy.thresholds.push_back(highThresh);
        
        SpaceThreshold lowThresh;
        lowThresh.name = "LOW";
        lowThresh.type = ThresholdType::LOW_THRESHOLD;
        lowThresh.percent = low;
        lowThresh.action = SpaceAction::NONE;
        defaultPolicy.thresholds.push_back(lowThresh);
        
        SpaceThreshold critThresh;
        critThresh.name = "CRITICAL";
        critThresh.type = ThresholdType::CRITICAL_THRESHOLD;
        critThresh.percent = critical;
        critThresh.action = SpaceAction::DELETE_EXPIRED;
        defaultPolicy.thresholds.push_back(critThresh);
        
        policies_[defaultPolicy.name] = defaultPolicy;
    }
    
    // =========================================================================
    // Candidate Selection
    // =========================================================================
    
    std::vector<MigrationCandidate> selectMigrationCandidates(
            const std::string& volser,
            size_t maxCandidates = 100,
            SelectionCriteria criteria = SelectionCriteria::OLDEST_REFERENCE) const {
        std::shared_lock lock(mutex_);
        
        std::priority_queue<MigrationCandidate> candidates;
        
        auto range = volumeDatasets_.equal_range(volser);
        for (auto it = range.first; it != range.second; ++it) {
            const auto& ds = it->second;
            
            if (!ds.isEligibleForMigration || ds.isMigrated) {
                continue;
            }
            
            MigrationCandidate candidate;
            candidate.dataset = ds;
            candidate.score = calculateScore(ds, criteria);
            candidate.recommendedAction = SpaceAction::MIGRATE_PRIMARY;
            candidate.reason = getSelectionReason(ds, criteria);
            
            candidates.push(candidate);
            
            // Limit queue size
            while (candidates.size() > maxCandidates) {
                candidates.pop();
            }
        }
        
        // Convert to vector (reverse order - highest score first)
        std::vector<MigrationCandidate> result;
        while (!candidates.empty()) {
            result.push_back(candidates.top());
            candidates.pop();
        }
        std::reverse(result.begin(), result.end());
        
        return result;
    }
    
    std::vector<DatasetSpaceInfo> selectDeletionCandidates(
            const std::string& volser,
            size_t maxCandidates = 100) const {
        std::shared_lock lock(mutex_);
        
        std::vector<DatasetSpaceInfo> candidates;
        
        auto range = volumeDatasets_.equal_range(volser);
        for (auto it = range.first; it != range.second; ++it) {
            const auto& ds = it->second;
            
            if (ds.isExpired || ds.isEligibleForDelete) {
                candidates.push_back(ds);
                if (candidates.size() >= maxCandidates) {
                    break;
                }
            }
        }
        
        // Sort by expiration date
        std::sort(candidates.begin(), candidates.end(),
            [](const DatasetSpaceInfo& a, const DatasetSpaceInfo& b) {
                return a.expirationDate < b.expirationDate;
            });
        
        return candidates;
    }
    
    // =========================================================================
    // Space Processing
    // =========================================================================
    
    struct ProcessingResult {
        std::string volser;
        size_t datasetsMigrated{0};
        size_t datasetsDeleted{0};
        uint64_t bytesMigrated{0};
        uint64_t bytesDeleted{0};
        uint64_t bytesReclaimed{0};
        double utilizationBefore{0.0};
        double utilizationAfter{0.0};
        std::vector<std::string> errors;
        std::vector<std::string> messages;
        std::chrono::milliseconds duration{0};
    };
    
    ProcessingResult processVolume(const std::string& volser) {
        ProcessingResult result;
        result.volser = volser;
        
        auto start = std::chrono::steady_clock::now();
        
        auto volOpt = getVolume(volser);
        if (!volOpt) {
            result.errors.push_back("Volume not found: " + volser);
            return result;
        }
        
        VolumeSpaceInfo vol = *volOpt;
        result.utilizationBefore = vol.utilizationPercent;
        
        // Find applicable policy
        SpacePolicy policy = findApplicablePolicy(volser, vol.storageGroup);
        
        // Check thresholds
        for (const auto& threshold : policy.thresholds) {
            if (!threshold.enabled) continue;
            
            if (vol.utilizationPercent >= threshold.percent) {
                result.messages.push_back("Threshold exceeded: " + threshold.name + 
                                         " (" + std::to_string(threshold.percent) + "%)");
                
                switch (threshold.action) {
                    case SpaceAction::MIGRATE_PRIMARY:
                        processMigration(volser, policy, threshold, result);
                        break;
                    case SpaceAction::DELETE_EXPIRED:
                        processDeletion(volser, policy, threshold, result);
                        break;
                    case SpaceAction::ALERT:
                        fireEvent(SpaceEvent::Type::THRESHOLD_EXCEEDED, volser,
                                 "Threshold " + threshold.name + " exceeded",
                                 vol.utilizationPercent);
                        break;
                    default:
                        break;
                }
            }
        }
        
        // Update volume info
        volOpt = getVolume(volser);
        if (volOpt) {
            result.utilizationAfter = volOpt->utilizationPercent;
        }
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Update statistics
        {
            std::unique_lock lock(mutex_);
            stats_.datasetsMigrated += result.datasetsMigrated;
            stats_.datasetsDeleted += result.datasetsDeleted;
            stats_.bytesMigrated += result.bytesMigrated;
            stats_.bytesDeleted += result.bytesDeleted;
            stats_.bytesReclaimed += result.bytesReclaimed;
        }
        
        return result;
    }
    
    std::vector<ProcessingResult> processAllVolumes() {
        std::vector<ProcessingResult> results;
        
        auto vols = getAllVolumes();
        for (const auto& vol : vols) {
            if (vol.isOnline && vol.isThresholdExceeded) {
                results.push_back(processVolume(vol.volser));
            }
        }
        
        {
            std::unique_lock lock(mutex_);
            stats_.cyclesCompleted++;
            stats_.lastCycleTime = std::chrono::system_clock::now();
        }
        
        return results;
    }
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    void setMigrationCallback(MigrationCallback callback) {
        std::unique_lock lock(mutex_);
        migrationCallback_ = std::move(callback);
    }
    
    void setDeleteCallback(DeleteCallback callback) {
        std::unique_lock lock(mutex_);
        deleteCallback_ = std::move(callback);
    }
    
    void addEventListener(EventCallback callback) {
        std::unique_lock lock(mutex_);
        eventCallbacks_.push_back(std::move(callback));
    }
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    Statistics getStatistics() const {
        std::shared_lock lock(mutex_);
        return stats_;
    }
    
    void resetStatistics() {
        std::unique_lock lock(mutex_);
        stats_ = Statistics{};
    }
    
    // =========================================================================
    // Space Analysis
    // =========================================================================
    
    struct SpaceAnalysis {
        uint64_t totalCapacity{0};
        uint64_t totalAllocated{0};
        uint64_t totalFree{0};
        double overallUtilization{0.0};
        
        uint32_t volumeCount{0};
        uint32_t volumesAboveHigh{0};
        uint32_t volumesAboveCritical{0};
        
        uint64_t estimatedReclaimable{0};
        uint64_t migratedDataSize{0};
        
        std::vector<std::string> criticalVolumes;
        std::vector<std::string> warningVolumes;
    };
    
    SpaceAnalysis analyzeSpace() const {
        std::shared_lock lock(mutex_);
        
        SpaceAnalysis analysis;
        
        for (const auto& [volser, vol] : volumes_) {
            analysis.volumeCount++;
            analysis.totalCapacity += vol.totalCapacity;
            analysis.totalAllocated += vol.allocatedSpace;
            analysis.totalFree += vol.freeSpace;
            
            if (vol.utilizationPercent >= config_.defaultCriticalThreshold) {
                analysis.volumesAboveCritical++;
                analysis.criticalVolumes.push_back(volser);
            } else if (vol.utilizationPercent >= config_.defaultHighThreshold) {
                analysis.volumesAboveHigh++;
                analysis.warningVolumes.push_back(volser);
            }
        }
        
        if (analysis.totalCapacity > 0) {
            analysis.overallUtilization = 
                (static_cast<double>(analysis.totalAllocated) / analysis.totalCapacity) * 100.0;
        }
        
        // Estimate reclaimable space
        for (const auto& [volser, ds] : volumeDatasets_) {
            if (ds.isExpired || ds.isEligibleForDelete) {
                analysis.estimatedReclaimable += ds.allocatedSpace;
            }
            if (ds.isMigrated) {
                analysis.migratedDataSize += ds.allocatedSpace;
            }
        }
        
        return analysis;
    }
    
    // =========================================================================
    // Space Forecasting
    // =========================================================================
    
    struct SpaceForecast {
        std::string volser;
        double currentUtilization{0.0};
        double dailyGrowthRate{0.0};        // % per day
        int daysToHighThreshold{-1};         // -1 = N/A
        int daysToCritical{-1};
        int daysToFull{-1};
        std::chrono::system_clock::time_point forecastDate;
    };
    
    SpaceForecast forecastVolume(const std::string& volser,
                                 double dailyGrowthPercent = 0.1) const {
        std::shared_lock lock(mutex_);
        
        SpaceForecast forecast;
        forecast.volser = volser;
        forecast.dailyGrowthRate = dailyGrowthPercent;
        forecast.forecastDate = std::chrono::system_clock::now();
        
        auto it = volumes_.find(volser);
        if (it == volumes_.end()) {
            return forecast;
        }
        
        const auto& vol = it->second;
        forecast.currentUtilization = vol.utilizationPercent;
        
        double remaining = 100.0 - vol.utilizationPercent;
        
        // Calculate days until thresholds
        if (dailyGrowthPercent > 0) {
            double toHigh = config_.defaultHighThreshold - vol.utilizationPercent;
            if (toHigh > 0) {
                forecast.daysToHighThreshold = static_cast<int>(toHigh / dailyGrowthPercent);
            } else {
                forecast.daysToHighThreshold = 0;
            }
            
            double toCritical = config_.defaultCriticalThreshold - vol.utilizationPercent;
            if (toCritical > 0) {
                forecast.daysToCritical = static_cast<int>(toCritical / dailyGrowthPercent);
            } else {
                forecast.daysToCritical = 0;
            }
            
            if (remaining > 0) {
                forecast.daysToFull = static_cast<int>(remaining / dailyGrowthPercent);
            } else {
                forecast.daysToFull = 0;
            }
        }
        
        return forecast;
    }
    
private:
    double calculateScore(const DatasetSpaceInfo& ds, SelectionCriteria criteria) const {
        switch (criteria) {
            case SelectionCriteria::OLDEST_REFERENCE:
                return static_cast<double>(ds.daysNonUsage);
                
            case SelectionCriteria::OLDEST_CREATION: {
                auto now = std::chrono::system_clock::now();
                auto age = std::chrono::duration_cast<std::chrono::hours>(
                    now - ds.creationDate).count();
                return static_cast<double>(age);
            }
            
            case SelectionCriteria::LARGEST_SIZE:
                return static_cast<double>(ds.allocatedSpace);
                
            case SelectionCriteria::SMALLEST_SIZE:
                return ds.allocatedSpace > 0 ? 
                    1.0 / static_cast<double>(ds.allocatedSpace) : 1000000.0;
                
            case SelectionCriteria::LOWEST_PRIORITY:
                return 10.0 - static_cast<double>(ds.priority);
                
            case SelectionCriteria::MOST_INACTIVE:
                return static_cast<double>(ds.daysNonUsage) * 
                       (ds.allocatedSpace / 1048576.0);  // Weight by size in MB
                
            default:
                return static_cast<double>(ds.daysNonUsage);
        }
    }
    
    std::string getSelectionReason(const DatasetSpaceInfo& ds, 
                                   SelectionCriteria criteria) const {
        switch (criteria) {
            case SelectionCriteria::OLDEST_REFERENCE:
                return "Last referenced " + std::to_string(ds.daysNonUsage) + " days ago";
            case SelectionCriteria::LARGEST_SIZE:
                return "Large dataset: " + std::to_string(ds.allocatedSpace / 1048576) + " MB";
            case SelectionCriteria::LOWEST_PRIORITY:
                return "Low priority: " + std::to_string(ds.priority);
            case SelectionCriteria::MOST_INACTIVE:
                return "Inactive for " + std::to_string(ds.daysNonUsage) + " days";
            default:
                return "Selected by policy";
        }
    }
    
    void checkThresholds(VolumeSpaceInfo& vol, bool wasExceeded) {
        bool nowExceeded = vol.utilizationPercent >= config_.defaultHighThreshold;
        vol.isThresholdExceeded = nowExceeded;
        vol.isCritical = vol.utilizationPercent >= config_.defaultCriticalThreshold;
        
        if (nowExceeded && !wasExceeded) {
            fireEvent(SpaceEvent::Type::THRESHOLD_EXCEEDED, vol.volser,
                     "High threshold exceeded", vol.utilizationPercent);
            stats_.thresholdExceededCount++;
        } else if (!nowExceeded && wasExceeded) {
            fireEvent(SpaceEvent::Type::THRESHOLD_CLEARED, vol.volser,
                     "Below threshold", vol.utilizationPercent);
        }
        
        if (vol.isCritical) {
            fireEvent(SpaceEvent::Type::CRITICAL_SPACE, vol.volser,
                     "Critical space condition", vol.utilizationPercent);
            stats_.criticalEventCount++;
        }
    }
    
    SpacePolicy findApplicablePolicy(const std::string& volser,
                                    const std::string& storageGroup) const {
        // Look for volume-specific policy
        auto it = policies_.find("VOL_" + volser);
        if (it != policies_.end()) {
            return it->second;
        }
        
        // Look for storage group policy
        it = policies_.find("SG_" + storageGroup);
        if (it != policies_.end()) {
            return it->second;
        }
        
        // Look for default policy
        it = policies_.find("DEFAULT");
        if (it != policies_.end()) {
            return it->second;
        }
        
        // Return built-in default
        SpacePolicy defaultPolicy;
        defaultPolicy.name = "BUILTIN_DEFAULT";
        
        SpaceThreshold high;
        high.type = ThresholdType::HIGH_THRESHOLD;
        high.percent = config_.defaultHighThreshold;
        high.action = SpaceAction::MIGRATE_PRIMARY;
        defaultPolicy.thresholds.push_back(high);
        
        return defaultPolicy;
    }
    
    void processMigration(const std::string& volser,
                         const SpacePolicy& policy,
                         const SpaceThreshold& threshold,
                         ProcessingResult& result) {
        auto candidates = selectMigrationCandidates(
            volser, 
            policy.maxDatasetsPerCycle,
            threshold.criteria);
        
        for (const auto& candidate : candidates) {
            if (migrationCallback_) {
                std::shared_lock lock(mutex_);
                if (migrationCallback_(candidate.dataset)) {
                    result.datasetsMigrated++;
                    result.bytesMigrated += candidate.dataset.allocatedSpace;
                    result.messages.push_back("Migrated: " + candidate.dataset.datasetName);
                    
                    totalMigrated_++;
                }
            }
            
            // Check if we've reached target
            auto volOpt = getVolume(volser);
            if (volOpt && volOpt->utilizationPercent <= 
                (threshold.targetPercent > 0 ? threshold.targetPercent : 
                 config_.defaultLowThreshold)) {
                break;
            }
        }
    }
    
    void processDeletion(const std::string& volser,
                        const SpacePolicy& policy,
                        const SpaceThreshold& /*threshold*/,
                        ProcessingResult& result) {
        auto candidates = selectDeletionCandidates(volser, policy.maxDatasetsPerCycle);
        
        for (const auto& ds : candidates) {
            if (deleteCallback_) {
                std::shared_lock lock(mutex_);
                if (deleteCallback_(ds)) {
                    result.datasetsDeleted++;
                    result.bytesDeleted += ds.allocatedSpace;
                    result.messages.push_back("Deleted: " + ds.datasetName);
                    
                    totalDeleted_++;
                }
            }
        }
    }
    
    void fireEvent(SpaceEvent::Type type, const std::string& volser,
                  const std::string& message, double percent = 0.0) {
        SpaceEvent event;
        event.type = type;
        event.volser = volser;
        event.message = message;
        event.percent = percent;
        event.timestamp = std::chrono::system_clock::now();
        
        for (const auto& callback : eventCallbacks_) {
            callback(event);
        }
    }
};

// =============================================================================
// Utility Functions
// =============================================================================

inline std::string formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 5) {
        size /= 1024.0;
        unitIndex++;
    }
    
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f %s", size, units[unitIndex]);
    return std::string(buf);
}

inline std::string thresholdTypeToString(ThresholdType type) {
    switch (type) {
        case ThresholdType::HIGH_THRESHOLD: return "HIGH";
        case ThresholdType::LOW_THRESHOLD: return "LOW";
        case ThresholdType::CRITICAL_THRESHOLD: return "CRITICAL";
        case ThresholdType::WARNING_THRESHOLD: return "WARNING";
        default: return "UNKNOWN";
    }
}

inline std::string spaceActionToString(SpaceAction action) {
    switch (action) {
        case SpaceAction::NONE: return "NONE";
        case SpaceAction::MIGRATE_PRIMARY: return "MIGRATE_PRIMARY";
        case SpaceAction::MIGRATE_ML1_TO_ML2: return "MIGRATE_ML1_TO_ML2";
        case SpaceAction::DELETE_EXPIRED: return "DELETE_EXPIRED";
        case SpaceAction::DELETE_MIGRATED: return "DELETE_MIGRATED";
        case SpaceAction::RECALL: return "RECALL";
        case SpaceAction::COMPRESS: return "COMPRESS";
        case SpaceAction::DEFRAGMENT: return "DEFRAGMENT";
        case SpaceAction::EXTEND: return "EXTEND";
        case SpaceAction::ALERT: return "ALERT";
        default: return "UNKNOWN";
    }
}

}  // namespace space
}  // namespace hsm

#endif // HSM_SPACE_MANAGEMENT_HPP
