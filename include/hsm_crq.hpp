/*
 * DFSMShsm Emulator - Common Recall Queue (CRQ)
 * @version 4.2.0
 * 
 * Provides multi-host recall coordination including:
 * - Shared recall queue across systems
 * - Priority-based recall scheduling
 * - Workload balancing
 * - Recall request deduplication
 * - Cross-system status tracking
 * - Recall performance optimization
 * 
 * Author: DFSMShsm Emulator Team
 * License: MIT
 */

#ifndef HSM_CRQ_HPP
#define HSM_CRQ_HPP

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
#include <condition_variable>
#include <optional>
#include <queue>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <ctime>

namespace dfsmshsm {
namespace crq {

// ============================================================================
// Forward Declarations
// ============================================================================

class RecallRequest;
class RecallQueue;
class HostManager;
class WorkloadBalancer;
class CRQManager;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Recall request priority
 */
enum class RecallPriority {
    CRITICAL = 0,     // Immediate recall needed
    HIGH = 1,         // High priority user request
    NORMAL = 2,       // Standard recall
    LOW = 3,          // Background recall
    BATCH = 4         // Batch/scheduled recall
};

/**
 * @brief Recall request status
 */
enum class RecallStatus {
    QUEUED,           // Waiting in queue
    ASSIGNED,         // Assigned to a host
    IN_PROGRESS,      // Recall actively running
    COMPLETED,        // Successfully completed
    FAILED,           // Recall failed
    CANCELLED,        // User cancelled
    TIMEOUT,          // Request timed out
    DUPLICATE         // Duplicate request merged
};

/**
 * @brief Host status
 */
enum class HostStatus {
    ACTIVE,           // Processing recalls
    IDLE,             // Available but no work
    BUSY,             // At capacity
    OFFLINE,          // Not available
    DRAINING,         // Completing current work, no new assignments
    ERROR             // Error state
};

/**
 * @brief Migration level source
 */
enum class MigrationLevel {
    ML1,              // Migration Level 1 (DASD)
    ML2,              // Migration Level 2 (Tape)
    CLOUD             // Cloud tier
};

/**
 * @brief Recall type
 */
enum class RecallType {
    FULL,             // Full dataset recall
    PARTIAL,          // Partial/range recall
    PREFETCH,         // Prefetch for anticipated access
    CONNECT           // Connect without recall (VSAM)
};

// ============================================================================
// Utility Structures
// ============================================================================

/**
 * @brief Recall request information
 */
struct RecallRequestInfo {
    std::string requestId;
    std::string datasetName;
    std::string requestingUser;
    std::string requestingJob;
    std::string requestingSystem;
    RecallPriority priority{RecallPriority::NORMAL};
    RecallStatus status{RecallStatus::QUEUED};
    RecallType type{RecallType::FULL};
    MigrationLevel sourceLevel{MigrationLevel::ML1};
    
    std::chrono::system_clock::time_point requestTime{std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point assignedTime;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    
    std::string assignedHost;
    std::string targetVolume;
    uint64_t datasetSize{0};
    uint64_t bytesRecalled{0};
    
    uint32_t retryCount{0};
    uint32_t maxRetries{3};
    std::string errorMessage;
    std::string errorCode;
    
    // For partial recalls
    uint64_t startOffset{0};
    uint64_t length{0};
    
    // Timeout settings
    std::chrono::seconds queueTimeout{3600};      // 1 hour default
    std::chrono::seconds recallTimeout{7200};     // 2 hours default
    
    bool isExpired() const {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - requestTime);
        return elapsed > queueTimeout;
    }
    
    std::chrono::seconds getWaitTime() const {
        auto now = std::chrono::system_clock::now();
        if (status == RecallStatus::COMPLETED || status == RecallStatus::FAILED) {
            return std::chrono::duration_cast<std::chrono::seconds>(startTime - requestTime);
        }
        return std::chrono::duration_cast<std::chrono::seconds>(now - requestTime);
    }
    
    std::chrono::seconds getRecallDuration() const {
        if (status != RecallStatus::COMPLETED && status != RecallStatus::FAILED) {
            return std::chrono::seconds(0);
        }
        return std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
    }
    
    std::string getStatusString() const {
        switch (status) {
            case RecallStatus::QUEUED: return "QUEUED";
            case RecallStatus::ASSIGNED: return "ASSIGNED";
            case RecallStatus::IN_PROGRESS: return "IN_PROGRESS";
            case RecallStatus::COMPLETED: return "COMPLETED";
            case RecallStatus::FAILED: return "FAILED";
            case RecallStatus::CANCELLED: return "CANCELLED";
            case RecallStatus::TIMEOUT: return "TIMEOUT";
            case RecallStatus::DUPLICATE: return "DUPLICATE";
            default: return "UNKNOWN";
        }
    }
    
    std::string getPriorityString() const {
        switch (priority) {
            case RecallPriority::CRITICAL: return "CRITICAL";
            case RecallPriority::HIGH: return "HIGH";
            case RecallPriority::NORMAL: return "NORMAL";
            case RecallPriority::LOW: return "LOW";
            case RecallPriority::BATCH: return "BATCH";
            default: return "UNKNOWN";
        }
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Request[" << requestId << "] " << datasetName
            << " Priority=" << getPriorityString()
            << " Status=" << getStatusString();
        if (!assignedHost.empty()) {
            oss << " Host=" << assignedHost;
        }
        return oss.str();
    }
};

/**
 * @brief Host information
 */
struct HostInfo {
    std::string hostName;
    std::string systemId;
    HostStatus status{HostStatus::IDLE};
    
