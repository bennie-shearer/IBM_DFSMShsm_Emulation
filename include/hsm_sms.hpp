/*
 * DFSMShsm Emulator - SMS (Storage Management Subsystem) Integration
 * @version 4.2.0
 * 
 * Provides comprehensive SMS integration including:
 * - Storage Class management (performance, availability)
 * - Data Class management (allocation attributes)
 * - Management Class management (backup, migration, retention)
 * - ACS (Automatic Class Selection) routines
 * - Storage Group management
 * - SMS configuration and validation
 * 
 * Author: DFSMShsm Emulator Team
 * License: MIT
 */

#ifndef HSM_SMS_HPP
#define HSM_SMS_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <variant>
#include <regex>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <ctime>

namespace dfsmshsm {
namespace sms {

// ============================================================================
// Forward Declarations
// ============================================================================

class StorageClass;
class DataClass;
class ManagementClass;
class StorageGroup;
class ACSRoutine;
class SMSConfiguration;
class SMSManager;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Storage class performance objectives
 */
enum class PerformanceObjective {
    ULTRA_HIGH,      // Sub-millisecond response
    HIGH,            // Single-digit millisecond response
    MEDIUM,          // Double-digit millisecond response
    LOW,             // Tape/archive performance acceptable
    CUSTOM           // User-defined objective
};

/**
 * @brief Availability objectives
 */
enum class AvailabilityObjective {
    CONTINUOUS,      // 99.999% availability
    HIGH,            // 99.99% availability
    STANDARD,        // 99.9% availability
    NONE             // No specific availability requirement
};

/**
 * @brief Data set organization types
 */
enum class DataSetOrganization {
    PHYSICAL_SEQUENTIAL,   // PS
    PARTITIONED,           // PO
    PARTITIONED_EXTENDED,  // PDSE
    VSAM_KSDS,            // VSAM Key Sequenced
    VSAM_ESDS,            // VSAM Entry Sequenced
    VSAM_RRDS,            // VSAM Relative Record
    VSAM_LDS,             // VSAM Linear
    DIRECT_ACCESS,        // DA
    UNDEFINED             // Unknown
};

/**
 * @brief Record format types
 */
enum class RecordFormat {
    FIXED,              // F
    FIXED_BLOCKED,      // FB
    VARIABLE,           // V
    VARIABLE_BLOCKED,   // VB
    UNDEFINED,          // U
    SPANNED,            // VS/VBS
    FIXED_STANDARD,     // FS/FBS
    UNDEFINED_FORMAT    // Unknown
};

/**
 * @brief Space allocation units
 */
enum class SpaceUnit {
    CYLINDERS,
    TRACKS,
    BLOCKS,
    BYTES,
    KILOBYTES,
    MEGABYTES,
    GIGABYTES,
    RECORDS
};

/**
 * @brief Migration tier levels
 */
enum class MigrationLevel {
    ML0,     // Primary storage (DASD)
    ML1,     // Migration Level 1 (fast DASD)
    ML2,     // Migration Level 2 (tape)
    CLOUD    // Cloud tier (extension)
};

/**
 * @brief Backup frequency options
 */
enum class BackupFrequency {
    DAILY,
    WEEKLY,
    MONTHLY,
    ON_CLOSE,
    ON_COMMAND,
    NEVER
};

/**
 * @brief ACS routine types
 */
enum class ACSRoutineType {
    STORAGE_CLASS,
    DATA_CLASS,
    MANAGEMENT_CLASS,
    STORAGE_GROUP,
    FILTER
};

/**
 * @brief Storage group status
 */
enum class StorageGroupStatus {
    ENABLED,
    DISABLED,
    QUIESCED,
    FULL,
    ERROR
};

/**
 * @brief Volume status in storage group
 */
enum class VolumeStatus {
    ONLINE,
    OFFLINE,
    PENDING,
    DISABLED,
    FULL,
    ERROR
};

// ============================================================================
// Utility Structures
// ============================================================================

/**
 * @brief Performance metrics for storage class
 */
struct PerformanceMetrics {
    double targetResponseTimeMs{10.0};
    double sustainedRateMBps{100.0};
    double burstRateMBps{500.0};
    uint32_t maxIOPS{10000};
    double readHitRatioTarget{0.90};
    double sequentialAccessBias{0.5};
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "ResponseTime=" << targetResponseTimeMs << "ms, "
            << "SustainedRate=" << sustainedRateMBps << "MB/s, "
            << "MaxIOPS=" << maxIOPS;
        return oss.str();
    }
};

/**
 * @brief Availability configuration
 */
struct AvailabilityConfig {
    AvailabilityObjective objective{AvailabilityObjective::STANDARD};
    bool dualCopyEnabled{false};
    bool cacheEnabled{true};
    uint32_t copyPoolCount{1};
    std::string preferredCopyPool;
    bool georeplicated{false};
    
    double getAvailabilityTarget() const {
        switch (objective) {
            case AvailabilityObjective::CONTINUOUS: return 0.99999;
            case AvailabilityObjective::HIGH: return 0.9999;
            case AvailabilityObjective::STANDARD: return 0.999;
            default: return 0.99;
        }
    }
};

/**
 * @brief Space allocation parameters
 */
struct SpaceAllocation {
    SpaceUnit unit{SpaceUnit::KILOBYTES};
    uint64_t primaryQuantity{0};
    uint64_t secondaryQuantity{0};
    uint32_t directoryBlocks{0};
    bool roundToTracks{false};
    bool compressible{true};
    
    uint64_t toBytes() const {
        uint64_t multiplier = 1;
        switch (unit) {
            case SpaceUnit::BYTES: multiplier = 1; break;
            case SpaceUnit::KILOBYTES: multiplier = 1024; break;
            case SpaceUnit::MEGABYTES: multiplier = 1024 * 1024; break;
            case SpaceUnit::GIGABYTES: multiplier = 1024ULL * 1024 * 1024; break;
            case SpaceUnit::TRACKS: multiplier = 56664; break;  // 3390 track size
            case SpaceUnit::CYLINDERS: multiplier = 56664 * 15; break;  // 15 tracks/cyl
            default: multiplier = 1024; break;
        }
        return primaryQuantity * multiplier;
    }
};

/**
 * @brief Retention policy configuration
 */
struct RetentionPolicy {
    uint32_t retentionDays{0};          // 0 = no limit
    bool expireNoBackup{false};
    bool retainUntilBackedUp{true};
    std::string expirationDate;          // YYYY/DDD format or blank
    bool deleteAfterExpiration{false};
    
    bool isExpired([[maybe_unused]] std::chrono::system_clock::time_point now) const {
        if (retentionDays == 0) return false;
        // Simplified check - would need creation date in practice
        return false;
    }
};

/**
 * @brief Migration policy configuration
 */
struct MigrationPolicy {
    bool autoMigrate{true};
    uint32_t primaryDaysNonUsage{30};
    uint32_t level1DaysNonUsage{60};
    bool commandMigrate{true};
    bool partialRelease{false};
    MigrationLevel targetLevel{MigrationLevel::ML1};
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "AutoMigrate=" << (autoMigrate ? "Y" : "N")
            << ", PrimaryDays=" << primaryDaysNonUsage
            << ", L1Days=" << level1DaysNonUsage;
        return oss.str();
    }
};

