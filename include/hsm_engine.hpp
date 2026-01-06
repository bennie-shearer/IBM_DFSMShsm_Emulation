/**
 * @file hsm_engine.hpp
 * @brief Main HSM Engine coordinating all subsystems
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 * 
 * The HSMEngine class is the main coordinator for the DFSMShsm emulator.
 * It integrates all subsystems including:
 * - Dataset and Volume management
 * - Migration and Recall
 * - Backup and Recovery
 * - Deduplication (NEW in v4.1.0)
 * - Quota Management (NEW in v4.1.0)
 * - Journaling (NEW in v4.1.0)
 * - Encryption (NEW in v4.1.0)
 * - Snapshots (NEW in v4.1.0)
 * - Replication (NEW in v4.1.0)
 * - Scheduling (NEW in v4.1.0)
 */
#ifndef DFSMSHSM_HSM_ENGINE_HPP

// Windows: Include rest_api.hpp first to ensure winsock2.h comes before windows.h
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#define DFSMSHSM_HSM_ENGINE_HPP

#include "hsm_types.hpp"
#include "dataset_catalog.hpp"
#include "volume_manager.hpp"
#include "migration_manager.hpp"
#include "recall_manager.hpp"
#include "backup_manager.hpp"
#include "health_monitor.hpp"
#include "deduplication_manager.hpp"
#include "quota_manager.hpp"
#include "journal_manager.hpp"
#include "encryption_manager.hpp"
#include "snapshot_manager.hpp"
#include "replication_manager.hpp"
#include "scheduler.hpp"
#include "compression.hpp"
#include "metrics_exporter.hpp"
#include "rest_api.hpp"
#include "compression.hpp"
#include "metrics_exporter.hpp"
#include "rest_api.hpp"
#include "event_system.hpp"
#include "hsm_logger.hpp"
#include "audit_logger.hpp"
#include "hsm_statistics.hpp"
#include "cache_manager.hpp"

#include <memory>
#include <filesystem>
#include <atomic>

namespace dfsmshsm {

/**
 * @class HSMEngine
 * @brief Main engine coordinating all HSM subsystems
 */
class HSMEngine {
public:
    HSMEngine();
    ~HSMEngine();
    HSMEngine(const HSMEngine&) = delete;
    HSMEngine& operator=(const HSMEngine&) = delete;
    
    // Lifecycle
    bool initialize(const std::filesystem::path& data_dir);
    bool start();
    void stop();
    bool isRunning() const { return running_; }
    static std::string getVersion() { return VERSION; }
    
    // Dataset Operations
    OperationResult createDataset(const std::string& name, uint64_t size,
                                   const std::string& volume = "",
                                   const std::string& owner = "SYSTEM",
                                   const std::string& group = "");
    OperationResult deleteDataset(const std::string& name);
    std::optional<DatasetInfo> getDataset(const std::string& name) const;
    std::vector<DatasetInfo> getAllDatasets() const;
    std::vector<DatasetInfo> getDatasetsByTier(StorageTier tier) const;
    std::vector<DatasetInfo> getDatasetsByState(DatasetState state) const;
    
    // Migration
    OperationResult migrateDataset(const std::string& name, StorageTier target);
    OperationResult runAutomaticMigration();
    void setMigrationPolicy(const MigrationPolicy& policy);
    MigrationPolicy getMigrationPolicy() const;
    
    // Recall
    OperationResult recallDataset(const std::string& name);
    
    // Backup
    OperationResult backupDataset(const std::string& name);
    OperationResult recoverDataset(const std::string& name);
    OperationResult runAutomaticBackup();
    void setBackupPolicy(const BackupPolicy& policy);
    BackupPolicy getBackupPolicy() const;
    
    // Volume
    bool createVolume(const std::string& name, StorageTier tier, uint64_t size);
    bool deleteVolume(const std::string& name);
    std::vector<VolumeInfo> getVolumes() const;
    std::vector<VolumeInfo> getVolumesByTier(StorageTier tier) const;
    
    // Deduplication (v4.1.0)
    OperationResult deduplicateDataset(const std::string& name, const std::vector<uint8_t>& data);
    std::optional<DedupeInfo> getDedupeInfo(const std::string& name) const;
    size_t runDedupeGarbageCollection();
    DedupeStatistics getDedupeStatistics() const;
    
