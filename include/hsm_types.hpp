/**
 * @file hsm_types.hpp
 * @brief Core type definitions for DFSMShsm Emulator
 * @version 4.2.0
 * @date 2025
 * @author Bennie Shearer
 * 
 * Copyright (c) 2025 Bennie Shearer
 * MIT License - see LICENSE file for details
 * 
 * This file contains all core type definitions, enumerations, and structures
 * used throughout the DFSMShsm hierarchical storage management emulator.
 */

#ifndef DFSMSHSM_HSM_TYPES_HPP
#define DFSMSHSM_HSM_TYPES_HPP

#include <string>
#include <cstdint>
#include <chrono>
#include <vector>
#include <optional>
#include <functional>
#include <atomic>
#include <sstream>
#include <array>
#include <map>

// Windows macro conflict resolution
// These Windows error codes conflict with our enum values
#ifdef _WIN32
#undef ERROR_INVALID_STATE
#undef ERROR_NO_SPACE
#undef ERROR_IO_ERROR
#undef ERROR_PERMISSION_DENIED
#undef ERROR_INVALID_PARAMETER
#undef ERROR_OPERATION_FAILED
#undef ERROR_TIMEOUT
#undef ERROR_RESOURCE_BUSY
#undef ERROR_NOT_SUPPORTED
#undef ERROR_CHECKSUM_MISMATCH
#undef ERROR_QUOTA_EXCEEDED
#undef ERROR_ENCRYPTION_FAILED
#undef ERROR_JOURNAL_CORRUPT
#undef ERROR_REPLICATION_FAILED
#undef ERROR_DEDUPE_FAILED
#endif

