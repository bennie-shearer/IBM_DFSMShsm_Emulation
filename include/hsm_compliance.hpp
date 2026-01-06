/*
 * DFSMShsm Emulator - Audit & Compliance
 * @version 4.2.0
 * 
 * Provides comprehensive compliance capabilities including:
 * - WORM (Write Once Read Many) storage
 * - Retention policy management
 * - Legal hold management
 * - Compliance audit logging
 * - Data governance
 * - Regulatory compliance reporting
 * 
 * Author: DFSMShsm Emulator Team
 * License: MIT
 */

#ifndef HSM_COMPLIANCE_HPP
#define HSM_COMPLIANCE_HPP

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
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <ctime>

namespace dfsmshsm {
namespace compliance {

// ============================================================================
// Forward Declarations
// ============================================================================

class RetentionPolicy;
class LegalHold;
class WORMVolume;
class AuditLog;
class ComplianceManager;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Retention type
 */
enum class RetentionType {
    TIME_BASED,        // Retain for specific duration
    EVENT_BASED,       // Retain until event occurs
    INDEFINITE,        // Retain forever
    LEGAL_HOLD,        // Under legal hold
    REGULATORY         // Regulatory requirement
};

/**
 * @brief Retention status
 */
enum class RetentionStatus {
    ACTIVE,            // Under retention
    EXPIRED,           // Retention period ended
    EXTENDED,          // Retention extended
    RELEASED,          // Released from retention
    HELD               // Under legal hold
};

/**
 * @brief Legal hold status
 */
enum class LegalHoldStatus {
    ACTIVE,            // Hold is active
    RELEASED,          // Hold has been released
    PENDING,           // Hold pending activation
    EXPIRED            // Hold expired
};

/**
 * @brief WORM volume status
 */
enum class WORMStatus {
    WRITABLE,          // Can write new data
    APPENDABLE,        // Can append only
    IMMUTABLE,         // Read-only
    EXPIRED,           // Retention expired
    ERROR              // Error state
};

/**
 * @brief Audit event type
 */
enum class AuditEventType {
    // Data lifecycle events
    DATA_CREATE,
    DATA_READ,
    DATA_MODIFY,
    DATA_DELETE,
    DATA_MIGRATE,
    DATA_RECALL,
    DATA_BACKUP,
    DATA_RESTORE,
    
    // Retention events
    RETENTION_SET,
    RETENTION_EXTEND,
    RETENTION_EXPIRE,
    RETENTION_RELEASE,
    
    // Legal hold events
    HOLD_PLACE,
    HOLD_RELEASE,
    HOLD_MODIFY,
    
    // WORM events
    WORM_WRITE,
    WORM_SEAL,
    WORM_VERIFY,
    
    // Access events
    ACCESS_GRANT,
    ACCESS_DENY,
    ACCESS_MODIFY,
    
    // Administrative events
    POLICY_CREATE,
    POLICY_MODIFY,
    POLICY_DELETE,
    CONFIG_CHANGE,
    
    // Compliance events
    COMPLIANCE_CHECK,
    COMPLIANCE_VIOLATION,
    COMPLIANCE_REPORT
};

/**
 * @brief Audit severity level
 */
enum class AuditSeverity {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL,
    SECURITY
};

/**
 * @brief Compliance standard
 */
enum class ComplianceStandard {
    SEC_17A4,          // SEC Rule 17a-4
    HIPAA,             // Health Insurance Portability
    SOX,               // Sarbanes-Oxley
    GDPR,              // General Data Protection Regulation
    PCI_DSS,           // Payment Card Industry
    FINRA,             // Financial Industry Regulatory Authority
    DOD_5015_2,        // Department of Defense
    ISO_27001,         // Information Security
    CUSTOM             // Custom requirements
};

// ============================================================================
// Utility Structures
// ============================================================================

/**
 * @brief Retention period specification
 */
struct RetentionPeriod {
    uint32_t years{0};
    uint32_t months{0};
    uint32_t days{0};
    bool indefinite{false};
    
    std::chrono::system_clock::time_point calculateEndDate(
            std::chrono::system_clock::time_point startDate) const {
        if (indefinite) {
            return std::chrono::system_clock::time_point::max();
        }
        
        // Approximate calculation
        auto duration = std::chrono::hours(24 * (years * 365 + months * 30 + days));
        return startDate + duration;
    }
    
    std::string toString() const {
        if (indefinite) return "INDEFINITE";
        
        std::ostringstream oss;
        if (years > 0) oss << years << "Y";
        if (months > 0) oss << months << "M";
        if (days > 0) oss << days << "D";
        return oss.str().empty() ? "0D" : oss.str();
    }
    
    static RetentionPeriod fromYears(uint32_t y) {
        RetentionPeriod p;
        p.years = y;
        return p;
    }
    
    static RetentionPeriod fromMonths(uint32_t m) {
        RetentionPeriod p;
        p.months = m;
        return p;
    }
    
    static RetentionPeriod fromDays(uint32_t d) {
        RetentionPeriod p;
        p.days = d;
        return p;
    }
    