    uint32_t maxConcurrentRecalls{10};
    uint32_t activeRecalls{0};
    uint32_t completedRecalls{0};
    uint32_t failedRecalls{0};
    
    uint64_t bytesRecalled{0};
    std::chrono::system_clock::time_point lastHeartbeat{std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point joinTime{std::chrono::system_clock::now()};
    
    // Performance metrics
    double avgRecallTimeSeconds{0.0};
    double avgThroughputMBps{0.0};
    uint32_t recallsInLastHour{0};
    
    // Capabilities
    bool canRecallML1{true};
    bool canRecallML2{true};
    bool canRecallCloud{false};
    std::vector<std::string> preferredVolumes;
    
    bool isAvailable() const {
        return (status == HostStatus::ACTIVE || status == HostStatus::IDLE) &&
               activeRecalls < maxConcurrentRecalls;
    }
    
    uint32_t getAvailableSlots() const {
        if (!isAvailable()) return 0;
        return maxConcurrentRecalls - activeRecalls;
    }
    
    double getUtilization() const {
        if (maxConcurrentRecalls == 0) return 0.0;
        return (static_cast<double>(activeRecalls) / maxConcurrentRecalls) * 100.0;
    }
    
    bool isHealthy() const {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat);
        return elapsed.count() < 60;  // Healthy if heartbeat within 60 seconds
    }
    
    std::string getStatusString() const {
        switch (status) {
            case HostStatus::ACTIVE: return "ACTIVE";
            case HostStatus::IDLE: return "IDLE";
            case HostStatus::BUSY: return "BUSY";
            case HostStatus::OFFLINE: return "OFFLINE";
            case HostStatus::DRAINING: return "DRAINING";
            case HostStatus::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << hostName << " [" << getStatusString() << "] "
            << "Active=" << activeRecalls << "/" << maxConcurrentRecalls
            << " Completed=" << completedRecalls;
        return oss.str();
    }
};

/**
 * @brief Queue statistics
 */
struct QueueStatistics {
    uint64_t totalRequests{0};
    uint64_t queuedRequests{0};
    uint64_t inProgressRequests{0};
    uint64_t completedRequests{0};
    uint64_t failedRequests{0};
    uint64_t cancelledRequests{0};
    uint64_t timedOutRequests{0};
    uint64_t duplicateRequests{0};
    
    uint64_t bytesRecalled{0};
    double avgWaitTimeSeconds{0.0};
    double avgRecallTimeSeconds{0.0};
    double avgThroughputMBps{0.0};
    
    uint32_t criticalPending{0};
    uint32_t highPending{0};
    uint32_t normalPending{0};
    uint32_t lowPending{0};
    uint32_t batchPending{0};
    
    std::chrono::system_clock::time_point startTime{std::chrono::system_clock::now()};
    
    double getSuccessRate() const {
        uint64_t total = completedRequests + failedRequests;
        if (total == 0) return 100.0;
        return (static_cast<double>(completedRequests) / total) * 100.0;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Queue Statistics:\n"
            << "  Total Requests: " << totalRequests << "\n"
            << "  Queued: " << queuedRequests << "\n"
            << "  In Progress: " << inProgressRequests << "\n"
            << "  Completed: " << completedRequests << "\n"
            << "  Failed: " << failedRequests << "\n"
            << "  Success Rate: " << std::fixed << std::setprecision(1) 
            << getSuccessRate() << "%\n"
            << "  Avg Wait: " << avgWaitTimeSeconds << "s\n"
            << "  Avg Recall: " << avgRecallTimeSeconds << "s";
        return oss.str();
    }
};

/**
 * @brief Workload distribution info
 */
struct WorkloadDistribution {
    std::map<std::string, uint32_t> requestsByHost;
    std::map<std::string, uint64_t> bytesByHost;
    std::map<RecallPriority, uint32_t> requestsByPriority;
    std::map<MigrationLevel, uint32_t> requestsByLevel;
    
    std::string getMostLoadedHost() const {
        std::string maxHost;
        uint32_t maxCount = 0;
        for (const auto& [host, count] : requestsByHost) {
            if (count > maxCount) {
                maxCount = count;
                maxHost = host;
            }
        }
        return maxHost;
    }
    
    std::string getLeastLoadedHost() const {
        std::string minHost;
        uint32_t minCount = UINT32_MAX;
        for (const auto& [host, count] : requestsByHost) {
            if (count < minCount) {
                minCount = count;
                minHost = host;
            }
        }
        return minHost;
    }
};

// ============================================================================
// Recall Request
// ============================================================================

/**
 * @brief Recall Request with full lifecycle management
 */
class RecallRequest {
public:
    RecallRequest() {
        generateRequestId();
    }
    
    explicit RecallRequest(const std::string& datasetName)
        : datasetName_(datasetName) {
        generateRequestId();
        info_.datasetName = datasetName;
        info_.requestId = requestId_;
    }
    
    // Request configuration
    RecallRequest& setDatasetName(const std::string& dsn) {
        datasetName_ = dsn;
        info_.datasetName = dsn;
        return *this;
    }
    
    RecallRequest& setPriority(RecallPriority priority) {
        info_.priority = priority;
        return *this;
    }
    
    RecallRequest& setRequestingUser(const std::string& user) {
        info_.requestingUser = user;
        return *this;
    }
    
