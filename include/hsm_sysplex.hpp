/*
 * DFSMShsm Emulator - Sysplex Coordination
 * @version 4.2.0
 * 
 * Emulates multi-system HSM coordination in a sysplex:
 * - CDS serialization and locking
 * - ENQ/DEQ (enqueue/dequeue) emulation
 * - GRS (Global Resource Serialization)
 * - Host management and failover
 * - Cross-system communication
 * - Workload distribution
 * - Sysplex-wide statistics
 */

#ifndef HSM_SYSPLEX_HPP
#define HSM_SYSPLEX_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <functional>
#include <optional>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <queue>
#include <random>
#include <future>

namespace hsm {
namespace sysplex {

// =============================================================================
// Enumerations
// =============================================================================

// ENQ scope
enum class ENQScope : uint8_t {
    STEP,           // Step level (local to address space)
    SYSTEM,         // System level (local to LPAR)
    SYSTEMS,        // Sysplex level (all systems)
    SYSPLEX = SYSTEMS
};

// ENQ type
enum class ENQType : uint8_t {
    EXCLUSIVE,      // Exclusive (write) lock
    SHARED          // Shared (read) lock
};

// ENQ return codes
enum class ENQResult : int {
    SUCCESS = 0,          // Lock acquired
    ALREADY_HELD = 4,     // Already held by requestor
    CONTENTION = 8,       // Resource busy, queued
    TIMEOUT = 12,         // Wait timed out
    DEADLOCK = 16,        // Deadlock detected
    ERROR = 20            // Error
};

// Host status
enum class HostStatus {
    ACTIVE,         // Host is active and processing
    STANDBY,        // Host is standby (warm)
    STARTING,       // Host is starting up
    STOPPING,       // Host is shutting down
    FAILED,         // Host has failed
    UNKNOWN         // Status unknown
};

// Host role
enum class HostRole {
    PRIMARY,        // Primary host
    SECONDARY,      // Secondary host
    TERTIARY,       // Tertiary host
    AUXILIARY       // Auxiliary host
};

// Message types for cross-system communication
enum class MessageType : uint8_t {
    HEARTBEAT,           // Keep-alive
    STATUS_REQUEST,      // Request status
    STATUS_RESPONSE,     // Status response
    WORK_REQUEST,        // Work assignment
    WORK_COMPLETE,       // Work completion
    LOCK_REQUEST,        // Lock request
    LOCK_GRANT,          // Lock granted
    LOCK_DENY,           // Lock denied
    LOCK_RELEASE,        // Lock release
    TAKEOVER_REQUEST,    // Request takeover
    TAKEOVER_ACK,        // Acknowledge takeover
    SHUTDOWN,            // Shutdown notification
    BROADCAST            // Broadcast message
};

// =============================================================================
// ENQ/DEQ Structures
// =============================================================================

struct ENQRequest {
    std::string qname;              // Major name (8 chars)
    std::string rname;              // Minor name (up to 255 chars)
    ENQScope scope{ENQScope::SYSTEM};
    ENQType type{ENQType::EXCLUSIVE};
    
    std::string owner;              // Owning task/job
    std::string systemId;           // Owning system
    
    bool wait{true};                // Wait if not available
    std::chrono::milliseconds timeout{30000};  // Wait timeout
    
    std::chrono::system_clock::time_point requestTime;
    
    std::string getResourceKey() const {
        return qname + "." + rname;
    }
};

struct ENQHolder {
    std::string owner;
    std::string systemId;
    ENQType type{ENQType::EXCLUSIVE};
    std::chrono::system_clock::time_point acquireTime;
    uint64_t holdCount{1};          // For recursive locks
};

struct ENQWaiter {
    ENQRequest request;
    std::promise<ENQResult> promise;
    std::chrono::system_clock::time_point waitStart;
};

// =============================================================================
// Resource Lock
// =============================================================================

class ResourceLock {
private:
    std::string qname_;
    std::string rname_;
    mutable std::mutex mutex_;
    
    std::vector<ENQHolder> holders_;     // Current holders
    std::queue<std::shared_ptr<ENQWaiter>> waiters_;  // Waiting requests
    
public:
    ResourceLock(const std::string& qname, const std::string& rname)
        : qname_(qname), rname_(rname) {}
    
