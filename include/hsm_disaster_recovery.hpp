/*******************************************************************************
 * DFSMShsm Emulator - Disaster Recovery System
 * @version 4.2.0
 * 
 * Provides comprehensive disaster recovery capabilities including site
 * switching, data replication, recovery point objectives (RPO), and
 * recovery time objectives (RTO) management.
 * 
 * Features:
 * - Multi-site configuration and management
 * - Synchronous and asynchronous data replication
 * - Recovery point management (RPO/RTO)
 * - Automated failover and failback
 * - Site health monitoring
 * - Recovery testing and validation
 * - Replication lag tracking
 * 
 * Author: DFSMShsm Emulator Team
 * License: MIT
 ******************************************************************************/

#ifndef HSM_DISASTER_RECOVERY_HPP
#define HSM_DISASTER_RECOVERY_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <optional>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <functional>
#include <algorithm>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>

namespace dfsmshsm {
namespace dr {

// ============================================================================
// Site and Replication Types
// ============================================================================

enum class SiteRole {
    PRIMARY,        // Active production site
    SECONDARY,      // Standby/DR site
    TERTIARY,       // Third-level DR site
    ARCHIVE,        // Long-term archive site
    INACTIVE        // Not participating
};

enum class SiteStatus {
    ONLINE,         // Fully operational
    DEGRADED,       // Operational with issues
    OFFLINE,        // Not available
    MAINTENANCE,    // Planned maintenance
    FAILING_OVER,   // Failover in progress
    FAILED_OVER,    // Has taken over as primary
    RECOVERING      // Recovery in progress
};

enum class ReplicationMode {
    SYNCHRONOUS,    // Wait for acknowledgment
    ASYNCHRONOUS,   // Fire and forget
    SEMI_SYNC,      // Wait for journal commit
    SNAPSHOT        // Periodic snapshots only
};

enum class ReplicationState {
    ACTIVE,         // Actively replicating
    PAUSED,         // Temporarily paused
    SUSPENDED,      // Administratively suspended
    BROKEN,         // Replication broken
    RESYNCING,      // Resynchronization in progress
    IDLE            // No pending changes
};

inline std::string siteRoleToString(SiteRole role) {
    switch (role) {
        case SiteRole::PRIMARY:   return "PRIMARY";
        case SiteRole::SECONDARY: return "SECONDARY";
        case SiteRole::TERTIARY:  return "TERTIARY";
        case SiteRole::ARCHIVE:   return "ARCHIVE";
        case SiteRole::INACTIVE:  return "INACTIVE";
        default: return "UNKNOWN";
    }
}

inline std::string siteStatusToString(SiteStatus status) {
    switch (status) {
        case SiteStatus::ONLINE:      return "ONLINE";
        case SiteStatus::DEGRADED:    return "DEGRADED";
        case SiteStatus::OFFLINE:     return "OFFLINE";
        case SiteStatus::MAINTENANCE: return "MAINTENANCE";
        case SiteStatus::FAILING_OVER: return "FAILING_OVER";
        case SiteStatus::FAILED_OVER: return "FAILED_OVER";
        case SiteStatus::RECOVERING:  return "RECOVERING";
        default: return "UNKNOWN";
    }
}

inline std::string replicationModeToString(ReplicationMode mode) {
    switch (mode) {
        case ReplicationMode::SYNCHRONOUS:  return "SYNCHRONOUS";
        case ReplicationMode::ASYNCHRONOUS: return "ASYNCHRONOUS";
        case ReplicationMode::SEMI_SYNC:    return "SEMI_SYNC";
        case ReplicationMode::SNAPSHOT:     return "SNAPSHOT";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Recovery Point and Time Objectives
// ============================================================================

struct RecoveryObjectives {
    std::chrono::seconds rpo;  // Recovery Point Objective
    std::chrono::seconds rto;  // Recovery Time Objective
    
    // Tiered objectives
    std::chrono::seconds rpo_critical;     // For critical data
    std::chrono::seconds rpo_important;    // For important data
    std::chrono::seconds rpo_standard;     // For standard data
    
    RecoveryObjectives()
        : rpo(std::chrono::hours(1))
        , rto(std::chrono::hours(4))
        , rpo_critical(std::chrono::minutes(15))
        , rpo_important(std::chrono::hours(1))
        , rpo_standard(std::chrono::hours(24)) {}
};

// ============================================================================
// Site Definition
// ============================================================================

struct Site {
    std::string site_id;
    std::string name;
    std::string location;
    std::string description;
    
    SiteRole role;
    SiteStatus status;
    
    // Network
    std::string primary_address;
    std::string secondary_address;
    int port;
    
    // Capacity
    size_t total_capacity;
    size_t used_capacity;
    size_t reserved_capacity;  // Reserved for failover
    
    // Health metrics
    double cpu_utilization;
    double memory_utilization;
    double storage_utilization;
    double network_latency_ms;
    
    // Timestamps
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point last_heartbeat;
    std::chrono::system_clock::time_point last_sync;
    std::chrono::system_clock::time_point role_changed;
    
    // Recovery objectives for this site
    RecoveryObjectives objectives;
    
    // Statistics
    uint64_t failover_count;
    uint64_t replication_errors;
    std::chrono::system_clock::time_point last_failover;
    
    Site()
        : role(SiteRole::INACTIVE)
        , status(SiteStatus::OFFLINE)
        , port(0)
        , total_capacity(0)
        , used_capacity(0)
        , reserved_capacity(0)
        , cpu_utilization(0)
        , memory_utilization(0)
        , storage_utilization(0)
        , network_latency_ms(0)
        , failover_count(0)
        , replication_errors(0) {}
    
    double availableCapacity() const {
        return total_capacity - used_capacity - reserved_capacity;
    }
    
    bool isHealthy() const {
        return status == SiteStatus::ONLINE &&
               cpu_utilization < 90 &&
               memory_utilization < 90 &&
               storage_utilization < 85;
    }
    
    bool canAcceptFailover() const {
        return (status == SiteStatus::ONLINE || status == SiteStatus::DEGRADED) &&
               role != SiteRole::PRIMARY &&
               availableCapacity() > 0;
    }
    
    std::string toSummary() const {
        std::ostringstream oss;
        oss << site_id << " | " << name 
            << " | " << siteRoleToString(role)
            << " | " << siteStatusToString(status);
        return oss.str();
    }
};

// ============================================================================
// Replication Link
// ============================================================================

struct ReplicationLink {
    std::string link_id;
    std::string source_site;
    std::string target_site;
    
    ReplicationMode mode;
    ReplicationState state;
    
    // Configuration
    bool bidirectional;
    int priority;                          // Lower = higher priority
    size_t bandwidth_limit_mbps;           // 0 = unlimited
    std::chrono::seconds sync_interval;    // For snapshot mode
    
    // State tracking
    uint64_t sequence_number;              // Last replicated sequence
    std::chrono::system_clock::time_point last_replicated;
    std::chrono::milliseconds replication_lag;
    
    // Statistics
    size_t bytes_replicated;
    uint64_t transactions_replicated;
    uint64_t replication_errors;
    double avg_latency_ms;
    
    // Queue status
    size_t pending_bytes;
    uint64_t pending_transactions;
    
    ReplicationLink()
        : mode(ReplicationMode::ASYNCHRONOUS)
        , state(ReplicationState::IDLE)
        , bidirectional(false)
        , priority(100)
        , bandwidth_limit_mbps(0)
        , sync_interval(std::chrono::minutes(15))
        , sequence_number(0)
        , replication_lag(0)
        , bytes_replicated(0)
        , transactions_replicated(0)
        , replication_errors(0)
        , avg_latency_ms(0)
        , pending_bytes(0)
        , pending_transactions(0) {}
    
    bool isHealthy() const {
        return state == ReplicationState::ACTIVE || 
               state == ReplicationState::IDLE;
    }
    
    bool meetsRPO(std::chrono::seconds rpo) const {
        return replication_lag <= rpo;
    }
};

// ============================================================================
// Recovery Point
// ============================================================================

struct RecoveryPoint {
    std::string point_id;
    std::string site_id;
    std::string description;
    
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point expiration;
    
    // What this point covers
    std::vector<std::string> volumes;
    std::vector<std::string> storage_groups;
    size_t data_size;
    
    // Consistency
    uint64_t sequence_number;
    std::string consistency_group;
    bool application_consistent;
    
    // Validation
    bool validated;
    std::chrono::system_clock::time_point last_validated;
    std::string validation_result;
    
    // Type
    enum class Type {
        AUTOMATIC,      // Scheduled/automatic
        MANUAL,         // Manually created
        PRE_CHANGE,     // Before a change
        POST_CHANGE,    // After a change
        FAILOVER        // Created during failover
    } type;
    
    RecoveryPoint()
        : data_size(0)
        , sequence_number(0)
        , application_consistent(false)
        , validated(false)
        , type(Type::AUTOMATIC) {}
    
    bool isExpired() const {
        return std::chrono::system_clock::now() >= expiration;
    }
};

// ============================================================================
// Failover Event
// ============================================================================

struct FailoverEvent {
    std::string event_id;
    std::chrono::system_clock::time_point started;
    std::chrono::system_clock::time_point completed;
    
    std::string source_site;
    std::string target_site;
    
    enum class Type {
        PLANNED,        // Planned switchover
        UNPLANNED,      // Disaster failover
        TEST,           // DR test
        FAILBACK        // Return to primary
    } type;
    
    enum class Status {
        IN_PROGRESS,
        COMPLETED,
        FAILED,
        ROLLED_BACK
    } status;
    
    // Recovery point used
    std::string recovery_point_id;
    std::chrono::system_clock::time_point recovery_point_time;
    
    // Data loss assessment
    std::chrono::seconds actual_data_loss;
    std::chrono::seconds target_rpo;
    bool rpo_met;
    
    // Recovery time
    std::chrono::seconds actual_recovery_time;
    std::chrono::seconds target_rto;
    bool rto_met;
    
    // Details
    std::vector<std::string> steps_completed;
    std::string error_message;
    std::string initiated_by;
    
    FailoverEvent()
        : type(Type::PLANNED)
        , status(Status::IN_PROGRESS)
        , actual_data_loss(0)
        , target_rpo(0)
        , rpo_met(false)
        , actual_recovery_time(0)
        , target_rto(0)
        , rto_met(false) {}
    
    std::chrono::seconds duration() const {
        auto end = (status == Status::IN_PROGRESS) ? 
            std::chrono::system_clock::now() : completed;
        return std::chrono::duration_cast<std::chrono::seconds>(end - started);
    }
};

// ============================================================================
// Disaster Recovery Manager
// ============================================================================

class DisasterRecoveryManager {
public:
    using FailoverCallback = std::function<void(const FailoverEvent&)>;
    using HealthCheckCallback = std::function<bool(const std::string& site_id)>;
    
    DisasterRecoveryManager() 
        : monitoring_active_(false)
        , next_event_num_(1) {}
    
    ~DisasterRecoveryManager() {
        stopMonitoring();
    }
    
    // Site Management
    void registerSite(const Site& site) {
        std::lock_guard<std::mutex> lock(mutex_);
        sites_[site.site_id] = site;
    }
    
    bool unregisterSite(const std::string& site_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Can't unregister primary site
        auto it = sites_.find(site_id);
        if (it != sites_.end() && it->second.role == SiteRole::PRIMARY) {
            return false;
        }
        
        // Remove associated links
        std::vector<std::string> links_to_remove;
        for (const auto& [id, link] : replication_links_) {
            if (link.source_site == site_id || link.target_site == site_id) {
                links_to_remove.push_back(id);
            }
        }
        for (const auto& id : links_to_remove) {
            replication_links_.erase(id);
        }
        
        return sites_.erase(site_id) > 0;
    }
    
    std::optional<Site> getSite(const std::string& site_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sites_.find(site_id);
        if (it != sites_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    void updateSiteStatus(const std::string& site_id, SiteStatus status) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sites_.find(site_id);
        if (it != sites_.end()) {
            it->second.status = status;
            it->second.last_heartbeat = std::chrono::system_clock::now();
        }
    }
    
    void updateSiteHealth(const std::string& site_id,
                          double cpu, double memory, double storage,
                          double latency) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sites_.find(site_id);
        if (it != sites_.end()) {
            it->second.cpu_utilization = cpu;
            it->second.memory_utilization = memory;
            it->second.storage_utilization = storage;
            it->second.network_latency_ms = latency;
            it->second.last_heartbeat = std::chrono::system_clock::now();
        }
    }
    
    std::vector<Site> listSites() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Site> result;
        for (const auto& [id, site] : sites_) {
            result.push_back(site);
        }
        return result;
    }
    
    std::optional<Site> getPrimarySite() const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, site] : sites_) {
            if (site.role == SiteRole::PRIMARY) {
                return site;
            }
        }
        return std::nullopt;
    }
    
