/*
 * DFSMShsm Emulator - Fast Replication
 * @version 4.2.0
 * 
 * Provides fast replication capabilities including:
 * - FlashCopy emulation
 * - Consistency groups
 * - Point-in-time copies
 * - Incremental replication
 * - Copy pair management
 * - Cross-site replication
 * 
 * Author: DFSMShsm Emulator Team
 * License: MIT
 */

#ifndef HSM_FAST_REPLICATION_HPP
#define HSM_FAST_REPLICATION_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <memory>
#include <functional>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <variant>
#include <queue>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <ctime>

namespace dfsmshsm {
namespace replication {

// ============================================================================
// Forward Declarations
// ============================================================================

class CopyPair;
class ConsistencyGroup;
class FlashCopyManager;
class ReplicationManager;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Copy type
 */
enum class CopyType {
    FLASHCOPY,           // Point-in-time copy
    INCREMENTAL,         // Incremental/differential copy
    FULL,                // Full copy
    SNAPSHOT,            // Snapshot (space-efficient)
    CLONE                // Full clone
};

/**
 * @brief Copy state
 */
enum class CopyState {
    NONE,                // No copy relationship
    PENDING,             // Copy pending
    COPYING,             // Copy in progress
    COPIED,              // Background copy complete
    SUSPENDED,           // Copy suspended
    FAILED,              // Copy failed
    SPLIT,               // Copy relationship split
    REVERSE_COPYING      // Reverse copy in progress
};

/**
 * @brief Copy direction
 */
enum class CopyDirection {
    FORWARD,             // Source to target
    REVERSE              // Target to source
};

/**
 * @brief Consistency group state
 */
enum class GroupState {
    ENABLED,             // Group is active
    SUSPENDED,           // Group is suspended
    DISABLED,            // Group is disabled
    ERROR                // Group in error state
};

/**
 * @brief Replication mode
 */
enum class ReplicationMode {
    SYNCHRONOUS,         // Wait for target write
    ASYNCHRONOUS,        // Don't wait for target
    NEAR_SYNCHRONOUS     // Periodic sync points
};

/**
 * @brief Recovery point objective
 */
enum class RPOLevel {
    ZERO,                // Zero data loss (synchronous)
    SECONDS,             // Seconds of data (near-sync)
    MINUTES,             // Minutes of data (async)
    HOURS                // Hours of data (periodic)
};

// ============================================================================
// Utility Structures
// ============================================================================

/**
 * @brief Copy pair relationship
 */
struct CopyPairInfo {
    std::string pairId;
    std::string sourceDsn;
    std::string sourceVolume;
    std::string targetDsn;
    std::string targetVolume;
    
    CopyType type{CopyType::FLASHCOPY};
    CopyState state{CopyState::NONE};
    CopyDirection direction{CopyDirection::FORWARD};
    ReplicationMode mode{ReplicationMode::ASYNCHRONOUS};
    
    std::chrono::system_clock::time_point createdTime{std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point completedTime;
    
    uint64_t sourceSize{0};
    uint64_t targetSize{0};
    uint64_t bytesCopied{0};
    uint64_t tracksTotal{0};
    uint64_t tracksCopied{0};
    
    std::string consistencyGroupName;
    uint32_t sequenceNumber{0};
    
    std::string errorMessage;
    uint32_t errorCode{0};
    
    double getProgress() const {
        if (tracksTotal == 0) return 0.0;
        return (static_cast<double>(tracksCopied) / tracksTotal) * 100.0;
    }
    
    bool isComplete() const {
        return state == CopyState::COPIED || tracksCopied >= tracksTotal;
    }
    
    std::string getStateString() const {
        switch (state) {
            case CopyState::NONE: return "NONE";
            case CopyState::PENDING: return "PENDING";
            case CopyState::COPYING: return "COPYING";
            case CopyState::COPIED: return "COPIED";
            case CopyState::SUSPENDED: return "SUSPENDED";
            case CopyState::FAILED: return "FAILED";
            case CopyState::SPLIT: return "SPLIT";
            case CopyState::REVERSE_COPYING: return "REVERSE_COPYING";
            default: return "UNKNOWN";
        }
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Pair[" << pairId << "] " << sourceDsn << " -> " << targetDsn
            << " State=" << getStateString()
            << " Progress=" << std::fixed << std::setprecision(1) << getProgress() << "%";
        return oss.str();
    }
};

/**
 * @brief Consistency group information
 */
struct ConsistencyGroupInfo {
    std::string groupName;
    std::string description;
    GroupState state{GroupState::ENABLED};
    ReplicationMode mode{ReplicationMode::ASYNCHRONOUS};
    RPOLevel rpoLevel{RPOLevel::MINUTES};
    