    static RetentionPeriod forever() {
        RetentionPeriod p;
        p.indefinite = true;
        return p;
    }
};

/**
 * @brief Data retention record
 */
struct RetentionRecord {
    std::string recordId;
    std::string datasetName;
    std::string policyId;
    RetentionType type{RetentionType::TIME_BASED};
    RetentionStatus status{RetentionStatus::ACTIVE};
    RetentionPeriod period;
    
    std::chrono::system_clock::time_point createdDate;
    std::chrono::system_clock::time_point startDate;
    std::chrono::system_clock::time_point endDate;
    std::chrono::system_clock::time_point lastModified;
    
    std::string createdBy;
    std::string modifiedBy;
    std::string reason;
    
    std::set<std::string> legalHoldIds;  // Active legal holds
    std::map<std::string, std::string> metadata;
    
    bool isExpired() const {
        if (status == RetentionStatus::HELD) return false;
        if (period.indefinite) return false;
        return std::chrono::system_clock::now() > endDate;
    }
    
    bool isUnderHold() const {
        return !legalHoldIds.empty();
    }
    
    bool canDelete() const {
        return isExpired() && !isUnderHold();
    }
    
    std::string getStatusString() const {
        switch (status) {
            case RetentionStatus::ACTIVE: return "ACTIVE";
            case RetentionStatus::EXPIRED: return "EXPIRED";
            case RetentionStatus::EXTENDED: return "EXTENDED";
            case RetentionStatus::RELEASED: return "RELEASED";
            case RetentionStatus::HELD: return "HELD";
            default: return "UNKNOWN";
        }
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Retention[" << recordId << "] " << datasetName
            << " Status=" << getStatusString()
            << " Period=" << period.toString();
        if (isUnderHold()) {
            oss << " (HELD)";
        }
        return oss.str();
    }
};

/**
 * @brief Legal hold record
 */
struct LegalHoldRecord {
    std::string holdId;
    std::string holdName;
    std::string description;
    std::string matterNumber;          // Legal matter/case number
    LegalHoldStatus status{LegalHoldStatus::PENDING};
    
    std::chrono::system_clock::time_point createdDate;
    std::chrono::system_clock::time_point effectiveDate;
    std::chrono::system_clock::time_point releaseDate;
    
    std::string createdBy;
    std::string authorizedBy;          // Legal authority
    std::string releasedBy;
    
    std::set<std::string> affectedDatasets;
    std::set<std::string> affectedUsers;
    std::vector<std::string> searchCriteria;
    
    std::map<std::string, std::string> metadata;
    
    bool isActive() const {
        return status == LegalHoldStatus::ACTIVE;
    }
    
    std::string getStatusString() const {
        switch (status) {
            case LegalHoldStatus::ACTIVE: return "ACTIVE";
            case LegalHoldStatus::RELEASED: return "RELEASED";
            case LegalHoldStatus::PENDING: return "PENDING";
            case LegalHoldStatus::EXPIRED: return "EXPIRED";
            default: return "UNKNOWN";
        }
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "LegalHold[" << holdId << "] " << holdName
            << " Matter=" << matterNumber
            << " Status=" << getStatusString()
            << " Datasets=" << affectedDatasets.size();
        return oss.str();
    }
};

/**
 * @brief WORM volume information
 */
struct WORMVolumeInfo {
    std::string volumeSerial;
    std::string volumeLabel;
    WORMStatus status{WORMStatus::WRITABLE};
    
    uint64_t capacity{0};
    uint64_t usedSpace{0};
    uint32_t fileCount{0};
    
    std::chrono::system_clock::time_point createdDate;
    std::chrono::system_clock::time_point sealedDate;
    std::chrono::system_clock::time_point retentionEndDate;
    
    RetentionPeriod defaultRetention;
    bool autoSeal{true};               // Seal when full
    bool immutableOnSeal{true};        // Make immutable when sealed
    
    std::set<std::string> datasetNames;
    std::string integrityChecksum;
    
    double getUsagePercent() const {
        if (capacity == 0) return 0.0;
        return (static_cast<double>(usedSpace) / capacity) * 100.0;
    }
    
    bool canWrite() const {
        return status == WORMStatus::WRITABLE || 
               status == WORMStatus::APPENDABLE;
    }
    
    std::string getStatusString() const {
        switch (status) {
            case WORMStatus::WRITABLE: return "WRITABLE";
            case WORMStatus::APPENDABLE: return "APPENDABLE";
            case WORMStatus::IMMUTABLE: return "IMMUTABLE";
            case WORMStatus::EXPIRED: return "EXPIRED";
            case WORMStatus::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "WORM[" << volumeSerial << "] " << volumeLabel
            << " Status=" << getStatusString()
            << " Usage=" << std::fixed << std::setprecision(1) 
            << getUsagePercent() << "%"
            << " Files=" << fileCount;
        return oss.str();
    }
};

/**
 * @brief Audit log entry
 */
struct AuditEntry {
    std::string entryId;
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
    AuditEventType eventType;
    AuditSeverity severity{AuditSeverity::INFO};
    
    std::string userId;
    std::string jobName;
    std::string hostName;
    std::string ipAddress;
    