    ENQResult tryAcquire(const ENQRequest& request, ENQHolder& holder) {
        std::lock_guard lock(mutex_);
        
        // Check if already held by this owner
        for (auto& h : holders_) {
            if (h.owner == request.owner && h.systemId == request.systemId) {
                if (request.type == ENQType::SHARED || h.type == ENQType::EXCLUSIVE) {
                    h.holdCount++;
                    holder = h;
                    return ENQResult::ALREADY_HELD;
                }
            }
        }
        
        // Check compatibility
        if (request.type == ENQType::EXCLUSIVE) {
            // Exclusive requires no other holders
            if (!holders_.empty()) {
                return ENQResult::CONTENTION;
            }
        } else {
            // Shared is compatible with other shared
            for (const auto& h : holders_) {
                if (h.type == ENQType::EXCLUSIVE) {
                    return ENQResult::CONTENTION;
                }
            }
        }
        
        // Grant the lock
        holder.owner = request.owner;
        holder.systemId = request.systemId;
        holder.type = request.type;
        holder.acquireTime = std::chrono::system_clock::now();
        holder.holdCount = 1;
        
        holders_.push_back(holder);
        return ENQResult::SUCCESS;
    }
    
    bool release(const std::string& owner, const std::string& systemId) {
        std::lock_guard lock(mutex_);
        
        for (auto it = holders_.begin(); it != holders_.end(); ++it) {
            if (it->owner == owner && it->systemId == systemId) {
                it->holdCount--;
                if (it->holdCount == 0) {
                    holders_.erase(it);
                    processWaiters();
                }
                return true;
            }
        }
        return false;
    }
    
    void addWaiter(std::shared_ptr<ENQWaiter> waiter) {
        std::lock_guard lock(mutex_);
        waiters_.push(waiter);
    }
    
    std::vector<ENQHolder> getHolders() const {
        std::lock_guard lock(mutex_);
        return holders_;
    }
    
    size_t getWaiterCount() const {
        std::lock_guard lock(mutex_);
        return waiters_.size();
    }
    
    bool isHeld() const {
        std::lock_guard lock(mutex_);
        return !holders_.empty();
    }
    
private:
    void processWaiters() {
        while (!waiters_.empty()) {
            auto waiter = waiters_.front();
            
            ENQHolder holder;
            ENQResult result = tryAcquireInternal(waiter->request, holder);
            
            if (result == ENQResult::SUCCESS || result == ENQResult::ALREADY_HELD) {
                waiters_.pop();
                waiter->promise.set_value(result);
            } else {
                break;  // Can't grant this one, stop processing
            }
        }
    }
    
    ENQResult tryAcquireInternal(const ENQRequest& request, ENQHolder& holder) {
        // Same logic as tryAcquire but without lock (already held)
        for (auto& h : holders_) {
            if (h.owner == request.owner && h.systemId == request.systemId) {
                h.holdCount++;
                holder = h;
                return ENQResult::ALREADY_HELD;
            }
        }
        
        if (request.type == ENQType::EXCLUSIVE) {
            if (!holders_.empty()) {
                return ENQResult::CONTENTION;
            }
        } else {
            for (const auto& h : holders_) {
                if (h.type == ENQType::EXCLUSIVE) {
                    return ENQResult::CONTENTION;
                }
            }
        }
        
        holder.owner = request.owner;
        holder.systemId = request.systemId;
        holder.type = request.type;
        holder.acquireTime = std::chrono::system_clock::now();
        holder.holdCount = 1;
        
        holders_.push_back(holder);
        return ENQResult::SUCCESS;
    }
};

// =============================================================================
// GRS Manager - Global Resource Serialization
// =============================================================================

class GRSManager {
public:
    struct Config {
        std::string systemId{"SYS1"};
        std::chrono::milliseconds defaultTimeout{30000};
        size_t maxResources{100000};
        bool enableDeadlockDetection{true};
        std::chrono::seconds deadlockCheckInterval{5};
        
        Config() = default;
    };
    
    struct Statistics {
        uint64_t enqRequests{0};
        uint64_t enqGranted{0};
        uint64_t enqWaited{0};
        uint64_t enqTimedOut{0};
        uint64_t enqDeadlocks{0};
        uint64_t deqRequests{0};
        uint64_t contentionEvents{0};
        std::chrono::system_clock::time_point lastReset;
    };
    
private:
    Config config_;
    mutable std::shared_mutex mutex_;
    