    // Replication Link Management
    void createReplicationLink(const ReplicationLink& link) {
        std::lock_guard<std::mutex> lock(mutex_);
        replication_links_[link.link_id] = link;
    }
    
    bool deleteReplicationLink(const std::string& link_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return replication_links_.erase(link_id) > 0;
    }
    
    std::optional<ReplicationLink> getReplicationLink(const std::string& link_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = replication_links_.find(link_id);
        if (it != replication_links_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    void updateReplicationState(const std::string& link_id, 
                                 ReplicationState state,
                                 std::chrono::milliseconds lag = std::chrono::milliseconds(0)) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = replication_links_.find(link_id);
        if (it != replication_links_.end()) {
            it->second.state = state;
            it->second.replication_lag = lag;
            if (state == ReplicationState::ACTIVE) {
                it->second.last_replicated = std::chrono::system_clock::now();
            }
        }
    }
    
    std::vector<ReplicationLink> getLinksForSite(const std::string& site_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ReplicationLink> result;
        for (const auto& [id, link] : replication_links_) {
            if (link.source_site == site_id || link.target_site == site_id) {
                result.push_back(link);
            }
        }
        return result;
    }
    
    // Recovery Point Management
    std::string createRecoveryPoint(const std::string& site_id,
                                     const std::string& description,
                                     RecoveryPoint::Type type = RecoveryPoint::Type::MANUAL) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        RecoveryPoint rp;
        rp.point_id = generatePointId(site_id);
        rp.site_id = site_id;
        rp.description = description;
        rp.type = type;
        rp.created = std::chrono::system_clock::now();
        rp.expiration = rp.created + std::chrono::hours(24 * 30);  // 30 days default
        
        recovery_points_[rp.point_id] = rp;
        return rp.point_id;
    }
    