    std::string resourceType;          // Dataset, Volume, Policy, etc.
    std::string resourceName;
    std::string action;
    std::string outcome;               // Success, Failure, Denied
    
    std::string details;
    std::string reason;
    std::string errorCode;
    
    std::map<std::string, std::string> attributes;
    
    // Compliance tracking
    std::set<ComplianceStandard> relatedStandards;
    bool complianceRelevant{false};
    
    std::string getEventTypeString() const {
        switch (eventType) {
            case AuditEventType::DATA_CREATE: return "DATA_CREATE";
            case AuditEventType::DATA_READ: return "DATA_READ";
            case AuditEventType::DATA_MODIFY: return "DATA_MODIFY";
            case AuditEventType::DATA_DELETE: return "DATA_DELETE";
            case AuditEventType::DATA_MIGRATE: return "DATA_MIGRATE";
            case AuditEventType::DATA_RECALL: return "DATA_RECALL";
            case AuditEventType::RETENTION_SET: return "RETENTION_SET";
            case AuditEventType::RETENTION_EXPIRE: return "RETENTION_EXPIRE";
            case AuditEventType::HOLD_PLACE: return "HOLD_PLACE";
            case AuditEventType::HOLD_RELEASE: return "HOLD_RELEASE";
            case AuditEventType::WORM_WRITE: return "WORM_WRITE";
            case AuditEventType::WORM_SEAL: return "WORM_SEAL";
            case AuditEventType::ACCESS_DENY: return "ACCESS_DENY";
            case AuditEventType::COMPLIANCE_VIOLATION: return "COMPLIANCE_VIOLATION";
            default: return "UNKNOWN";
        }
    }
    
    std::string getSeverityString() const {
        switch (severity) {
            case AuditSeverity::DEBUG: return "DEBUG";
            case AuditSeverity::INFO: return "INFO";
            case AuditSeverity::WARNING: return "WARNING";
            case AuditSeverity::ERROR: return "ERROR";
            case AuditSeverity::CRITICAL: return "CRITICAL";
            case AuditSeverity::SECURITY: return "SECURITY";
            default: return "UNKNOWN";
        }
    }
    
    std::string toString() const {
        auto time = std::chrono::system_clock::to_time_t(timestamp);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
            << " [" << getSeverityString() << "] "
            << getEventTypeString()
            << " User=" << userId
            << " Resource=" << resourceName
            << " " << outcome;
        return oss.str();
    }
};

/**
 * @brief Compliance check result
 */
struct ComplianceCheckResult {
    bool compliant{true};
    ComplianceStandard standard;
    std::chrono::system_clock::time_point checkTime{std::chrono::system_clock::now()};
    
    std::vector<std::string> violations;
    std::vector<std::string> warnings;
    std::vector<std::string> recommendations;
    
    uint32_t totalChecks{0};
    uint32_t passedChecks{0};
    uint32_t failedChecks{0};
    
    double getComplianceScore() const {
        if (totalChecks == 0) return 100.0;
        return (static_cast<double>(passedChecks) / totalChecks) * 100.0;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Compliance Check: " << (compliant ? "COMPLIANT" : "NON-COMPLIANT")
            << " Score=" << std::fixed << std::setprecision(1) 
            << getComplianceScore() << "%"
            << " (" << passedChecks << "/" << totalChecks << " passed)"
            << " Violations=" << violations.size();
        return oss.str();
    }
};

// ============================================================================
// Retention Policy
// ============================================================================

/**
 * @brief Retention Policy Definition
 */
class RetentionPolicy {
public:
    RetentionPolicy() {
        policyId_ = generatePolicyId();
        createdDate_ = std::chrono::system_clock::now();
    }
    
    explicit RetentionPolicy(const std::string& name)
        : RetentionPolicy() {
        policyName_ = name;
    }
    
    // Getters
    const std::string& getPolicyId() const { return policyId_; }
    const std::string& getPolicyName() const { return policyName_; }
    const std::string& getDescription() const { return description_; }
    RetentionType getRetentionType() const { return retentionType_; }
    const RetentionPeriod& getDefaultPeriod() const { return defaultPeriod_; }
    bool isActive() const { return active_; }
    
    // Setters
    RetentionPolicy& setName(const std::string& name) {
        policyName_ = name;
        return *this;
    }
    
    RetentionPolicy& setDescription(const std::string& desc) {
        description_ = desc;
        return *this;
    }
    
    RetentionPolicy& setRetentionType(RetentionType type) {
        retentionType_ = type;
        return *this;
    }
    
    RetentionPolicy& setDefaultPeriod(const RetentionPeriod& period) {
        defaultPeriod_ = period;
        return *this;
    }
    
    RetentionPolicy& setActive(bool active) {
        active_ = active;
        return *this;
    }
    
    RetentionPolicy& addApplicablePattern(const std::string& pattern) {
        applicablePatterns_.push_back(pattern);
        return *this;
    }
    
    RetentionPolicy& addComplianceStandard(ComplianceStandard standard) {
        complianceStandards_.insert(standard);
        return *this;
    }
    
    RetentionPolicy& setMinRetention(const RetentionPeriod& period) {
        minRetention_ = period;
        return *this;
    }
    