/**
 * @brief Backup policy configuration  
 */
struct BackupPolicy {
    BackupFrequency frequency{BackupFrequency::DAILY};
    uint32_t backupVersionsLimit{5};
    uint32_t backupVersionsDataDeleted{2};
    bool adminOrUserCommand{true};
    bool autoBackup{true};
    uint32_t retainDaysOnlyBackup{60};
    uint32_t retainDaysExtraBackup{30};
    
    std::string getFrequencyString() const {
        switch (frequency) {
            case BackupFrequency::DAILY: return "DAILY";
            case BackupFrequency::WEEKLY: return "WEEKLY";
            case BackupFrequency::MONTHLY: return "MONTHLY";
            case BackupFrequency::ON_CLOSE: return "ON_CLOSE";
            case BackupFrequency::ON_COMMAND: return "ON_COMMAND";
            case BackupFrequency::NEVER: return "NEVER";
            default: return "UNKNOWN";
        }
    }
};

/**
 * @brief ACS filter criteria
 */
struct ACSFilterCriteria {
    std::string dataSetNameMask;         // Pattern with wildcards
    std::string storageClassFilter;
    std::string dataClassFilter;
    std::string managementClassFilter;
    std::string userIdMask;
    std::string jobNameMask;
    std::string stepNameMask;
    std::string ddNameMask;
    DataSetOrganization dsorgFilter{DataSetOrganization::UNDEFINED};
    
    bool matches(const std::string& dsn, const std::string& userId = "",
                 [[maybe_unused]] const std::string& jobName = "") const {
        // Convert mask to regex pattern
        auto maskToRegex = [](const std::string& mask) -> std::regex {
            std::string pattern = "^";
            for (char c : mask) {
                switch (c) {
                    case '*': pattern += ".*"; break;
                    case '%': pattern += "."; break;
                    case '.': pattern += "\\."; break;
                    default: pattern += c; break;
                }
            }
            pattern += "$";
            return std::regex(pattern, std::regex::icase);
        };
        
        if (!dataSetNameMask.empty()) {
            try {
                std::regex dsnRegex = maskToRegex(dataSetNameMask);
                if (!std::regex_match(dsn, dsnRegex)) {
                    return false;
                }
            } catch (...) {
                return false;
            }
        }
        
        if (!userIdMask.empty() && !userId.empty()) {
            try {
                std::regex userRegex = maskToRegex(userIdMask);
                if (!std::regex_match(userId, userRegex)) {
                    return false;
                }
            } catch (...) {
                return false;
            }
        }
        
        return true;
    }
};

/**
 * @brief ACS routine result
 */
struct ACSResult {
    bool matched{false};
    std::string selectedClass;
    std::string reason;
    int ruleNumber{0};
    std::chrono::microseconds processingTime{0};
    
    static ACSResult noMatch(const std::string& reason = "No matching rule") {
        return ACSResult{false, "", reason, 0, std::chrono::microseconds(0)};
    }
    
    static ACSResult match(const std::string& className, int rule, 
                          std::chrono::microseconds time) {
        return ACSResult{true, className, "Rule matched", rule, time};
    }
};

/**
 * @brief Data set allocation request for ACS processing
 */
struct AllocationRequest {
    std::string dataSetName;
    std::string userId;
    std::string jobName;
    std::string stepName;
    std::string ddName;
    DataSetOrganization dsorg{DataSetOrganization::UNDEFINED};
    RecordFormat recfm{RecordFormat::UNDEFINED_FORMAT};
    uint32_t lrecl{0};
    uint32_t blksize{0};
    SpaceAllocation space;
    std::string requestedStorageClass;
    std::string requestedDataClass;
    std::string requestedManagementClass;
    std::string requestedStorageGroup;
    std::map<std::string, std::string> customAttributes;
    std::chrono::system_clock::time_point requestTime{std::chrono::system_clock::now()};
    
    std::string getHighLevelQualifier() const {
        size_t dotPos = dataSetName.find('.');
        if (dotPos != std::string::npos) {
            return dataSetName.substr(0, dotPos);
        }
        return dataSetName;
    }
};

/**
 * @brief ACS allocation result
 */
struct AllocationResult {
    bool success{false};
    std::string storageClass;
    std::string dataClass;
    std::string managementClass;
    std::string storageGroup;
    std::string selectedVolume;
    std::string errorMessage;
    ACSResult storageClassResult;
    ACSResult dataClassResult;
    ACSResult managementClassResult;
    ACSResult storageGroupResult;
    std::chrono::microseconds totalProcessingTime{0};
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Success=" << (success ? "Y" : "N")
            << ", SC=" << storageClass
            << ", DC=" << dataClass
            << ", MC=" << managementClass
            << ", SG=" << storageGroup;
        if (!success) {
            oss << ", Error=" << errorMessage;
        }
        return oss.str();
    }
};

// ============================================================================
// Storage Class
// ============================================================================

/**
 * @brief Storage Class definition for SMS
 * 
 * Storage classes define performance and availability goals for data sets.
 */
class StorageClass {
public:
    StorageClass() = default;
    
    explicit StorageClass(const std::string& name, 
                         const std::string& description = "")
        : name_(name), description_(description) {
        createdTime_ = std::chrono::system_clock::now();
        modifiedTime_ = createdTime_;
    }
    
    // Getters
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    PerformanceObjective getPerformanceObjective() const { return perfObjective_; }
    AvailabilityObjective getAvailabilityObjective() const { return availObjective_; }
    const PerformanceMetrics& getPerformanceMetrics() const { return perfMetrics_; }
    const AvailabilityConfig& getAvailabilityConfig() const { return availConfig_; }
    bool isActive() const { return active_; }
    
    // Setters
    StorageClass& setDescription(const std::string& desc) {
        description_ = desc;
        markModified();
        return *this;
    }
    
    StorageClass& setPerformanceObjective(PerformanceObjective obj) {
        perfObjective_ = obj;
        applyPerformanceDefaults();
        markModified();
        return *this;
    }
    
    StorageClass& setAvailabilityObjective(AvailabilityObjective obj) {
        availObjective_ = obj;
        applyAvailabilityDefaults();
        markModified();
        return *this;
    }
    
    StorageClass& setPerformanceMetrics(const PerformanceMetrics& metrics) {
        perfMetrics_ = metrics;
        perfObjective_ = PerformanceObjective::CUSTOM;
        markModified();
        return *this;
    }
    
    StorageClass& setAvailabilityConfig(const AvailabilityConfig& config) {
        availConfig_ = config;
        markModified();
        return *this;
    }
    
    StorageClass& setActive(bool active) {
        active_ = active;
        markModified();
        return *this;
    }
    
    // Validation
    bool isValid() const {
        if (name_.empty() || name_.length() > 8) return false;
        // Storage class names must be 1-8 characters
        for (char c : name_) {
            if (!std::isalnum(c) && c != '@' && c != '#' && c != '$') {
                return false;
            }
        }
        return true;
    }
    