    std::unordered_map<std::string, std::shared_ptr<ResourceLock>> resources_;
    Statistics stats_;
    
    std::atomic<bool> running_{false};
    std::thread deadlockThread_;
    
public:
    GRSManager() : config_() {
        stats_.lastReset = std::chrono::system_clock::now();
    }
    
    explicit GRSManager(const Config& config) : config_(config) {
        stats_.lastReset = std::chrono::system_clock::now();
    }
    
    ~GRSManager() {
        stop();
    }
    
    // ENQ - Acquire resource lock
    ENQResult enq(ENQRequest request) {
        std::unique_lock lock(mutex_);
        
        stats_.enqRequests++;
        
        if (request.systemId.empty()) {
            request.systemId = config_.systemId;
        }
        request.requestTime = std::chrono::system_clock::now();
        
        std::string key = request.getResourceKey();
        
        // Get or create resource lock
        auto& resource = resources_[key];
        if (!resource) {
            resource = std::make_shared<ResourceLock>(request.qname, request.rname);
        }
        
        lock.unlock();  // Release main lock for acquisition
        
        ENQHolder holder;
        ENQResult result = resource->tryAcquire(request, holder);
        
        if (result == ENQResult::SUCCESS || result == ENQResult::ALREADY_HELD) {
            std::lock_guard statsLock(mutex_);
            stats_.enqGranted++;
            return result;
        }
        
        // Contention
        {
            std::lock_guard statsLock(mutex_);
            stats_.contentionEvents++;
        }
        
        if (!request.wait) {
            return ENQResult::CONTENTION;
        }
        
        // Wait for resource
        {
            std::lock_guard statsLock(mutex_);
            stats_.enqWaited++;
        }
        
        auto waiter = std::make_shared<ENQWaiter>();
        waiter->request = request;
        waiter->waitStart = std::chrono::system_clock::now();
        
        std::future<ENQResult> future = waiter->promise.get_future();
        resource->addWaiter(waiter);
        
        // Wait with timeout
        auto status = future.wait_for(request.timeout);
        
        if (status == std::future_status::timeout) {
            std::lock_guard statsLock(mutex_);
            stats_.enqTimedOut++;
            return ENQResult::TIMEOUT;
        }
        
        return future.get();
    }
    
    // DEQ - Release resource lock
    bool deq(const std::string& qname, const std::string& rname,
             const std::string& owner, const std::string& systemId = "") {
        std::shared_lock lock(mutex_);
        
        stats_.deqRequests++;
        
        std::string key = qname + "." + rname;
        std::string sysId = systemId.empty() ? config_.systemId : systemId;
        
        auto it = resources_.find(key);
        if (it == resources_.end()) {
            return false;
        }
        
        return it->second->release(owner, sysId);
    }
    
    // Query resource status
    struct ResourceStatus {
        std::string qname;
        std::string rname;
        bool isHeld{false};
        std::vector<ENQHolder> holders;
        size_t waiterCount{0};
    };
    
    std::optional<ResourceStatus> queryResource(const std::string& qname,
                                                 const std::string& rname) const {
        std::shared_lock lock(mutex_);
        
        std::string key = qname + "." + rname;
        auto it = resources_.find(key);
        if (it == resources_.end()) {
            return std::nullopt;
        }
        
        ResourceStatus status;
        status.qname = qname;
        status.rname = rname;
        status.isHeld = it->second->isHeld();
        status.holders = it->second->getHolders();
        status.waiterCount = it->second->getWaiterCount();
        
        return status;
    }
    
    std::vector<ResourceStatus> listResources(const std::string& qnamePattern = "*") const {
        std::shared_lock lock(mutex_);
        
        std::vector<ResourceStatus> result;
        
        for (const auto& [key, resource] : resources_) {
            if (!resource->isHeld()) continue;
            
            // Parse key
            auto dotPos = key.find('.');
            std::string qname = key.substr(0, dotPos);
            std::string rname = dotPos != std::string::npos ? 
                               key.substr(dotPos + 1) : "";
            
            if (qnamePattern != "*" && qname != qnamePattern) {
                continue;
            }
            
            ResourceStatus status;
            status.qname = qname;
            status.rname = rname;
            status.isHeld = true;
            status.holders = resource->getHolders();
            status.waiterCount = resource->getWaiterCount();
            
            result.push_back(status);
        }
        
        return result;
    }
    