    RecallRequest& setRequestingJob(const std::string& job) {
        info_.requestingJob = job;
        return *this;
    }
    
    RecallRequest& setRequestingSystem(const std::string& system) {
        info_.requestingSystem = system;
        return *this;
    }
    
    RecallRequest& setSourceLevel(MigrationLevel level) {
        info_.sourceLevel = level;
        return *this;
    }
    
    RecallRequest& setType(RecallType type) {
        info_.type = type;
        return *this;
    }
    
    RecallRequest& setTargetVolume(const std::string& volume) {
        info_.targetVolume = volume;
        return *this;
    }
    
    RecallRequest& setDatasetSize(uint64_t size) {
        info_.datasetSize = size;
        return *this;
    }
    
    RecallRequest& setPartialRange(uint64_t offset, uint64_t length) {
        info_.type = RecallType::PARTIAL;
        info_.startOffset = offset;
        info_.length = length;
        return *this;
    }
    
    RecallRequest& setTimeouts(std::chrono::seconds queueTimeout,
                               std::chrono::seconds recallTimeout) {
        info_.queueTimeout = queueTimeout;
        info_.recallTimeout = recallTimeout;
        return *this;
    }
    
    // Status management
    void assign(const std::string& hostName) {
        info_.status = RecallStatus::ASSIGNED;
        info_.assignedHost = hostName;
        info_.assignedTime = std::chrono::system_clock::now();
    }
    
    void start() {
        info_.status = RecallStatus::IN_PROGRESS;
        info_.startTime = std::chrono::system_clock::now();
    }
    
    void complete(uint64_t bytesRecalled = 0) {
        info_.status = RecallStatus::COMPLETED;
        info_.endTime = std::chrono::system_clock::now();
        info_.bytesRecalled = bytesRecalled > 0 ? bytesRecalled : info_.datasetSize;
    }
    
    void fail(const std::string& errorMessage, const std::string& errorCode = "") {
        info_.status = RecallStatus::FAILED;
        info_.endTime = std::chrono::system_clock::now();
        info_.errorMessage = errorMessage;
        info_.errorCode = errorCode;
        info_.retryCount++;
    }
    
    void cancel() {
        info_.status = RecallStatus::CANCELLED;
        info_.endTime = std::chrono::system_clock::now();
    }
    
    void timeout() {
        info_.status = RecallStatus::TIMEOUT;
        info_.endTime = std::chrono::system_clock::now();
        info_.errorMessage = "Request timed out";
    }
    
    void markDuplicate(const std::string& originalRequestId) {
        info_.status = RecallStatus::DUPLICATE;
        info_.errorMessage = "Duplicate of " + originalRequestId;
    }
    
    bool canRetry() const {
        return info_.retryCount < info_.maxRetries;
    }
    
    void retry() {
        info_.status = RecallStatus::QUEUED;
        info_.assignedHost.clear();
        info_.errorMessage.clear();
    }
    
    // Getters
    const std::string& getRequestId() const { return requestId_; }
    const std::string& getDatasetName() const { return datasetName_; }
    const RecallRequestInfo& getInfo() const { return info_; }
    RecallRequestInfo& getInfo() { return info_; }
    
    // Comparison for priority queue
    bool operator<(const RecallRequest& other) const {
        // Lower priority value = higher priority
        if (info_.priority != other.info_.priority) {
            return info_.priority > other.info_.priority;
        }
        // Earlier request time = higher priority
        return info_.requestTime > other.info_.requestTime;
    }

private:
    std::string requestId_;
    std::string datasetName_;
    RecallRequestInfo info_;
    
    void generateRequestId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "RCL" << std::put_time(std::localtime(&time), "%Y%m%d%H%M%S")
            << std::setfill('0') << std::setw(6) << (++counter);
        requestId_ = oss.str();
        info_.requestId = requestId_;
    }
};

// ============================================================================
// Recall Queue
// ============================================================================

/**
 * @brief Priority-based recall queue
 */
class RecallQueue {
public:
    RecallQueue() = default;
    
    // Queue operations
    std::string enqueue(RecallRequest request) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const auto& dsn = request.getDatasetName();
        
        // Check for duplicate
        auto it = activeRequests_.find(dsn);
        if (it != activeRequests_.end()) {
            const auto& existing = it->second;
            if (existing.getInfo().status == RecallStatus::QUEUED ||
                existing.getInfo().status == RecallStatus::ASSIGNED ||
                existing.getInfo().status == RecallStatus::IN_PROGRESS) {
                // Mark as duplicate
                stats_.duplicateRequests++;
                return existing.getRequestId();  // Return original request ID
            }
        }
        
        std::string requestId = request.getRequestId();
        activeRequests_[dsn] = request;
        priorityQueue_.push(request);
        
        stats_.totalRequests++;
        stats_.queuedRequests++;
        updatePriorityStats(request.getInfo().priority, 1);
        
        condition_.notify_one();
        
        return requestId;
    }
    
    std::optional<RecallRequest> dequeue() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        while (!priorityQueue_.empty()) {
            RecallRequest request = priorityQueue_.top();
            priorityQueue_.pop();
            
            // Verify request is still valid
            auto it = activeRequests_.find(request.getDatasetName());
            if (it != activeRequests_.end() && 
                it->second.getInfo().status == RecallStatus::QUEUED) {
                
                stats_.queuedRequests--;
                updatePriorityStats(request.getInfo().priority, -1);
                
                return request;
            }
        }
        
        return std::nullopt;
    }
    