    // Serialization
    std::string toConfigString() const {
        std::ostringstream oss;
        oss << "DEFINE STORCLAS(" << name_ << ")\n"
            << "  DESCRIPTION('" << description_ << "')\n"
            << "  PERFORMANCE(" << static_cast<int>(perfObjective_) << ")\n"
            << "  AVAILABILITY(" << static_cast<int>(availObjective_) << ")\n"
            << "  RESPONSE_TIME(" << perfMetrics_.targetResponseTimeMs << ")\n"
            << "  DUAL_COPY(" << (availConfig_.dualCopyEnabled ? "Y" : "N") << ")\n"
            << "  CACHE(" << (availConfig_.cacheEnabled ? "Y" : "N") << ")";
        return oss.str();
    }

private:
    std::string name_;
    std::string description_;
    PerformanceObjective perfObjective_{PerformanceObjective::MEDIUM};
    AvailabilityObjective availObjective_{AvailabilityObjective::STANDARD};
    PerformanceMetrics perfMetrics_;
    AvailabilityConfig availConfig_;
    bool active_{true};
    std::chrono::system_clock::time_point createdTime_;
    std::chrono::system_clock::time_point modifiedTime_;
    
    void markModified() {
        modifiedTime_ = std::chrono::system_clock::now();
    }
    
    void applyPerformanceDefaults() {
        switch (perfObjective_) {
            case PerformanceObjective::ULTRA_HIGH:
                perfMetrics_.targetResponseTimeMs = 0.5;
                perfMetrics_.sustainedRateMBps = 500;
                perfMetrics_.maxIOPS = 100000;
                break;
            case PerformanceObjective::HIGH:
                perfMetrics_.targetResponseTimeMs = 5.0;
                perfMetrics_.sustainedRateMBps = 200;
                perfMetrics_.maxIOPS = 50000;
                break;
            case PerformanceObjective::MEDIUM:
                perfMetrics_.targetResponseTimeMs = 20.0;
                perfMetrics_.sustainedRateMBps = 100;
                perfMetrics_.maxIOPS = 10000;
                break;
            case PerformanceObjective::LOW:
                perfMetrics_.targetResponseTimeMs = 100.0;
                perfMetrics_.sustainedRateMBps = 20;
                perfMetrics_.maxIOPS = 1000;
                break;
            default:
                break;
        }
    }
    
    void applyAvailabilityDefaults() {
        switch (availObjective_) {
            case AvailabilityObjective::CONTINUOUS:
                availConfig_.dualCopyEnabled = true;
                availConfig_.cacheEnabled = true;
                availConfig_.copyPoolCount = 3;
                availConfig_.georeplicated = true;
                break;
            case AvailabilityObjective::HIGH:
                availConfig_.dualCopyEnabled = true;
                availConfig_.cacheEnabled = true;
                availConfig_.copyPoolCount = 2;
                break;
            case AvailabilityObjective::STANDARD:
                availConfig_.dualCopyEnabled = false;
                availConfig_.cacheEnabled = true;
                availConfig_.copyPoolCount = 1;
                break;
            default:
                availConfig_.dualCopyEnabled = false;
                availConfig_.cacheEnabled = false;
                break;
        }
    }
};

// ============================================================================
// Data Class
// ============================================================================

/**
 * @brief Data Class definition for SMS
 * 
 * Data classes define allocation and data set characteristics.
 */
class DataClass {
public:
    DataClass() = default;
    
    explicit DataClass(const std::string& name,
                      const std::string& description = "")
        : name_(name), description_(description) {
        createdTime_ = std::chrono::system_clock::now();
        modifiedTime_ = createdTime_;
    }
    
    // Getters
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    RecordFormat getRecordFormat() const { return recfm_; }
    uint32_t getLogicalRecordLength() const { return lrecl_; }
    uint32_t getBlockSize() const { return blksize_; }
    const SpaceAllocation& getSpaceAllocation() const { return space_; }
    DataSetOrganization getDataSetOrganization() const { return dsorg_; }
    bool isCompressible() const { return compressible_; }
    bool isActive() const { return active_; }
    
    // Setters
    DataClass& setDescription(const std::string& desc) {
        description_ = desc;
        markModified();
        return *this;
    }
    
    DataClass& setRecordFormat(RecordFormat rf) {
        recfm_ = rf;
        markModified();
        return *this;
    }
    
    DataClass& setLogicalRecordLength(uint32_t lrecl) {
        lrecl_ = lrecl;
        markModified();
        return *this;
    }
    
    DataClass& setBlockSize(uint32_t blksize) {
        blksize_ = blksize;
        markModified();
        return *this;
    }
    
    DataClass& setSpaceAllocation(const SpaceAllocation& space) {
        space_ = space;
        markModified();
        return *this;
    }
    
    DataClass& setDataSetOrganization(DataSetOrganization dsorg) {
        dsorg_ = dsorg;
        markModified();
        return *this;
    }
    
    DataClass& setCompressible(bool comp) {
        compressible_ = comp;
        markModified();
        return *this;
    }
    
    DataClass& setActive(bool active) {
        active_ = active;
        markModified();
        return *this;
    }
    
    // Calculate optimal block size
    uint32_t calculateOptimalBlockSize() const {
        if (blksize_ > 0) return blksize_;
        if (lrecl_ == 0) return 27998;  // Half-track blocking
        
        switch (recfm_) {
            case RecordFormat::FIXED:
            case RecordFormat::FIXED_BLOCKED:
                // Calculate to fit half-track
                return (27998 / lrecl_) * lrecl_;
            case RecordFormat::VARIABLE:
            case RecordFormat::VARIABLE_BLOCKED:
                return std::min(32760u, (27998 / (lrecl_ + 4)) * (lrecl_ + 4));
            default:
                return 27998;
        }
    }
    
    // Validation
    bool isValid() const {
        if (name_.empty() || name_.length() > 8) return false;
        for (char c : name_) {
            if (!std::isalnum(c) && c != '@' && c != '#' && c != '$') {
                return false;
            }
        }
        return true;
    }
    
    // Serialization
    std::string toConfigString() const {
        std::ostringstream oss;
        oss << "DEFINE DATACLAS(" << name_ << ")\n"
            << "  DESCRIPTION('" << description_ << "')\n"
            << "  RECFM(" << static_cast<int>(recfm_) << ")\n"
            << "  LRECL(" << lrecl_ << ")\n"
            << "  BLKSIZE(" << blksize_ << ")\n"
            << "  DSORG(" << static_cast<int>(dsorg_) << ")\n"
            << "  COMPRESSIBLE(" << (compressible_ ? "Y" : "N") << ")";
        return oss.str();
    }

private:
    std::string name_;
    std::string description_;
    RecordFormat recfm_{RecordFormat::FIXED_BLOCKED};
    uint32_t lrecl_{80};
    uint32_t blksize_{0};  // 0 = system determined
    SpaceAllocation space_;
    DataSetOrganization dsorg_{DataSetOrganization::PHYSICAL_SEQUENTIAL};
    bool compressible_{true};
    bool active_{true};
    std::chrono::system_clock::time_point createdTime_;
    std::chrono::system_clock::time_point modifiedTime_;
    
    void markModified() {
        modifiedTime_ = std::chrono::system_clock::now();
    }
};

// ============================================================================
// Management Class
// ============================================================================

/**
 * @brief Management Class definition for SMS
 * 
 * Management classes define backup, migration, and retention policies.
 */
class ManagementClass {
public:
    ManagementClass() = default;
    