    std::vector<std::string> pairIds;
    
    std::chrono::system_clock::time_point createdTime{std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point lastSyncTime;
    std::chrono::system_clock::time_point lastCheckpointTime;
    
    uint32_t syncIntervalSeconds{60};
    uint32_t checkpointIntervalSeconds{300};
    
    uint64_t totalSourceSize{0};
    uint64_t totalBytesCopied{0};
    
    std::string errorMessage;
    
    size_t getPairCount() const { return pairIds.size(); }
    
    double getOverallProgress() const {
        if (totalSourceSize == 0) return 0.0;
        return (static_cast<double>(totalBytesCopied) / totalSourceSize) * 100.0;
    }
    
    std::string getStateString() const {
        switch (state) {
            case GroupState::ENABLED: return "ENABLED";
            case GroupState::SUSPENDED: return "SUSPENDED";
            case GroupState::DISABLED: return "DISABLED";
            case GroupState::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Group[" << groupName << "] " << getStateString()
            << " Pairs=" << pairIds.size()
            << " Progress=" << std::fixed << std::setprecision(1) 
            << getOverallProgress() << "%";
        return oss.str();
    }
};

/**
 * @brief FlashCopy options
 */
struct FlashCopyOptions {
    bool backgroundCopy{true};       // Perform background copy
    bool persistent{true};           // Persist relationship
    bool nocopy{false};              // Establish without copying
    bool split{false};               // Split after copy
    bool freeze{false};              // Freeze source during copy
    bool verify{true};               // Verify copy
    std::chrono::seconds timeout{3600};
    std::string consistencyGroup;
    uint32_t priority{5};            // 1-9, 5 is normal
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "BackgroundCopy=" << (backgroundCopy ? "Y" : "N")
            << ", Persistent=" << (persistent ? "Y" : "N")
            << ", Priority=" << priority;
        return oss.str();
    }
};

/**
 * @brief Replication result
 */
struct ReplicationResult {
    bool success{false};
    std::string pairId;
    std::string groupName;
    CopyState finalState{CopyState::NONE};
    
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::chrono::milliseconds duration{0};
    
    uint64_t bytesProcessed{0};
    uint64_t tracksProcessed{0};
    double throughputMBps{0.0};
    
    std::string errorMessage;
    uint32_t errorCode{0};
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Replication " << (success ? "SUCCEEDED" : "FAILED")
            << " Pair=" << pairId
            << " Bytes=" << bytesProcessed
            << " Duration=" << duration.count() << "ms";
        if (!success) {
            oss << " Error=" << errorMessage;
        }
        return oss.str();
    }
};

/**
 * @brief Replication statistics
 */
struct ReplicationStatistics {
    uint64_t copyOperations{0};
    uint64_t successfulCopies{0};
    uint64_t failedCopies{0};
    uint64_t bytesCopied{0};
    uint64_t tracksCopied{0};
    
    uint32_t activePairs{0};
    uint32_t pendingPairs{0};
    uint32_t completedPairs{0};
    uint32_t failedPairs{0};
    
    double avgCopyTimeMs{0.0};
    double avgThroughputMBps{0.0};
    
    std::chrono::system_clock::time_point startTime{std::chrono::system_clock::now()};
    