    std::optional<RecallRequest> dequeueForHost(const std::string& hostName,
                                                 const HostInfo& hostInfo) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Find best matching request for this host
        std::vector<RecallRequest> skipped;
        std::optional<RecallRequest> result;
        
        while (!priorityQueue_.empty()) {
            RecallRequest request = priorityQueue_.top();
            priorityQueue_.pop();
            
            auto it = activeRequests_.find(request.getDatasetName());
            if (it == activeRequests_.end() || 
                it->second.getInfo().status != RecallStatus::QUEUED) {
                continue;
            }
            
            // Check if host can handle this request
            bool canHandle = true;
            switch (request.getInfo().sourceLevel) {
                case MigrationLevel::ML1:
                    canHandle = hostInfo.canRecallML1;
                    break;
                case MigrationLevel::ML2:
                    canHandle = hostInfo.canRecallML2;
                    break;
                case MigrationLevel::CLOUD:
                    canHandle = hostInfo.canRecallCloud;
                    break;
            }
            
            if (canHandle) {
                result = request;
                stats_.queuedRequests--;
                updatePriorityStats(request.getInfo().priority, -1);
                break;
            } else {
                skipped.push_back(request);
            }
        }
        
        // Put skipped requests back
        for (auto& req : skipped) {
            priorityQueue_.push(req);
        }
        
        return result;
    }
    
    bool cancel(const std::string& requestId) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [dsn, request] : activeRequests_) {
            if (request.getRequestId() == requestId) {
                if (request.getInfo().status == RecallStatus::QUEUED) {
                    request.cancel();
                    stats_.queuedRequests--;
                    stats_.cancelledRequests++;
                    updatePriorityStats(request.getInfo().priority, -1);
                    return true;
                }
            }
        }
        return false;
    }
    
    bool updateStatus(const std::string& datasetName, RecallStatus status) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = activeRequests_.find(datasetName);
        if (it == activeRequests_.end()) return false;
        
        RecallStatus oldStatus = it->second.getInfo().status;
        it->second.getInfo().status = status;
        
        // Update statistics
        updateStatusStats(oldStatus, status);
        
        return true;
    }
    
    // Query operations
    std::optional<RecallRequestInfo> getRequest(const std::string& requestId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [dsn, request] : activeRequests_) {
            if (request.getRequestId() == requestId) {
                return request.getInfo();
            }
        }
        return std::nullopt;
    }
    
    std::optional<RecallRequestInfo> getRequestByDataset(const std::string& dsn) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = activeRequests_.find(dsn);
        if (it != activeRequests_.end()) {
            return it->second.getInfo();
        }
        return std::nullopt;
    }
    
    std::vector<RecallRequestInfo> getQueuedRequests() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<RecallRequestInfo> result;
        for (const auto& [dsn, request] : activeRequests_) {
            if (request.getInfo().status == RecallStatus::QUEUED) {
                result.push_back(request.getInfo());
            }
        }
        
        // Sort by priority and time
        std::sort(result.begin(), result.end(),
            [](const RecallRequestInfo& a, const RecallRequestInfo& b) {
                if (a.priority != b.priority) {
                    return a.priority < b.priority;
                }
                return a.requestTime < b.requestTime;
            });
        
        return result;
    }
    
    std::vector<RecallRequestInfo> getRequestsByHost(const std::string& hostName) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<RecallRequestInfo> result;
        for (const auto& [dsn, request] : activeRequests_) {
            if (request.getInfo().assignedHost == hostName) {
                result.push_back(request.getInfo());
            }
        }
        return result;
    }
    
    // Statistics
    const QueueStatistics& getStatistics() const { return stats_; }
    
    size_t getQueueDepth() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_.queuedRequests;
    }
    
    size_t getActiveCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_.inProgressRequests;
    }
    
    // Cleanup expired requests
    size_t cleanupExpired() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t cleaned = 0;
        
        for (auto it = activeRequests_.begin(); it != activeRequests_.end(); ) {
            auto& request = it->second;
            if (request.getInfo().status == RecallStatus::QUEUED &&
                request.getInfo().isExpired()) {
                
                request.timeout();
                stats_.queuedRequests--;
                stats_.timedOutRequests++;
                updatePriorityStats(request.getInfo().priority, -1);
                
                // Move to history
                completedRequests_[request.getRequestId()] = request.getInfo();
                it = activeRequests_.erase(it);
                cleaned++;
            } else {
                ++it;
            }
        }
        
        return cleaned;
    }
    
    // Wait for request
    bool waitForCompletion(const std::string& requestId, 
                          std::chrono::seconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        return condition_.wait_for(lock, timeout, [this, &requestId]() {
            for (const auto& [dsn, request] : activeRequests_) {
                if (request.getRequestId() == requestId) {
                    return request.getInfo().status == RecallStatus::COMPLETED ||
                           request.getInfo().status == RecallStatus::FAILED ||
                           request.getInfo().status == RecallStatus::CANCELLED;
                }
            }
            return true;  // Request not found, don't wait
        });
    }