    explicit ManagementClass(const std::string& name,
                            const std::string& description = "")
        : name_(name), description_(description) {
        createdTime_ = std::chrono::system_clock::now();
        modifiedTime_ = createdTime_;
    }
    
    // Getters
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    const BackupPolicy& getBackupPolicy() const { return backupPolicy_; }
    const MigrationPolicy& getMigrationPolicy() const { return migrationPolicy_; }
    const RetentionPolicy& getRetentionPolicy() const { return retentionPolicy_; }
    bool isActive() const { return active_; }
    
    // Setters
    ManagementClass& setDescription(const std::string& desc) {
        description_ = desc;
        markModified();
        return *this;
    }
    
    ManagementClass& setBackupPolicy(const BackupPolicy& policy) {
        backupPolicy_ = policy;
        markModified();
        return *this;
    }
    
    ManagementClass& setMigrationPolicy(const MigrationPolicy& policy) {
        migrationPolicy_ = policy;
        markModified();
        return *this;
    }
    
    ManagementClass& setRetentionPolicy(const RetentionPolicy& policy) {
        retentionPolicy_ = policy;
        markModified();
        return *this;
    }
    
    ManagementClass& setActive(bool active) {
        active_ = active;
        markModified();
        return *this;
    }
    
    // Quick setters for common scenarios
    ManagementClass& enableAutoBackup(BackupFrequency freq = BackupFrequency::DAILY) {
        backupPolicy_.autoBackup = true;
        backupPolicy_.frequency = freq;
        markModified();
        return *this;
    }
    
    ManagementClass& enableAutoMigration(uint32_t primaryDays = 30, 
                                         uint32_t level1Days = 60) {
        migrationPolicy_.autoMigrate = true;
        migrationPolicy_.primaryDaysNonUsage = primaryDays;
        migrationPolicy_.level1DaysNonUsage = level1Days;
        markModified();
        return *this;
    }
    
    ManagementClass& setRetentionDays(uint32_t days) {
        retentionPolicy_.retentionDays = days;
        markModified();
        return *this;
    }
    
    // Validation
    bool isValid() const {
        if (name_.empty() || name_.length() > 8) return false;
        for (char c : name_) {
            if (!std::isalnum(c) && c != '@' && c != '#' && c != '$') {
                return false;
            }
        }
        return true;
    }
    
    // Serialization
    std::string toConfigString() const {
        std::ostringstream oss;
        oss << "DEFINE MGMTCLAS(" << name_ << ")\n"
            << "  DESCRIPTION('" << description_ << "')\n"
            << "  BACKUP_FREQ(" << backupPolicy_.getFrequencyString() << ")\n"
            << "  BACKUP_VERSIONS(" << backupPolicy_.backupVersionsLimit << ")\n"
            << "  AUTO_MIGRATE(" << (migrationPolicy_.autoMigrate ? "Y" : "N") << ")\n"
            << "  PRIMARY_DAYS(" << migrationPolicy_.primaryDaysNonUsage << ")\n"
            << "  LEVEL1_DAYS(" << migrationPolicy_.level1DaysNonUsage << ")\n"
            << "  RETENTION_DAYS(" << retentionPolicy_.retentionDays << ")";
        return oss.str();
    }

private:
    std::string name_;
    std::string description_;
    BackupPolicy backupPolicy_;
    MigrationPolicy migrationPolicy_;
    RetentionPolicy retentionPolicy_;
    bool active_{true};
    std::chrono::system_clock::time_point createdTime_;
    std::chrono::system_clock::time_point modifiedTime_;
    
    void markModified() {
        modifiedTime_ = std::chrono::system_clock::now();
    }
};

// ============================================================================
// Storage Group
// ============================================================================

/**
 * @brief Volume information within a storage group
 */
struct VolumeInfo {
    std::string volser;
    VolumeStatus status{VolumeStatus::ONLINE};
    uint64_t totalCapacity{0};
    uint64_t usedCapacity{0};
    uint64_t freeCapacity{0};
    uint32_t allocationCount{0};
    std::chrono::system_clock::time_point lastAccessTime;
    
    double getUsagePercent() const {
        if (totalCapacity == 0) return 0.0;
        return (static_cast<double>(usedCapacity) / totalCapacity) * 100.0;
    }
    
    bool isAvailable() const {
        return status == VolumeStatus::ONLINE && 
               getUsagePercent() < 95.0;
    }
};

/**
 * @brief Storage Group definition for SMS
 * 
 * Storage groups are collections of volumes managed as a unit.
 */
class StorageGroup {
public:
    /**
     * @brief Storage group types
     */
    enum class Type {
        POOL,        // DASD pool
        VIO,         // Virtual I/O
        DUMMY,       // For testing
        COPY,        // Copy pool
        BACKUP,      // Backup pool
        OVERFLOW     // Overflow pool
    };
    
    StorageGroup() = default;
    
    explicit StorageGroup(const std::string& name, Type type = Type::POOL,
                         const std::string& description = "")
        : name_(name), type_(type), description_(description) {
        createdTime_ = std::chrono::system_clock::now();
        modifiedTime_ = createdTime_;
    }
    
    // Getters
    const std::string& getName() const { return name_; }
    Type getType() const { return type_; }
    const std::string& getDescription() const { return description_; }
    StorageGroupStatus getStatus() const { return status_; }
    const std::vector<VolumeInfo>& getVolumes() const { return volumes_; }
    uint32_t getThresholdPercent() const { return thresholdPercent_; }
    bool isActive() const { return active_; }
    
    // Volume management
    StorageGroup& addVolume(const VolumeInfo& vol) {
        volumes_.push_back(vol);
        markModified();
        return *this;
    }
    
    StorageGroup& addVolume(const std::string& volser, uint64_t capacity = 0) {
        VolumeInfo vol;
        vol.volser = volser;
        vol.totalCapacity = capacity;
        vol.freeCapacity = capacity;
        vol.status = VolumeStatus::ONLINE;
        vol.lastAccessTime = std::chrono::system_clock::now();
        volumes_.push_back(vol);
        markModified();
        return *this;
    }
    
    bool removeVolume(const std::string& volser) {
        auto it = std::find_if(volumes_.begin(), volumes_.end(),
            [&volser](const VolumeInfo& v) { return v.volser == volser; });
        if (it != volumes_.end()) {
            volumes_.erase(it);
            markModified();
            return true;
        }
        return false;
    }
    
    VolumeInfo* findVolume(const std::string& volser) {
        for (auto& vol : volumes_) {
            if (vol.volser == volser) return &vol;
        }
        return nullptr;
    }
    
    const VolumeInfo* findVolume(const std::string& volser) const {
        for (const auto& vol : volumes_) {
            if (vol.volser == volser) return &vol;
        }
        return nullptr;
    }
    
    // Setters
    StorageGroup& setDescription(const std::string& desc) {
        description_ = desc;
        markModified();
        return *this;
    }
    
    StorageGroup& setStatus(StorageGroupStatus status) {
        status_ = status;
        markModified();
        return *this;
    }
    
    StorageGroup& setThresholdPercent(uint32_t percent) {
        thresholdPercent_ = std::min(percent, 100u);
        markModified();
        return *this;
    }
    
    StorageGroup& setActive(bool active) {
        active_ = active;
        markModified();
        return *this;
    }
    
    StorageGroup& setOverflowGroup(const std::string& overflow) {
        overflowGroup_ = overflow;
        markModified();
        return *this;
    }
    