    double getSuccessRate() const {
        if (copyOperations == 0) return 100.0;
        return (static_cast<double>(successfulCopies) / copyOperations) * 100.0;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Replication Statistics:\n"
            << "  Copy Operations: " << copyOperations << "\n"
            << "  Successful: " << successfulCopies << "\n"
            << "  Failed: " << failedCopies << "\n"
            << "  Success Rate: " << std::fixed << std::setprecision(1) 
            << getSuccessRate() << "%\n"
            << "  Bytes Copied: " << bytesCopied << "\n"
            << "  Avg Throughput: " << avgThroughputMBps << " MB/s";
        return oss.str();
    }
};

// ============================================================================
// Copy Pair
// ============================================================================

/**
 * @brief Manages a single copy pair relationship
 */
class CopyPair {
public:
    CopyPair() {
        generatePairId();
    }
    
    CopyPair(const std::string& sourceDsn, const std::string& targetDsn)
        : sourceDsn_(sourceDsn), targetDsn_(targetDsn) {
        generatePairId();
        info_.sourceDsn = sourceDsn;
        info_.targetDsn = targetDsn;
        info_.pairId = pairId_;
    }
    
    // Configuration
    CopyPair& setSourceVolume(const std::string& vol) {
        info_.sourceVolume = vol;
        return *this;
    }
    
    CopyPair& setTargetVolume(const std::string& vol) {
        info_.targetVolume = vol;
        return *this;
    }
    
    CopyPair& setCopyType(CopyType type) {
        info_.type = type;
        return *this;
    }
    
    CopyPair& setReplicationMode(ReplicationMode mode) {
        info_.mode = mode;
        return *this;
    }
    
    CopyPair& setSourceSize(uint64_t size) {
        info_.sourceSize = size;
        info_.tracksTotal = (size + 56663) / 56664;  // 3390 track size
        return *this;
    }
    
    CopyPair& setConsistencyGroup(const std::string& groupName) {
        info_.consistencyGroupName = groupName;
        return *this;
    }
    
    // State management
    void establish() {
        info_.state = CopyState::PENDING;
        info_.createdTime = std::chrono::system_clock::now();
    }
    
    void startCopy() {
        info_.state = CopyState::COPYING;
        info_.startTime = std::chrono::system_clock::now();
    }
    
    void updateProgress(uint64_t tracksCopied, uint64_t bytesCopied) {
        info_.tracksCopied = tracksCopied;
        info_.bytesCopied = bytesCopied;
        
        if (info_.tracksCopied >= info_.tracksTotal) {
            completeCopy();
        }
    }
    
    void completeCopy() {
        info_.state = CopyState::COPIED;
        info_.completedTime = std::chrono::system_clock::now();
        info_.tracksCopied = info_.tracksTotal;
        info_.bytesCopied = info_.sourceSize;
    }
    
    void suspend() {
        info_.state = CopyState::SUSPENDED;
    }
    
    void resume() {
        if (info_.state == CopyState::SUSPENDED) {
            info_.state = CopyState::COPYING;
        }
    }
    
    void split() {
        info_.state = CopyState::SPLIT;
    }
    
    void fail(const std::string& message, uint32_t errorCode = 0) {
        info_.state = CopyState::FAILED;
        info_.errorMessage = message;
        info_.errorCode = errorCode;
    }
    
    void reverseDirection() {
        // Swap source and target
        std::swap(info_.sourceDsn, info_.targetDsn);
        std::swap(info_.sourceVolume, info_.targetVolume);
        std::swap(info_.sourceSize, info_.targetSize);
        info_.direction = (info_.direction == CopyDirection::FORWARD) ?
                          CopyDirection::REVERSE : CopyDirection::FORWARD;
        info_.state = CopyState::REVERSE_COPYING;
    }
    
    // Getters
    const std::string& getPairId() const { return pairId_; }
    const std::string& getSourceDsn() const { return sourceDsn_; }
    const std::string& getTargetDsn() const { return targetDsn_; }
    const CopyPairInfo& getInfo() const { return info_; }
    CopyPairInfo& getInfo() { return info_; }
    