    RetentionPolicy& setMaxRetention(const RetentionPeriod& period) {
        maxRetention_ = period;
        return *this;
    }
    
    RetentionPolicy& setAllowExtension(bool allow) {
        allowExtension_ = allow;
        return *this;
    }
    
    RetentionPolicy& setAllowEarlyRelease(bool allow) {
        allowEarlyRelease_ = allow;
        return *this;
    }
    
    // Check if dataset matches policy
    bool matchesDataset(const std::string& datasetName) const {
        if (applicablePatterns_.empty()) return true;
        
        for (const auto& pattern : applicablePatterns_) {
            // Simple pattern matching
            std::string regexPattern = "^";
            for (char c : pattern) {
                switch (c) {
                    case '*': regexPattern += ".*"; break;
                    case '%': regexPattern += "."; break;
                    case '.': regexPattern += "\\."; break;
                    default: regexPattern += c; break;
                }
            }
            regexPattern += "$";
            
            try {
                std::regex re(regexPattern, std::regex::icase);
                if (std::regex_match(datasetName, re)) {
                    return true;
                }
            } catch (...) {
                continue;
            }
        }
        return false;
    }
    
    // Create retention record for dataset
    RetentionRecord createRecord(const std::string& datasetName,
                                const std::string& createdBy) const {
        RetentionRecord record;
        record.recordId = generateRecordId();
        record.datasetName = datasetName;
        record.policyId = policyId_;
        record.type = retentionType_;
        record.status = RetentionStatus::ACTIVE;
        record.period = defaultPeriod_;
        record.createdDate = std::chrono::system_clock::now();
        record.startDate = record.createdDate;
        record.endDate = defaultPeriod_.calculateEndDate(record.startDate);
        record.lastModified = record.createdDate;
        record.createdBy = createdBy;
        return record;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Policy[" << policyId_ << "] " << policyName_
            << " Type=" << static_cast<int>(retentionType_)
            << " Period=" << defaultPeriod_.toString()
            << " Active=" << (active_ ? "Y" : "N");
        return oss.str();
    }

private:
    std::string policyId_;
    std::string policyName_;
    std::string description_;
    RetentionType retentionType_{RetentionType::TIME_BASED};
    RetentionPeriod defaultPeriod_;
    RetentionPeriod minRetention_;
    RetentionPeriod maxRetention_;
    std::vector<std::string> applicablePatterns_;
    std::set<ComplianceStandard> complianceStandards_;
    bool active_{true};
    bool allowExtension_{true};
    bool allowEarlyRelease_{false};
    std::chrono::system_clock::time_point createdDate_;
    
    static std::string generatePolicyId() {
        static std::atomic<uint64_t> counter{0};
        std::ostringstream oss;
        oss << "POL" << std::setfill('0') << std::setw(8) << (++counter);
        return oss.str();
    }
    
    static std::string generateRecordId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "RET" << std::put_time(std::localtime(&time), "%Y%m%d%H%M%S")
            << "-" << std::setfill('0') << std::setw(6) << (++counter);
        return oss.str();
    }
};

// ============================================================================
// Legal Hold Manager
// ============================================================================

/**
 * @brief Legal Hold Manager
 */
class LegalHoldManager {
public:
    LegalHoldManager() = default;
    
    // Create legal hold
    std::string createHold(const std::string& holdName,
                          const std::string& matterNumber,
                          const std::string& authorizedBy) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        LegalHoldRecord hold;
        hold.holdId = generateHoldId();
        hold.holdName = holdName;
        hold.matterNumber = matterNumber;
        hold.status = LegalHoldStatus::PENDING;
        hold.createdDate = std::chrono::system_clock::now();
        hold.authorizedBy = authorizedBy;
        
        holds_[hold.holdId] = hold;
        return hold.holdId;
    }
    
    // Activate hold
    bool activateHold(const std::string& holdId) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = holds_.find(holdId);
        if (it == holds_.end()) return false;
        
        it->second.status = LegalHoldStatus::ACTIVE;
        it->second.effectiveDate = std::chrono::system_clock::now();
        return true;
    }
    
    // Release hold
    bool releaseHold(const std::string& holdId, const std::string& releasedBy) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = holds_.find(holdId);
        if (it == holds_.end()) return false;
        