    // Capacity calculations
    uint64_t getTotalCapacity() const {
        uint64_t total = 0;
        for (const auto& vol : volumes_) {
            total += vol.totalCapacity;
        }
        return total;
    }
    
    uint64_t getFreeCapacity() const {
        uint64_t free = 0;
        for (const auto& vol : volumes_) {
            if (vol.isAvailable()) {
                free += vol.freeCapacity;
            }
        }
        return free;
    }
    
    double getUsagePercent() const {
        uint64_t total = getTotalCapacity();
        if (total == 0) return 0.0;
        uint64_t free = getFreeCapacity();
        return ((total - free) / static_cast<double>(total)) * 100.0;
    }
    
    bool isOverThreshold() const {
        return getUsagePercent() >= thresholdPercent_;
    }
    
    // Select best volume for allocation
    std::optional<std::string> selectVolumeForAllocation(uint64_t requiredSpace) const {
        const VolumeInfo* bestVol = nullptr;
        
        for (const auto& vol : volumes_) {
            if (!vol.isAvailable()) continue;
            if (vol.freeCapacity < requiredSpace) continue;
            
            // Select volume with most free space
            if (!bestVol || vol.freeCapacity > bestVol->freeCapacity) {
                bestVol = &vol;
            }
        }
        
        if (bestVol) {
            return bestVol->volser;
        }
        return std::nullopt;
    }
    
    // Count available volumes
    size_t getAvailableVolumeCount() const {
        return std::count_if(volumes_.begin(), volumes_.end(),
            [](const VolumeInfo& v) { return v.isAvailable(); });
    }
    
    // Validation
    bool isValid() const {
        if (name_.empty() || name_.length() > 8) return false;
        for (char c : name_) {
            if (!std::isalnum(c) && c != '@' && c != '#' && c != '$') {
                return false;
            }
        }
        return true;
    }
    
    // Get type string
    std::string getTypeString() const {
        switch (type_) {
            case Type::POOL: return "POOL";
            case Type::VIO: return "VIO";
            case Type::DUMMY: return "DUMMY";
            case Type::COPY: return "COPY";
            case Type::BACKUP: return "BACKUP";
            case Type::OVERFLOW: return "OVERFLOW";
            default: return "UNKNOWN";
        }
    }
    
    // Serialization
    std::string toConfigString() const {
        std::ostringstream oss;
        oss << "DEFINE STORGRP(" << name_ << ")\n"
            << "  TYPE(" << getTypeString() << ")\n"
            << "  DESCRIPTION('" << description_ << "')\n"
            << "  THRESHOLD(" << thresholdPercent_ << ")\n"
            << "  VOLUMES(";
        for (size_t i = 0; i < volumes_.size(); ++i) {
            if (i > 0) oss << ",";
            oss << volumes_[i].volser;
        }
        oss << ")";
        if (!overflowGroup_.empty()) {
            oss << "\n  OVERFLOW(" << overflowGroup_ << ")";
        }
        return oss.str();
    }

private:
    std::string name_;
    Type type_{Type::POOL};
    std::string description_;
    StorageGroupStatus status_{StorageGroupStatus::ENABLED};
    std::vector<VolumeInfo> volumes_;
    uint32_t thresholdPercent_{85};
    std::string overflowGroup_;
    bool active_{true};
    std::chrono::system_clock::time_point createdTime_;
    std::chrono::system_clock::time_point modifiedTime_;
    
    void markModified() {
        modifiedTime_ = std::chrono::system_clock::now();
    }
};

// ============================================================================
// ACS Routine
// ============================================================================

/**
 * @brief ACS (Automatic Class Selection) Routine
 * 
 * ACS routines automatically select SMS constructs based on data set characteristics.
 */
class ACSRoutine {
public:
    /**
     * @brief ACS rule definition
     */
    struct Rule {
        int priority{0};
        ACSFilterCriteria filter;
        std::string targetClass;
        std::string description;
        bool enabled{true};
        
        bool matches(const AllocationRequest& request) const {
            if (!enabled) return false;
            return filter.matches(request.dataSetName, request.userId, request.jobName);
        }
    };
    
    ACSRoutine() = default;
    
    explicit ACSRoutine(const std::string& name, ACSRoutineType type,
                       const std::string& description = "")
        : name_(name), type_(type), description_(description) {
        createdTime_ = std::chrono::system_clock::now();
        modifiedTime_ = createdTime_;
    }
    
    // Getters
    const std::string& getName() const { return name_; }
    ACSRoutineType getType() const { return type_; }
    const std::string& getDescription() const { return description_; }
    const std::vector<Rule>& getRules() const { return rules_; }
    const std::string& getDefaultClass() const { return defaultClass_; }
    bool isActive() const { return active_; }
    
    // Rule management
    ACSRoutine& addRule(const Rule& rule) {
        rules_.push_back(rule);
        sortRules();
        markModified();
        return *this;
    }
    
    ACSRoutine& addRule(int priority, const std::string& dsnMask,
                       const std::string& targetClass,
                       const std::string& description = "") {
        Rule rule;
        rule.priority = priority;
        rule.filter.dataSetNameMask = dsnMask;
        rule.targetClass = targetClass;
        rule.description = description;
        rules_.push_back(rule);
        sortRules();
        markModified();
        return *this;
    }
    
    bool removeRule(size_t index) {
        if (index >= rules_.size()) return false;
        rules_.erase(rules_.begin() + static_cast<std::ptrdiff_t>(index));
        markModified();
        return true;
    }
    
    // Setters
    ACSRoutine& setDescription(const std::string& desc) {
        description_ = desc;
        markModified();
        return *this;
    }
    
    ACSRoutine& setDefaultClass(const std::string& defaultClass) {
        defaultClass_ = defaultClass;
        markModified();
        return *this;
    }
    
    ACSRoutine& setActive(bool active) {
        active_ = active;
        markModified();
        return *this;
    }
    
    // Execute ACS routine
    ACSResult execute(const AllocationRequest& request) const {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        if (!active_) {
            return ACSResult::noMatch("ACS routine is inactive");
        }
        
        // Process rules in priority order
        for (size_t i = 0; i < rules_.size(); ++i) {
            const auto& rule = rules_[i];
            if (rule.matches(request)) {
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                    endTime - startTime);
                return ACSResult::match(rule.targetClass, static_cast<int>(i), duration);
            }
        }
        
        // No rule matched, use default
        if (!defaultClass_.empty()) {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                endTime - startTime);
            return ACSResult{true, defaultClass_, "Using default class", -1, duration};
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime);
        return ACSResult{false, "", "No matching rule and no default", -1, duration};
    }
    
    // Get type string
    std::string getTypeString() const {
        switch (type_) {
            case ACSRoutineType::STORAGE_CLASS: return "STORCLAS";
            case ACSRoutineType::DATA_CLASS: return "DATACLAS";
            case ACSRoutineType::MANAGEMENT_CLASS: return "MGMTCLAS";
            case ACSRoutineType::STORAGE_GROUP: return "STORGRP";
            case ACSRoutineType::FILTER: return "FILTER";
            default: return "UNKNOWN";
        }
    }
    
    // Validation
    bool isValid() const {
        if (name_.empty() || name_.length() > 8) return false;
        return true;
    }
    
    // Serialization
    std::string toConfigString() const {
        std::ostringstream oss;
        oss << "DEFINE ACSRTN(" << name_ << ")\n"
            << "  TYPE(" << getTypeString() << ")\n"
            << "  DESCRIPTION('" << description_ << "')\n"
            << "  DEFAULT(" << defaultClass_ << ")\n"
            << "  RULES(\n";
        for (const auto& rule : rules_) {
            oss << "    WHEN(DSN=" << rule.filter.dataSetNameMask << ")"
                << " SELECT(" << rule.targetClass << ")"
                << " /* " << rule.description << " */\n";
        }
        oss << "  )";
        return oss.str();
    }