    Statistics getStatistics() const {
        std::shared_lock lock(mutex_);
        return stats_;
    }
    
    void resetStatistics() {
        std::unique_lock lock(mutex_);
        stats_ = Statistics{};
        stats_.lastReset = std::chrono::system_clock::now();
    }
    
    void start() {
        if (running_.exchange(true)) return;
        
        if (config_.enableDeadlockDetection) {
            deadlockThread_ = std::thread([this]() {
                while (running_) {
                    std::this_thread::sleep_for(config_.deadlockCheckInterval);
                    checkDeadlocks();
                }
            });
        }
    }
    
    void stop() {
        if (!running_.exchange(false)) return;
        
        if (deadlockThread_.joinable()) {
            deadlockThread_.join();
        }
    }
    
private:
    void checkDeadlocks() {
        // Simplified deadlock detection
        // In a real implementation, this would build a wait-for graph
        std::shared_lock lock(mutex_);
        
        // For now, just check for long-held locks
        auto now = std::chrono::system_clock::now();
        
        for (const auto& [key, resource] : resources_) {
            auto holders = resource->getHolders();
            for (const auto& holder : holders) {
                auto duration = std::chrono::duration_cast<std::chrono::minutes>(
                    now - holder.acquireTime);
                if (duration.count() > 30) {  // Held for more than 30 minutes
                    // Log warning about long-held lock
                }
            }
        }
    }
};

// =============================================================================
// Host Information
// =============================================================================

struct HostInfo {
    std::string hostId;             // Host identifier
    std::string systemId;           // z/OS system ID
    std::string sysplexName;        // Sysplex name
    
    HostStatus status{HostStatus::UNKNOWN};
    HostRole role{HostRole::SECONDARY};
    
    std::string ipAddress;
    uint16_t port{0};
    
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point lastHeartbeat;
    std::chrono::system_clock::time_point lastActivity;
    
    uint64_t migrationsProcessed{0};
    uint64_t recallsProcessed{0};
    uint64_t backupsProcessed{0};
    
    int workloadPercent{0};          // Current workload 0-100
    int maxTasks{100};               // Maximum concurrent tasks
    int activeTasks{0};              // Current active tasks
    
    bool isAvailable() const {
        return status == HostStatus::ACTIVE || status == HostStatus::STANDBY;
    }
};

// =============================================================================
// Cross-System Message
// =============================================================================

struct SysplexMessage {
    MessageType type{MessageType::HEARTBEAT};
    std::string sourceHost;
    std::string targetHost;          // Empty = broadcast
    std::string correlationId;
    
    std::chrono::system_clock::time_point timestamp;
    
    std::map<std::string, std::string> payload;
    
    std::string serialize() const {
        std::string result;
        result += std::to_string(static_cast<int>(type)) + "|";
        result += sourceHost + "|";
        result += targetHost + "|";
        result += correlationId + "|";
        for (const auto& [k, v] : payload) {
            result += k + "=" + v + ";";
        }
        return result;
    }
};

// =============================================================================
// Sysplex Coordinator
// =============================================================================

class SysplexCoordinator {
public:
    struct Config {
        std::string hostId;
        std::string systemId;
        std::string sysplexName{"PLEX1"};
        
        std::chrono::seconds heartbeatInterval{30};
        std::chrono::seconds failureThreshold{90};
        
        bool enableAutomaticTakeover{true};
        int takeoverPriority{50};           // Lower = higher priority
        
        GRSManager::Config grsConfig;
        
        Config() = default;
    };
    
    using MessageHandler = std::function<void(const SysplexMessage&)>;
    using TakeoverHandler = std::function<bool(const std::string& failedHost)>;
    using WorkHandler = std::function<bool(const std::map<std::string, std::string>&)>;
    
private:
    Config config_;
    mutable std::shared_mutex mutex_;
    
    HostInfo localHost_;
    std::map<std::string, HostInfo> remoteHosts_;
    
    GRSManager grs_;
    