    bool isActive() const {
        return info_.state == CopyState::PENDING ||
               info_.state == CopyState::COPYING ||
               info_.state == CopyState::REVERSE_COPYING;
    }
    
    bool isComplete() const {
        return info_.state == CopyState::COPIED ||
               info_.state == CopyState::SPLIT;
    }

private:
    std::string pairId_;
    std::string sourceDsn_;
    std::string targetDsn_;
    CopyPairInfo info_;
    
    void generatePairId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "FCP" << std::put_time(std::localtime(&time), "%Y%m%d%H%M%S")
            << std::setfill('0') << std::setw(4) << (++counter % 10000);
        pairId_ = oss.str();
        info_.pairId = pairId_;
    }
};

// ============================================================================
// Consistency Group
// ============================================================================

/**
 * @brief Manages a consistency group of copy pairs
 */
class ConsistencyGroup {
public:
    ConsistencyGroup() = default;
    
    explicit ConsistencyGroup(const std::string& name,
                             const std::string& description = "")
        : name_(name) {
        info_.groupName = name;
        info_.description = description;
        info_.createdTime = std::chrono::system_clock::now();
    }
    
    // Configuration
    ConsistencyGroup& setDescription(const std::string& desc) {
        info_.description = desc;
        return *this;
    }
    
    ConsistencyGroup& setReplicationMode(ReplicationMode mode) {
        info_.mode = mode;
        return *this;
    }
    
    ConsistencyGroup& setRPOLevel(RPOLevel rpo) {
        info_.rpoLevel = rpo;
        return *this;
    }
    
    ConsistencyGroup& setSyncInterval(uint32_t seconds) {
        info_.syncIntervalSeconds = seconds;
        return *this;
    }
    
    ConsistencyGroup& setCheckpointInterval(uint32_t seconds) {
        info_.checkpointIntervalSeconds = seconds;
        return *this;
    }
    
    // Pair management
    bool addPair(const std::string& pairId, uint64_t sourceSize = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (std::find(info_.pairIds.begin(), info_.pairIds.end(), pairId) 
                != info_.pairIds.end()) {
            return false;  // Already exists
        }
        
        info_.pairIds.push_back(pairId);
        info_.totalSourceSize += sourceSize;
        return true;
    }
    
    bool removePair(const std::string& pairId) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = std::find(info_.pairIds.begin(), info_.pairIds.end(), pairId);
        if (it == info_.pairIds.end()) return false;
        
        info_.pairIds.erase(it);
        return true;
    }
    
    bool containsPair(const std::string& pairId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::find(info_.pairIds.begin(), info_.pairIds.end(), pairId)
               != info_.pairIds.end();
    }
    
    // State management
    void enable() {
        info_.state = GroupState::ENABLED;
    }
    
    void suspend() {
        info_.state = GroupState::SUSPENDED;
    }
    
    void disable() {
        info_.state = GroupState::DISABLED;
    }
    
    void setError(const std::string& message) {
        info_.state = GroupState::ERROR;
        info_.errorMessage = message;
    }
    
    // Sync point management
    void recordSyncPoint() {
        info_.lastSyncTime = std::chrono::system_clock::now();
    }
    
    void recordCheckpoint() {
        info_.lastCheckpointTime = std::chrono::system_clock::now();
    }
    
    void updateProgress(uint64_t bytesCopied) {
        info_.totalBytesCopied = bytesCopied;
    }
    
    // Getters
    const std::string& getName() const { return name_; }
    const ConsistencyGroupInfo& getInfo() const { return info_; }
    ConsistencyGroupInfo& getInfo() { return info_; }
    
    std::vector<std::string> getPairIds() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return info_.pairIds;
    }
    
    bool isEnabled() const {
        return info_.state == GroupState::ENABLED;
    }
    
    bool needsSync() const {
        if (!isEnabled()) return false;
        
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - info_.lastSyncTime);
        return elapsed.count() >= info_.syncIntervalSeconds;
    }
    
    bool needsCheckpoint() const {
        if (!isEnabled()) return false;
        
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - info_.lastCheckpointTime);
        return elapsed.count() >= info_.checkpointIntervalSeconds;
    }

