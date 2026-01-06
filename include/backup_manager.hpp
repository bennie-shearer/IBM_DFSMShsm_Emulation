/**
 * @file backup_manager.hpp
 * @brief Dataset backup and recovery management
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_BACKUP_MANAGER_HPP
#define DFSMSHSM_BACKUP_MANAGER_HPP

#include "hsm_types.hpp"
#include "dataset_catalog.hpp"
#include "volume_manager.hpp"
#include <memory>
#include <atomic>

namespace dfsmshsm {

class BackupManager {
public:
    BackupManager(std::shared_ptr<DatasetCatalog> catalog, std::shared_ptr<VolumeManager> volumes)
        : catalog_(catalog), volumes_(volumes), running_(false), total_backups_(0), bytes_backed_up_(0) {}

    bool start() { running_ = true; return true; }
    void stop() { running_ = false; }
    bool isRunning() const { return running_; }

    void setPolicy(const BackupPolicy& policy) { std::lock_guard<std::mutex> lock(mtx_); policy_ = policy; }
    BackupPolicy getPolicy() const { std::lock_guard<std::mutex> lock(mtx_); return policy_; }

    OperationResult backupDataset(const std::string& name) {
        OperationResult result; result.type = OperationType::OP_BACKUP; result.operation_id = generateOperationId();
        result.timestamp = std::chrono::system_clock::now();

        auto ds = catalog_->getDataset(name);
        if (!ds) { result.status = OperationStatus::FAILED; result.message = "Dataset not found"; return result; }

        std::string vol = policy_.backup_volume;
        if (vol.empty()) vol = volumes_->findVolumeWithSpace(StorageTier::BACKUP, ds->size);
        if (vol.empty()) { result.status = OperationStatus::FAILED; result.message = "No backup space"; return result; }

        volumes_->allocateSpace(vol, ds->size, name + ".BACKUP");

        DatasetInfo updated = *ds;
        updated.has_backup = true;
        updated.backup_location = vol;
        catalog_->updateDataset(updated);

        ++total_backups_; bytes_backed_up_ += ds->size;
        result.status = OperationStatus::COMPLETED;
        result.message = "Backed up to " + vol;
        result.bytes_processed = ds->size;
        return result;
    }

    OperationResult recoverDataset(const std::string& name) {
        OperationResult result; result.type = OperationType::OP_RECOVER; result.operation_id = generateOperationId();
        result.timestamp = std::chrono::system_clock::now();

        auto ds = catalog_->getDataset(name);
        if (!ds) { result.status = OperationStatus::FAILED; result.message = "Dataset not found"; return result; }
        if (!ds->has_backup) { result.status = OperationStatus::FAILED; result.message = "No backup exists"; return result; }

        DatasetInfo updated = *ds;
        updated.state = DatasetState::ACTIVE;
        updated.last_accessed = std::chrono::system_clock::now();
        catalog_->updateDataset(updated);

        result.status = OperationStatus::COMPLETED;
        result.message = "Recovered from backup";
        result.bytes_processed = ds->size;
        return result;
    }

    std::vector<DatasetInfo> getDatasetsWithoutBackup() const {
        auto all = catalog_->getAllDatasets();
        std::vector<DatasetInfo> result;
        for (const auto& ds : all) if (!ds.has_backup) result.push_back(ds);
        return result;
    }

    uint64_t getTotalBackups() const { return total_backups_; }
    uint64_t getBytesBackedUp() const { return bytes_backed_up_; }

private:
    std::shared_ptr<DatasetCatalog> catalog_;
    std::shared_ptr<VolumeManager> volumes_;
    BackupPolicy policy_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> total_backups_, bytes_backed_up_;
    mutable std::mutex mtx_;
};

} // namespace dfsmshsm
#endif