private:
    std::string name_;
    ACSRoutineType type_{ACSRoutineType::STORAGE_CLASS};
    std::string description_;
    std::vector<Rule> rules_;
    std::string defaultClass_;
    bool active_{true};
    std::chrono::system_clock::time_point createdTime_;
    std::chrono::system_clock::time_point modifiedTime_;
    
    void markModified() {
        modifiedTime_ = std::chrono::system_clock::now();
    }
    
    void sortRules() {
        std::sort(rules_.begin(), rules_.end(),
            [](const Rule& a, const Rule& b) { return a.priority < b.priority; });
    }
};

// ============================================================================
// SMS Configuration
// ============================================================================

/**
 * @brief SMS Configuration container
 * 
 * Holds all SMS constructs and their relationships.
 */
class SMSConfiguration {
public:
    SMSConfiguration() = default;
    
    explicit SMSConfiguration(const std::string& name)
        : name_(name) {
        createdTime_ = std::chrono::system_clock::now();
        modifiedTime_ = createdTime_;
    }
    
    // Storage Class operations
    bool addStorageClass(const StorageClass& sc) {
        if (!sc.isValid()) return false;
        std::lock_guard<std::shared_mutex> lock(mutex_);
        storageClasses_[sc.getName()] = sc;
        markModified();
        return true;
    }
    
    StorageClass* getStorageClass(const std::string& name) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = storageClasses_.find(name);
        if (it != storageClasses_.end()) return &it->second;
        return nullptr;
    }
    
    bool removeStorageClass(const std::string& name) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        return storageClasses_.erase(name) > 0;
    }
    
    // Data Class operations
    bool addDataClass(const DataClass& dc) {
        if (!dc.isValid()) return false;
        std::lock_guard<std::shared_mutex> lock(mutex_);
        dataClasses_[dc.getName()] = dc;
        markModified();
        return true;
    }
    
    DataClass* getDataClass(const std::string& name) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = dataClasses_.find(name);
        if (it != dataClasses_.end()) return &it->second;
        return nullptr;
    }
    
    bool removeDataClass(const std::string& name) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        return dataClasses_.erase(name) > 0;
    }
    
    // Management Class operations
    bool addManagementClass(const ManagementClass& mc) {
        if (!mc.isValid()) return false;
        std::lock_guard<std::shared_mutex> lock(mutex_);
        managementClasses_[mc.getName()] = mc;
        markModified();
        return true;
    }
    
    ManagementClass* getManagementClass(const std::string& name) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = managementClasses_.find(name);
        if (it != managementClasses_.end()) return &it->second;
        return nullptr;
    }
    
    bool removeManagementClass(const std::string& name) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        return managementClasses_.erase(name) > 0;
    }
    
    // Storage Group operations
    bool addStorageGroup(const StorageGroup& sg) {
        if (!sg.isValid()) return false;
        std::lock_guard<std::shared_mutex> lock(mutex_);
        storageGroups_[sg.getName()] = sg;
        markModified();
        return true;
    }
    
    StorageGroup* getStorageGroup(const std::string& name) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = storageGroups_.find(name);
        if (it != storageGroups_.end()) return &it->second;
        return nullptr;
    }
    
    bool removeStorageGroup(const std::string& name) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        return storageGroups_.erase(name) > 0;
    }
    
    // ACS Routine operations
    bool addACSRoutine(const ACSRoutine& acs) {
        if (!acs.isValid()) return false;
        std::lock_guard<std::shared_mutex> lock(mutex_);
        acsRoutines_[acs.getName()] = acs;
        markModified();
        return true;
    }
    
    ACSRoutine* getACSRoutine(const std::string& name) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = acsRoutines_.find(name);
        if (it != acsRoutines_.end()) return &it->second;
        return nullptr;
    }
    
    bool removeACSRoutine(const std::string& name) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        return acsRoutines_.erase(name) > 0;
    }
    
    // Configuration access
    const std::string& getName() const { return name_; }
    
    std::map<std::string, StorageClass>& getStorageClasses() { return storageClasses_; }
    std::map<std::string, DataClass>& getDataClasses() { return dataClasses_; }
    std::map<std::string, ManagementClass>& getManagementClasses() { return managementClasses_; }
    std::map<std::string, StorageGroup>& getStorageGroups() { return storageGroups_; }
    std::map<std::string, ACSRoutine>& getACSRoutines() { return acsRoutines_; }
    
    // Statistics
    size_t getStorageClassCount() const { return storageClasses_.size(); }
    size_t getDataClassCount() const { return dataClasses_.size(); }
    size_t getManagementClassCount() const { return managementClasses_.size(); }
    size_t getStorageGroupCount() const { return storageGroups_.size(); }
    size_t getACSRoutineCount() const { return acsRoutines_.size(); }
    
    // Validation
    bool validate(std::vector<std::string>& errors) const {
        errors.clear();
        
        // Check for orphaned references in ACS routines
        for (const auto& [name, acs] : acsRoutines_) {
            const auto& defaultClass = acs.getDefaultClass();
            if (!defaultClass.empty()) {
                bool found = false;
                switch (acs.getType()) {
                    case ACSRoutineType::STORAGE_CLASS:
                        found = storageClasses_.find(defaultClass) != storageClasses_.end();
                        break;
                    case ACSRoutineType::DATA_CLASS:
                        found = dataClasses_.find(defaultClass) != dataClasses_.end();
                        break;
                    case ACSRoutineType::MANAGEMENT_CLASS:
                        found = managementClasses_.find(defaultClass) != managementClasses_.end();
                        break;
                    case ACSRoutineType::STORAGE_GROUP:
                        found = storageGroups_.find(defaultClass) != storageGroups_.end();
                        break;
                    default:
                        found = true;
                        break;
                }
                if (!found) {
                    errors.push_back("ACS routine '" + name + "' references undefined class: " + defaultClass);
                }
            }
        }
        
        // Check storage groups have volumes
        for (const auto& [name, sg] : storageGroups_) {
            if (sg.getVolumes().empty() && sg.getType() != StorageGroup::Type::DUMMY) {
                errors.push_back("Storage group '" + name + "' has no volumes");
            }
        }
        
        return errors.empty();
    }
    
    // Serialization
    std::string toConfigString() const {
        std::ostringstream oss;
        oss << "/*\n * SMS Configuration: " << name_ << "\n */\n\n";
        
        oss << "/* Storage Classes */\n";
        for (const auto& [name, sc] : storageClasses_) {
            oss << sc.toConfigString() << "\n\n";
        }
        
        oss << "/* Data Classes */\n";
        for (const auto& [name, dc] : dataClasses_) {
            oss << dc.toConfigString() << "\n\n";
        }
        
        oss << "/* Management Classes */\n";
        for (const auto& [name, mc] : managementClasses_) {
            oss << mc.toConfigString() << "\n\n";
        }
        
        oss << "/* Storage Groups */\n";
        for (const auto& [name, sg] : storageGroups_) {
            oss << sg.toConfigString() << "\n\n";
        }
        
        oss << "/* ACS Routines */\n";
        for (const auto& [name, acs] : acsRoutines_) {
            oss << acs.toConfigString() << "\n\n";
        }
        
        return oss.str();
    }