    std::vector<MessageHandler> messageHandlers_;
    TakeoverHandler takeoverHandler_;
    WorkHandler workHandler_;
    
    std::atomic<bool> running_{false};
    std::thread heartbeatThread_;
    std::thread monitorThread_;
    
    std::queue<SysplexMessage> outboundQueue_;
    std::queue<SysplexMessage> inboundQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    
    // CDS lock names
    static constexpr const char* MCDS_LOCK = "ARCMCDS";
    static constexpr const char* BCDS_LOCK = "ARCBCDS";
    static constexpr const char* OCDS_LOCK = "ARCOCDS";
    
public:
    SysplexCoordinator() : config_(), grs_() {
        initializeLocalHost();
    }
    
    explicit SysplexCoordinator(const Config& config) 
        : config_(config), grs_(config.grsConfig) {
        initializeLocalHost();
    }
    
    ~SysplexCoordinator() {
        stop();
    }
    
    // =========================================================================
    // Host Management
    // =========================================================================
    
    void registerHost(const HostInfo& host) {
        std::unique_lock lock(mutex_);
        
        if (host.hostId == config_.hostId) {
            localHost_ = host;
        } else {
            remoteHosts_[host.hostId] = host;
        }
    }
    
    void unregisterHost(const std::string& hostId) {
        std::unique_lock lock(mutex_);
        remoteHosts_.erase(hostId);
    }
    
