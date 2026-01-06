/**
 * @file migration_manager.hpp
 * @brief Dataset migration between storage tiers
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_MIGRATION_MANAGER_HPP
#define DFSMSHSM_MIGRATION_MANAGER_HPP

#include "hsm_types.hpp"
#include "dataset_catalog.hpp"
#include "volume_manager.hpp"
#include <memory>
#include <atomic>

namespace dfsmshsm {

class MigrationManager {
public:
    MigrationManager(std::shared_ptr<DatasetCatalog> catalog, std::shared_ptr<VolumeManager> volumes)
        : catalog_(catalog), volumes_(volumes), running_(false), total_migrations_(0), bytes_migrated_(0) {}

    bool start() { running_ = true; return true; }
    void stop() { running_ = false; }
    bool isRunning() const { return running_; }

    void setPolicy(const MigrationPolicy& policy) { std::lock_guard<std::mutex> lock(mtx_); policy_ = policy; }
    MigrationPolicy getPolicy() const { std::lock_guard<std::mutex> lock(mtx_); return policy_; }

    OperationResult migrateDataset(const std::string& name, StorageTier target) {
        OperationResult result; result.type = OperationType::OP_MIGRATE; result.operation_id = generateOperationId();
        result.timestamp = std::chrono::system_clock::now();

        auto ds = catalog_->getDataset(name);
        if (!ds) { result.status = OperationStatus::FAILED; result.message = "Dataset not found"; return result; }
        if (ds->tier == target) { result.status = OperationStatus::FAILED; result.message = "Already on target tier"; return result; }

        std::string vol = volumes_->findVolumeWithSpace(target, ds->size);
        if (vol.empty()) { result.status = OperationStatus::FAILED; result.message = "No space on target tier"; return result; }

        // Simulate migration
        volumes_->freeSpace(ds->volume, ds->size, name);
        volumes_->allocateSpace(vol, ds->size, name);
        
        DatasetInfo updated = *ds;
        updated.tier = target; updated.volume = vol;
        updated.state = DatasetState::MIGRATED;
        updated.migrated_time = std::chrono::system_clock::now();
        catalog_->updateDataset(updated);

        ++total_migrations_; bytes_migrated_ += ds->size;
        result.status = OperationStatus::COMPLETED;
        result.message = "Migrated to " + tierToString(target);
        result.bytes_processed = ds->size;
        return result;
    }

    std::vector<DatasetInfo> getMigrationCandidates() const {
        return catalog_->getMigrationCandidates(policy_.age_threshold_days, policy_.size_threshold_bytes);
    }

    OperationResult runAutomaticMigration() {
        OperationResult result; result.type = OperationType::OP_MIGRATE;
        if (!policy_.enabled) { result.status = OperationStatus::CANCELLED; result.message = "Auto-migration disabled"; return result; }

        auto candidates = getMigrationCandidates();
        uint64_t bytes = 0; size_t count = 0;
        for (const auto& ds : candidates) {
            auto r = migrateDataset(ds.name, policy_.target_tier);
            if (r.status == OperationStatus::COMPLETED) { bytes += r.bytes_processed; ++count; }
        }
        result.status = OperationStatus::COMPLETED;
        result.message = "Migrated " + std::to_string(count) + " datasets";
        result.bytes_processed = bytes;
        return result;
    }

    uint64_t getTotalMigrations() const { return total_migrations_; }
    uint64_t getBytesMigrated() const { return bytes_migrated_; }

private:
    std::shared_ptr<DatasetCatalog> catalog_;
    std::shared_ptr<VolumeManager> volumes_;
    MigrationPolicy policy_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> total_migrations_, bytes_migrated_;
    mutable std::mutex mtx_;
};

} // namespace dfsmshsm
#endif
