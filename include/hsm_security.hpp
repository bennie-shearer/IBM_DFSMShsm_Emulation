/*
 * DFSMShsm Emulator - RACF/SAF Integration
 * @version 4.2.0
 * 
 * Emulates z/OS security integration for HSM:
 * - SAF (System Authorization Facility) interface
 * - RACF resource checking
 * - STGADMIN class profiles
 * - FACILITY class authorization
 * - Dataset access verification
 * - User/group management
 * - Audit logging
 */

#ifndef HSM_SECURITY_HPP
#define HSM_SECURITY_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <functional>
#include <optional>
#include <bitset>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <atomic>

namespace hsm {
namespace security {

// =============================================================================
// Security Enumerations
// =============================================================================

// Access levels (matching z/OS RACF)
enum class AccessLevel : uint8_t {
    NONE    = 0x00,
    EXECUTE = 0x01,
    READ    = 0x02,
    UPDATE  = 0x04,
    CONTROL = 0x08,
    ALTER   = 0x10
};

inline AccessLevel operator|(AccessLevel a, AccessLevel b) {
    return static_cast<AccessLevel>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline AccessLevel operator&(AccessLevel a, AccessLevel b) {
    return static_cast<AccessLevel>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool hasAccess(AccessLevel granted, AccessLevel required) {
    return (static_cast<uint8_t>(granted) & static_cast<uint8_t>(required)) == 
           static_cast<uint8_t>(required);
}

// SAF return codes
enum class SAFReturnCode : int {
    SUCCESS = 0,           // Access granted
    NOT_DEFINED = 4,       // Resource not defined, default access
    NOT_AUTHORIZED = 8,    // Access denied
    UNDEFINED_USER = 12,   // User not defined
    INVALID_REQUEST = 16,  // Invalid request
    SERVICE_ERROR = 20     // Service error
};

// Resource classes (RACF classes)
enum class ResourceClass {
    DATASET,       // Dataset resources
    FACILITY,      // Facility resources
    STGADMIN,      // Storage admin resources
    PROGRAM,       // Program resources
    STARTED,       // Started task resources
    CONSOLE,       // Console resources
    TAPEVOL,       // Tape volume resources
    DASDVOL,       // DASD volume resources
    USER,          // User resources
    GROUP,         // Group resources
    CUSTOM         // Custom class
};

// HSM-specific operations
enum class HSMOperation {
    MIGRATE,
    RECALL,
    BACKUP,
    RECOVER,
    DELETE,
    LIST,
    QUERY,
    HOLD,
    RELEASE,
    AUDIT,
    RECYCLE,
    ADDVOL,
    DELVOL,
    SETSYS,
    AUTH,          // Authorization management
    ADMIN          // Administration
};

// Audit event types
enum class AuditEvent {
    SUCCESS,           // Successful access
    FAILURE,           // Failed access
    VIOLATION,         // Security violation
    WARNING,           // Warning condition
    ADMIN_ACTION,      // Administrative action
    PROFILE_CHANGE,    // Profile modification
    PASSWORD_CHANGE,   // Password change
    LOGON,             // User logon
    LOGOFF             // User logoff
};

// =============================================================================
// User and Group Definitions
// =============================================================================

struct UserProfile {
    std::string userId;
    std::string defaultGroup;
    std::vector<std::string> connectGroups;
    
    std::string name;
    std::string department;
    std::string owner;
    
    bool isSpecial{false};          // SPECIAL authority
    bool isOperations{false};       // OPERATIONS authority
    bool isAuditor{false};          // AUDITOR authority
    bool isRevoked{false};          // Account revoked
    bool isProtected{false};        // Protected user
    
    std::chrono::system_clock::time_point createdTime;
    std::chrono::system_clock::time_point lastAccessTime;
    std::chrono::system_clock::time_point passwordLastChanged;
    
    int passwordInterval{90};        // Days between password changes
    int invalidPasswordCount{0};
    int maxInvalidAttempts{3};
    
    std::map<std::string, std::string> attributes;
    
    bool isValid() const {
        return !userId.empty() && !isRevoked;
    }
    
    bool hasAuthority(const std::string& auth) const {
        if (auth == "SPECIAL") return isSpecial;
        if (auth == "OPERATIONS") return isOperations;
        if (auth == "AUDITOR") return isAuditor;
        return false;
    }
};

struct GroupProfile {
    std::string groupId;
    std::string owner;
    std::string superiorGroup;
    
    std::vector<std::string> subGroups;
    std::vector<std::string> members;
    
    bool isUniversal{false};
    
    std::chrono::system_clock::time_point createdTime;
    
    std::map<std::string, std::string> attributes;
};

// =============================================================================
// Resource Profiles
// =============================================================================

struct ResourceProfile {
    std::string profileName;
    ResourceClass resourceClass{ResourceClass::DATASET};
    std::string owner;
    
    bool isGeneric{false};          // Generic profile (with wildcards)
    bool isDiscrete{false};          // Discrete profile
    bool isWarningMode{false};       // Warning mode (log but allow)
    bool isAuditAll{false};          // Audit all access
    bool isAuditFailures{false};     // Audit failures only
    
    AccessLevel universalAccess{AccessLevel::NONE};  // UACC
    
    std::chrono::system_clock::time_point createdTime;
    std::chrono::system_clock::time_point lastReferenced;
    std::chrono::system_clock::time_point lastUpdated;
    
    // Access list
    struct AccessEntry {
        std::string identity;        // User or group
        bool isGroup{false};
        AccessLevel access{AccessLevel::NONE};
    };
    std::vector<AccessEntry> accessList;
    
    // Conditional access
    struct ConditionalAccess {
        std::string condition;       // Time, terminal, etc.
        AccessLevel access{AccessLevel::NONE};
        std::string startTime;
        std::string endTime;
        std::vector<std::string> terminals;
    };
    std::vector<ConditionalAccess> conditionalAccess;
    
    std::map<std::string, std::string> segments;  // RACF segments
};

// =============================================================================
// STGADMIN Profiles for HSM
// =============================================================================

struct STGADMINProfile {
    std::string profileName;
    
    // Common STGADMIN profiles
    // ADM.HSMARC - ARC command authorization
    // ADM.HMIG.dsname - Dataset migration
    // ADM.HREC.dsname - Dataset recall
    // ADM.HBAK.dsname - Dataset backup
    // ADM.HREC.dsname - Dataset recover
    // ADM.HDEL.dsname - Delete migrated/backup
    
    enum class ProfileType {
        HMIG,           // Migration
        HREC,           // Recall
        HBAK,           // Backup
        HRCV,           // Recover
        HDEL,           // Delete
        HSMARC,         // ARC commands
        HSMAUTH,        // HSM authorization
        HSMAUDIT,       // HSM audit
        HSMADMIN        // HSM administration
    };
    
    ProfileType type{ProfileType::HMIG};
    std::string datasetPattern;      // Dataset pattern for authorization
    
    AccessLevel universalAccess{AccessLevel::NONE};
    std::vector<ResourceProfile::AccessEntry> accessList;
    
    bool isActive{true};
    std::chrono::system_clock::time_point createdTime;
};

// =============================================================================
// Security Check Request/Response
// =============================================================================

struct SecurityRequest {
    std::string userId;
    ResourceClass resourceClass{ResourceClass::DATASET};
    std::string resourceName;
    AccessLevel requestedAccess{AccessLevel::READ};
    HSMOperation operation{HSMOperation::MIGRATE};
    
    std::string terminal;            // Terminal ID
    std::string jobName;             // Job name
    std::string programName;         // Program name
    
    std::chrono::system_clock::time_point requestTime;
    
    std::map<std::string, std::string> context;
};

struct SecurityResponse {
    SAFReturnCode returnCode{SAFReturnCode::SUCCESS};
    AccessLevel grantedAccess{AccessLevel::NONE};
    
    std::string message;
    std::string profileUsed;         // Profile that matched
    bool isWarningMode{false};       // Would have denied but warning mode
    
    std::chrono::system_clock::time_point responseTime;
    std::chrono::microseconds processingTime{0};
};

// =============================================================================
// Audit Record
// =============================================================================

struct AuditRecord {
    uint64_t recordId{0};
    AuditEvent eventType{AuditEvent::SUCCESS};
    
    std::string userId;
    std::string resourceName;
    ResourceClass resourceClass{ResourceClass::DATASET};
    HSMOperation operation{HSMOperation::MIGRATE};
    
    AccessLevel requestedAccess{AccessLevel::NONE};
    AccessLevel grantedAccess{AccessLevel::NONE};
    SAFReturnCode result{SAFReturnCode::SUCCESS};
    
    std::string terminal;
    std::string jobName;
    std::string systemId;
    
    std::chrono::system_clock::time_point timestamp;
    
    std::string message;
    std::map<std::string, std::string> details;
    
    std::string toString() const {
        std::ostringstream oss;
        auto time_t_val = std::chrono::system_clock::to_time_t(timestamp);
        oss << std::ctime(&time_t_val);
        oss << " " << userId << " " << resourceName;
        oss << " " << static_cast<int>(result);
        oss << " " << message;
        return oss.str();
    }
};

// =============================================================================
// Security Manager
// =============================================================================

class SecurityManager {
public:
    struct Config {
        bool enabled{true};
        bool auditEnabled{true};
        bool warningMode{false};         // Global warning mode
        bool cacheProfiles{true};
        size_t cacheSize{10000};
        std::chrono::seconds cacheExpiry{300};
        
        AccessLevel defaultAccess{AccessLevel::NONE};
        bool failOpen{false};             // Allow if check fails
        
        std::string systemId{"SYSTEM"};
        
        Config() = default;
    };
    
    using AuditCallback = std::function<void(const AuditRecord&)>;
    
private:
    Config config_;
    mutable std::shared_mutex mutex_;
    
    // User/Group databases
    std::unordered_map<std::string, UserProfile> users_;
    std::unordered_map<std::string, GroupProfile> groups_;
    
    // Resource profiles by class
    std::map<ResourceClass, std::map<std::string, ResourceProfile>> profiles_;
    
    // STGADMIN profiles
    std::map<std::string, STGADMINProfile> stgadminProfiles_;
    
    // Cache
    struct CacheEntry {
        SecurityResponse response;
        std::chrono::system_clock::time_point timestamp;
    };
    mutable std::unordered_map<std::string, CacheEntry> cache_;
    
    // Audit
    std::vector<AuditCallback> auditCallbacks_;
    std::vector<AuditRecord> auditLog_;
    std::atomic<uint64_t> nextAuditId_{1};
    
    // Statistics
    struct Statistics {
        uint64_t totalChecks{0};
        uint64_t successChecks{0};
        uint64_t failedChecks{0};
        uint64_t cacheHits{0};
        uint64_t cacheMisses{0};
        uint64_t violationCount{0};
    } stats_;
    
public:
    SecurityManager() : config_() {}
    explicit SecurityManager(const Config& config) : config_(config) {}
    
    // =========================================================================
    // User Management
    // =========================================================================
    
    bool addUser(const UserProfile& user) {
        std::unique_lock lock(mutex_);
        if (users_.count(user.userId)) {
            return false;
        }
        users_[user.userId] = user;
        
        // Add to default group
        if (!user.defaultGroup.empty() && groups_.count(user.defaultGroup)) {
            groups_[user.defaultGroup].members.push_back(user.userId);
        }
        
        auditAction(AuditEvent::ADMIN_ACTION, "SYSTEM", 
                   "USER." + user.userId, "User added");
        return true;
    }
    
    bool updateUser(const UserProfile& user) {
        std::unique_lock lock(mutex_);
        auto it = users_.find(user.userId);
        if (it == users_.end()) {
            return false;
        }
        it->second = user;
        invalidateCache(user.userId);
        
        auditAction(AuditEvent::PROFILE_CHANGE, "SYSTEM",
                   "USER." + user.userId, "User updated");
        return true;
    }
    
    bool deleteUser(const std::string& userId) {
        std::unique_lock lock(mutex_);
        auto it = users_.find(userId);
        if (it == users_.end()) {
            return false;
        }
        
        // Remove from groups
        for (auto& [groupId, group] : groups_) {
            auto mit = std::find(group.members.begin(), group.members.end(), userId);
            if (mit != group.members.end()) {
                group.members.erase(mit);
            }
        }
        
        users_.erase(it);
        invalidateCache(userId);
        
        auditAction(AuditEvent::ADMIN_ACTION, "SYSTEM",
                   "USER." + userId, "User deleted");
        return true;
    }
    
    std::optional<UserProfile> getUser(const std::string& userId) const {
        std::shared_lock lock(mutex_);
        auto it = users_.find(userId);
        if (it != users_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    bool revokeUser(const std::string& userId) {
        std::unique_lock lock(mutex_);
        auto it = users_.find(userId);
        if (it == users_.end()) {
            return false;
        }
        it->second.isRevoked = true;
        invalidateCache(userId);
        
        auditAction(AuditEvent::ADMIN_ACTION, "SYSTEM",
                   "USER." + userId, "User revoked");
        return true;
    }
    
    bool resumeUser(const std::string& userId) {
        std::unique_lock lock(mutex_);
        auto it = users_.find(userId);
        if (it == users_.end()) {
            return false;
        }
        it->second.isRevoked = false;
        it->second.invalidPasswordCount = 0;
        
        auditAction(AuditEvent::ADMIN_ACTION, "SYSTEM",
                   "USER." + userId, "User resumed");
        return true;
    }
    
    // =========================================================================
    // Group Management
    // =========================================================================
    
    bool addGroup(const GroupProfile& group) {
        std::unique_lock lock(mutex_);
        if (groups_.count(group.groupId)) {
            return false;
        }
        groups_[group.groupId] = group;
        
        auditAction(AuditEvent::ADMIN_ACTION, "SYSTEM",
                   "GROUP." + group.groupId, "Group added");
        return true;
    }
    
    bool connectUserToGroup(const std::string& userId, const std::string& groupId) {
        std::unique_lock lock(mutex_);
        
        auto uit = users_.find(userId);
        auto git = groups_.find(groupId);
        
        if (uit == users_.end() || git == groups_.end()) {
            return false;
        }
        
        // Add user to group members
        if (std::find(git->second.members.begin(), git->second.members.end(), userId) 
            == git->second.members.end()) {
            git->second.members.push_back(userId);
        }
        
        // Add group to user's connect groups
        if (std::find(uit->second.connectGroups.begin(), 
                     uit->second.connectGroups.end(), groupId) 
            == uit->second.connectGroups.end()) {
            uit->second.connectGroups.push_back(groupId);
        }
        
        invalidateCache(userId);
        return true;
    }
    
    std::optional<GroupProfile> getGroup(const std::string& groupId) const {
        std::shared_lock lock(mutex_);
        auto it = groups_.find(groupId);
        if (it != groups_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<std::string> getUserGroups(const std::string& userId) const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> result;
        
        auto it = users_.find(userId);
        if (it != users_.end()) {
            result.push_back(it->second.defaultGroup);
            for (const auto& g : it->second.connectGroups) {
                result.push_back(g);
            }
        }
        return result;
    }
    
    // =========================================================================
    // Profile Management
    // =========================================================================
    
    bool addProfile(const ResourceProfile& profile) {
        std::unique_lock lock(mutex_);
        profiles_[profile.resourceClass][profile.profileName] = profile;
        
        auditAction(AuditEvent::PROFILE_CHANGE, "SYSTEM",
                   profile.profileName, "Profile added");
        return true;
    }
    
    bool deleteProfile(ResourceClass resourceClass, const std::string& profileName) {
        std::unique_lock lock(mutex_);
        auto& classProfiles = profiles_[resourceClass];
        auto it = classProfiles.find(profileName);
        if (it == classProfiles.end()) {
            return false;
        }
        classProfiles.erase(it);
        
        auditAction(AuditEvent::PROFILE_CHANGE, "SYSTEM",
                   profileName, "Profile deleted");
        return true;
    }
    
    bool permitAccess(ResourceClass resourceClass, const std::string& profileName,
                     const std::string& identity, AccessLevel access, bool isGroup = false) {
        std::unique_lock lock(mutex_);
        
        auto& classProfiles = profiles_[resourceClass];
        auto it = classProfiles.find(profileName);
        if (it == classProfiles.end()) {
            return false;
        }
        
        // Check if already in list
        for (auto& entry : it->second.accessList) {
            if (entry.identity == identity) {
                entry.access = access;
                return true;
            }
        }
        
        // Add new entry
        ResourceProfile::AccessEntry entry;
        entry.identity = identity;
        entry.isGroup = isGroup;
        entry.access = access;
        it->second.accessList.push_back(entry);
        
        return true;
    }
    
    std::optional<ResourceProfile> getProfile(ResourceClass resourceClass,
                                              const std::string& profileName) const {
        std::shared_lock lock(mutex_);
        auto cit = profiles_.find(resourceClass);
        if (cit == profiles_.end()) {
            return std::nullopt;
        }
        auto pit = cit->second.find(profileName);
        if (pit != cit->second.end()) {
            return pit->second;
        }
        return std::nullopt;
    }
    
    // =========================================================================
    // STGADMIN Profile Management
    // =========================================================================
    
    bool addSTGADMINProfile(const STGADMINProfile& profile) {
        std::unique_lock lock(mutex_);
        stgadminProfiles_[profile.profileName] = profile;
        return true;
    }
    
    std::optional<STGADMINProfile> getSTGADMINProfile(const std::string& name) const {
        std::shared_lock lock(mutex_);
        auto it = stgadminProfiles_.find(name);
        if (it != stgadminProfiles_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    // =========================================================================
    // Security Checking (SAF Interface)
    // =========================================================================
    
    SecurityResponse checkAccess(const SecurityRequest& request) {
        auto start = std::chrono::steady_clock::now();
        
        SecurityResponse response;
        response.responseTime = std::chrono::system_clock::now();
        
        if (!config_.enabled) {
            response.returnCode = SAFReturnCode::SUCCESS;
            response.grantedAccess = request.requestedAccess;
            response.message = "Security disabled";
            return response;
        }
        
        // Check cache
        std::string cacheKey = makeCacheKey(request);
        if (config_.cacheProfiles) {
            std::shared_lock lock(mutex_);
            auto cit = cache_.find(cacheKey);
            if (cit != cache_.end()) {
                auto age = std::chrono::system_clock::now() - cit->second.timestamp;
                if (age < config_.cacheExpiry) {
                    stats_.cacheHits++;
                    return cit->second.response;
                }
            }
            stats_.cacheMisses++;
        }
        
        // Perform check
        {
            std::shared_lock lock(mutex_);
            response = performAccessCheck(request);
        }
        
        auto end = std::chrono::steady_clock::now();
        response.processingTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        response.responseTime = std::chrono::system_clock::now();
        
        // Update cache
        if (config_.cacheProfiles) {
            std::unique_lock lock(mutex_);
            cache_[cacheKey] = {response, std::chrono::system_clock::now()};
            
            // Limit cache size
            while (cache_.size() > config_.cacheSize) {
                cache_.erase(cache_.begin());
            }
        }
        
        // Update statistics
        stats_.totalChecks++;
        if (response.returnCode == SAFReturnCode::SUCCESS) {
            stats_.successChecks++;
        } else if (response.returnCode == SAFReturnCode::NOT_AUTHORIZED) {
            stats_.failedChecks++;
        }
        
        // Audit
        if (config_.auditEnabled) {
            AuditRecord record;
            record.recordId = nextAuditId_++;
            record.eventType = (response.returnCode == SAFReturnCode::SUCCESS) ?
                              AuditEvent::SUCCESS : AuditEvent::FAILURE;
            record.userId = request.userId;
            record.resourceName = request.resourceName;
            record.resourceClass = request.resourceClass;
            record.operation = request.operation;
            record.requestedAccess = request.requestedAccess;
            record.grantedAccess = response.grantedAccess;
            record.result = response.returnCode;
            record.terminal = request.terminal;
            record.jobName = request.jobName;
            record.systemId = config_.systemId;
            record.timestamp = std::chrono::system_clock::now();
            record.message = response.message;
            
            recordAudit(record);
        }
        
        return response;
    }
    
    // Convenience methods
    SecurityResponse checkDatasetAccess(const std::string& userId,
                                        const std::string& datasetName,
                                        AccessLevel access) {
        SecurityRequest request;
        request.userId = userId;
        request.resourceClass = ResourceClass::DATASET;
        request.resourceName = datasetName;
        request.requestedAccess = access;
        request.requestTime = std::chrono::system_clock::now();
        return checkAccess(request);
    }
    
    SecurityResponse checkHSMOperation(const std::string& userId,
                                       HSMOperation operation,
                                       const std::string& datasetName = "") {
        SecurityRequest request;
        request.userId = userId;
        request.resourceClass = ResourceClass::STGADMIN;
        request.operation = operation;
        request.requestTime = std::chrono::system_clock::now();
        
        // Build STGADMIN resource name
        switch (operation) {
            case HSMOperation::MIGRATE:
                request.resourceName = "ADM.HMIG." + datasetName;
                break;
            case HSMOperation::RECALL:
                request.resourceName = "ADM.HREC." + datasetName;
                break;
            case HSMOperation::BACKUP:
                request.resourceName = "ADM.HBAK." + datasetName;
                break;
            case HSMOperation::RECOVER:
                request.resourceName = "ADM.HRCV." + datasetName;
                break;
            case HSMOperation::DELETE:
                request.resourceName = "ADM.HDEL." + datasetName;
                break;
            case HSMOperation::ADMIN:
                request.resourceName = "ADM.HSMADMIN";
                break;
            default:
                request.resourceName = "ADM.HSMARC";
        }
        
        request.requestedAccess = AccessLevel::UPDATE;
        return checkAccess(request);
    }
    
    SecurityResponse checkFacility(const std::string& userId,
                                   const std::string& facilityName) {
        SecurityRequest request;
        request.userId = userId;
        request.resourceClass = ResourceClass::FACILITY;
        request.resourceName = facilityName;
        request.requestedAccess = AccessLevel::READ;
        request.requestTime = std::chrono::system_clock::now();
        return checkAccess(request);
    }
    
    // =========================================================================
    // Audit Management
    // =========================================================================
    
    void addAuditCallback(AuditCallback callback) {
        std::unique_lock lock(mutex_);
        auditCallbacks_.push_back(std::move(callback));
    }
    
    std::vector<AuditRecord> getAuditLog(size_t maxRecords = 1000) const {
        std::shared_lock lock(mutex_);
        
        size_t start = 0;
        if (auditLog_.size() > maxRecords) {
            start = auditLog_.size() - maxRecords;
        }
        
        return std::vector<AuditRecord>(auditLog_.begin() + start, auditLog_.end());
    }
    
    std::vector<AuditRecord> getAuditLogForUser(const std::string& userId,
                                                 size_t maxRecords = 100) const {
        std::shared_lock lock(mutex_);
        
        std::vector<AuditRecord> result;
        for (auto it = auditLog_.rbegin(); it != auditLog_.rend() && 
             result.size() < maxRecords; ++it) {
            if (it->userId == userId) {
                result.push_back(*it);
            }
        }
        return result;
    }
    
    std::vector<AuditRecord> getViolations(size_t maxRecords = 100) const {
        std::shared_lock lock(mutex_);
        
        std::vector<AuditRecord> result;
        for (auto it = auditLog_.rbegin(); it != auditLog_.rend() && 
             result.size() < maxRecords; ++it) {
            if (it->eventType == AuditEvent::VIOLATION ||
                it->eventType == AuditEvent::FAILURE) {
                result.push_back(*it);
            }
        }
        return result;
    }
    
    void clearAuditLog() {
        std::unique_lock lock(mutex_);
        auditLog_.clear();
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
    // Cache Management
    // =========================================================================
    
    void clearCache() {
        std::unique_lock lock(mutex_);
        cache_.clear();
    }
    
    size_t getCacheSize() const {
        std::shared_lock lock(mutex_);
        return cache_.size();
    }
    
private:
    SecurityResponse performAccessCheck(const SecurityRequest& request) {
        SecurityResponse response;
        
        // 1. Check if user exists and is valid
        auto userIt = users_.find(request.userId);
        if (userIt == users_.end()) {
            response.returnCode = SAFReturnCode::UNDEFINED_USER;
            response.message = "User not defined: " + request.userId;
            return response;
        }
        
        const UserProfile& user = userIt->second;
        if (!user.isValid()) {
            response.returnCode = SAFReturnCode::NOT_AUTHORIZED;
            response.message = "User revoked: " + request.userId;
            return response;
        }
        
        // 2. Check for special authorities
        if (user.isSpecial) {
            response.returnCode = SAFReturnCode::SUCCESS;
            response.grantedAccess = AccessLevel::ALTER;
            response.message = "SPECIAL authority";
            return response;
        }
        
        if (user.isOperations && 
            (request.resourceClass == ResourceClass::DATASET ||
             request.resourceClass == ResourceClass::DASDVOL)) {
            response.returnCode = SAFReturnCode::SUCCESS;
            response.grantedAccess = AccessLevel::CONTROL;
            response.message = "OPERATIONS authority";
            return response;
        }
        
        // 3. Find matching profile
        auto profileOpt = findMatchingProfile(request.resourceClass, request.resourceName);
        if (!profileOpt) {
            // No profile found - use default
            if (config_.failOpen) {
                response.returnCode = SAFReturnCode::NOT_DEFINED;
                response.grantedAccess = config_.defaultAccess;
                response.message = "No profile, default access";
            } else {
                response.returnCode = SAFReturnCode::NOT_AUTHORIZED;
                response.message = "No profile defined";
            }
            return response;
        }
        
        const ResourceProfile& profile = *profileOpt;
        response.profileUsed = profile.profileName;
        
        // 4. Check UACC
        if (hasAccess(profile.universalAccess, request.requestedAccess)) {
            response.returnCode = SAFReturnCode::SUCCESS;
            response.grantedAccess = profile.universalAccess;
            response.message = "UACC access";
            return response;
        }
        
        // 5. Check access list
        AccessLevel maxAccess = AccessLevel::NONE;
        
        // Check user directly
        for (const auto& entry : profile.accessList) {
            if (!entry.isGroup && entry.identity == request.userId) {
                if (hasAccess(entry.access, request.requestedAccess)) {
                    response.returnCode = SAFReturnCode::SUCCESS;
                    response.grantedAccess = entry.access;
                    response.message = "User access list";
                    return response;
                }
                maxAccess = entry.access;
            }
        }
        
        // Check groups
        auto userGroups = getUserGroupsInternal(request.userId);
        for (const auto& entry : profile.accessList) {
            if (entry.isGroup) {
                for (const auto& group : userGroups) {
                    if (entry.identity == group) {
                        if (hasAccess(entry.access, request.requestedAccess)) {
                            response.returnCode = SAFReturnCode::SUCCESS;
                            response.grantedAccess = entry.access;
                            response.message = "Group access: " + group;
                            return response;
                        }
                        if (static_cast<uint8_t>(entry.access) > 
                            static_cast<uint8_t>(maxAccess)) {
                            maxAccess = entry.access;
                        }
                    }
                }
            }
        }
        
        // 6. Check warning mode
        if (profile.isWarningMode || config_.warningMode) {
            response.returnCode = SAFReturnCode::SUCCESS;
            response.grantedAccess = request.requestedAccess;
            response.isWarningMode = true;
            response.message = "WARNING MODE - would have denied";
            stats_.violationCount++;
            return response;
        }
        
        // 7. Access denied
        response.returnCode = SAFReturnCode::NOT_AUTHORIZED;
        response.grantedAccess = maxAccess;
        response.message = "Insufficient access";
        stats_.violationCount++;
        
        return response;
    }
    
    std::optional<ResourceProfile> findMatchingProfile(ResourceClass resourceClass,
                                                       const std::string& resourceName) const {
        auto cit = profiles_.find(resourceClass);
        if (cit == profiles_.end()) {
            return std::nullopt;
        }
        
        const auto& classProfiles = cit->second;
        
        // Try discrete profile first
        auto pit = classProfiles.find(resourceName);
        if (pit != classProfiles.end()) {
            return pit->second;
        }
        
        // Try generic profiles
        std::string bestMatch;
        size_t bestMatchLen = 0;
        
        for (const auto& [name, profile] : classProfiles) {
            if (profile.isGeneric && matchGenericProfile(resourceName, name)) {
                if (name.size() > bestMatchLen) {
                    bestMatch = name;
                    bestMatchLen = name.size();
                }
            }
        }
        
        if (!bestMatch.empty()) {
            return classProfiles.at(bestMatch);
        }
        
        return std::nullopt;
    }
    
    bool matchGenericProfile(const std::string& resource, const std::string& profile) const {
        // Simple wildcard matching for * and %
        size_t ri = 0, pi = 0;
        
        while (ri < resource.size() && pi < profile.size()) {
            if (profile[pi] == '*') {
                // Match any sequence
                if (pi + 1 >= profile.size()) {
                    return true;
                }
                
                // Find next non-wildcard
                size_t nextPi = pi + 1;
                while (nextPi < profile.size() && 
                       (profile[nextPi] == '*' || profile[nextPi] == '%')) {
                    nextPi++;
                }
                
                if (nextPi >= profile.size()) {
                    return true;
                }
                
                // Search for match
                while (ri < resource.size()) {
                    if (std::toupper(resource[ri]) == std::toupper(profile[nextPi])) {
                        break;
                    }
                    ri++;
                }
                pi = nextPi;
            } else if (profile[pi] == '%') {
                // Match single character
                ri++;
                pi++;
            } else if (std::toupper(resource[ri]) == std::toupper(profile[pi])) {
                ri++;
                pi++;
            } else {
                return false;
            }
        }
        
        // Skip trailing wildcards
        while (pi < profile.size() && profile[pi] == '*') {
            pi++;
        }
        
        return ri == resource.size() && pi == profile.size();
    }
    
    std::vector<std::string> getUserGroupsInternal(const std::string& userId) const {
        std::vector<std::string> result;
        auto it = users_.find(userId);
        if (it != users_.end()) {
            result.push_back(it->second.defaultGroup);
            for (const auto& g : it->second.connectGroups) {
                result.push_back(g);
            }
        }
        return result;
    }
    
    std::string makeCacheKey(const SecurityRequest& request) const {
        return request.userId + "|" + 
               std::to_string(static_cast<int>(request.resourceClass)) + "|" +
               request.resourceName + "|" +
               std::to_string(static_cast<int>(request.requestedAccess));
    }
    
    void invalidateCache(const std::string& userId) {
        std::vector<std::string> keysToRemove;
        for (const auto& [key, entry] : cache_) {
            if (key.find(userId + "|") == 0) {
                keysToRemove.push_back(key);
            }
        }
        for (const auto& key : keysToRemove) {
            cache_.erase(key);
        }
    }
    
    void recordAudit(const AuditRecord& record) {
        auditLog_.push_back(record);
        
        // Limit log size
        while (auditLog_.size() > 100000) {
            auditLog_.erase(auditLog_.begin());
        }
        
        // Notify callbacks
        for (const auto& callback : auditCallbacks_) {
            callback(record);
        }
    }
    
    void auditAction(AuditEvent eventType, const std::string& userId,
                    const std::string& resource, const std::string& message) {
        if (!config_.auditEnabled) return;
        
        AuditRecord record;
        record.recordId = nextAuditId_++;
        record.eventType = eventType;
        record.userId = userId;
        record.resourceName = resource;
        record.timestamp = std::chrono::system_clock::now();
        record.message = message;
        record.systemId = config_.systemId;
        
        recordAudit(record);
    }
};

// =============================================================================
// Utility Functions
// =============================================================================

inline std::string accessLevelToString(AccessLevel level) {
    switch (level) {
        case AccessLevel::NONE: return "NONE";
        case AccessLevel::EXECUTE: return "EXECUTE";
        case AccessLevel::READ: return "READ";
        case AccessLevel::UPDATE: return "UPDATE";
        case AccessLevel::CONTROL: return "CONTROL";
        case AccessLevel::ALTER: return "ALTER";
        default: return "UNKNOWN";
    }
}

inline std::string safReturnCodeToString(SAFReturnCode code) {
    switch (code) {
        case SAFReturnCode::SUCCESS: return "SUCCESS";
        case SAFReturnCode::NOT_DEFINED: return "NOT_DEFINED";
        case SAFReturnCode::NOT_AUTHORIZED: return "NOT_AUTHORIZED";
        case SAFReturnCode::UNDEFINED_USER: return "UNDEFINED_USER";
        case SAFReturnCode::INVALID_REQUEST: return "INVALID_REQUEST";
        case SAFReturnCode::SERVICE_ERROR: return "SERVICE_ERROR";
        default: return "UNKNOWN";
    }
}

inline std::string resourceClassToString(ResourceClass cls) {
    switch (cls) {
        case ResourceClass::DATASET: return "DATASET";
        case ResourceClass::FACILITY: return "FACILITY";
        case ResourceClass::STGADMIN: return "STGADMIN";
        case ResourceClass::PROGRAM: return "PROGRAM";
        case ResourceClass::STARTED: return "STARTED";
        case ResourceClass::CONSOLE: return "CONSOLE";
        case ResourceClass::TAPEVOL: return "TAPEVOL";
        case ResourceClass::DASDVOL: return "DASDVOL";
        case ResourceClass::USER: return "USER";
        case ResourceClass::GROUP: return "GROUP";
        case ResourceClass::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

inline std::string hsmOperationToString(HSMOperation op) {
    switch (op) {
        case HSMOperation::MIGRATE: return "MIGRATE";
        case HSMOperation::RECALL: return "RECALL";
        case HSMOperation::BACKUP: return "BACKUP";
        case HSMOperation::RECOVER: return "RECOVER";
        case HSMOperation::DELETE: return "DELETE";
        case HSMOperation::LIST: return "LIST";
        case HSMOperation::QUERY: return "QUERY";
        case HSMOperation::HOLD: return "HOLD";
        case HSMOperation::RELEASE: return "RELEASE";
        case HSMOperation::AUDIT: return "AUDIT";
        case HSMOperation::RECYCLE: return "RECYCLE";
        case HSMOperation::ADDVOL: return "ADDVOL";
        case HSMOperation::DELVOL: return "DELVOL";
        case HSMOperation::SETSYS: return "SETSYS";
        case HSMOperation::AUTH: return "AUTH";
        case HSMOperation::ADMIN: return "ADMIN";
        default: return "UNKNOWN";
    }
}

}  // namespace security
}  // namespace hsm

#endif // HSM_SECURITY_HPP