        it->second.status = LegalHoldStatus::RELEASED;
        it->second.releaseDate = std::chrono::system_clock::now();
        it->second.releasedBy = releasedBy;
        return true;
    }
    
    // Add dataset to hold
    bool addDatasetToHold(const std::string& holdId, const std::string& datasetName) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = holds_.find(holdId);
        if (it == holds_.end()) return false;
        if (it->second.status != LegalHoldStatus::ACTIVE) return false;
        
        it->second.affectedDatasets.insert(datasetName);
        datasetHolds_[datasetName].insert(holdId);
        return true;
    }
    
    // Remove dataset from hold
    bool removeDatasetFromHold(const std::string& holdId, const std::string& datasetName) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = holds_.find(holdId);
        if (it == holds_.end()) return false;
        
        it->second.affectedDatasets.erase(datasetName);
        
        auto dsIt = datasetHolds_.find(datasetName);
        if (dsIt != datasetHolds_.end()) {
            dsIt->second.erase(holdId);
            if (dsIt->second.empty()) {
                datasetHolds_.erase(dsIt);
            }
        }
        return true;
    }
    
    // Check if dataset is under hold
    bool isDatasetUnderHold(const std::string& datasetName) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = datasetHolds_.find(datasetName);
        if (it == datasetHolds_.end()) return false;
        
        for (const auto& holdId : it->second) {
            auto holdIt = holds_.find(holdId);
            if (holdIt != holds_.end() && holdIt->second.isActive()) {
                return true;
            }
        }
        return false;
    }
    
    // Get holds for dataset
    std::vector<LegalHoldRecord> getHoldsForDataset(const std::string& datasetName) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<LegalHoldRecord> result;
        auto it = datasetHolds_.find(datasetName);
        if (it == datasetHolds_.end()) return result;
        
        for (const auto& holdId : it->second) {
            auto holdIt = holds_.find(holdId);
            if (holdIt != holds_.end()) {
                result.push_back(holdIt->second);
            }
        }
        return result;
    }
    
    // Get all holds
    std::vector<LegalHoldRecord> getAllHolds() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<LegalHoldRecord> result;
        for (const auto& [id, hold] : holds_) {
            result.push_back(hold);
        }
        return result;
    }
    
    // Get active holds
    std::vector<LegalHoldRecord> getActiveHolds() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<LegalHoldRecord> result;
        for (const auto& [id, hold] : holds_) {
            if (hold.isActive()) {
                result.push_back(hold);
            }
        }
        return result;
    }

private:
    std::map<std::string, LegalHoldRecord> holds_;
    std::map<std::string, std::set<std::string>> datasetHolds_;  // dataset -> hold IDs
    mutable std::mutex mutex_;
    
    static std::string generateHoldId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "LH" << std::put_time(std::localtime(&time), "%Y%m%d%H%M%S")
            << "-" << std::setfill('0') << std::setw(4) << (++counter);
        return oss.str();
    }
};

// ============================================================================
// WORM Volume Manager
// ============================================================================

/**
 * @brief WORM Volume Manager
 */
class WORMVolumeManager {
public:
    WORMVolumeManager() = default;
    
    // Create WORM volume
    std::string createVolume(const std::string& volumeSerial,
                            uint64_t capacity,
                            const RetentionPeriod& defaultRetention = RetentionPeriod::fromYears(7)) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        WORMVolumeInfo vol;
        vol.volumeSerial = volumeSerial;
        vol.capacity = capacity;
        vol.status = WORMStatus::WRITABLE;
        vol.createdDate = std::chrono::system_clock::now();
        vol.defaultRetention = defaultRetention;
        
        volumes_[volumeSerial] = vol;
        return volumeSerial;
    }
    
    // Write to volume
    bool writeToVolume(const std::string& volumeSerial,
                      const std::string& datasetName,
                      uint64_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = volumes_.find(volumeSerial);
        if (it == volumes_.end()) return false;
        if (!it->second.canWrite()) return false;
        if (it->second.usedSpace + size > it->second.capacity) return false;
        
        it->second.usedSpace += size;
        it->second.fileCount++;
        it->second.datasetNames.insert(datasetName);
        
        // Check if should auto-seal
        if (it->second.autoSeal && 
            it->second.getUsagePercent() >= 95.0) {
            sealVolumeInternal(it->second);
        }
        
        return true;
    }
    
    // Seal volume (make immutable)
    bool sealVolume(const std::string& volumeSerial) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = volumes_.find(volumeSerial);
        if (it == volumes_.end()) return false;
        
        return sealVolumeInternal(it->second);
    }
    
    // Verify volume integrity
    bool verifyVolume(const std::string& volumeSerial) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = volumes_.find(volumeSerial);
        if (it == volumes_.end()) return false;
        
        // In real implementation, would verify checksums
        return it->second.status != WORMStatus::ERROR;
    }
    
    // Get volume info
    std::optional<WORMVolumeInfo> getVolumeInfo(const std::string& volumeSerial) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = volumes_.find(volumeSerial);
        if (it != volumes_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    // Get all volumes
    std::vector<WORMVolumeInfo> getAllVolumes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<WORMVolumeInfo> result;
        for (const auto& [serial, vol] : volumes_) {
            result.push_back(vol);
        }
        return result;
    }
    
    // Find volume with space
    std::optional<std::string> findAvailableVolume(uint64_t requiredSpace) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [serial, vol] : volumes_) {
            if (vol.canWrite() && 
                vol.capacity - vol.usedSpace >= requiredSpace) {
                return serial;
            }
        }
        return std::nullopt;
    }