    std::optional<RecoveryPoint> getRecoveryPoint(const std::string& point_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = recovery_points_.find(point_id);
        if (it != recovery_points_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<RecoveryPoint> getRecoveryPointsForSite(const std::string& site_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<RecoveryPoint> result;
        for (const auto& [id, rp] : recovery_points_) {
            if (rp.site_id == site_id && !rp.isExpired()) {
                result.push_back(rp);
            }
        }
        std::sort(result.begin(), result.end(),
            [](const RecoveryPoint& a, const RecoveryPoint& b) {
                return a.created > b.created;
            });
        return result;
    }
    
    std::optional<RecoveryPoint> getLatestRecoveryPoint(const std::string& site_id) const {
        auto points = getRecoveryPointsForSite(site_id);
        if (!points.empty()) {
            return points[0];
        }
        return std::nullopt;
    }
    
    bool deleteRecoveryPoint(const std::string& point_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return recovery_points_.erase(point_id) > 0;
    }
    
    // Failover Operations
    struct FailoverOptions {
        std::string target_site;
        std::string recovery_point_id;  // Empty for latest
        bool force;                      // Force even if not all checks pass
        bool create_recovery_point;      // Create RP before failover
        FailoverEvent::Type type;
        std::string initiated_by;
        
        FailoverOptions()
            : force(false)
            , create_recovery_point(true)
            , type(FailoverEvent::Type::PLANNED) {}
    };
    
    struct FailoverResult {
        bool success;
        std::string event_id;
        std::string error_message;
        std::chrono::seconds recovery_time;
        std::chrono::seconds data_loss;
        
        FailoverResult() : success(false), recovery_time(0), data_loss(0) {}
    };
    
    FailoverResult initiateFailover(const FailoverOptions& options) {
        FailoverResult result;
        
        // Validate target site
        auto target_opt = getSite(options.target_site);
        if (!target_opt) {
            result.error_message = "Target site not found: " + options.target_site;
            return result;
        }
        
        if (!target_opt->canAcceptFailover() && !options.force) {
            result.error_message = "Target site cannot accept failover";
            return result;
        }
        
        // Find current primary
        auto primary_opt = getPrimarySite();
        if (!primary_opt) {
            result.error_message = "No primary site found";
            return result;
        }
        
        // Create failover event
        FailoverEvent event;
        event.event_id = generateEventId();
        event.started = std::chrono::system_clock::now();
        event.source_site = primary_opt->site_id;
        event.target_site = options.target_site;
        event.type = options.type;
        event.status = FailoverEvent::Status::IN_PROGRESS;
        event.initiated_by = options.initiated_by;
        event.target_rpo = primary_opt->objectives.rpo;
        event.target_rto = primary_opt->objectives.rto;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            failover_events_[event.event_id] = event;
        }
        
        // Execute failover steps
        result = executeFailover(event, options);
        
        // Update event
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = failover_events_.find(event.event_id);
            if (it != failover_events_.end()) {
                it->second.completed = std::chrono::system_clock::now();
                it->second.status = result.success ? 
                    FailoverEvent::Status::COMPLETED : 
                    FailoverEvent::Status::FAILED;
                it->second.actual_recovery_time = result.recovery_time;
                it->second.actual_data_loss = result.data_loss;
                it->second.rpo_met = result.data_loss <= event.target_rpo;
                it->second.rto_met = result.recovery_time <= event.target_rto;
                
                if (!result.success) {
                    it->second.error_message = result.error_message;
                }
            }
        }
        
        // Notify callbacks
        if (failover_callback_) {
            auto event_copy = getFailoverEvent(event.event_id);
            if (event_copy) {
                failover_callback_(*event_copy);
            }
        }
        
        result.event_id = event.event_id;
        return result;
    }
    