    // Quota (v4.1.0)
    bool setUserQuota(const std::string& user_id, uint64_t space_limit, uint64_t inode_limit = 0);
    bool setGroupQuota(const std::string& group_id, uint64_t space_limit, uint64_t inode_limit = 0);
    std::optional<QuotaUsage> getUserQuotaUsage(const std::string& user_id) const;
    std::optional<QuotaUsage> getGroupQuotaUsage(const std::string& group_id) const;
    QuotaCheckResult checkQuota(const std::string& user_id, const std::string& group_id, uint64_t space_needed);
    
    // Encryption (v4.1.0)
    OperationResult encryptDataset(const std::string& name, const std::string& key_id = "");
    OperationResult decryptDataset(const std::string& name);
    std::string generateEncryptionKey(const std::string& name);
    std::optional<std::string> rotateEncryptionKey(const std::string& old_key_id);
    
    // Snapshots (v4.1.0)
    std::optional<std::string> createSnapshot(const std::string& name, const std::string& description = "", uint32_t retention_days = 0);
    bool deleteSnapshot(const std::string& snapshot_id);
    OperationResult restoreFromSnapshot(const std::string& snapshot_id);
    std::vector<SnapshotInfo> getDatasetSnapshots(const std::string& name) const;
    
    // Replication (v4.1.0)
    std::optional<std::string> replicateDataset(const std::string& name, StorageTier target_tier, bool synchronous = false);
    bool deleteReplica(const std::string& replica_id);
    bool syncReplica(const std::string& replica_id);
    std::vector<ReplicaInfo> getDatasetReplicas(const std::string& name) const;
    void setReplicationPolicy(const std::string& name, const ReplicationPolicy& policy);
    
    // Scheduling (v4.1.0)
    std::string scheduleTask(const std::string& name, OperationType operation, ScheduleFrequency frequency);
    bool cancelTask(const std::string& task_id);
    std::vector<ScheduledTask> getScheduledTasks() const;
    
    // Journaling (v4.1.0)
    uint64_t beginTransaction();
    bool commitTransaction(uint64_t txn_id);
    bool rollbackTransaction(uint64_t txn_id);
    void journalCheckpoint();
    
    // Health and Monitoring
    HealthReport getHealthReport();
    HSMStatistics getStatistics() const;
    std::string getStatusReport() const;
    std::string generateFullReport() const;
    
    // Component Access
    DatasetCatalog& getCatalog() { return *catalog_; }
    VolumeManager& getVolumeManager() { return *volumes_; }
    DeduplicationManager& getDedupeManager() { return *dedupe_; }
    QuotaManager& getQuotaManager() { return *quota_; }
    JournalManager& getJournalManager() { return *journal_; }
    EncryptionManager& getEncryptionManager() { return *encryption_; }
    SnapshotManager& getSnapshotManager() { return *snapshots_; }
    ReplicationManager& getReplicationManager() { return *replication_; }
    Scheduler& getScheduler() { return *scheduler_; }

private:
    std::filesystem::path data_dir_;
    std::atomic<bool> running_;
    
    std::shared_ptr<DatasetCatalog> catalog_;
    std::shared_ptr<VolumeManager> volumes_;
    std::unique_ptr<MigrationManager> migration_;
    std::unique_ptr<RecallManager> recall_;
    std::unique_ptr<BackupManager> backup_;
    std::unique_ptr<HealthMonitor> health_;
    std::unique_ptr<CacheManager> cache_;
    
    // v4.1.0 components
    std::unique_ptr<DeduplicationManager> dedupe_;
    std::unique_ptr<QuotaManager> quota_;
    std::unique_ptr<JournalManager> journal_;
    std::unique_ptr<EncryptionManager> encryption_;
    std::unique_ptr<SnapshotManager> snapshots_;
    std::unique_ptr<ReplicationManager> replication_;
    std::unique_ptr<Scheduler> scheduler_;
};

} // namespace dfsmshsm
#endif // DFSMSHSM_HSM_ENGINE_HPP