private:
    std::map<std::string, WORMVolumeInfo> volumes_;
    mutable std::mutex mutex_;
    
    bool sealVolumeInternal(WORMVolumeInfo& vol) {
        if (vol.status != WORMStatus::WRITABLE &&
            vol.status != WORMStatus::APPENDABLE) {
            return false;
        }
        
        vol.status = vol.immutableOnSeal ? WORMStatus::IMMUTABLE : WORMStatus::APPENDABLE;
        vol.sealedDate = std::chrono::system_clock::now();
        vol.retentionEndDate = vol.defaultRetention.calculateEndDate(vol.sealedDate);
        
        // Generate integrity checksum
        std::ostringstream oss;
        oss << std::hex << std::hash<std::string>{}(vol.volumeSerial + 
            std::to_string(vol.usedSpace));
        vol.integrityChecksum = oss.str();
        
        return true;
    }
};

// ============================================================================
// Audit Log
// ============================================================================

/**
 * @brief Audit Log Manager
 */
class AuditLog {
public:
    AuditLog() = default;
    
    explicit AuditLog(const std::string& name)
        : logName_(name) {}
    
    // Log events
    void log(AuditEventType eventType, AuditSeverity severity,
            const std::string& userId, const std::string& resourceName,
            const std::string& action, const std::string& outcome,
            const std::string& details = "") {
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        AuditEntry entry;
        entry.entryId = generateEntryId();
        entry.eventType = eventType;
        entry.severity = severity;
        entry.userId = userId;
        entry.resourceName = resourceName;
        entry.action = action;
        entry.outcome = outcome;
        entry.details = details;
        
        entries_.push_back(entry);
        
        // Trim if too large
        if (entries_.size() > maxEntries_) {
            entries_.erase(entries_.begin(), 
                          entries_.begin() + static_cast<std::ptrdiff_t>(entries_.size() - maxEntries_));
        }
    }
    
    void logDataAccess(const std::string& userId, const std::string& datasetName,
                      AuditEventType eventType, bool success) {
        log(eventType, success ? AuditSeverity::INFO : AuditSeverity::WARNING,
            userId, datasetName, 
            (eventType == AuditEventType::DATA_READ) ? "READ" : "WRITE",
            success ? "SUCCESS" : "DENIED");
    }
    
    void logRetentionEvent(const std::string& userId, const std::string& datasetName,
                          AuditEventType eventType, const std::string& details = "") {
        log(eventType, AuditSeverity::INFO, userId, datasetName,
            "RETENTION", "SUCCESS", details);
    }
    
    void logComplianceViolation(const std::string& resourceName,
                               const std::string& violation,
                               ComplianceStandard standard) {
        AuditEntry entry;
        entry.entryId = generateEntryId();
        entry.eventType = AuditEventType::COMPLIANCE_VIOLATION;
        entry.severity = AuditSeverity::CRITICAL;
        entry.resourceName = resourceName;
        entry.details = violation;
        entry.complianceRelevant = true;
        entry.relatedStandards.insert(standard);
        
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.push_back(entry);
    }
    
    // Query entries
    std::vector<AuditEntry> getEntries(
            size_t maxEntries = 1000,
            std::optional<AuditEventType> eventType = std::nullopt,
            std::optional<AuditSeverity> minSeverity = std::nullopt) const {
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<AuditEntry> result;
        
        for (auto it = entries_.rbegin(); it != entries_.rend() && result.size() < maxEntries; ++it) {
            if (eventType && it->eventType != *eventType) continue;
            if (minSeverity && it->severity < *minSeverity) continue;
            result.push_back(*it);
        }
        
        return result;
    }
    
    std::vector<AuditEntry> getEntriesForResource(const std::string& resourceName,
                                                  size_t maxEntries = 100) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<AuditEntry> result;
        for (const auto& entry : entries_) {
            if (entry.resourceName == resourceName) {
                result.push_back(entry);
                if (result.size() >= maxEntries) break;
            }
        }
        return result;
    }
    
    std::vector<AuditEntry> getComplianceEntries(size_t maxEntries = 100) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<AuditEntry> result;
        for (const auto& entry : entries_) {
            if (entry.complianceRelevant) {
                result.push_back(entry);
                if (result.size() >= maxEntries) break;
            }
        }
        return result;
    }
    
    // Export
    bool exportToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) return false;
        
        file << "AUDIT LOG: " << logName_ << "\n";
        file << "Exported: " << formatCurrentTime() << "\n";
        file << "Entries: " << entries_.size() << "\n\n";
        
        for (const auto& entry : entries_) {
            file << entry.toString() << "\n";
        }
        
        return true;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }

private:
    std::string logName_{"AUDIT"};
    std::vector<AuditEntry> entries_;
    size_t maxEntries_{100000};
    mutable std::mutex mutex_;
    
    static std::string generateEntryId() {
        static std::atomic<uint64_t> counter{0};
        std::ostringstream oss;
        oss << "AUD" << std::setfill('0') << std::setw(12) << (++counter);
        return oss.str();
    }
    
    static std::string formatCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

// ============================================================================
// Compliance Manager
// ============================================================================

/**
 * @brief Compliance Manager
 * 
 * Central management of all compliance functions.
 */
class ComplianceManager {
public:
    /**
     * @brief Manager statistics
     */
    struct Statistics {
        uint64_t retentionRecordsCreated{0};
        uint64_t retentionRecordsExpired{0};
        uint64_t legalHoldsPlaced{0};
        uint64_t legalHoldsReleased{0};
        uint64_t wormBytesWritten{0};
        uint64_t complianceChecks{0};
        uint64_t complianceViolations{0};
        std::chrono::system_clock::time_point startTime{std::chrono::system_clock::now()};
        