    FailoverResult initiateFailback(const std::string& original_primary) {
        FailoverOptions options;
        options.target_site = original_primary;
        options.type = FailoverEvent::Type::FAILBACK;
        options.initiated_by = "system";
        return initiateFailover(options);
    }
    
    std::optional<FailoverEvent> getFailoverEvent(const std::string& event_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = failover_events_.find(event_id);
        if (it != failover_events_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<FailoverEvent> getFailoverHistory(size_t limit = 100) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<FailoverEvent> result;
        for (const auto& [id, event] : failover_events_) {
            result.push_back(event);
        }
        std::sort(result.begin(), result.end(),
            [](const FailoverEvent& a, const FailoverEvent& b) {
                return a.started > b.started;
            });
        if (result.size() > limit) {
            result.resize(limit);
        }
        return result;
    }
    
    // DR Testing
    struct DRTestResult {
        bool success;
        std::string test_id;
        std::chrono::system_clock::time_point started;
        std::chrono::system_clock::time_point completed;
        
        bool rpo_validated;
        bool rto_validated;
        bool data_integrity_ok;
        bool application_recovery_ok;
        
        std::vector<std::string> issues_found;
        std::string recommendations;
        
        DRTestResult() 
            : success(false)
            , rpo_validated(false)
            , rto_validated(false)
            , data_integrity_ok(false)
            , application_recovery_ok(false) {}
    };
    
    DRTestResult runDRTest(const std::string& target_site,
                            bool full_test = false) {
        DRTestResult result;
        result.test_id = "TEST_" + std::to_string(next_event_num_++);
        result.started = std::chrono::system_clock::now();
        
        // Validate site exists
        auto site_opt = getSite(target_site);
        if (!site_opt) {
            result.issues_found.push_back("Target site not found");
            result.completed = std::chrono::system_clock::now();
            return result;
        }
        
        // Check replication links
        auto links = getLinksForSite(target_site);
        for (const auto& link : links) {
            if (!link.isHealthy()) {
                result.issues_found.push_back(
                    "Replication link " + link.link_id + " is not healthy");
            }
        }
        
        // Check recovery points
        auto rp = getLatestRecoveryPoint(target_site);
        if (!rp) {
            result.issues_found.push_back("No recovery points available");
            result.rpo_validated = false;
        } else {
            auto age = std::chrono::system_clock::now() - rp->created;
            auto primary = getPrimarySite();
            if (primary && age <= primary->objectives.rpo) {
                result.rpo_validated = true;
            } else {
                result.issues_found.push_back("RPO not met - latest recovery point too old");
            }
        }
        
        // Simulate RTO test
        if (full_test && site_opt->canAcceptFailover()) {
            auto start = std::chrono::steady_clock::now();
            
            // Simulate failover validation
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            auto elapsed = std::chrono::steady_clock::now() - start;
            auto primary = getPrimarySite();
            if (primary) {
                auto rto = primary->objectives.rto;
                result.rto_validated = elapsed <= rto;
            }
        }
        
        // Data integrity check (simulated)
        result.data_integrity_ok = result.issues_found.empty();
        result.application_recovery_ok = result.data_integrity_ok;
        
        result.success = result.issues_found.empty();
        result.completed = std::chrono::system_clock::now();
        
        // Generate recommendations
        if (!result.success) {
            result.recommendations = "Address identified issues before DR event";
        } else {
            result.recommendations = "DR configuration validated successfully";
        }
        
        return result;
    }
    
    // Monitoring
    void startMonitoring(std::chrono::seconds interval = std::chrono::seconds(60)) {
        if (monitoring_active_) return;
        
        monitoring_active_ = true;
        monitoring_thread_ = std::thread([this, interval]() {
            while (monitoring_active_) {
                performHealthChecks();
                
                std::unique_lock<std::mutex> lock(monitor_mutex_);
                monitor_cv_.wait_for(lock, interval, [this]() {
                    return !monitoring_active_;
                });
            }
        });
    }
    
    void stopMonitoring() {
        if (!monitoring_active_) return;
        
        monitoring_active_ = false;
        monitor_cv_.notify_all();
        
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
    }
    
    void setFailoverCallback(FailoverCallback callback) {
        failover_callback_ = std::move(callback);
    }
    
    void setHealthCheckCallback(HealthCheckCallback callback) {
        health_check_callback_ = std::move(callback);
    }
    
    // Status Report
    struct DRStatusReport {
        bool healthy;
        std::string primary_site;
        std::vector<std::string> secondary_sites;
        
        size_t total_sites;
        size_t online_sites;
        size_t total_links;
        size_t healthy_links;
        
        std::chrono::milliseconds max_replication_lag;
        std::optional<RecoveryPoint> latest_recovery_point;
        
        std::vector<std::string> warnings;
        std::vector<std::string> critical_issues;
    };
    
    DRStatusReport getStatusReport() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        DRStatusReport report;
        report.healthy = true;
        report.total_sites = sites_.size();
        report.online_sites = 0;
        report.total_links = replication_links_.size();
        report.healthy_links = 0;
        report.max_replication_lag = std::chrono::milliseconds(0);
        
        for (const auto& [id, site] : sites_) {
            if (site.status == SiteStatus::ONLINE) {
                report.online_sites++;
            }
            if (site.role == SiteRole::PRIMARY) {
                report.primary_site = id;
            } else if (site.role == SiteRole::SECONDARY) {
                report.secondary_sites.push_back(id);
            }
            
            if (site.status == SiteStatus::OFFLINE) {
                report.warnings.push_back("Site " + id + " is offline");
            }
        }
        
        for (const auto& [id, link] : replication_links_) {
            if (link.isHealthy()) {
                report.healthy_links++;
            } else {
                report.warnings.push_back("Replication link " + id + " is not healthy");
            }
            
            if (link.replication_lag > report.max_replication_lag) {
                report.max_replication_lag = link.replication_lag;
            }
        }
        
        // Check for critical issues
        if (report.primary_site.empty()) {
            report.critical_issues.push_back("No primary site configured");
            report.healthy = false;
        }
        
        if (report.secondary_sites.empty()) {
            report.critical_issues.push_back("No secondary sites configured");
            report.healthy = false;
        }
        
        if (report.healthy_links == 0 && report.total_links > 0) {
            report.critical_issues.push_back("No healthy replication links");
            report.healthy = false;
        }
        
        if (!report.warnings.empty() || !report.critical_issues.empty()) {
            report.healthy = false;
        }
        
        return report;
    }
    
private:
    std::string generatePointId(const std::string& site_id) {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_buf);
#endif
        