private:
    std::priority_queue<RecallRequest> priorityQueue_;
    std::map<std::string, RecallRequest> activeRequests_;
    std::map<std::string, RecallRequestInfo> completedRequests_;
    QueueStatistics stats_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    
    void updatePriorityStats(RecallPriority priority, int delta) {
        switch (priority) {
            case RecallPriority::CRITICAL:
                stats_.criticalPending += delta;
                break;
            case RecallPriority::HIGH:
                stats_.highPending += delta;
                break;
            case RecallPriority::NORMAL:
                stats_.normalPending += delta;
                break;
            case RecallPriority::LOW:
                stats_.lowPending += delta;
                break;
            case RecallPriority::BATCH:
                stats_.batchPending += delta;
                break;
        }
    }
    
    void updateStatusStats(RecallStatus oldStatus, RecallStatus newStatus) {
        if (oldStatus == newStatus) return;
        
        switch (oldStatus) {
            case RecallStatus::QUEUED:
                if (stats_.queuedRequests > 0) stats_.queuedRequests--;
                break;
            case RecallStatus::IN_PROGRESS:
                if (stats_.inProgressRequests > 0) stats_.inProgressRequests--;
                break;
            default:
                break;
        }
        
        switch (newStatus) {
            case RecallStatus::QUEUED:
                stats_.queuedRequests++;
                break;
            case RecallStatus::IN_PROGRESS:
                stats_.inProgressRequests++;
                break;
            case RecallStatus::COMPLETED:
                stats_.completedRequests++;
                break;
            case RecallStatus::FAILED:
                stats_.failedRequests++;
                break;
            case RecallStatus::CANCELLED:
                stats_.cancelledRequests++;
                break;
            case RecallStatus::TIMEOUT:
                stats_.timedOutRequests++;
                break;
            default:
                break;
        }
    }
};

// ============================================================================
// Host Manager
// ============================================================================

/**
 * @brief Manages participating hosts in the CRQ
 */
class HostManager {
public:
    HostManager() = default;
    
    // Host registration
    bool registerHost(const HostInfo& host) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (hosts_.find(host.hostName) != hosts_.end()) {
            return false;  // Already registered
        }
        
        hosts_[host.hostName] = host;
        return true;
    }
    
    bool unregisterHost(const std::string& hostName) {
        std::lock_guard<std::mutex> lock(mutex_);
        return hosts_.erase(hostName) > 0;
    }
    
    // Host status updates
    bool updateHeartbeat(const std::string& hostName) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = hosts_.find(hostName);
        if (it == hosts_.end()) return false;
        
        it->second.lastHeartbeat = std::chrono::system_clock::now();
        return true;
    }
    
    bool updateStatus(const std::string& hostName, HostStatus status) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = hosts_.find(hostName);
        if (it == hosts_.end()) return false;
        
        it->second.status = status;
        return true;
    }
    
    bool updateActiveRecalls(const std::string& hostName, uint32_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = hosts_.find(hostName);
        if (it == hosts_.end()) return false;
        
        it->second.activeRecalls = count;
        
        // Update status based on load
        if (count >= it->second.maxConcurrentRecalls) {
            it->second.status = HostStatus::BUSY;
        } else if (count > 0) {
            it->second.status = HostStatus::ACTIVE;
        } else {
            it->second.status = HostStatus::IDLE;
        }
        
        return true;
    }
    
    bool incrementRecallCount(const std::string& hostName, bool success) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = hosts_.find(hostName);
        if (it == hosts_.end()) return false;
        
        if (success) {
            it->second.completedRecalls++;
        } else {
            it->second.failedRecalls++;
        }
        
        return true;
    }
    
    // Host queries
    std::optional<HostInfo> getHost(const std::string& hostName) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = hosts_.find(hostName);
        if (it != hosts_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<HostInfo> getActiveHosts() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<HostInfo> result;
        for (const auto& [name, host] : hosts_) {
            if (host.isAvailable() && host.isHealthy()) {
                result.push_back(host);
            }
        }
        return result;
    }
    
    std::vector<HostInfo> getAllHosts() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<HostInfo> result;
        for (const auto& [name, host] : hosts_) {
            result.push_back(host);
        }
        return result;
    }
    
    // Find best host for a recall
    std::optional<std::string> selectHostForRecall(const RecallRequestInfo& request) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const HostInfo* bestHost = nullptr;
        double bestScore = -1.0;
        
        for (const auto& [name, host] : hosts_) {
            if (!host.isAvailable() || !host.isHealthy()) continue;
            
            // Check capability
            bool capable = false;
            switch (request.sourceLevel) {
                case MigrationLevel::ML1:
                    capable = host.canRecallML1;
                    break;
                case MigrationLevel::ML2:
                    capable = host.canRecallML2;
                    break;
                case MigrationLevel::CLOUD:
                    capable = host.canRecallCloud;
                    break;
            }
            if (!capable) continue;
            
            // Calculate score (lower is better)
            double score = calculateHostScore(host, request);
            
            if (bestScore < 0 || score < bestScore) {
                bestScore = score;
                bestHost = &host;
            }
        }
        
        if (bestHost) {
            return bestHost->hostName;
        }
        return std::nullopt;
    }
    
    // Health check
    size_t markUnhealthyHosts() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t marked = 0;
        for (auto& [name, host] : hosts_) {
            if (!host.isHealthy() && host.status != HostStatus::OFFLINE) {
                host.status = HostStatus::ERROR;
                marked++;
            }
        }
        return marked;
    }
    
    // Statistics
    size_t getHostCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return hosts_.size();
    }
    
    size_t getActiveHostCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t count = 0;
        for (const auto& [name, host] : hosts_) {
            if (host.isAvailable() && host.isHealthy()) {
                count++;
            }
        }
        return count;
    }
    
    uint32_t getTotalCapacity() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        uint32_t total = 0;
        for (const auto& [name, host] : hosts_) {
            if (host.isAvailable()) {
                total += host.getAvailableSlots();
            }
        }
        return total;
    }

