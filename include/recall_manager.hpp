/**
 * @file recall_manager.hpp
 * @brief Dataset recall from migration tiers
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_RECALL_MANAGER_HPP
#define DFSMSHSM_RECALL_MANAGER_HPP

#include "hsm_types.hpp"
#include "dataset_catalog.hpp"
#include "volume_manager.hpp"
#include <memory>
#include <atomic>
#include <queue>

namespace dfsmshsm {

class RecallManager {
public:
    RecallManager(std::shared_ptr<DatasetCatalog> catalog, std::shared_ptr<VolumeManager> volumes)
        : catalog_(catalog), volumes_(volumes), running_(false), total_recalls_(0), bytes_recalled_(0) {}

    bool start() { running_ = true; return true; }
    void stop() { running_ = false; }
    bool isRunning() const { return running_; }

    OperationResult recallDataset(const std::string& name) {
        OperationResult result; result.type = OperationType::OP_RECALL; result.operation_id = generateOperationId();
        result.timestamp = std::chrono::system_clock::now();

        auto ds = catalog_->getDataset(name);
        if (!ds) { result.status = OperationStatus::FAILED; result.message = "Dataset not found"; return result; }
        if (ds->tier == StorageTier::PRIMARY) { result.status = OperationStatus::FAILED; result.message = "Already on PRIMARY"; return result; }

        std::string vol = volumes_->findVolumeWithSpace(StorageTier::PRIMARY, ds->size);
        if (vol.empty()) { result.status = OperationStatus::FAILED; result.message = "No space on PRIMARY"; return result; }

        volumes_->freeSpace(ds->volume, ds->size, name);
        volumes_->allocateSpace(vol, ds->size, name);

        DatasetInfo updated = *ds;
        updated.tier = StorageTier::PRIMARY; updated.volume = vol;
        updated.state = DatasetState::RECALLED;
        updated.last_accessed = std::chrono::system_clock::now();
        catalog_->updateDataset(updated);

        ++total_recalls_; bytes_recalled_ += ds->size;
        result.status = OperationStatus::COMPLETED;
        result.message = "Recalled to PRIMARY";
        result.bytes_processed = ds->size;
        return result;
    }

    bool queueRecall(const std::string& name) {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push(name);
        return true;
    }

    OperationResult processQueue() {
        OperationResult result; result.type = OperationType::OP_RECALL;
        std::queue<std::string> toProcess;
        { std::lock_guard<std::mutex> lock(mtx_); std::swap(toProcess, queue_); }
        uint64_t bytes = 0; size_t count = 0;
        while (!toProcess.empty()) {
            auto r = recallDataset(toProcess.front());
            if (r.status == OperationStatus::COMPLETED) { bytes += r.bytes_processed; ++count; }
            toProcess.pop();
        }
        result.status = OperationStatus::COMPLETED;
        result.message = "Recalled " + std::to_string(count) + " datasets";
        result.bytes_processed = bytes;
        return result;
    }

    size_t getQueueSize() const { std::lock_guard<std::mutex> lock(mtx_); return queue_.size(); }
    uint64_t getTotalRecalls() const { return total_recalls_; }
    uint64_t getBytesRecalled() const { return bytes_recalled_; }

private:
    std::shared_ptr<DatasetCatalog> catalog_;
    std::shared_ptr<VolumeManager> volumes_;
    std::queue<std::string> queue_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> total_recalls_, bytes_recalled_;
    mutable std::mutex mtx_;
};

} // namespace dfsmshsm
#endif