        std::string toString() const {
            std::ostringstream oss;
            oss << "Compliance Stats:\n"
                << "  Retention Records: " << retentionRecordsCreated 
                << " (" << retentionRecordsExpired << " expired)\n"
                << "  Legal Holds: " << legalHoldsPlaced 
                << " placed, " << legalHoldsReleased << " released\n"
                << "  WORM Bytes: " << wormBytesWritten << "\n"
                << "  Compliance Checks: " << complianceChecks 
                << " (" << complianceViolations << " violations)";
            return oss.str();
        }
    };
    
    ComplianceManager() {
        legalHoldManager_ = std::make_unique<LegalHoldManager>();
        wormManager_ = std::make_unique<WORMVolumeManager>();
        auditLog_ = std::make_unique<AuditLog>("COMPLIANCE");
    }
    
    // Retention policy management
    bool addRetentionPolicy(const RetentionPolicy& policy) {
        std::lock_guard<std::mutex> lock(mutex_);
        policies_[policy.getPolicyId()] = policy;
        
        auditLog_->log(AuditEventType::POLICY_CREATE, AuditSeverity::INFO,
                      "SYSTEM", policy.getPolicyName(), "CREATE", "SUCCESS");
        return true;
    }
    
    bool removeRetentionPolicy(const std::string& policyId) {
        std::lock_guard<std::mutex> lock(mutex_);
        return policies_.erase(policyId) > 0;
    }
    
    RetentionPolicy* getRetentionPolicy(const std::string& policyId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = policies_.find(policyId);
        if (it != policies_.end()) return &it->second;
        return nullptr;
    }
    
    // Apply retention to dataset
    std::string applyRetention(const std::string& datasetName,
                              const std::string& policyId,
                              const std::string& userId) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto policyIt = policies_.find(policyId);
        if (policyIt == policies_.end()) return "";
        
        auto record = policyIt->second.createRecord(datasetName, userId);
        retentionRecords_[record.recordId] = record;
        datasetRetention_[datasetName] = record.recordId;
        
        stats_.retentionRecordsCreated++;
        
        auditLog_->logRetentionEvent(userId, datasetName, 
                                    AuditEventType::RETENTION_SET,
                                    "Policy: " + policyId);
        
        return record.recordId;
    }
    
    // Check if dataset can be deleted
    bool canDeleteDataset(const std::string& datasetName) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check retention
        auto retIt = datasetRetention_.find(datasetName);
        if (retIt != datasetRetention_.end()) {
            auto recIt = retentionRecords_.find(retIt->second);
            if (recIt != retentionRecords_.end()) {
                if (!recIt->second.canDelete()) {
                    return false;
                }
            }
        }
        
        // Check legal holds
        if (legalHoldManager_->isDatasetUnderHold(datasetName)) {
            return false;
        }
        
        return true;
    }
    
    // Legal hold access
    LegalHoldManager& getLegalHoldManager() { return *legalHoldManager_; }
    const LegalHoldManager& getLegalHoldManager() const { return *legalHoldManager_; }
    
    // WORM access
    WORMVolumeManager& getWORMManager() { return *wormManager_; }
    const WORMVolumeManager& getWORMManager() const { return *wormManager_; }
    
    // Audit log access
    AuditLog& getAuditLog() { return *auditLog_; }
    const AuditLog& getAuditLog() const { return *auditLog_; }
    
    // Compliance checks
    ComplianceCheckResult checkCompliance(ComplianceStandard standard) {
        ComplianceCheckResult result;
        result.standard = standard;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check retention policies exist
        result.totalChecks++;
        if (!policies_.empty()) {
            result.passedChecks++;
        } else {
            result.violations.push_back("No retention policies defined");
            result.failedChecks++;
        }
        
        // Check audit logging enabled
        result.totalChecks++;
        if (auditLog_->size() > 0) {
            result.passedChecks++;
        } else {
            result.warnings.push_back("Audit log appears empty");
            result.passedChecks++;  // Warning, not failure
        }
        
        // Check for expired retention
        result.totalChecks++;
        bool hasExpired = false;
        for (const auto& [id, record] : retentionRecords_) {
            if (record.isExpired() && record.status != RetentionStatus::EXPIRED) {
                hasExpired = true;
                result.warnings.push_back("Expired retention not processed: " + 
                                         record.datasetName);
            }
        }
        if (!hasExpired) {
            result.passedChecks++;
        } else {
            result.failedChecks++;
        }
        
        // Standard-specific checks
        switch (standard) {
            case ComplianceStandard::SEC_17A4:
                // SEC 17a-4 requires WORM storage
                result.totalChecks++;
                if (wormManager_->getAllVolumes().size() > 0) {
                    result.passedChecks++;
                } else {
                    result.violations.push_back("SEC 17a-4 requires WORM storage");
                    result.failedChecks++;
                }
                break;
                
            case ComplianceStandard::HIPAA:
                // HIPAA requires access logging
                result.totalChecks++;
                result.passedChecks++;  // We have audit logging
                result.recommendations.push_back("Ensure PHI access is logged");
                break;
                
            default:
                break;
        }
        
        result.compliant = (result.failedChecks == 0);
        stats_.complianceChecks++;
        if (!result.compliant) {
            stats_.complianceViolations += result.violations.size();
        }
        
        return result;
    }
    
    // Process expired retention
    std::vector<std::string> processExpiredRetention() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> expired;
        
        for (auto& [id, record] : retentionRecords_) {
            if (record.isExpired() && record.status == RetentionStatus::ACTIVE) {
                record.status = RetentionStatus::EXPIRED;
                expired.push_back(record.datasetName);
                stats_.retentionRecordsExpired++;
                
                auditLog_->logRetentionEvent("SYSTEM", record.datasetName,
                                            AuditEventType::RETENTION_EXPIRE);
            }
        }
        
        return expired;
    }
    
    // Statistics
    const Statistics& getStatistics() const { return stats_; }
    
    void resetStatistics() {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_ = Statistics{};
    }
    
    // Report generation
    std::string generateComplianceReport(ComplianceStandard standard) const {
        auto result = const_cast<ComplianceManager*>(this)->checkCompliance(standard);
        
        std::ostringstream oss;
        oss << "COMPLIANCE REPORT\n"
            << "=================\n"
            << "Standard: " << static_cast<int>(standard) << "\n"
            << "Generated: " << formatCurrentTime() << "\n\n"
            << result.toString() << "\n\n";
        
        if (!result.violations.empty()) {
            oss << "VIOLATIONS:\n";
            for (const auto& v : result.violations) {
                oss << "  - " << v << "\n";
            }
        }
        
        if (!result.warnings.empty()) {
            oss << "\nWARNINGS:\n";
            for (const auto& w : result.warnings) {
                oss << "  - " << w << "\n";
            }
        }
        
        if (!result.recommendations.empty()) {
            oss << "\nRECOMMENDATIONS:\n";
            for (const auto& r : result.recommendations) {
                oss << "  - " << r << "\n";
            }
        }
        
        oss << "\n" << stats_.toString();
        
        return oss.str();
    }