private:
    std::string name_{"DEFAULT"};
    std::map<std::string, StorageClass> storageClasses_;
    std::map<std::string, DataClass> dataClasses_;
    std::map<std::string, ManagementClass> managementClasses_;
    std::map<std::string, StorageGroup> storageGroups_;
    std::map<std::string, ACSRoutine> acsRoutines_;
    mutable std::shared_mutex mutex_;
    std::chrono::system_clock::time_point createdTime_;
    std::chrono::system_clock::time_point modifiedTime_;
    
    void markModified() {
        modifiedTime_ = std::chrono::system_clock::now();
    }
};

// ============================================================================
// SMS Manager
// ============================================================================

/**
 * @brief SMS Manager - Main entry point for SMS operations
 * 
 * Provides comprehensive SMS management including:
 * - Configuration management
 * - ACS processing
 * - Volume selection
 * - SMS construct queries
 */
class SMSManager {
public:
    /**
     * @brief SMS Manager statistics
     */
    struct Statistics {
        uint64_t allocationsProcessed{0};
        uint64_t allocationsSuccessful{0};
        uint64_t allocationsFailed{0};
        uint64_t acsRoutineExecutions{0};
        uint64_t configurationChanges{0};
        std::chrono::system_clock::time_point startTime{std::chrono::system_clock::now()};
        
        double getSuccessRate() const {
            if (allocationsProcessed == 0) return 100.0;
            return (static_cast<double>(allocationsSuccessful) / allocationsProcessed) * 100.0;
        }
        
        std::string toString() const {
            std::ostringstream oss;
            oss << "Allocations: " << allocationsProcessed
                << " (Success: " << allocationsSuccessful
                << ", Failed: " << allocationsFailed
                << ", Rate: " << std::fixed << std::setprecision(1) 
                << getSuccessRate() << "%)";
            return oss.str();
        }
    };
    
    SMSManager() {
        config_ = std::make_unique<SMSConfiguration>("DEFAULT");
    }
    
    explicit SMSManager(const std::string& configName) {
        config_ = std::make_unique<SMSConfiguration>(configName);
    }
    
    // Configuration access
    SMSConfiguration& getConfiguration() { return *config_; }
    const SMSConfiguration& getConfiguration() const { return *config_; }
    
    // Process allocation request through ACS
    AllocationResult processAllocation(const AllocationRequest& request) {
        std::lock_guard<std::mutex> lock(statsMutex_);
        auto startTime = std::chrono::high_resolution_clock::now();
        
        AllocationResult result;
        stats_.allocationsProcessed++;
        
        // Execute ACS routines in order
        result.storageClassResult = executeACSForType(request, ACSRoutineType::STORAGE_CLASS);
        if (result.storageClassResult.matched) {
            result.storageClass = result.storageClassResult.selectedClass;
        } else if (!request.requestedStorageClass.empty()) {
            result.storageClass = request.requestedStorageClass;
        }
        
        result.dataClassResult = executeACSForType(request, ACSRoutineType::DATA_CLASS);
        if (result.dataClassResult.matched) {
            result.dataClass = result.dataClassResult.selectedClass;
        } else if (!request.requestedDataClass.empty()) {
            result.dataClass = request.requestedDataClass;
        }
        
        result.managementClassResult = executeACSForType(request, ACSRoutineType::MANAGEMENT_CLASS);
        if (result.managementClassResult.matched) {
            result.managementClass = result.managementClassResult.selectedClass;
        } else if (!request.requestedManagementClass.empty()) {
            result.managementClass = request.requestedManagementClass;
        }
        
        result.storageGroupResult = executeACSForType(request, ACSRoutineType::STORAGE_GROUP);
        if (result.storageGroupResult.matched) {
            result.storageGroup = result.storageGroupResult.selectedClass;
        } else if (!request.requestedStorageGroup.empty()) {
            result.storageGroup = request.requestedStorageGroup;
        }
        
        // Select volume from storage group
        if (!result.storageGroup.empty()) {
            auto* sg = config_->getStorageGroup(result.storageGroup);
            if (sg) {
                auto volOpt = sg->selectVolumeForAllocation(request.space.toBytes());
                if (volOpt) {
                    result.selectedVolume = *volOpt;
                    result.success = true;
                    stats_.allocationsSuccessful++;
                } else {
                    result.errorMessage = "No available volume in storage group";
                    stats_.allocationsFailed++;
                }
            } else {
                result.errorMessage = "Storage group not found: " + result.storageGroup;
                stats_.allocationsFailed++;
            }
        } else {
            result.errorMessage = "No storage group selected";
            stats_.allocationsFailed++;
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.totalProcessingTime = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime);
        
        return result;
    }
    
    // Validate class assignments
    bool validateStorageClass(const std::string& name) const {
        return config_->getStorageClass(name) != nullptr;
    }
    
    bool validateDataClass(const std::string& name) const {
        return config_->getDataClass(name) != nullptr;
    }
    
    bool validateManagementClass(const std::string& name) const {
        return config_->getManagementClass(name) != nullptr;
    }
    
    bool validateStorageGroup(const std::string& name) const {
        return config_->getStorageGroup(name) != nullptr;
    }
    