private:
    std::map<std::string, HostInfo> hosts_;
    mutable std::mutex mutex_;
    
    double calculateHostScore(const HostInfo& host, 
                             const RecallRequestInfo& request) const {
        double score = 0.0;
        
        // Factor 1: Current utilization (prefer less loaded hosts)
        score += host.getUtilization();
        
        // Factor 2: Average recall time (prefer faster hosts)
        score += host.avgRecallTimeSeconds / 10.0;
        
        // Factor 3: Failure rate (prefer more reliable hosts)
        uint32_t total = host.completedRecalls + host.failedRecalls;
        if (total > 0) {
            double failureRate = static_cast<double>(host.failedRecalls) / total;
            score += failureRate * 50.0;
        }
        
        // Factor 4: Preferred volume match (bonus for matching)
        if (!request.targetVolume.empty() && !host.preferredVolumes.empty()) {
            for (const auto& vol : host.preferredVolumes) {
                if (vol == request.targetVolume) {
                    score -= 20.0;  // Bonus for preferred volume
                    break;
                }
            }
        }
        
        return score;
    }
};

// ============================================================================
// Workload Balancer
// ============================================================================

/**
 * @brief Balances recall workload across hosts
 */
class WorkloadBalancer {
public:
    /**
     * @brief Balancing strategy
     */
    enum class Strategy {
        ROUND_ROBIN,      // Simple rotation
        LEAST_LOADED,     // Assign to least loaded host
        FASTEST,          // Assign to historically fastest host
        AFFINITY,         // Consider data affinity
        ADAPTIVE          // Combine multiple factors
    };
    
    WorkloadBalancer() = default;
    
    void setStrategy(Strategy strategy) { strategy_ = strategy; }
    Strategy getStrategy() const { return strategy_; }
    
    // Select host using configured strategy
    std::optional<std::string> selectHost(const std::vector<HostInfo>& hosts,
                                          const RecallRequestInfo& request) {
        if (hosts.empty()) return std::nullopt;
        
        switch (strategy_) {
            case Strategy::ROUND_ROBIN:
                return selectRoundRobin(hosts);
            case Strategy::LEAST_LOADED:
                return selectLeastLoaded(hosts);
            case Strategy::FASTEST:
                return selectFastest(hosts);
            case Strategy::AFFINITY:
                return selectWithAffinity(hosts, request);
            case Strategy::ADAPTIVE:
            default:
                return selectAdaptive(hosts, request);
        }
    }
    
    // Get workload distribution
    WorkloadDistribution getDistribution(const std::vector<HostInfo>& hosts) const {
        WorkloadDistribution dist;
        
        for (const auto& host : hosts) {
            dist.requestsByHost[host.hostName] = host.activeRecalls;
            dist.bytesByHost[host.hostName] = host.bytesRecalled;
        }
        
        return dist;
    }
    
    // Check if rebalancing is needed
    bool needsRebalancing(const std::vector<HostInfo>& hosts) const {
        if (hosts.size() < 2) return false;
        
        uint32_t minLoad = UINT32_MAX;
        uint32_t maxLoad = 0;
        
        for (const auto& host : hosts) {
            if (host.isAvailable()) {
                minLoad = std::min(minLoad, host.activeRecalls);
                maxLoad = std::max(maxLoad, host.activeRecalls);
            }
        }
        
        // Rebalance if difference is more than 3 or 50%
        return (maxLoad - minLoad > 3) || 
               (maxLoad > 0 && (maxLoad - minLoad) > maxLoad / 2);
    }

private:
    Strategy strategy_{Strategy::ADAPTIVE};
    std::atomic<size_t> roundRobinIndex_{0};
    
    std::optional<std::string> selectRoundRobin(const std::vector<HostInfo>& hosts) {
        if (hosts.empty()) return std::nullopt;
        
        size_t startIndex = roundRobinIndex_++;
        for (size_t i = 0; i < hosts.size(); ++i) {
            const auto& host = hosts[(startIndex + i) % hosts.size()];
            if (host.isAvailable()) {
                return host.hostName;
            }
        }
        return std::nullopt;
    }
    
    std::optional<std::string> selectLeastLoaded(const std::vector<HostInfo>& hosts) {
        const HostInfo* best = nullptr;
        uint32_t minLoad = UINT32_MAX;
        
        for (const auto& host : hosts) {
            if (host.isAvailable() && host.activeRecalls < minLoad) {
                minLoad = host.activeRecalls;
                best = &host;
            }
        }
        
        if (best) return best->hostName;
        return std::nullopt;
    }
    
    std::optional<std::string> selectFastest(const std::vector<HostInfo>& hosts) {
        const HostInfo* best = nullptr;
        double bestTime = std::numeric_limits<double>::max();
        
        for (const auto& host : hosts) {
            if (host.isAvailable() && host.avgRecallTimeSeconds < bestTime) {
                bestTime = host.avgRecallTimeSeconds;
                best = &host;
            }
        }
        
        if (best) return best->hostName;
        return std::nullopt;
    }
    
    std::optional<std::string> selectWithAffinity(const std::vector<HostInfo>& hosts,
                                                   const RecallRequestInfo& request) {
        // First try to find a host with affinity to target volume
        if (!request.targetVolume.empty()) {
            for (const auto& host : hosts) {
                if (!host.isAvailable()) continue;
                
                for (const auto& vol : host.preferredVolumes) {
                    if (vol == request.targetVolume) {
                        return host.hostName;
                    }
                }
            }
        }
        
        // Fall back to least loaded
        return selectLeastLoaded(hosts);
    }
    