private:
    std::string name_;
    ConsistencyGroupInfo info_;
    mutable std::mutex mutex_;
};

// ============================================================================
// FlashCopy Manager
// ============================================================================

/**
 * @brief Manages FlashCopy operations
 */
class FlashCopyManager {
public:
    FlashCopyManager() = default;
    
    // Establish FlashCopy
    ReplicationResult establishFlashCopy(const std::string& sourceDsn,
                                         const std::string& targetDsn,
                                         const FlashCopyOptions& options = FlashCopyOptions()) {
        ReplicationResult result;
        result.startTime = std::chrono::system_clock::now();
        
        // Create copy pair
        auto pair = std::make_shared<CopyPair>(sourceDsn, targetDsn);
        pair->setCopyType(CopyType::FLASHCOPY);
        pair->setSourceSize(100 * 1024 * 1024);  // Simulated 100MB
        
        // Add to consistency group if specified
        if (!options.consistencyGroup.empty()) {
            pair->setConsistencyGroup(options.consistencyGroup);
            
            auto groupIt = groups_.find(options.consistencyGroup);
            if (groupIt != groups_.end()) {
                groupIt->second->addPair(pair->getPairId(), 
                                        pair->getInfo().sourceSize);
            }
        }
        
        pair->establish();
        
        std::string pairId = pair->getPairId();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pairs_[pairId] = pair;
            stats_.copyOperations++;
        }
        
