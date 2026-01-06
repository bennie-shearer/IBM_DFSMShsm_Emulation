/**
 * @file hsm_engine.cpp
 * @brief HSM engine implementation
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */

#include "hsm_engine.hpp"
#include "performance_monitor.hpp"
#include <sstream>
#include <iomanip>

namespace dfsmshsm {

HSMEngine::HSMEngine() : running_(false) {}

HSMEngine::~HSMEngine() { stop(); }

bool HSMEngine::initialize(const std::filesystem::path& data_dir) {
    data_dir_ = data_dir;
    try {
        if (!std::filesystem::exists(data_dir_)) {
            std::filesystem::create_directories(data_dir_);
        }
        
        // Initialize core components
        catalog_ = std::make_shared<DatasetCatalog>();
        volumes_ = std::make_shared<VolumeManager>();
        migration_ = std::make_unique<MigrationManager>(catalog_, volumes_);
        recall_ = std::make_unique<RecallManager>(catalog_, volumes_);
        backup_ = std::make_unique<BackupManager>(catalog_, volumes_);
        health_ = std::make_unique<HealthMonitor>(data_dir_);
        cache_ = std::make_unique<CacheManager>();
        
        // Initialize v4.1.0 components
        dedupe_ = std::make_unique<DeduplicationManager>();
        quota_ = std::make_unique<QuotaManager>();
        journal_ = std::make_unique<JournalManager>(data_dir_ / "journal");
        encryption_ = std::make_unique<EncryptionManager>();
        snapshots_ = std::make_unique<SnapshotManager>();
        replication_ = std::make_unique<ReplicationManager>();
        scheduler_ = std::make_unique<Scheduler>();
        
        // Initialize logging and audit
        HSMLogger::instance().initialize(data_dir_ / "logs");
        AuditLogger::instance().initialize(data_dir_ / "audit");
        
        LOG_INFO("HSMEngine", "Initialized v4.1.0 at " + data_dir_.string());
        EventSystem::instance().emit(EventType::SYSTEM_STARTUP, "HSMEngine", "System initialized v4.1.0");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("HSMEngine", std::string("Init failed: ") + e.what());
        return false;
    }
}

bool HSMEngine::start() {
    if (running_) return false;
    running_ = true;
    
    // Start core components
    migration_->start();
    recall_->start();
    backup_->start();
    health_->start();
    
    // Start v4.1.0 components
    dedupe_->start();
    quota_->start();
    journal_->start();
    encryption_->start();
    snapshots_->start();
    replication_->start();
    scheduler_->start();
    
    LOG_INFO("HSMEngine", "Started all subsystems");
    return true;
}

void HSMEngine::stop() {
    if (!running_) return;
    running_ = false;
    
    // Stop v4.1.0 components
    scheduler_->stop();
    replication_->stop();
    snapshots_->stop();
    encryption_->stop();
    journal_->stop();
    quota_->stop();
    dedupe_->stop();
    
    // Stop core components
    health_->stop();
    backup_->stop();
    recall_->stop();
    migration_->stop();
    
    EventSystem::instance().emit(EventType::SYSTEM_SHUTDOWN, "HSMEngine", "System shutdown");
    LOG_INFO("HSMEngine", "Stopped all subsystems");
    
    // Shutdown loggers to release file handles
    HSMLogger::instance().shutdown();
    AuditLogger::instance().shutdown();
}

OperationResult HSMEngine::createDataset(const std::string& name, uint64_t size,
                                          const std::string& volume,
                                          const std::string& owner,
                                          const std::string& group) {
    ScopedTimer timer(OperationType::OP_BACKUP, size);
    OperationResult result;
    result.type = OperationType::OP_BACKUP;
    result.operation_id = generateOperationId();
    result.timestamp = std::chrono::system_clock::now();
    
    // Check quota
    if (quota_->isEnforcementEnabled()) {
        auto quota_result = quota_->checkQuota(owner, group, size, 1);
        if (quota_result == QuotaCheckResult::HARD_EXCEEDED ||
            quota_result == QuotaCheckResult::GRACE_EXPIRED) {
            result.status = OperationStatus::FAILED;
            result.return_code = HSMReturnCode::ERROR_QUOTA_EXCEEDED;
            result.message = "Quota exceeded for " + owner;
            return result;
        }
    }
    
    std::string vol = volume.empty() ? 
        volumes_->findVolumeWithSpace(StorageTier::PRIMARY, size) : volume;
    if (vol.empty()) {
        result.status = OperationStatus::FAILED;
        result.return_code = HSMReturnCode::ERROR_NO_SPACE;
        result.message = "No space available";
        return result;
    }
    
    // Begin transaction
    uint64_t txn_id = journal_->beginTransaction();
    
    DatasetInfo ds;
    ds.name = name;
    ds.volume = vol;
    ds.size = size;
    ds.owner = owner;
    ds.group = group;
    ds.tier = StorageTier::PRIMARY;
    ds.state = DatasetState::ACTIVE;
    ds.created = ds.last_accessed = ds.last_modified = std::chrono::system_clock::now();
    
    if (!catalog_->addDataset(ds)) {
        journal_->rollback(txn_id);
        result.status = OperationStatus::FAILED;
        result.return_code = HSMReturnCode::ERR_ALREADY_EXISTS;
        result.message = "Dataset already exists";
        return result;
    }
    
    volumes_->allocateSpace(vol, size, name);
    journal_->writeMetadata(txn_id, name, "CREATE:" + std::to_string(size));
    
    // Update quota
    quota_->updateUsage(owner, group, static_cast<int64_t>(size), 1);
    
    // Commit transaction
    journal_->commit(txn_id);
    
    EventSystem::instance().emit(EventType::DATASET_CREATED, "HSMEngine", 
                                  "Created " + name, name, vol);
    AuditLogger::instance().logOperation(OperationType::OP_BACKUP, name, 
                                          OperationStatus::COMPLETED, 
                                          "Size: " + std::to_string(size));
    
    result.status = OperationStatus::COMPLETED;
    result.return_code = HSMReturnCode::SUCCESS;
    result.message = "Created on " + vol;
    result.bytes_processed = size;
    return result;
}

OperationResult HSMEngine::deleteDataset(const std::string& name) {
    OperationResult result;
    result.type = OperationType::OP_DELETE;
    result.operation_id = generateOperationId();
    result.timestamp = std::chrono::system_clock::now();
    
    auto ds = catalog_->getDataset(name);
    if (!ds) {
        result.status = OperationStatus::FAILED;
        result.return_code = HSMReturnCode::ERR_NOT_FOUND;
        result.message = "Not found";
        return result;
    }
    
    uint64_t txn_id = journal_->beginTransaction();
    
    // Remove from deduplication
    dedupe_->removeDataset(name);
    
    // Delete all snapshots
    auto snaps = snapshots_->getDatasetSnapshots(name);
    for (const auto& snap : snaps) {
        snapshots_->deleteSnapshot(snap.id);
    }
    
    // Delete all replicas
    auto reps = replication_->getDatasetReplicas(name);
    for (const auto& rep : reps) {
        replication_->deleteReplica(rep.id);
    }
    
    volumes_->freeSpace(ds->volume, ds->size, name);
    catalog_->removeDataset(name);
    journal_->writeMetadata(txn_id, name, "DELETE");
    
    // Update quota
    quota_->updateUsage(ds->owner, ds->group, -static_cast<int64_t>(ds->size), -1);
    
    journal_->commit(txn_id);
    
    cache_->invalidate(name);
    
    EventSystem::instance().emit(EventType::DATASET_DELETED, "HSMEngine", 
                                  "Deleted " + name, name);
    AuditLogger::instance().logOperation(OperationType::OP_DELETE, name, 
                                          OperationStatus::COMPLETED);
    
    result.status = OperationStatus::COMPLETED;
    result.return_code = HSMReturnCode::SUCCESS;
    result.message = "Deleted";
    result.bytes_processed = ds->size;
    return result;
}

std::optional<DatasetInfo> HSMEngine::getDataset(const std::string& name) const {
    // Check cache first
    auto cached = cache_->getMetadata(name);
    if (cached) return cached;
    
    auto ds = catalog_->getDataset(name);
    if (ds) {
        const_cast<CacheManager*>(cache_.get())->cacheMetadata(name, *ds);
    }
    return ds;
}

std::vector<DatasetInfo> HSMEngine::getAllDatasets() const {
    return catalog_->getAllDatasets();
}

std::vector<DatasetInfo> HSMEngine::getDatasetsByTier(StorageTier tier) const {
    return catalog_->getDatasetsByTier(tier);
}

std::vector<DatasetInfo> HSMEngine::getDatasetsByState(DatasetState state) const {
    return catalog_->getDatasetsByState(state);
}

OperationResult HSMEngine::migrateDataset(const std::string& name, StorageTier target) {
    ScopedTimer timer(OperationType::OP_MIGRATE);
    auto result = migration_->migrateDataset(name, target);
    
    if (result.status == OperationStatus::COMPLETED) {
        HSMStatisticsManager::instance().recordMigration(result.bytes_processed);
        EventSystem::instance().emit(EventType::DATASET_MIGRATED, "HSMEngine", 
                                      "Migrated " + name, name);
        cache_->invalidateMetadata(name);
    }
    
    AuditLogger::instance().logOperation(OperationType::OP_MIGRATE, name, 
                                          result.status, result.message);
    return result;
}

OperationResult HSMEngine::runAutomaticMigration() {
    return migration_->runAutomaticMigration();
}

void HSMEngine::setMigrationPolicy(const MigrationPolicy& policy) {
    migration_->setPolicy(policy);
}

MigrationPolicy HSMEngine::getMigrationPolicy() const {
    return migration_->getPolicy();
}

OperationResult HSMEngine::recallDataset(const std::string& name) {
    ScopedTimer timer(OperationType::OP_RECALL);
    auto result = recall_->recallDataset(name);
    
    if (result.status == OperationStatus::COMPLETED) {
        HSMStatisticsManager::instance().recordRecall(result.bytes_processed);
        EventSystem::instance().emit(EventType::DATASET_RECALLED, "HSMEngine", 
                                      "Recalled " + name, name);
        cache_->invalidateMetadata(name);
    }
    
    AuditLogger::instance().logOperation(OperationType::OP_RECALL, name, 
                                          result.status, result.message);
    return result;
}

OperationResult HSMEngine::backupDataset(const std::string& name) {
    ScopedTimer timer(OperationType::OP_BACKUP);
    auto result = backup_->backupDataset(name);
    
    if (result.status == OperationStatus::COMPLETED) {
        HSMStatisticsManager::instance().recordBackup(result.bytes_processed);
        EventSystem::instance().emit(EventType::DATASET_BACKED_UP, "HSMEngine", 
                                      "Backed up " + name, name);
    }
    
    AuditLogger::instance().logOperation(OperationType::OP_BACKUP, name, 
                                          result.status, result.message);
    return result;
}

OperationResult HSMEngine::recoverDataset(const std::string& name) {
    auto result = backup_->recoverDataset(name);
    AuditLogger::instance().logOperation(OperationType::OP_RECOVER, name, 
                                          result.status, result.message);
    return result;
}

OperationResult HSMEngine::runAutomaticBackup() {
    OperationResult result;
    result.type = OperationType::OP_BACKUP;
    auto ds = backup_->getDatasetsWithoutBackup();
    uint64_t bytes = 0;
    size_t count = 0;
    
    for (const auto& d : ds) {
        auto r = backupDataset(d.name);
        if (r.status == OperationStatus::COMPLETED) {
            bytes += r.bytes_processed;
            ++count;
        }
    }
    
    result.status = OperationStatus::COMPLETED;
    result.message = "Backed up " + std::to_string(count) + " datasets";
    result.bytes_processed = bytes;
    return result;
}

void HSMEngine::setBackupPolicy(const BackupPolicy& policy) {
    backup_->setPolicy(policy);
}

BackupPolicy HSMEngine::getBackupPolicy() const {
    return backup_->getPolicy();
}

bool HSMEngine::createVolume(const std::string& name, StorageTier tier, uint64_t size) {
    bool ok = volumes_->createVolume(name, tier, size);
    if (ok) {
        EventSystem::instance().emit(EventType::VOLUME_CREATED, "HSMEngine", 
                                      "Created volume " + name, "", name);
        LOG_INFO("HSMEngine", "Created volume " + name + " (" + tierToString(tier) + ")");
    }
    return ok;
}

bool HSMEngine::deleteVolume(const std::string& name) {
    bool ok = volumes_->deleteVolume(name);
    if (ok) {
        EventSystem::instance().emit(EventType::VOLUME_DELETED, "HSMEngine", 
                                      "Deleted volume " + name, "", name);
        LOG_INFO("HSMEngine", "Deleted volume " + name);
    }
    return ok;
}

std::vector<VolumeInfo> HSMEngine::getVolumes() const {
    return volumes_->getAllVolumes();
}

std::vector<VolumeInfo> HSMEngine::getVolumesByTier(StorageTier tier) const {
    return volumes_->getVolumesByTier(tier);
}

// Deduplication Operations
OperationResult HSMEngine::deduplicateDataset(const std::string& name,
                                               const std::vector<uint8_t>& data) {
    OperationResult result;
    result.type = OperationType::OP_DEDUPE;
    result.operation_id = generateOperationId();
    result.timestamp = std::chrono::system_clock::now();
    
    auto ds = catalog_->getDataset(name);
    if (!ds) {
        result.status = OperationStatus::FAILED;
        result.return_code = HSMReturnCode::ERR_NOT_FOUND;
        result.message = "Dataset not found";
        return result;
    }
    
    auto hashes = dedupe_->storeWithDedupe(name, data);
    
    DatasetInfo updated = *ds;
    updated.is_deduplicated = true;
    updated.state = DatasetState::DEDUPLICATED;
    catalog_->updateDataset(updated);
    
    result.status = OperationStatus::COMPLETED;
    result.return_code = HSMReturnCode::SUCCESS;
    result.message = "Deduplicated into " + std::to_string(hashes.size()) + " blocks";
    result.bytes_processed = data.size();
    return result;
}

std::optional<DedupeInfo> HSMEngine::getDedupeInfo(const std::string& name) const {
    return dedupe_->getDedupeInfo(name);
}

size_t HSMEngine::runDedupeGarbageCollection() {
    return dedupe_->runGarbageCollection();
}

DedupeStatistics HSMEngine::getDedupeStatistics() const {
    return dedupe_->getStatistics();
}

// Quota Operations
bool HSMEngine::setUserQuota(const std::string& user_id, uint64_t space_limit,
                              uint64_t inode_limit) {
    return quota_->setQuota(user_id, false, space_limit, inode_limit);
}

bool HSMEngine::setGroupQuota(const std::string& group_id, uint64_t space_limit,
                               uint64_t inode_limit) {
    return quota_->setQuota(group_id, true, space_limit, inode_limit);
}

std::optional<QuotaUsage> HSMEngine::getUserQuotaUsage(const std::string& user_id) const {
    return quota_->getUsage(user_id, false);
}

std::optional<QuotaUsage> HSMEngine::getGroupQuotaUsage(const std::string& group_id) const {
    return quota_->getUsage(group_id, true);
}

QuotaCheckResult HSMEngine::checkQuota(const std::string& user_id,
                                        const std::string& group_id,
                                        uint64_t space_needed) {
    return quota_->checkQuota(user_id, group_id, space_needed, 0);
}

// Encryption Operations
OperationResult HSMEngine::encryptDataset(const std::string& name,
                                           const std::string& key_id) {
    OperationResult result;
    result.type = OperationType::OP_ENCRYPT;
    result.operation_id = generateOperationId();
    result.timestamp = std::chrono::system_clock::now();
    
    auto ds = catalog_->getDataset(name);
    if (!ds) {
        result.status = OperationStatus::FAILED;
        result.return_code = HSMReturnCode::ERR_NOT_FOUND;
        result.message = "Dataset not found";
        return result;
    }
    
    std::string actual_key_id = key_id;
    if (actual_key_id.empty()) {
        actual_key_id = encryption_->generateKey(name + "_key");
    }
    
    DatasetInfo updated = *ds;
    updated.is_encrypted = true;
    updated.encryption = EncryptionType::AES_256;
    updated.state = DatasetState::ENCRYPTED;
    catalog_->updateDataset(updated);
    
    EventSystem::instance().emit(EventType::DATASET_ENCRYPTED, "HSMEngine",
                                  "Encrypted " + name, name);
    
    result.status = OperationStatus::COMPLETED;
    result.return_code = HSMReturnCode::SUCCESS;
    result.message = "Encrypted with key " + actual_key_id;
    result.details["key_id"] = actual_key_id;
    return result;
}

OperationResult HSMEngine::decryptDataset(const std::string& name) {
    OperationResult result;
    result.type = OperationType::OP_DECRYPT;
    result.operation_id = generateOperationId();
    result.timestamp = std::chrono::system_clock::now();
    
    auto ds = catalog_->getDataset(name);
    if (!ds) {
        result.status = OperationStatus::FAILED;
        result.return_code = HSMReturnCode::ERR_NOT_FOUND;
        result.message = "Dataset not found";
        return result;
    }
    
    if (!ds->is_encrypted) {
        result.status = OperationStatus::FAILED;
        result.message = "Dataset is not encrypted";
        return result;
    }
    
    DatasetInfo updated = *ds;
    updated.is_encrypted = false;
    updated.encryption = EncryptionType::NONE;
    updated.state = DatasetState::ACTIVE;
    catalog_->updateDataset(updated);
    
    result.status = OperationStatus::COMPLETED;
    result.return_code = HSMReturnCode::SUCCESS;
    result.message = "Decrypted";
    return result;
}

std::string HSMEngine::generateEncryptionKey(const std::string& name) {
    return encryption_->generateKey(name);
}

std::optional<std::string> HSMEngine::rotateEncryptionKey(const std::string& old_key_id) {
    return encryption_->rotateKey(old_key_id);
}

// Snapshot Operations
std::optional<std::string> HSMEngine::createSnapshot(const std::string& name,
                                                       const std::string& description,
                                                       uint32_t retention_days) {
    auto ds = catalog_->getDataset(name);
    if (!ds) return std::nullopt;
    
    return snapshots_->createSnapshot(name, description, retention_days);
}

bool HSMEngine::deleteSnapshot(const std::string& snapshot_id) {
    return snapshots_->deleteSnapshot(snapshot_id);
}

OperationResult HSMEngine::restoreFromSnapshot(const std::string& snapshot_id) {
    OperationResult result;
    result.type = OperationType::OP_SNAPSHOT;
    result.operation_id = generateOperationId();
    result.timestamp = std::chrono::system_clock::now();
    
    if (snapshots_->restoreFromSnapshot(snapshot_id)) {
        result.status = OperationStatus::COMPLETED;
        result.return_code = HSMReturnCode::SUCCESS;
        result.message = "Restored from " + snapshot_id;
    } else {
        result.status = OperationStatus::FAILED;
        result.return_code = HSMReturnCode::ERR_NOT_FOUND;
        result.message = "Snapshot not found";
    }
    
    return result;
}

std::vector<SnapshotInfo> HSMEngine::getDatasetSnapshots(const std::string& name) const {
    return snapshots_->getDatasetSnapshots(name);
}

// Replication Operations
std::optional<std::string> HSMEngine::replicateDataset(const std::string& name,
                                                         StorageTier target_tier,
                                                         bool synchronous) {
    auto ds = catalog_->getDataset(name);
    if (!ds) return std::nullopt;
    
    std::string target_vol = volumes_->findVolumeWithSpace(target_tier, ds->size);
    if (target_vol.empty()) return std::nullopt;
    
    return replication_->createReplica(name, ds->volume, target_vol, target_tier, synchronous);
}

bool HSMEngine::deleteReplica(const std::string& replica_id) {
    return replication_->deleteReplica(replica_id);
}

bool HSMEngine::syncReplica(const std::string& replica_id) {
    return replication_->syncReplica(replica_id);
}

std::vector<ReplicaInfo> HSMEngine::getDatasetReplicas(const std::string& name) const {
    return replication_->getDatasetReplicas(name);
}

void HSMEngine::setReplicationPolicy(const std::string& name, const ReplicationPolicy& policy) {
    replication_->setPolicy(name, policy);
}

// Scheduling Operations
std::string HSMEngine::scheduleTask(const std::string& name,
                                     OperationType operation,
                                     ScheduleFrequency frequency) {
    return scheduler_->scheduleTask(name, operation, frequency,
        [this, operation](const ScheduledTask& task) {
            // Log task execution with task details
            LOG_INFO("HSMEngine", "Executing scheduled task: " + task.name + 
                " (ID: " + task.id + ")");
            
            switch (operation) {
                case OperationType::OP_MIGRATE:
                    runAutomaticMigration();
                    return true;
                case OperationType::OP_BACKUP:
                    runAutomaticBackup();
                    return true;
                case OperationType::OP_HEALTH_CHECK:
                    getHealthReport();
                    return true;
                default:
                    LOG_WARNING("HSMEngine", "Unknown operation type for task: " + task.name);
                    return false;
            }
        });
}

bool HSMEngine::cancelTask(const std::string& task_id) {
    return scheduler_->cancelTask(task_id);
}

std::vector<ScheduledTask> HSMEngine::getScheduledTasks() const {
    return scheduler_->getAllTasks();
}

// Journaling Operations
uint64_t HSMEngine::beginTransaction() {
    return journal_->beginTransaction();
}

bool HSMEngine::commitTransaction(uint64_t txn_id) {
    return journal_->commit(txn_id);
}

bool HSMEngine::rollbackTransaction(uint64_t txn_id) {
    return journal_->rollback(txn_id);
}

void HSMEngine::journalCheckpoint() {
    journal_->checkpoint();
}

// Health and Monitoring
HealthReport HSMEngine::getHealthReport() {
    return health_->checkHealth();
}

HSMStatistics HSMEngine::getStatistics() const {
    auto stats = HSMStatisticsManager::instance().getStatistics();
    auto all = catalog_->getAllDatasets();
    
    stats.total_datasets = all.size();
    stats.active_datasets = catalog_->getDatasetsByState(DatasetState::ACTIVE).size();
    stats.migrated_datasets = catalog_->getDatasetsByState(DatasetState::MIGRATED).size();
    stats.backed_up_datasets = 0;
    stats.encrypted_datasets = 0;
    stats.deduplicated_datasets = 0;
    
    for (const auto& d : all) {
        if (d.has_backup) ++stats.backed_up_datasets;
        if (d.is_encrypted) ++stats.encrypted_datasets;
        if (d.is_deduplicated) ++stats.deduplicated_datasets;
    }
    
    auto dedupe_stats = dedupe_->getStatistics();
    stats.bytes_dedupe_saved = dedupe_stats.bytes_saved;
    stats.dedupe_ratio = dedupe_stats.dedupe_ratio;
    
    stats.total_snapshots = snapshots_->getTotalSnapshotCount();
    stats.replicated_datasets = 0;
    for (const auto& d : all) {
        if (replication_->getReplicaCount(d.name) > 0) {
            ++stats.replicated_datasets;
        }
    }
    
    return stats;
}

std::string HSMEngine::getStatusReport() const {
    std::ostringstream oss;
    oss << "=== DFSMShsm Status v4.1.0 ===\n\n";
    oss << "Running: " << (running_ ? "Yes" : "No") << "\n\n";
    
    auto stats = getStatistics();
    oss << "Datasets: " << stats.total_datasets << " total, " 
        << stats.active_datasets << " active\n";
    oss << "Migrations: " << stats.total_migrations 
        << ", Recalls: " << stats.total_recalls << "\n";
    oss << "Backups: " << stats.total_backups << "\n";
    oss << "Encrypted: " << stats.encrypted_datasets << "\n";
    oss << "Deduplicated: " << stats.deduplicated_datasets << "\n";
    oss << "Snapshots: " << stats.total_snapshots << "\n";
    oss << "Replicated: " << stats.replicated_datasets << "\n";
    
    return oss.str();
}

std::string HSMEngine::generateFullReport() const {
    std::ostringstream oss;
    oss << getStatusReport() << "\n";
    oss << dedupe_->generateReport() << "\n";
    oss << quota_->generateReport() << "\n";
    oss << journal_->generateReport() << "\n";
    oss << encryption_->generateReport() << "\n";
    oss << snapshots_->generateReport() << "\n";
    oss << replication_->generateReport() << "\n";
    oss << scheduler_->generateReport() << "\n";
    oss << cache_->generateReport() << "\n";
    return oss.str();
}

} // namespace dfsmshsm