    std::optional<HostInfo> getHost(const std::string& hostId) const {
        std::shared_lock lock(mutex_);
        
        if (hostId == config_.hostId) {
            return localHost_;
        }
        
        auto it = remoteHosts_.find(hostId);
        if (it != remoteHosts_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<HostInfo> getAllHosts() const {
        std::shared_lock lock(mutex_);
        
        std::vector<HostInfo> result;
        result.push_back(localHost_);
        for (const auto& [id, host] : remoteHosts_) {
            result.push_back(host);
        }
        return result;
    }
    
    std::vector<HostInfo> getActiveHosts() const {
        std::shared_lock lock(mutex_);
        
        std::vector<HostInfo> result;
        if (localHost_.status == HostStatus::ACTIVE) {
            result.push_back(localHost_);
        }
        for (const auto& [id, host] : remoteHosts_) {
            if (host.status == HostStatus::ACTIVE) {
                result.push_back(host);
            }
        }
        return result;
    }
    
    HostInfo getLocalHost() const {
        std::shared_lock lock(mutex_);
        return localHost_;
    }
    
    void updateLocalStatus(HostStatus status) {
        std::unique_lock lock(mutex_);
        localHost_.status = status;
        localHost_.lastActivity = std::chrono::system_clock::now();
    }
    
    void updateLocalWorkload(int tasks, int maxTasks) {
        std::unique_lock lock(mutex_);
        localHost_.activeTasks = tasks;
        localHost_.maxTasks = maxTasks;
        localHost_.workloadPercent = maxTasks > 0 ? (tasks * 100 / maxTasks) : 0;
    }
    
    // =========================================================================
    // CDS Locking
    // =========================================================================
    
    ENQResult lockMCDS(const std::string& owner, ENQType type = ENQType::EXCLUSIVE) {
        ENQRequest request;
        request.qname = "ARCGPA";
        request.rname = MCDS_LOCK;
        request.type = type;
        request.owner = owner;
        request.scope = ENQScope::SYSPLEX;
        return grs_.enq(request);
    }
    
    bool unlockMCDS(const std::string& owner) {
        return grs_.deq("ARCGPA", MCDS_LOCK, owner);
    }
    
    ENQResult lockBCDS(const std::string& owner, ENQType type = ENQType::EXCLUSIVE) {
        ENQRequest request;
        request.qname = "ARCGPA";
        request.rname = BCDS_LOCK;
        request.type = type;
        request.owner = owner;
        request.scope = ENQScope::SYSPLEX;
        return grs_.enq(request);
    }
    
    bool unlockBCDS(const std::string& owner) {
        return grs_.deq("ARCGPA", BCDS_LOCK, owner);
    }
    
    ENQResult lockOCDS(const std::string& owner, ENQType type = ENQType::EXCLUSIVE) {
        ENQRequest request;
        request.qname = "ARCGPA";
        request.rname = OCDS_LOCK;
        request.type = type;
        request.owner = owner;
        request.scope = ENQScope::SYSPLEX;
        return grs_.enq(request);
    }
    
    bool unlockOCDS(const std::string& owner) {
        return grs_.deq("ARCGPA", OCDS_LOCK, owner);
    }
    
    // Lock dataset for HSM operation
    ENQResult lockDataset(const std::string& owner, const std::string& datasetName,
                          ENQType type = ENQType::EXCLUSIVE) {
        ENQRequest request;
        request.qname = "SYSZDSN";
        request.rname = datasetName;
        request.type = type;
        request.owner = owner;
        request.scope = ENQScope::SYSPLEX;
        return grs_.enq(request);
    }
    
    bool unlockDataset(const std::string& owner, const std::string& datasetName) {
        return grs_.deq("SYSZDSN", datasetName, owner);
    }
    
    // =========================================================================
    // Message Handling
    // =========================================================================
    
    void sendMessage(const SysplexMessage& message) {
        std::lock_guard lock(queueMutex_);
        outboundQueue_.push(message);
        queueCV_.notify_one();
    }
    
    void broadcast(MessageType type, const std::map<std::string, std::string>& payload) {
        SysplexMessage msg;
        msg.type = type;
        msg.sourceHost = config_.hostId;
        msg.targetHost = "";  // Broadcast
        msg.timestamp = std::chrono::system_clock::now();
        msg.payload = payload;
        sendMessage(msg);
    }
    
    void receiveMessage(const SysplexMessage& message) {
        // Process internally
        handleMessage(message);
        
        // Notify handlers
        std::shared_lock lock(mutex_);
        for (const auto& handler : messageHandlers_) {
            handler(message);
        }
    }
    
    void addMessageHandler(MessageHandler handler) {
        std::unique_lock lock(mutex_);
        messageHandlers_.push_back(std::move(handler));
    }
    
    void setTakeoverHandler(TakeoverHandler handler) {
        std::unique_lock lock(mutex_);
        takeoverHandler_ = std::move(handler);
    }
    
    void setWorkHandler(WorkHandler handler) {
        std::unique_lock lock(mutex_);
        workHandler_ = std::move(handler);
    }
    
    // =========================================================================
    // Work Distribution
    // =========================================================================
    
    std::optional<std::string> selectHostForWork() const {
        std::shared_lock lock(mutex_);
        
        std::string bestHost;
        int lowestWorkload = 101;
        
        // Check local host
        if (localHost_.status == HostStatus::ACTIVE &&
            localHost_.workloadPercent < 100) {
            bestHost = localHost_.hostId;
            lowestWorkload = localHost_.workloadPercent;
        }
        
        // Check remote hosts
        for (const auto& [id, host] : remoteHosts_) {
            if (host.status == HostStatus::ACTIVE &&
                host.workloadPercent < lowestWorkload) {
                bestHost = id;
                lowestWorkload = host.workloadPercent;
            }
        }
        
        if (bestHost.empty()) {
            return std::nullopt;
        }
        return bestHost;
    }
    
    bool requestWork(const std::string& targetHost,
                     const std::map<std::string, std::string>& workRequest) {
        SysplexMessage msg;
        msg.type = MessageType::WORK_REQUEST;
        msg.sourceHost = config_.hostId;
        msg.targetHost = targetHost;
        msg.timestamp = std::chrono::system_clock::now();
        msg.payload = workRequest;
        
        // Generate correlation ID
        static std::atomic<uint64_t> correlationCounter{0};
        msg.correlationId = config_.hostId + "_" + 
                           std::to_string(correlationCounter++);
        
        sendMessage(msg);
        return true;
    }
    
    // =========================================================================
    // Takeover/Failover
    // =========================================================================
    
    bool requestTakeover(const std::string& failedHost) {
        if (!config_.enableAutomaticTakeover) {
            return false;
        }
        
        // Send takeover request
        SysplexMessage msg;
        msg.type = MessageType::TAKEOVER_REQUEST;
        msg.sourceHost = config_.hostId;
        msg.targetHost = "";  // Broadcast
        msg.timestamp = std::chrono::system_clock::now();
        msg.payload["failedHost"] = failedHost;
        msg.payload["priority"] = std::to_string(config_.takeoverPriority);
        
        broadcast(MessageType::TAKEOVER_REQUEST, msg.payload);
        
        // Assume takeover for now
        if (takeoverHandler_) {
            return takeoverHandler_(failedHost);
        }
        
        return true;
    }
    
    // =========================================================================
    // GRS Access
    // =========================================================================
    
    GRSManager& getGRS() { return grs_; }
    const GRSManager& getGRS() const { return grs_; }
    
    // =========================================================================
    // Lifecycle
    // =========================================================================
    
    void start() {
        if (running_.exchange(true)) return;
        
        localHost_.status = HostStatus::ACTIVE;
        localHost_.startTime = std::chrono::system_clock::now();
        localHost_.lastHeartbeat = localHost_.startTime;
        
        grs_.start();
        
        // Start heartbeat thread
        heartbeatThread_ = std::thread([this]() {
            while (running_) {
                sendHeartbeat();
                std::this_thread::sleep_for(config_.heartbeatInterval);
            }
        });
        
        // Start monitor thread
        monitorThread_ = std::thread([this]() {
            while (running_) {
                monitorHosts();
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
        });
    }
    
    void stop() {
        if (!running_.exchange(false)) return;
        
        localHost_.status = HostStatus::STOPPING;
        
        // Notify other hosts
        broadcast(MessageType::SHUTDOWN, {});
        
        grs_.stop();
        
        if (heartbeatThread_.joinable()) {
            heartbeatThread_.join();
        }
        if (monitorThread_.joinable()) {
            monitorThread_.join();
        }
        
        localHost_.status = HostStatus::UNKNOWN;
    }
    
    bool isRunning() const {
        return running_;
    }
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    struct SysplexStatistics {
        size_t totalHosts{0};
        size_t activeHosts{0};
        size_t failedHosts{0};
        
        uint64_t totalMigrations{0};
        uint64_t totalRecalls{0};
        uint64_t totalBackups{0};
        
        uint64_t messagesSent{0};
        uint64_t messagesReceived{0};
        
        GRSManager::Statistics grsStats;
    };
    
    SysplexStatistics getStatistics() const {
        std::shared_lock lock(mutex_);
        
        SysplexStatistics stats;
        stats.totalHosts = 1 + remoteHosts_.size();
        
        stats.activeHosts = (localHost_.status == HostStatus::ACTIVE) ? 1 : 0;
        stats.totalMigrations = localHost_.migrationsProcessed;
        stats.totalRecalls = localHost_.recallsProcessed;
        stats.totalBackups = localHost_.backupsProcessed;
        
        for (const auto& [id, host] : remoteHosts_) {
            if (host.status == HostStatus::ACTIVE) {
                stats.activeHosts++;
            } else if (host.status == HostStatus::FAILED) {
                stats.failedHosts++;
            }
            stats.totalMigrations += host.migrationsProcessed;
            stats.totalRecalls += host.recallsProcessed;
            stats.totalBackups += host.backupsProcessed;
        }
        
        stats.grsStats = grs_.getStatistics();
        
        return stats;
    }
    
private:
    void initializeLocalHost() {
        localHost_.hostId = config_.hostId;
        localHost_.systemId = config_.systemId;
        localHost_.sysplexName = config_.sysplexName;
        localHost_.status = HostStatus::STARTING;
        localHost_.role = HostRole::SECONDARY;
    }
    
    void sendHeartbeat() {
        std::map<std::string, std::string> payload;
        payload["status"] = std::to_string(static_cast<int>(localHost_.status));
        payload["workload"] = std::to_string(localHost_.workloadPercent);
        payload["tasks"] = std::to_string(localHost_.activeTasks);
        
        broadcast(MessageType::HEARTBEAT, payload);
        
        std::unique_lock lock(mutex_);
        localHost_.lastHeartbeat = std::chrono::system_clock::now();
    }
    
    void monitorHosts() {
        std::unique_lock lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        
        for (auto& [id, host] : remoteHosts_) {
            auto timeSinceHeartbeat = std::chrono::duration_cast<std::chrono::seconds>(
                now - host.lastHeartbeat);
            
            if (timeSinceHeartbeat > config_.failureThreshold) {
                if (host.status == HostStatus::ACTIVE) {
                    host.status = HostStatus::FAILED;
                    
                    // Trigger takeover if enabled
                    if (config_.enableAutomaticTakeover) {
                        lock.unlock();
                        requestTakeover(id);
                        lock.lock();
                    }
                }
            }
        }
    }
    
    void handleMessage(const SysplexMessage& message) {
        switch (message.type) {
            case MessageType::HEARTBEAT:
                handleHeartbeat(message);
                break;
            case MessageType::WORK_REQUEST:
                handleWorkRequest(message);
                break;
            case MessageType::TAKEOVER_REQUEST:
                handleTakeoverRequest(message);
                break;
            default:
                break;
        }
    }
    
    void handleHeartbeat(const SysplexMessage& message) {
        std::unique_lock lock(mutex_);
        
        auto it = remoteHosts_.find(message.sourceHost);
        if (it != remoteHosts_.end()) {
            it->second.lastHeartbeat = message.timestamp;
            
            auto statusIt = message.payload.find("status");
            if (statusIt != message.payload.end()) {
                it->second.status = static_cast<HostStatus>(std::stoi(statusIt->second));
            }
            
            auto workloadIt = message.payload.find("workload");
            if (workloadIt != message.payload.end()) {
                it->second.workloadPercent = std::stoi(workloadIt->second);
            }
        }
    }
    
    void handleWorkRequest(const SysplexMessage& message) {
        if (workHandler_) {
            bool handled = workHandler_(message.payload);
            
            // Send response
            SysplexMessage response;
            response.type = MessageType::WORK_COMPLETE;
            response.sourceHost = config_.hostId;
            response.targetHost = message.sourceHost;
            response.correlationId = message.correlationId;
            response.payload["success"] = handled ? "true" : "false";
            
            sendMessage(response);
        }
    }
    
    void handleTakeoverRequest(const SysplexMessage& message) {
        auto failedHostIt = message.payload.find("failedHost");
        if (failedHostIt == message.payload.end()) return;
        
        auto priorityIt = message.payload.find("priority");
        int remotePriority = priorityIt != message.payload.end() ? 
                            std::stoi(priorityIt->second) : 50;
        
        // If our priority is lower (better), we should handle takeover
        if (config_.takeoverPriority < remotePriority) {
            if (takeoverHandler_) {
                takeoverHandler_(failedHostIt->second);
            }
        }
    }
};

// =============================================================================
// Utility Functions
// =============================================================================

inline std::string enqScopeToString(ENQScope scope) {
    switch (scope) {
        case ENQScope::STEP: return "STEP";
        case ENQScope::SYSTEM: return "SYSTEM";
        case ENQScope::SYSTEMS: return "SYSTEMS";
        default: return "UNKNOWN";
    }
}

inline std::string enqTypeToString(ENQType type) {
    switch (type) {
        case ENQType::EXCLUSIVE: return "EXCLUSIVE";
        case ENQType::SHARED: return "SHARED";
        default: return "UNKNOWN";
    }
}

inline std::string enqResultToString(ENQResult result) {
    switch (result) {
        case ENQResult::SUCCESS: return "SUCCESS";
        case ENQResult::ALREADY_HELD: return "ALREADY_HELD";
        case ENQResult::CONTENTION: return "CONTENTION";
        case ENQResult::TIMEOUT: return "TIMEOUT";
        case ENQResult::DEADLOCK: return "DEADLOCK";
        case ENQResult::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

inline std::string hostStatusToString(HostStatus status) {
    switch (status) {
        case HostStatus::ACTIVE: return "ACTIVE";
        case HostStatus::STANDBY: return "STANDBY";
        case HostStatus::STARTING: return "STARTING";
        case HostStatus::STOPPING: return "STOPPING";
        case HostStatus::FAILED: return "FAILED";
        case HostStatus::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

inline std::string hostRoleToString(HostRole role) {
    switch (role) {
        case HostRole::PRIMARY: return "PRIMARY";
        case HostRole::SECONDARY: return "SECONDARY";
        case HostRole::TERTIARY: return "TERTIARY";
        case HostRole::AUXILIARY: return "AUXILIARY";
        default: return "UNKNOWN";
    }
}

}  // namespace sysplex
}  // namespace hsm

#endif // HSM_SYSPLEX_HPP