        // Simulate copy if background copy is enabled
        if (options.backgroundCopy && !options.nocopy) {
            pair->startCopy();
            
            // Simulate progress
            uint64_t totalTracks = pair->getInfo().tracksTotal;
            uint64_t tracksCopied = 0;
            uint64_t bytesCopied = 0;
            
            while (tracksCopied < totalTracks) {
                tracksCopied = std::min(tracksCopied + 100, totalTracks);
                bytesCopied = tracksCopied * 56664;
                pair->updateProgress(tracksCopied, bytesCopied);
            }
            
            pair->completeCopy();
            
            if (options.split) {
                pair->split();
            }
            
            result.success = true;
            result.finalState = pair->getInfo().state;
            
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.successfulCopies++;
                stats_.bytesCopied += bytesCopied;
                stats_.tracksCopied += totalTracks;
            }
        } else {
            result.success = true;
            result.finalState = CopyState::PENDING;
        }
        
        result.pairId = pairId;
        result.groupName = options.consistencyGroup;
        result.bytesProcessed = pair->getInfo().bytesCopied;
        result.tracksProcessed = pair->getInfo().tracksCopied;
        result.endTime = std::chrono::system_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.endTime - result.startTime);
        
        if (result.duration.count() > 0) {
            result.throughputMBps = (result.bytesProcessed / (1024.0 * 1024.0)) /
                                   (result.duration.count() / 1000.0);
        }
        
        return result;
    }
    
    // Withdraw FlashCopy
    bool withdrawFlashCopy(const std::string& pairId) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = pairs_.find(pairId);
        if (it == pairs_.end()) return false;
        
        const auto& groupName = it->second->getInfo().consistencyGroupName;
        if (!groupName.empty()) {
            auto groupIt = groups_.find(groupName);
            if (groupIt != groups_.end()) {
                groupIt->second->removePair(pairId);
            }
        }
        
        pairs_.erase(it);
        return true;
    }
    
    // Query FlashCopy
    std::optional<CopyPairInfo> getFlashCopyStatus(const std::string& pairId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = pairs_.find(pairId);
        if (it != pairs_.end()) {
            return it->second->getInfo();
        }
        return std::nullopt;
    }
    
    std::vector<CopyPairInfo> getAllPairs() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<CopyPairInfo> result;
        for (const auto& [id, pair] : pairs_) {
            result.push_back(pair->getInfo());
        }
        return result;
    }
    
    std::vector<CopyPairInfo> getActivePairs() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<CopyPairInfo> result;
        for (const auto& [id, pair] : pairs_) {
            if (pair->isActive()) {
                result.push_back(pair->getInfo());
            }
        }
        return result;
    }
    
    // Consistency group management
    bool createConsistencyGroup(const std::string& name,
                               const std::string& description = "",
                               ReplicationMode mode = ReplicationMode::ASYNCHRONOUS) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (groups_.find(name) != groups_.end()) {
            return false;
        }
        
        auto group = std::make_shared<ConsistencyGroup>(name, description);
        group->setReplicationMode(mode);
        groups_[name] = group;
        return true;
    }
    
    bool deleteConsistencyGroup(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = groups_.find(name);
        if (it == groups_.end()) return false;
        
        // Remove all pairs from group
        for (const auto& pairId : it->second->getPairIds()) {
            auto pairIt = pairs_.find(pairId);
            if (pairIt != pairs_.end()) {
                pairIt->second->getInfo().consistencyGroupName.clear();
            }
        }
        
        groups_.erase(it);
        return true;
    }
    
    std::optional<ConsistencyGroupInfo> getConsistencyGroup(
            const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = groups_.find(name);
        if (it != groups_.end()) {
            return it->second->getInfo();
        }
        return std::nullopt;
    }
    
    std::vector<ConsistencyGroupInfo> getAllConsistencyGroups() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<ConsistencyGroupInfo> result;
        for (const auto& [name, group] : groups_) {
            result.push_back(group->getInfo());
        }
        return result;
    }
    
    // Group operations
    bool freezeConsistencyGroup(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = groups_.find(name);
        if (it == groups_.end()) return false;
        
        // Suspend all pairs in group
        for (const auto& pairId : it->second->getPairIds()) {
            auto pairIt = pairs_.find(pairId);
            if (pairIt != pairs_.end()) {
                pairIt->second->suspend();
            }
        }
        
        it->second->suspend();
        return true;
    }
    
    bool unfreezeConsistencyGroup(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = groups_.find(name);
        if (it == groups_.end()) return false;
        
        // Resume all pairs in group
        for (const auto& pairId : it->second->getPairIds()) {
            auto pairIt = pairs_.find(pairId);
            if (pairIt != pairs_.end()) {
                pairIt->second->resume();
            }
        }
        
        it->second->enable();
        return true;
    }
    
    // Statistics
    const ReplicationStatistics& getStatistics() const { return stats_; }
    
    size_t getPairCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pairs_.size();
    }
    
    size_t getGroupCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return groups_.size();
    }

private:
    std::map<std::string, std::shared_ptr<CopyPair>> pairs_;
    std::map<std::string, std::shared_ptr<ConsistencyGroup>> groups_;
    ReplicationStatistics stats_;
    mutable std::mutex mutex_;
};

// ============================================================================
// Replication Manager
// ============================================================================

/**
 * @brief Main replication manager
 * 
 * Coordinates all replication operations including:
 * - FlashCopy
 * - Incremental replication
 * - Cross-site replication
 * - Consistency groups
 */
class ReplicationManager {
public:
    /**
     * @brief Manager configuration
     */
    struct Configuration {
        uint32_t maxConcurrentCopies{10};
        uint32_t defaultPriority{5};
        bool enableCompression{false};
        bool enableEncryption{false};
        std::chrono::seconds defaultTimeout{3600};
        ReplicationMode defaultMode{ReplicationMode::ASYNCHRONOUS};
        RPOLevel defaultRPO{RPOLevel::MINUTES};
    };
    
    ReplicationManager() {
        flashCopyManager_ = std::make_unique<FlashCopyManager>();
    }
    
    explicit ReplicationManager(const Configuration& config)
        : config_(config) {
        flashCopyManager_ = std::make_unique<FlashCopyManager>();
    }
    
    // Configuration
    void setConfiguration(const Configuration& config) {
        config_ = config;
    }
    
    const Configuration& getConfiguration() const { return config_; }
    