namespace dfsmshsm {

//=============================================================================
// Version Information
//=============================================================================
constexpr const char* VERSION = "4.2.0";
constexpr int VERSION_MAJOR = 4;
constexpr int VERSION_MINOR = 2;
constexpr int VERSION_PATCH = 0;
constexpr const char* VERSION_DATE = "2025-01-06";
constexpr const char* COPYRIGHT = "Copyright (c) 2025 Bennie Shearer";

//=============================================================================
// Core Enumerations
//=============================================================================

/**
 * @enum StorageTier
 * @brief Represents the hierarchical storage levels
 */
enum class StorageTier { 
    PRIMARY,        ///< Fast primary storage (SSD/NVMe)
    MIGRATION_1,    ///< First migration level (HDD)
    MIGRATION_2,    ///< Second migration level (slower HDD)
    TAPE,           ///< Tape storage
    BACKUP,         ///< Backup storage
    CLOUD,          ///< Cloud storage
    ARCHIVE         ///< Long-term archive (v4.1.0)
};

/**
 * @enum DatasetState
 * @brief Represents the current state of a dataset
 */
enum class DatasetState { 
    ACTIVE,         ///< Available on primary storage
    MIGRATED,       ///< Migrated to lower tier
    RECALLED,       ///< Recently recalled to primary
    BACKED_UP,      ///< Has backup copy
    ARCHIVED,       ///< Archived for long-term retention
    DELETED,        ///< Marked for deletion
    COMPRESSED,     ///< Compressed on storage
    CACHED,         ///< Cached in memory
    ENCRYPTED,      ///< Encrypted on storage (v4.1.0)
    REPLICATED,     ///< Replicated across tiers (v4.1.0)
    SNAPSHOT,       ///< Point-in-time snapshot (v4.1.0)
    DEDUPLICATED    ///< Content deduplicated (v4.1.0)
};

/**
 * @enum OperationType
 * @brief Types of HSM operations (OP_ prefix avoids Windows macro conflicts)
 */
enum class OperationType { 
    OP_MIGRATE,     ///< Migrate to lower tier
    OP_RECALL,      ///< Recall to primary tier
    OP_BACKUP,      ///< Create backup copy
    OP_RECOVER,     ///< Recover from backup
    OP_DELETE,      ///< Delete dataset
    OP_DUMP,        ///< Dump to external media
    OP_RESTORE,     ///< Restore from dump
    OP_AUDIT,       ///< Audit operation
    OP_COMPRESS,    ///< Compress data
    OP_DECOMPRESS,  ///< Decompress data
    OP_DEFRAG,      ///< Defragment volume
    OP_FREEVOL,     ///< Free volume space
    OP_ARCHDEL,     ///< Archive delete
    OP_HEALTH_CHECK,///< Health check
    OP_ENCRYPT,     ///< Encrypt dataset (v4.1.0)
    OP_DECRYPT,     ///< Decrypt dataset (v4.1.0)
    OP_REPLICATE,   ///< Replicate dataset (v4.1.0)
    OP_SNAPSHOT,    ///< Create snapshot (v4.1.0)
    OP_DEDUPE,      ///< Deduplication (v4.1.0)
    OP_EXPIRE,      ///< Expiration processing (v4.1.0)
    OP_QUOTA_CHECK  ///< Quota check (v4.1.0)
};

/**
 * @enum OperationStatus
 * @brief Status of an operation
 */
enum class OperationStatus { 
    PENDING,        ///< Waiting to start
    IN_PROGRESS,    ///< Currently executing
    COMPLETED,      ///< Successfully completed
    FAILED,         ///< Failed with error
    CANCELLED,      ///< Cancelled by user
    QUEUED,         ///< In operation queue
    SCHEDULED,      ///< Scheduled for future execution
    RETRYING,       ///< Retrying after failure (v4.1.0)
    PARTIAL         ///< Partially completed (v4.1.0)
};

/**
 * @enum MigrationCriteria
 * @brief Criteria for automatic migration
 */
enum class MigrationCriteria { 
    AGE,            ///< Based on last access age
    SIZE,           ///< Based on dataset size
    SPACE_PRESSURE, ///< Based on available space
    MANUAL,         ///< Manual migration
    SCHEDULED,      ///< Scheduled migration
    POLICY,         ///< Policy-based migration
    FREQUENCY,      ///< Based on access frequency (v4.1.0)
    COST            ///< Based on storage cost (v4.1.0)
};

/**
 * @enum EventType
 * @brief Types of system events
 */
enum class EventType { 
    DATASET_CREATED,
    DATASET_DELETED,
    DATASET_MIGRATED,
    DATASET_RECALLED,
    DATASET_BACKED_UP,
    DATASET_ENCRYPTED,      ///< v4.1.0
    DATASET_REPLICATED,     ///< v4.1.0
    DATASET_SNAPSHOT,       ///< v4.1.0
    DATASET_EXPIRED,        ///< v4.1.0
    VOLUME_CREATED,
    VOLUME_DELETED,
    VOLUME_FULL,
    SPACE_THRESHOLD_EXCEEDED,
    HEALTH_WARNING,
    HEALTH_CRITICAL,
    OPERATION_STARTED,
    OPERATION_COMPLETED,
    OPERATION_FAILED,
    SYSTEM_STARTUP,
    SYSTEM_SHUTDOWN,
    QUOTA_WARNING,          ///< v4.1.0
    QUOTA_EXCEEDED,         ///< v4.1.0
    JOURNAL_CHECKPOINT,     ///< v4.1.0
    REPLICATION_SYNC,       ///< v4.1.0
    SCHEDULE_TRIGGERED      ///< v4.1.0
};

/**
 * @enum HealthStatus
 * @brief System health status
 */
enum class HealthStatus { 
    HEALTHY,        ///< All systems normal
    WARNING,        ///< Some warnings present
    CRITICAL,       ///< Critical issues detected
    UNKNOWN,        ///< Status unknown
    DEGRADED        ///< Degraded performance (v4.1.0)
};

/**
 * @enum CompressionType
 * @brief Compression algorithms
 */
enum class CompressionType { 
    NONE,           ///< No compression
    RLE,            ///< Run-length encoding
    LZ4,            ///< LZ4 fast compression (v4.1.0)
    ZSTD            ///< Zstandard (v4.1.0)
};

/**
 * @enum EncryptionType
 * @brief Encryption algorithms (v4.1.0)
 */
enum class EncryptionType {
    NONE,           ///< No encryption
    AES_128,        ///< AES 128-bit
    AES_256,        ///< AES 256-bit
    XOR_SIMPLE      ///< Simple XOR (for testing)
};

/**
 * @enum JournalEntryType
 * @brief Journal entry types (v4.1.0)
 */
enum class JournalEntryType {
    BEGIN_TXN,      ///< Transaction begin
    COMMIT_TXN,     ///< Transaction commit
    ROLLBACK_TXN,   ///< Transaction rollback
    CHECKPOINT,     ///< Checkpoint
    DATA_WRITE,     ///< Data write
    METADATA_UPDATE ///< Metadata update
};

/**
 * @enum ScheduleFrequency
 * @brief Schedule frequency types (v4.1.0)
 */
enum class ScheduleFrequency {
    ONCE,           ///< Run once
    HOURLY,         ///< Every hour
    DAILY,          ///< Every day
    WEEKLY,         ///< Every week
    MONTHLY,        ///< Every month
    CUSTOM          ///< Custom CRON expression
};

/**
 * @enum QuotaCheckResult
 * @brief Result of quota check (v4.1.0)
 */
enum class QuotaCheckResult {
    OK,                     ///< Within limits
    SOFT_EXCEEDED,          ///< Soft limit exceeded (warning)
    HARD_EXCEEDED,          ///< Hard limit exceeded (blocked)
    GRACE_EXPIRED,          ///< Soft limit + grace period exceeded
    NO_QUOTA                ///< No quota defined
};

/**
 * @enum HSMReturnCode
 * @brief Detailed return codes
 */
enum class HSMReturnCode { 
    SUCCESS = 0,
    ERR_NOT_FOUND,
    ERR_ALREADY_EXISTS,
    ERROR_INVALID_STATE,
    ERROR_NO_SPACE,
    ERROR_IO_ERROR,
    ERROR_PERMISSION_DENIED,
    ERROR_INVALID_PARAMETER,
    ERROR_OPERATION_FAILED,
    ERROR_TIMEOUT,
    ERROR_RESOURCE_BUSY,
    ERROR_NOT_SUPPORTED,
    ERROR_CHECKSUM_MISMATCH,
    ERROR_QUOTA_EXCEEDED,       ///< v4.1.0
    ERROR_ENCRYPTION_FAILED,    ///< v4.1.0
    ERROR_JOURNAL_CORRUPT,      ///< v4.1.0
    ERROR_REPLICATION_FAILED,   ///< v4.1.0
    ERROR_DEDUPE_FAILED         ///< v4.1.0
};

//=============================================================================
// Core Structures
//=============================================================================

/**
 * @struct DatasetInfo
 * @brief Complete information about a dataset
 */
struct DatasetInfo {
    std::string name;                       ///< Dataset name
    std::string volume;                     ///< Current volume
    std::string backup_location;            ///< Backup volume
    std::string owner;                      ///< Owner ID (v4.1.0)
    std::string group;                      ///< Group ID (v4.1.0)
    std::string dsorg = "PS";               ///< Dataset organization (v4.1.0)
    std::string encryption_key_id;          ///< Encryption key identifier (v4.1.0)
    uint64_t size = 0;                      ///< Original size in bytes
    uint64_t compressed_size = 0;           ///< Compressed size
    uint64_t dedupe_size = 0;               ///< Size after deduplication (v4.1.0)
    uint32_t record_length = 80;            ///< Record length (v4.1.0)
    uint32_t block_size = 6160;             ///< Block size (v4.1.0)
    int retention_days = 0;                 ///< Retention period in days (v4.1.0)
    StorageTier tier = StorageTier::PRIMARY;
    DatasetState state = DatasetState::ACTIVE;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point last_accessed;
    std::chrono::system_clock::time_point last_modified;
    std::chrono::system_clock::time_point migrated_time;
    std::chrono::system_clock::time_point expiration_time;  ///< v4.1.0
    uint32_t access_count = 0;
    uint32_t checksum = 0;
    uint32_t version = 1;
    uint32_t replica_count = 0;             ///< v4.1.0
    uint32_t snapshot_count = 0;            ///< v4.1.0
    bool has_backup = false;
    bool is_encrypted = false;              ///< v4.1.0
    bool is_replicated = false;             ///< v4.1.0
    bool is_deduplicated = false;           ///< v4.1.0
    bool compressed = false;                ///< Compression enabled flag (v4.1.0)
    bool encrypted = false;                 ///< Encryption enabled flag (v4.1.0)
    bool deduplicated = false;              ///< Deduplication enabled flag (v4.1.0)
    CompressionType compression = CompressionType::NONE;
    EncryptionType encryption = EncryptionType::NONE;   ///< v4.1.0
    std::string content_hash;               ///< For deduplication (v4.1.0)
    std::vector<std::string> replica_locations; ///< v4.1.0
    std::vector<std::string> tags;          ///< Dataset tags (v4.1.0)
    std::map<std::string, std::string> metadata; ///< Custom metadata (v4.1.0)
};

/**
 * @struct VolumeInfo
 * @brief Information about a storage volume
 */
struct VolumeInfo {
    std::string name;
    std::string device_type;                ///< Device type (v4.1.0)
    std::string storage_group;              ///< Storage group (v4.1.0)
    StorageTier tier = StorageTier::PRIMARY;
    uint64_t total_space = 0;
    uint64_t used_space = 0;
    uint64_t available_space = 0;
    uint64_t free_space = 0;                ///< Alias for available_space (v4.1.0)
    uint64_t reserved_space = 0;            ///< v4.1.0
    bool online = true;
    bool read_only = false;                 ///< v4.1.0
    std::vector<std::string> datasets;
    double fragmentation_percent = 0.0;
    uint32_t dataset_count = 0;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point last_accessed;
};

/**
 * @struct MigrationPolicy
 * @brief Policy for automatic migration
 */
struct MigrationPolicy {
    MigrationCriteria criteria = MigrationCriteria::AGE;
    uint32_t age_threshold_days = 30;
    uint32_t space_pressure_percent = 80;
    uint64_t size_threshold_bytes = 104857600;  // 100MB
    StorageTier source_tier = StorageTier::PRIMARY;
    StorageTier target_tier = StorageTier::MIGRATION_1;
    bool enabled = true;
    bool compress_on_migrate = false;
    bool encrypt_on_migrate = false;        ///< v4.1.0
    bool dedupe_on_migrate = false;         ///< v4.1.0
    std::string schedule;
    uint32_t max_concurrent = 4;            ///< v4.1.0
    uint32_t priority = 5;                  ///< v4.1.0
};

/**
 * @struct BackupPolicy
 * @brief Policy for automatic backup
 */
struct BackupPolicy {
    uint32_t retention_days = 30;
    uint32_t frequency_hours = 24;
    uint32_t max_versions = 5;
    bool incremental = false;
    bool compress = true;
    bool encrypt = false;                   ///< v4.1.0
    bool verify_after_backup = true;        ///< v4.1.0
    std::string backup_volume;
    std::vector<std::string> exclude_patterns;  ///< v4.1.0
};

/**
 * @struct QuotaInfo
 * @brief Quota information for users/groups (v4.1.0)
 */
struct QuotaInfo {
    std::string id;                         ///< User or group ID
    bool is_group = false;                  ///< True if group quota
    uint64_t space_limit = 0;               ///< Maximum space in bytes
    uint64_t space_used = 0;                ///< Current space used
    uint64_t inode_limit = 0;               ///< Maximum number of datasets
    uint64_t inode_used = 0;                ///< Current dataset count
    uint64_t soft_limit = 0;                ///< Soft limit (warning threshold)
    uint32_t grace_period_days = 7;         ///< Days before hard enforcement
    std::chrono::system_clock::time_point soft_exceeded_time;
    bool enabled = true;
};

/**
 * @struct ReplicationPolicy
 * @brief Replication policy (v4.1.0)
 */
struct ReplicationPolicy {
    uint32_t replica_count = 2;             ///< Number of replicas
    std::vector<StorageTier> target_tiers;  ///< Target tiers for replicas
    bool sync_replication = false;          ///< Synchronous replication
    uint32_t sync_interval_seconds = 3600;  ///< Async sync interval
    bool verify_replicas = true;            ///< Verify replica integrity
    bool enabled = false;
};

/**
 * @struct LifecyclePolicy
 * @brief Data lifecycle policy (v4.1.0)
 */
struct LifecyclePolicy {
    std::string name;
    uint32_t tier_down_days = 30;           ///< Days before migration
    uint32_t archive_days = 365;            ///< Days before archival
    uint32_t expiration_days = 0;           ///< Days before expiration (0=never)
    bool compress_on_archive = true;
    bool encrypt_on_archive = true;
    bool delete_on_expiration = true;
    bool enabled = false;
    std::vector<std::string> apply_to_patterns;
    StorageTier target_tier = StorageTier::MIGRATION_1;  ///< Target tier for migration
    std::string pattern;                    ///< Single pattern (convenience)
    