private:
    std::map<std::string, RetentionPolicy> policies_;
    std::map<std::string, RetentionRecord> retentionRecords_;
    std::map<std::string, std::string> datasetRetention_;  // dataset -> record ID
    
    std::unique_ptr<LegalHoldManager> legalHoldManager_;
    std::unique_ptr<WORMVolumeManager> wormManager_;
    std::unique_ptr<AuditLog> auditLog_;
    
    Statistics stats_;
    mutable std::mutex mutex_;
    mutable std::mutex statsMutex_;
    
    static std::string formatCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a standard retention policy
 */
inline RetentionPolicy createStandardRetentionPolicy(
        const std::string& name,
        uint32_t retentionYears = 7) {
    
    RetentionPolicy policy(name);
    policy.setRetentionType(RetentionType::TIME_BASED);
    policy.setDefaultPeriod(RetentionPeriod::fromYears(retentionYears));
    policy.setAllowExtension(true);
    policy.setAllowEarlyRelease(false);
    
    return policy;
}

/**
 * @brief Create a SEC 17a-4 compliant retention policy
 */
inline RetentionPolicy createSEC17a4Policy(const std::string& name) {
    RetentionPolicy policy(name);
    policy.setDescription("SEC Rule 17a-4 Compliant Retention");
    policy.setRetentionType(RetentionType::REGULATORY);
    policy.setDefaultPeriod(RetentionPeriod::fromYears(6));
    policy.setMinRetention(RetentionPeriod::fromYears(3));  // First 2 years accessible
    policy.setMaxRetention(RetentionPeriod::fromYears(6));
    policy.addComplianceStandard(ComplianceStandard::SEC_17A4);
    policy.setAllowExtension(true);
    policy.setAllowEarlyRelease(false);  // Cannot release early
    
    return policy;
}

/**
 * @brief Create a HIPAA compliant retention policy
 */
inline RetentionPolicy createHIPAAPolicy(const std::string& name) {
    RetentionPolicy policy(name);
    policy.setDescription("HIPAA Compliant Retention");
    policy.setRetentionType(RetentionType::REGULATORY);
    policy.setDefaultPeriod(RetentionPeriod::fromYears(6));
    policy.addComplianceStandard(ComplianceStandard::HIPAA);
    
    return policy;
}

/**
 * @brief Create a legal hold
 */
inline std::string createLegalHold(
        LegalHoldManager& manager,
        const std::string& holdName,
        const std::string& matterNumber,
        const std::string& authorizedBy,
        const std::vector<std::string>& datasets) {
    
    std::string holdId = manager.createHold(holdName, matterNumber, authorizedBy);
    manager.activateHold(holdId);
    
    for (const auto& dsn : datasets) {
        manager.addDatasetToHold(holdId, dsn);
    }
    
    return holdId;
}

} // namespace compliance
} // namespace dfsmshsm

#endif // HSM_COMPLIANCE_HPP