    // FlashCopy operations
    ReplicationResult createPointInTimeCopy(const std::string& sourceDsn,
                                            const std::string& targetDsn,
                                            const std::string& consistencyGroup = "") {
        FlashCopyOptions options;
        options.backgroundCopy = true;
        options.consistencyGroup = consistencyGroup;
        options.priority = config_.defaultPriority;
        options.timeout = config_.defaultTimeout;
        
        return flashCopyManager_->establishFlashCopy(sourceDsn, targetDsn, options);
    }
    
    ReplicationResult createSnapshot(const std::string& sourceDsn,
                                     const std::string& targetDsn) {
        FlashCopyOptions options;
        options.backgroundCopy = false;  // Space-efficient snapshot
        options.nocopy = true;
        
        auto pair = std::make_shared<CopyPair>(sourceDsn, targetDsn);
        pair->setCopyType(CopyType::SNAPSHOT);
        
        return flashCopyManager_->establishFlashCopy(sourceDsn, targetDsn, options);
    }
    
    bool deleteReplication(const std::string& pairId) {
        return flashCopyManager_->withdrawFlashCopy(pairId);
    }
    
    // Consistency group operations
    bool createConsistencyGroup(const std::string& name,
                               const std::string& description = "") {
        return flashCopyManager_->createConsistencyGroup(name, description, 
                                                         config_.defaultMode);
    }
    
    bool deleteConsistencyGroup(const std::string& name) {
        return flashCopyManager_->deleteConsistencyGroup(name);
    }
    
    ReplicationResult createGroupSnapshot(const std::string& groupName,
                                          const std::string& targetPrefix) {
        ReplicationResult result;
        result.startTime = std::chrono::system_clock::now();
        result.groupName = groupName;
        
        auto groupInfo = flashCopyManager_->getConsistencyGroup(groupName);
        if (!groupInfo) {
            result.success = false;
            result.errorMessage = "Consistency group not found";
            return result;
        }
        
        // Freeze group for consistent snapshot
        flashCopyManager_->freezeConsistencyGroup(groupName);
        
        // Create snapshots for all pairs
        for (const auto& pairId : groupInfo->pairIds) {
            auto pairInfo = flashCopyManager_->getFlashCopyStatus(pairId);
            if (pairInfo) {
                std::string targetDsn = targetPrefix + "." + pairInfo->sourceDsn;
                auto copyResult = createSnapshot(pairInfo->sourceDsn, targetDsn);
                result.bytesProcessed += copyResult.bytesProcessed;
            }
        }
        
        // Unfreeze group
        flashCopyManager_->unfreezeConsistencyGroup(groupName);
        
        result.success = true;
        result.endTime = std::chrono::system_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.endTime - result.startTime);
        