    // Compatibility aliases
    uint32_t days_to_migration = 30;        ///< Alias for tier_down_days
    uint32_t days_to_archive = 365;         ///< Alias for archive_days
    uint32_t days_to_expiration = 0;        ///< Alias for expiration_days
    uint32_t days_before_migration = 30;    ///< Test compatibility alias
    std::vector<std::string> patterns;      ///< Alias for apply_to_patterns
};

/**
 * @struct SnapshotInfo
 * @brief Snapshot information (v4.1.0)
 */
struct SnapshotInfo {
    std::string id;
    std::string dataset_name;
    std::string description;
    uint64_t size = 0;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point expiration;
    uint32_t version = 0;
    bool is_consistent = true;
    std::string content_hash;
};

/**
 * @struct JournalEntry
 * @brief Write-ahead log entry (v4.1.0)
 */
struct JournalEntry {
    uint64_t sequence_number = 0;
    uint64_t transaction_id = 0;
    JournalEntryType type = JournalEntryType::DATA_WRITE;
    std::string target;
    std::vector<uint8_t> data;
    uint32_t checksum = 0;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct ScheduledTask
 * @brief Scheduled task definition (v4.1.0)
 */
struct ScheduledTask {
    std::string id;
    std::string name;
    OperationType operation = OperationType::OP_MIGRATE;
    ScheduleFrequency frequency = ScheduleFrequency::DAILY;
    std::string cron_expression;            ///< For custom schedules
    std::chrono::system_clock::time_point next_run;
    std::chrono::system_clock::time_point last_run;
    uint32_t run_count = 0;
    uint32_t failure_count = 0;
    bool enabled = true;
    std::map<std::string, std::string> parameters;
};

/**
 * @struct DedupeInfo
 * @brief Deduplication information (v4.1.0)
 */
struct DedupeInfo {
    std::string content_hash;               ///< SHA-256 hash
    uint64_t original_size = 0;
    uint64_t dedupe_size = 0;
    uint32_t reference_count = 0;           ///< Number of references
    std::vector<std::string> referencing_datasets;
    std::chrono::system_clock::time_point created;
};

/**
 * @struct OperationResult
 * @brief Result of an HSM operation
 */
struct OperationResult {
    OperationType type = OperationType::OP_MIGRATE;
    OperationStatus status = OperationStatus::PENDING;
    HSMReturnCode return_code = HSMReturnCode::SUCCESS;  ///< v4.1.0
    std::string message;
    std::string operation_id;
    std::chrono::system_clock::time_point timestamp;
    uint64_t bytes_processed = 0;
    std::chrono::milliseconds duration{0};
    std::map<std::string, std::string> details;  ///< v4.1.0
    