    // Quick setup helpers
    void createDefaultConfiguration() {
        // Create default storage classes
        StorageClass scHigh("SCHIGH", "High performance storage");
        scHigh.setPerformanceObjective(PerformanceObjective::HIGH)
              .setAvailabilityObjective(AvailabilityObjective::HIGH);
        config_->addStorageClass(scHigh);
        
        StorageClass scStd("SCSTD", "Standard storage");
        scStd.setPerformanceObjective(PerformanceObjective::MEDIUM)
             .setAvailabilityObjective(AvailabilityObjective::STANDARD);
        config_->addStorageClass(scStd);
        
        StorageClass scLow("SCLOW", "Archive storage");
        scLow.setPerformanceObjective(PerformanceObjective::LOW)
             .setAvailabilityObjective(AvailabilityObjective::NONE);
        config_->addStorageClass(scLow);
        
        // Create default data classes
        DataClass dcSeq("DCSEQ", "Sequential data sets");
        dcSeq.setDataSetOrganization(DataSetOrganization::PHYSICAL_SEQUENTIAL)
             .setRecordFormat(RecordFormat::FIXED_BLOCKED)
             .setLogicalRecordLength(80);
        config_->addDataClass(dcSeq);
        
        DataClass dcPds("DCPDS", "Partitioned data sets");
        dcPds.setDataSetOrganization(DataSetOrganization::PARTITIONED_EXTENDED)
             .setRecordFormat(RecordFormat::FIXED_BLOCKED)
             .setLogicalRecordLength(80);
        config_->addDataClass(dcPds);
        
        DataClass dcVsam("DCVSAM", "VSAM data sets");
        dcVsam.setDataSetOrganization(DataSetOrganization::VSAM_KSDS);
        config_->addDataClass(dcVsam);
        
        // Create default management classes
        ManagementClass mcStd("MCSTD", "Standard management");
        mcStd.enableAutoBackup(BackupFrequency::DAILY)
             .enableAutoMigration(30, 60)
             .setRetentionDays(365);
        config_->addManagementClass(mcStd);
        
        ManagementClass mcCrit("MCCRIT", "Critical data management");
        BackupPolicy critBackup;
        critBackup.frequency = BackupFrequency::DAILY;
        critBackup.backupVersionsLimit = 10;
        critBackup.autoBackup = true;
        mcCrit.setBackupPolicy(critBackup);
        MigrationPolicy critMig;
        critMig.autoMigrate = false;  // Don't migrate critical data
        mcCrit.setMigrationPolicy(critMig);
        config_->addManagementClass(mcCrit);
        
        ManagementClass mcTemp("MCTEMP", "Temporary data management");
        mcTemp.setRetentionDays(7);
        BackupPolicy tempBackup;
        tempBackup.autoBackup = false;
        mcTemp.setBackupPolicy(tempBackup);
        config_->addManagementClass(mcTemp);
        
        // Create default storage groups
        StorageGroup sgPrim("SGPRIM", StorageGroup::Type::POOL, "Primary storage pool");
        sgPrim.addVolume("VOL001", 3339ULL * 1024 * 1024 * 1024);  // 3339GB (3390-54)
        sgPrim.addVolume("VOL002", 3339ULL * 1024 * 1024 * 1024);
        sgPrim.addVolume("VOL003", 3339ULL * 1024 * 1024 * 1024);
        sgPrim.setThresholdPercent(85);
        config_->addStorageGroup(sgPrim);
        
        StorageGroup sgArch("SGARCH", StorageGroup::Type::POOL, "Archive storage pool");
        sgArch.addVolume("ARC001", 10ULL * 1024 * 1024 * 1024 * 1024);  // 10TB
        sgArch.setThresholdPercent(95);
        config_->addStorageGroup(sgArch);
        
        // Create default ACS routines
        ACSRoutine acsStorclas("ACSSTOR", ACSRoutineType::STORAGE_CLASS, "Storage class selection");
        acsStorclas.addRule(10, "SYS1.*", "SCHIGH", "System datasets get high performance");
        acsStorclas.addRule(20, "*.TEMP.*", "SCLOW", "Temporary datasets get low performance");
        acsStorclas.addRule(30, "*.ARCHIVE.*", "SCLOW", "Archive datasets get low performance");
        acsStorclas.setDefaultClass("SCSTD");
        config_->addACSRoutine(acsStorclas);
        
        ACSRoutine acsMgmtclas("ACSMGMT", ACSRoutineType::MANAGEMENT_CLASS, "Management class selection");
        acsMgmtclas.addRule(10, "SYS1.*", "MCCRIT", "System datasets are critical");
        acsMgmtclas.addRule(20, "*.TEMP.*", "MCTEMP", "Temporary datasets");
        acsMgmtclas.setDefaultClass("MCSTD");
        config_->addACSRoutine(acsMgmtclas);
        
        ACSRoutine acsStorgrp("ACSSGRP", ACSRoutineType::STORAGE_GROUP, "Storage group selection");
        acsStorgrp.addRule(10, "*.ARCHIVE.*", "SGARCH", "Archive data to archive pool");
        acsStorgrp.setDefaultClass("SGPRIM");
        config_->addACSRoutine(acsStorgrp);
        
        stats_.configurationChanges++;
    }
    
    // Statistics
    const Statistics& getStatistics() const { return stats_; }
    
    void resetStatistics() {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_ = Statistics{};
    }
    
    // Configuration summary
    std::string getConfigurationSummary() const {
        std::ostringstream oss;
        oss << "SMS Configuration Summary\n"
            << "========================\n"
            << "Storage Classes:    " << config_->getStorageClassCount() << "\n"
            << "Data Classes:       " << config_->getDataClassCount() << "\n"
            << "Management Classes: " << config_->getManagementClassCount() << "\n"
            << "Storage Groups:     " << config_->getStorageGroupCount() << "\n"
            << "ACS Routines:       " << config_->getACSRoutineCount() << "\n"
            << "\n" << stats_.toString();
        return oss.str();
    }

private:
    std::unique_ptr<SMSConfiguration> config_;
    Statistics stats_;
    mutable std::mutex statsMutex_;
    
    ACSResult executeACSForType(const AllocationRequest& request, ACSRoutineType type) {
        for (auto& [name, acs] : config_->getACSRoutines()) {
            if (acs.getType() == type && acs.isActive()) {
                stats_.acsRoutineExecutions++;
                return acs.execute(request);
            }
        }
        return ACSResult::noMatch("No active ACS routine for type");
    }
};

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a high-performance storage class
 */
inline StorageClass createHighPerformanceStorageClass(const std::string& name) {
    StorageClass sc(name, "High performance storage");
    sc.setPerformanceObjective(PerformanceObjective::HIGH)
      .setAvailabilityObjective(AvailabilityObjective::HIGH);
    return sc;
}

/**
 * @brief Create a standard storage class
 */
inline StorageClass createStandardStorageClass(const std::string& name) {
    StorageClass sc(name, "Standard storage");
    sc.setPerformanceObjective(PerformanceObjective::MEDIUM)
      .setAvailabilityObjective(AvailabilityObjective::STANDARD);
    return sc;
}

/**
 * @brief Create an archive storage class
 */
inline StorageClass createArchiveStorageClass(const std::string& name) {
    StorageClass sc(name, "Archive storage");
    sc.setPerformanceObjective(PerformanceObjective::LOW)
      .setAvailabilityObjective(AvailabilityObjective::NONE);
    return sc;
}

/**
 * @brief Create a standard management class with daily backups
 */
inline ManagementClass createStandardManagementClass(const std::string& name) {
    ManagementClass mc(name, "Standard management with daily backup");
    mc.enableAutoBackup(BackupFrequency::DAILY)
      .enableAutoMigration(30, 60)
      .setRetentionDays(365);
    return mc;
}

/**
 * @brief Create a critical data management class
 */
inline ManagementClass createCriticalManagementClass(const std::string& name) {
    ManagementClass mc(name, "Critical data - no migration, frequent backup");
    BackupPolicy bp;
    bp.frequency = BackupFrequency::DAILY;
    bp.backupVersionsLimit = 10;
    bp.autoBackup = true;
    mc.setBackupPolicy(bp);
    
    MigrationPolicy mp;
    mp.autoMigrate = false;
    mc.setMigrationPolicy(mp);
    
    return mc;
}

/**
 * @brief Create a temporary data management class
 */
inline ManagementClass createTemporaryManagementClass(const std::string& name) {
    ManagementClass mc(name, "Temporary data - short retention, no backup");
    mc.setRetentionDays(7);
    
    BackupPolicy bp;
    bp.autoBackup = false;
    mc.setBackupPolicy(bp);
    
    return mc;
}

} // namespace sms
} // namespace dfsmshsm

#endif // HSM_SMS_HPP