    std::optional<std::string> selectAdaptive(const std::vector<HostInfo>& hosts,
                                               const RecallRequestInfo& request) {
        const HostInfo* best = nullptr;
        double bestScore = std::numeric_limits<double>::max();
        
        for (const auto& host : hosts) {
            if (!host.isAvailable()) continue;
            
            double score = 0.0;
            
            // Utilization (40% weight)
            score += host.getUtilization() * 0.4;
            
            // Performance (30% weight)
            if (host.avgRecallTimeSeconds > 0) {
                score += (host.avgRecallTimeSeconds / 60.0) * 30.0;  // Normalize to minutes
            }
            
            // Reliability (20% weight)
            uint32_t total = host.completedRecalls + host.failedRecalls;
            if (total > 0) {
                double failRate = static_cast<double>(host.failedRecalls) / total;
                score += failRate * 20.0;
            }
            
            // Affinity (10% weight)
            if (!request.targetVolume.empty()) {
                bool hasAffinity = false;
                for (const auto& vol : host.preferredVolumes) {
                    if (vol == request.targetVolume) {
                        hasAffinity = true;
                        break;
                    }
                }
                if (!hasAffinity) {
                    score += 10.0;
                }
            }
            
            if (score < bestScore) {
                bestScore = score;
                best = &host;
            }
        }
        
        if (best) return best->hostName;
        return std::nullopt;
    }
};

// ============================================================================
// CRQ Manager
// ============================================================================

/**
 * @brief Common Recall Queue Manager - Main entry point
 * 
 * Coordinates recall operations across multiple hosts including:
 * - Queue management
 * - Host coordination
 * - Workload balancing
 * - Statistics and monitoring
 */
class CRQManager {
public:
    /**
     * @brief Manager configuration
     */
    struct Configuration {
        uint32_t maxQueueDepth{10000};
        std::chrono::seconds defaultQueueTimeout{3600};
        std::chrono::seconds defaultRecallTimeout{7200};
        std::chrono::seconds healthCheckInterval{30};
        std::chrono::seconds cleanupInterval{300};
        bool enableDeduplication{true};
        bool enableRetry{true};
        uint32_t maxRetries{3};
        WorkloadBalancer::Strategy balancingStrategy{WorkloadBalancer::Strategy::ADAPTIVE};
    };
    
    CRQManager() {
        queue_ = std::make_unique<RecallQueue>();
        hostManager_ = std::make_unique<HostManager>();
        balancer_ = std::make_unique<WorkloadBalancer>();
    }
    
    explicit CRQManager(const Configuration& config)
        : config_(config) {
        queue_ = std::make_unique<RecallQueue>();
        hostManager_ = std::make_unique<HostManager>();
        balancer_ = std::make_unique<WorkloadBalancer>();
        balancer_->setStrategy(config.balancingStrategy);
    }
    
    // Configuration
    void setConfiguration(const Configuration& config) {
        config_ = config;
        balancer_->setStrategy(config.balancingStrategy);
    }
    
    const Configuration& getConfiguration() const { return config_; }
    
    // Host management
    bool registerHost(const HostInfo& host) {
        return hostManager_->registerHost(host);
    }
    
    bool unregisterHost(const std::string& hostName) {
        return hostManager_->unregisterHost(hostName);
    }
    
    bool updateHostHeartbeat(const std::string& hostName) {
        return hostManager_->updateHeartbeat(hostName);
    }
    
    // Request submission
    std::string submitRecall(RecallRequest request) {
        // Apply default timeouts
        if (request.getInfo().queueTimeout == std::chrono::seconds(3600)) {
            request.getInfo().queueTimeout = config_.defaultQueueTimeout;
        }
        if (request.getInfo().recallTimeout == std::chrono::seconds(7200)) {
            request.getInfo().recallTimeout = config_.defaultRecallTimeout;
        }
        
        return queue_->enqueue(std::move(request));
    }
    
    std::string submitRecall(const std::string& datasetName,
                             RecallPriority priority = RecallPriority::NORMAL,
                             const std::string& user = "",
                             const std::string& job = "") {
        RecallRequest request(datasetName);
        request.setPriority(priority)
               .setRequestingUser(user)
               .setRequestingJob(job);
        
        return submitRecall(std::move(request));
    }
    
    // Request management
    bool cancelRecall(const std::string& requestId) {
        return queue_->cancel(requestId);
    }
    
    std::optional<RecallRequestInfo> getRecallStatus(const std::string& requestId) const {
        return queue_->getRequest(requestId);
    }
    
    std::vector<RecallRequestInfo> getQueuedRecalls() const {
        return queue_->getQueuedRequests();
    }
    
    // Processing
    std::optional<RecallRequest> getNextRecall(const std::string& hostName) {
        auto hostInfo = hostManager_->getHost(hostName);
        if (!hostInfo || !hostInfo->isAvailable()) {
            return std::nullopt;
        }
        
        auto request = queue_->dequeueForHost(hostName, *hostInfo);
        if (request) {
            request->assign(hostName);
            hostManager_->updateActiveRecalls(hostName, hostInfo->activeRecalls + 1);
        }
        
        return request;
    }
    