    [[nodiscard]] explicit operator bool() const {
        return status == OperationStatus::COMPLETED || 
               return_code == HSMReturnCode::SUCCESS;
    }
    
    [[nodiscard]] bool isSuccess() const {
        return return_code == HSMReturnCode::SUCCESS;
    }
};

/**
 * @struct OperationRecord
 * @brief Complete record of an HSM operation (v4.1.0)
 */
struct OperationRecord {
    std::string operation_id;
    OperationType type = OperationType::OP_MIGRATE;
    OperationStatus status = OperationStatus::PENDING;
    std::string dataset_name;
    std::string source_volume;
    std::string target_volume;
    StorageTier source_tier = StorageTier::PRIMARY;
    StorageTier target_tier = StorageTier::MIGRATION_1;
    uint64_t bytes_processed = 0;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::string error_message;
    std::string user;
    int priority = 0;
};

/**
 * @struct HSMStatistics
 * @brief System-wide statistics
 */
struct HSMStatistics {
    uint64_t total_datasets = 0;
    uint64_t active_datasets = 0;
    uint64_t migrated_datasets = 0;
    uint64_t backed_up_datasets = 0;
    uint64_t encrypted_datasets = 0;        ///< v4.1.0
    uint64_t replicated_datasets = 0;       ///< v4.1.0
    uint64_t deduplicated_datasets = 0;     ///< v4.1.0
    uint64_t total_migrations = 0;
    uint64_t total_recalls = 0;
    uint64_t total_backups = 0;
    uint64_t total_snapshots = 0;           ///< v4.1.0
    uint64_t bytes_migrated = 0;
    uint64_t bytes_recalled = 0;
    uint64_t bytes_backed_up = 0;
    uint64_t bytes_dedupe_saved = 0;        ///< v4.1.0
    uint64_t cache_hits = 0;
    uint64_t cache_misses = 0;
    uint64_t compression_savings = 0;
    double space_reclaimed_percent = 0.0;
    double dedupe_ratio = 0.0;              ///< v4.1.0
    std::chrono::system_clock::time_point last_updated;
};

/**
 * @struct HealthReport
 * @brief System health report
 */
struct HealthReport {
    HealthStatus overall_status = HealthStatus::UNKNOWN;
    double cpu_usage_percent = 0.0;
    double memory_usage_percent = 0.0;
    double primary_space_percent = 0.0;
    double iops_current = 0.0;              ///< v4.1.0
    double throughput_mbps = 0.0;           ///< v4.1.0
    uint32_t pending_operations = 0;
    uint32_t failed_operations_24h = 0;
    uint32_t journal_backlog = 0;           ///< v4.1.0
    uint32_t replication_lag_seconds = 0;   ///< v4.1.0
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct Event
 * @brief System event
 */
struct Event {
    EventType type;
    std::string source;
    std::string message;
    std::string dataset_name;
    std::string volume_name;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> details;  ///< v4.1.0
};

/**
 * @struct AuditRecord
 * @brief Audit log record
 */
struct AuditRecord {
    std::string operation_id;
    std::string user;
    std::string target;
    std::string details;
    std::string source_ip;                  ///< v4.1.0
    OperationType operation;
    OperationStatus status;
    std::chrono::system_clock::time_point timestamp;
};

//=============================================================================
// Type Aliases
//=============================================================================

using EventCallback = std::function<void(const Event&)>;
using ProgressCallback = std::function<void(uint64_t current, uint64_t total)>;
using ContentHash = std::array<uint8_t, 32>;  ///< SHA-256 hash

//=============================================================================
// Utility Functions
//=============================================================================

inline std::string tierToString(StorageTier t) {
    switch(t) {
        case StorageTier::PRIMARY: return "PRIMARY";
        case StorageTier::MIGRATION_1: return "MIGRATION-1";
        case StorageTier::MIGRATION_2: return "MIGRATION-2";
        case StorageTier::TAPE: return "TAPE";
        case StorageTier::BACKUP: return "BACKUP";
        case StorageTier::CLOUD: return "CLOUD";
        case StorageTier::ARCHIVE: return "ARCHIVE";
    }
    return "UNKNOWN";
}

inline std::optional<StorageTier> stringToTier(const std::string& s) {
    if (s == "PRIMARY") return StorageTier::PRIMARY;
    if (s == "ML1" || s == "MIGRATION-1") return StorageTier::MIGRATION_1;
    if (s == "ML2" || s == "MIGRATION-2") return StorageTier::MIGRATION_2;
    if (s == "TAPE") return StorageTier::TAPE;
    if (s == "BACKUP") return StorageTier::BACKUP;
    if (s == "CLOUD") return StorageTier::CLOUD;
    if (s == "ARCHIVE") return StorageTier::ARCHIVE;
    return std::nullopt;
}

inline std::string stateToString(DatasetState s) {
    switch(s) {
        case DatasetState::ACTIVE: return "ACTIVE";
        case DatasetState::MIGRATED: return "MIGRATED";
        case DatasetState::RECALLED: return "RECALLED";
        case DatasetState::BACKED_UP: return "BACKED_UP";
        case DatasetState::ARCHIVED: return "ARCHIVED";
        case DatasetState::DELETED: return "DELETED";
        case DatasetState::COMPRESSED: return "COMPRESSED";
        case DatasetState::CACHED: return "CACHED";
        case DatasetState::ENCRYPTED: return "ENCRYPTED";
        case DatasetState::REPLICATED: return "REPLICATED";
        case DatasetState::SNAPSHOT: return "SNAPSHOT";
        case DatasetState::DEDUPLICATED: return "DEDUPLICATED";
    }
    return "UNKNOWN";
}

inline std::string operationToString(OperationType o) {
    switch(o) {
        case OperationType::OP_MIGRATE: return "MIGRATE";
        case OperationType::OP_RECALL: return "RECALL";
        case OperationType::OP_BACKUP: return "BACKUP";
        case OperationType::OP_RECOVER: return "RECOVER";
        case OperationType::OP_DELETE: return "DELETE";
        case OperationType::OP_DUMP: return "DUMP";
        case OperationType::OP_RESTORE: return "RESTORE";
        case OperationType::OP_AUDIT: return "AUDIT";
        case OperationType::OP_COMPRESS: return "COMPRESS";
        case OperationType::OP_DECOMPRESS: return "DECOMPRESS";
        case OperationType::OP_DEFRAG: return "DEFRAG";
        case OperationType::OP_FREEVOL: return "FREEVOL";
        case OperationType::OP_ARCHDEL: return "ARCHDEL";
        case OperationType::OP_HEALTH_CHECK: return "HEALTH_CHECK";
        case OperationType::OP_ENCRYPT: return "ENCRYPT";
        case OperationType::OP_DECRYPT: return "DECRYPT";
        case OperationType::OP_REPLICATE: return "REPLICATE";
        case OperationType::OP_SNAPSHOT: return "SNAPSHOT";
        case OperationType::OP_DEDUPE: return "DEDUPE";
        case OperationType::OP_EXPIRE: return "EXPIRE";
        case OperationType::OP_QUOTA_CHECK: return "QUOTA_CHECK";
    }
    return "UNKNOWN";
}

inline std::string statusToString(OperationStatus s) {
    switch(s) {
        case OperationStatus::PENDING: return "PENDING";
        case OperationStatus::IN_PROGRESS: return "IN_PROGRESS";
        case OperationStatus::COMPLETED: return "COMPLETED";
        case OperationStatus::FAILED: return "FAILED";
        case OperationStatus::CANCELLED: return "CANCELLED";
        case OperationStatus::QUEUED: return "QUEUED";
        case OperationStatus::SCHEDULED: return "SCHEDULED";
        case OperationStatus::RETRYING: return "RETRYING";
        case OperationStatus::PARTIAL: return "PARTIAL";
    }
    return "UNKNOWN";
}

inline std::string healthToString(HealthStatus h) {
    switch(h) {
        case HealthStatus::HEALTHY: return "HEALTHY";
        case HealthStatus::WARNING: return "WARNING";
        case HealthStatus::CRITICAL: return "CRITICAL";
        case HealthStatus::UNKNOWN: return "UNKNOWN";
        case HealthStatus::DEGRADED: return "DEGRADED";
    }
    return "UNKNOWN";
}

inline std::string returnCodeToString(HSMReturnCode rc) {
    switch(rc) {
        case HSMReturnCode::SUCCESS: return "SUCCESS";
        case HSMReturnCode::ERR_NOT_FOUND: return "NOT_FOUND";
        case HSMReturnCode::ERR_ALREADY_EXISTS: return "ALREADY_EXISTS";
        case HSMReturnCode::ERROR_INVALID_STATE: return "INVALID_STATE";
        case HSMReturnCode::ERROR_NO_SPACE: return "NO_SPACE";
        case HSMReturnCode::ERROR_IO_ERROR: return "IO_ERROR";
        case HSMReturnCode::ERROR_PERMISSION_DENIED: return "PERMISSION_DENIED";
        case HSMReturnCode::ERROR_INVALID_PARAMETER: return "INVALID_PARAMETER";
        case HSMReturnCode::ERROR_OPERATION_FAILED: return "OPERATION_FAILED";
        case HSMReturnCode::ERROR_TIMEOUT: return "TIMEOUT";
        case HSMReturnCode::ERROR_RESOURCE_BUSY: return "RESOURCE_BUSY";
        case HSMReturnCode::ERROR_NOT_SUPPORTED: return "NOT_SUPPORTED";
        case HSMReturnCode::ERROR_CHECKSUM_MISMATCH: return "CHECKSUM_MISMATCH";
        case HSMReturnCode::ERROR_QUOTA_EXCEEDED: return "QUOTA_EXCEEDED";
        case HSMReturnCode::ERROR_ENCRYPTION_FAILED: return "ENCRYPTION_FAILED";
        case HSMReturnCode::ERROR_JOURNAL_CORRUPT: return "JOURNAL_CORRUPT";
        case HSMReturnCode::ERROR_REPLICATION_FAILED: return "REPLICATION_FAILED";
        case HSMReturnCode::ERROR_DEDUPE_FAILED: return "DEDUPE_FAILED";
    }
    return "UNKNOWN";
}

inline std::string encryptionTypeToString(EncryptionType e) {
    switch(e) {
        case EncryptionType::NONE: return "NONE";
        case EncryptionType::AES_128: return "AES-128";
        case EncryptionType::AES_256: return "AES-256";
        case EncryptionType::XOR_SIMPLE: return "XOR";
    }
    return "UNKNOWN";
}

inline std::string frequencyToString(ScheduleFrequency f) {
    switch(f) {
        case ScheduleFrequency::ONCE: return "ONCE";
        case ScheduleFrequency::HOURLY: return "HOURLY";
        case ScheduleFrequency::DAILY: return "DAILY";
        case ScheduleFrequency::WEEKLY: return "WEEKLY";
        case ScheduleFrequency::MONTHLY: return "MONTHLY";
        case ScheduleFrequency::CUSTOM: return "CUSTOM";
    }
    return "UNKNOWN";
}

inline std::string quotaResultToString(QuotaCheckResult r) {
    switch(r) {
        case QuotaCheckResult::OK: return "OK";
        case QuotaCheckResult::SOFT_EXCEEDED: return "SOFT_EXCEEDED";
        case QuotaCheckResult::HARD_EXCEEDED: return "HARD_EXCEEDED";
        case QuotaCheckResult::GRACE_EXPIRED: return "GRACE_EXPIRED";
        case QuotaCheckResult::NO_QUOTA: return "NO_QUOTA";
    }
    return "UNKNOWN";
}

inline std::string generateOperationId() {
    static std::atomic<uint64_t> counter{0};
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream oss;
    oss << "OP-" << ms << "-" << ++counter;
    return oss.str();
}

inline std::string formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int unit = 0;
    double value = static_cast<double>(bytes);
    while (value >= 1024.0 && unit < 5) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value << " " << units[unit];
    return oss.str();
}

inline std::string formatDuration(std::chrono::milliseconds ms) {
    auto total_seconds = ms.count() / 1000;
    auto hours = total_seconds / 3600;
    auto minutes = (total_seconds % 3600) / 60;
    auto seconds = total_seconds % 60;
    std::ostringstream oss;
    if (hours > 0) oss << hours << "h ";
    if (minutes > 0 || hours > 0) oss << minutes << "m ";
    oss << seconds << "s";
    return oss.str();
}

} // namespace dfsmshsm
#endif // DFSMSHSM_HSM_TYPES_HPP