        std::ostringstream oss;
        oss << "RP_" << site_id << "_"
            << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
        return oss.str();
    }
    
    std::string generateEventId() {
        std::ostringstream oss;
        oss << "FO_" << std::setfill('0') << std::setw(6) << (next_event_num_++);
        return oss.str();
    }
    
    FailoverResult executeFailover(FailoverEvent& event, 
                                    const FailoverOptions& options) {
        FailoverResult result;
        auto start_time = std::chrono::steady_clock::now();
        
        // Step 1: Create recovery point if requested
        if (options.create_recovery_point) {
            event.steps_completed.push_back("Creating recovery point");
            auto rp_id = createRecoveryPoint(event.source_site, 
                                              "Pre-failover recovery point",
                                              RecoveryPoint::Type::FAILOVER);
            event.recovery_point_id = rp_id;
        }
        
        // Step 2: Pause replication
        event.steps_completed.push_back("Pausing replication");
        for (auto& [id, link] : replication_links_) {
            if (link.source_site == event.source_site) {
                link.state = ReplicationState::PAUSED;
            }
        }
        
        // Step 3: Calculate data loss
        auto links = getLinksForSite(event.target_site);
        std::chrono::milliseconds max_lag(0);
        for (const auto& link : links) {
            if (link.replication_lag > max_lag) {
                max_lag = link.replication_lag;
            }
        }
        result.data_loss = std::chrono::duration_cast<std::chrono::seconds>(max_lag);
        
        // Step 4: Switch roles
        event.steps_completed.push_back("Switching site roles");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Demote old primary
            auto old_primary = sites_.find(event.source_site);
            if (old_primary != sites_.end()) {
                old_primary->second.role = SiteRole::SECONDARY;
                old_primary->second.status = SiteStatus::DEGRADED;
                old_primary->second.role_changed = std::chrono::system_clock::now();
            }
            
            // Promote new primary
            auto new_primary = sites_.find(event.target_site);
            if (new_primary != sites_.end()) {
                new_primary->second.role = SiteRole::PRIMARY;
                new_primary->second.status = SiteStatus::FAILED_OVER;
                new_primary->second.role_changed = std::chrono::system_clock::now();
                new_primary->second.failover_count++;
                new_primary->second.last_failover = std::chrono::system_clock::now();
            }
        }
        
        // Step 5: Resume replication (reversed direction)
        event.steps_completed.push_back("Resuming replication");
        for (auto& [id, link] : replication_links_) {
            if (link.target_site == event.source_site) {
                link.state = ReplicationState::ACTIVE;
            }
        }
        
        event.steps_completed.push_back("Failover complete");
        
        auto end_time = std::chrono::steady_clock::now();
        result.recovery_time = std::chrono::duration_cast<std::chrono::seconds>(
            end_time - start_time);
        
        result.success = true;
        return result;
    }
    
    void performHealthChecks() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        
        for (auto& [id, site] : sites_) {
            // Check heartbeat timeout
            auto since_heartbeat = now - site.last_heartbeat;
            if (since_heartbeat > std::chrono::minutes(5)) {
                if (site.status == SiteStatus::ONLINE) {
                    site.status = SiteStatus::DEGRADED;
                }
            }
            
            // Run custom health check if configured
            if (health_check_callback_) {
                bool healthy = health_check_callback_(id);
                if (!healthy && site.status == SiteStatus::ONLINE) {
                    site.status = SiteStatus::DEGRADED;
                }
            }
        }
    }
    
    std::map<std::string, Site> sites_;
    std::map<std::string, ReplicationLink> replication_links_;
    std::map<std::string, RecoveryPoint> recovery_points_;
    std::map<std::string, FailoverEvent> failover_events_;
    
    std::atomic<bool> monitoring_active_;
    std::atomic<uint32_t> next_event_num_;
    
    std::thread monitoring_thread_;
    std::mutex monitor_mutex_;
    std::condition_variable monitor_cv_;
    
    FailoverCallback failover_callback_;
    HealthCheckCallback health_check_callback_;
    
    mutable std::mutex mutex_;
};

} // namespace dr
} // namespace dfsmshsm

#endif // HSM_DISASTER_RECOVERY_HPP