    bool reportRecallComplete(const std::string& requestId,
                              const std::string& hostName,
                              uint64_t bytesRecalled) {
        auto info = queue_->getRequest(requestId);
        if (!info) return false;
        
        queue_->updateStatus(info->datasetName, RecallStatus::COMPLETED);
        
        auto hostInfo = hostManager_->getHost(hostName);
        if (hostInfo) {
            hostManager_->updateActiveRecalls(hostName, 
                hostInfo->activeRecalls > 0 ? hostInfo->activeRecalls - 1 : 0);
            hostManager_->incrementRecallCount(hostName, true);
        }
        
        return true;
    }
    
    bool reportRecallFailed(const std::string& requestId,
                            const std::string& hostName,
                            const std::string& errorMessage) {
        auto info = queue_->getRequest(requestId);
        if (!info) return false;
        
        queue_->updateStatus(info->datasetName, RecallStatus::FAILED);
        
        auto hostInfo = hostManager_->getHost(hostName);
        if (hostInfo) {
            hostManager_->updateActiveRecalls(hostName,
                hostInfo->activeRecalls > 0 ? hostInfo->activeRecalls - 1 : 0);
            hostManager_->incrementRecallCount(hostName, false);
        }
        
        // Handle retry if enabled
        if (config_.enableRetry && info->retryCount < config_.maxRetries) {
            // Re-queue for retry
            RecallRequest retryRequest(info->datasetName);
            retryRequest.getInfo() = *info;
            retryRequest.getInfo().retryCount++;
            retryRequest.getInfo().status = RecallStatus::QUEUED;
            retryRequest.getInfo().assignedHost.clear();
            queue_->enqueue(std::move(retryRequest));
        }
        
        return true;
    }
    
    // Workload balancing
    std::optional<std::string> selectHostForRecall(const RecallRequestInfo& request) {
        auto hosts = hostManager_->getActiveHosts();
        return balancer_->selectHost(hosts, request);
    }
    
    bool needsRebalancing() const {
        auto hosts = hostManager_->getActiveHosts();
        return balancer_->needsRebalancing(hosts);
    }
    
    WorkloadDistribution getWorkloadDistribution() const {
        auto hosts = hostManager_->getAllHosts();
        return balancer_->getDistribution(hosts);
    }
    
    // Maintenance
    void performMaintenance() {
        // Cleanup expired requests
        queue_->cleanupExpired();
        
        // Mark unhealthy hosts
        hostManager_->markUnhealthyHosts();
    }
    
    // Statistics
    QueueStatistics getQueueStatistics() const {
        return queue_->getStatistics();
    }
    
    size_t getQueueDepth() const {
        return queue_->getQueueDepth();
    }
    
    size_t getActiveHostCount() const {
        return hostManager_->getActiveHostCount();
    }
    
    uint32_t getTotalCapacity() const {
        return hostManager_->getTotalCapacity();
    }
    
    // Component access
    RecallQueue& getQueue() { return *queue_; }
    const RecallQueue& getQueue() const { return *queue_; }
    HostManager& getHostManager() { return *hostManager_; }
    const HostManager& getHostManager() const { return *hostManager_; }
    WorkloadBalancer& getBalancer() { return *balancer_; }
    
    // Summary report
    std::string generateSummaryReport() const {
        std::ostringstream oss;
        oss << "Common Recall Queue Summary\n"
            << "===========================\n"
            << "Hosts: " << hostManager_->getActiveHostCount() << "/" 
            << hostManager_->getHostCount() << " active\n"
            << "Capacity: " << hostManager_->getTotalCapacity() << " slots available\n"
            << "Queue Depth: " << queue_->getQueueDepth() << "\n"
            << "Active Recalls: " << queue_->getActiveCount() << "\n\n"
            << queue_->getStatistics().toString();
        return oss.str();
    }

private:
    Configuration config_;
    std::unique_ptr<RecallQueue> queue_;
    std::unique_ptr<HostManager> hostManager_;
    std::unique_ptr<WorkloadBalancer> balancer_;
};

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a standard recall request
 */
inline RecallRequest createRecallRequest(
        const std::string& datasetName,
        RecallPriority priority = RecallPriority::NORMAL,
        const std::string& user = "") {
    
    RecallRequest request(datasetName);
    request.setPriority(priority)
           .setRequestingUser(user);
    return request;
}

/**
 * @brief Create a high-priority recall request
 */
inline RecallRequest createUrgentRecall(
        const std::string& datasetName,
        const std::string& user = "",
        const std::string& job = "") {
    
    RecallRequest request(datasetName);
    request.setPriority(RecallPriority::CRITICAL)
           .setRequestingUser(user)
           .setRequestingJob(job)
           .setTimeouts(std::chrono::seconds(300), std::chrono::seconds(600));
    return request;
}

/**
 * @brief Create a batch recall request
 */
inline RecallRequest createBatchRecall(
        const std::string& datasetName,
        const std::string& job = "") {
    
    RecallRequest request(datasetName);
    request.setPriority(RecallPriority::BATCH)
           .setRequestingJob(job)
           .setTimeouts(std::chrono::seconds(86400), std::chrono::seconds(86400));
    return request;
}

/**
 * @brief Create a host info structure
 */
inline HostInfo createHostInfo(
        const std::string& hostName,
        uint32_t maxConcurrent = 10) {
    
    HostInfo host;
    host.hostName = hostName;
    host.maxConcurrentRecalls = maxConcurrent;
    host.status = HostStatus::IDLE;
    return host;
}

} // namespace crq
} // namespace dfsmshsm

#endif // HSM_CRQ_HPP