        return result;
    }
    
    // Incremental replication
    ReplicationResult createIncrementalCopy(const std::string& sourceDsn,
                                            const std::string& targetDsn) {
        ReplicationResult result;
        result.startTime = std::chrono::system_clock::now();
        
        // Check for existing pair
        auto pairs = flashCopyManager_->getAllPairs();
        for (const auto& pair : pairs) {
            if (pair.sourceDsn == sourceDsn && pair.targetDsn == targetDsn &&
                pair.state == CopyState::COPIED) {
                // Update existing pair with incremental changes
                result.success = true;
                result.pairId = pair.pairId;
                result.bytesProcessed = 0;  // Only changed blocks
                break;
            }
        }
        
        if (!result.success) {
            // Create new full copy
            FlashCopyOptions options;
            options.backgroundCopy = true;
            result = flashCopyManager_->establishFlashCopy(sourceDsn, targetDsn, options);
        }
        
        result.endTime = std::chrono::system_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.endTime - result.startTime);
        
        return result;
    }
    
    // Reverse replication
    bool reverseReplication(const std::string& pairId) {
        auto status = flashCopyManager_->getFlashCopyStatus(pairId);
        if (!status) return false;
        
        // Can only reverse completed pairs
        if (status->state != CopyState::COPIED) return false;
        
        // This would update the pair direction
        // Simplified: just return success
        return true;
    }
    
    // Query operations
    std::optional<CopyPairInfo> getReplicationStatus(const std::string& pairId) const {
        return flashCopyManager_->getFlashCopyStatus(pairId);
    }
    
    std::vector<CopyPairInfo> getAllReplications() const {
        return flashCopyManager_->getAllPairs();
    }
    
    std::vector<CopyPairInfo> getActiveReplications() const {
        return flashCopyManager_->getActivePairs();
    }
    
    std::optional<ConsistencyGroupInfo> getConsistencyGroupStatus(
            const std::string& name) const {
        return flashCopyManager_->getConsistencyGroup(name);
    }
    
    std::vector<ConsistencyGroupInfo> getAllConsistencyGroups() const {
        return flashCopyManager_->getAllConsistencyGroups();
    }
    
    // Component access
    FlashCopyManager& getFlashCopyManager() { return *flashCopyManager_; }
    const FlashCopyManager& getFlashCopyManager() const { return *flashCopyManager_; }
    
    // Statistics
    ReplicationStatistics getStatistics() const {
        return flashCopyManager_->getStatistics();
    }
    
    // Summary report
    std::string generateSummaryReport() const {
        std::ostringstream oss;
        oss << "Replication Manager Summary\n"
            << "===========================\n"
            << "Copy Pairs: " << flashCopyManager_->getPairCount() << "\n"
            << "Consistency Groups: " << flashCopyManager_->getGroupCount() << "\n"
            << "Active Replications: " << getActiveReplications().size() << "\n\n"
            << flashCopyManager_->getStatistics().toString();
        return oss.str();
    }

private:
    Configuration config_;
    std::unique_ptr<FlashCopyManager> flashCopyManager_;
};

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a FlashCopy pair
 */
inline CopyPair createFlashCopyPair(
        const std::string& sourceDsn,
        const std::string& targetDsn,
        const std::string& consistencyGroup = "") {
    
    CopyPair pair(sourceDsn, targetDsn);
    pair.setCopyType(CopyType::FLASHCOPY);
    if (!consistencyGroup.empty()) {
        pair.setConsistencyGroup(consistencyGroup);
    }
    return pair;
}

/**
 * @brief Create standard FlashCopy options
 */
inline FlashCopyOptions createStandardFlashCopyOptions() {
    FlashCopyOptions options;
    options.backgroundCopy = true;
    options.persistent = true;
    options.verify = true;
    options.priority = 5;
    return options;
}

/**
 * @brief Create FlashCopy options for immediate split
 */
inline FlashCopyOptions createSplitFlashCopyOptions() {
    FlashCopyOptions options;
    options.backgroundCopy = true;
    options.persistent = false;
    options.split = true;
    options.priority = 7;  // Higher priority for split
    return options;
}

/**
 * @brief Create a synchronous consistency group
 */
inline std::shared_ptr<ConsistencyGroup> createSyncConsistencyGroup(
        const std::string& name,
        const std::string& description = "") {
    
    auto group = std::make_shared<ConsistencyGroup>(name, description);
    group->setReplicationMode(ReplicationMode::SYNCHRONOUS);
    group->setRPOLevel(RPOLevel::ZERO);
    group->setSyncInterval(0);  // Immediate sync
    return group;
}

/**
 * @brief Create an asynchronous consistency group
 */
inline std::shared_ptr<ConsistencyGroup> createAsyncConsistencyGroup(
        const std::string& name,
        uint32_t syncIntervalSeconds = 60,
        const std::string& description = "") {
    
    auto group = std::make_shared<ConsistencyGroup>(name, description);
    group->setReplicationMode(ReplicationMode::ASYNCHRONOUS);
    group->setRPOLevel(RPOLevel::MINUTES);
    group->setSyncInterval(syncIntervalSeconds);
    return group;
}

} // namespace replication
} // namespace dfsmshsm

#endif // HSM_FAST_REPLICATION_HPP
