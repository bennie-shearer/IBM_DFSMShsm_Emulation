/**
 * @file dataset_catalog.hpp
 * @brief Dataset catalog management
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_DATASET_CATALOG_HPP
#define DFSMSHSM_DATASET_CATALOG_HPP

#include "hsm_types.hpp"
#include <map>
#include <mutex>
#include <optional>
#include <algorithm>

namespace dfsmshsm {

class DatasetCatalog {
public:
    DatasetCatalog() = default;

    bool addDataset(const DatasetInfo& info) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (datasets_.find(info.name) != datasets_.end()) return false;
        datasets_[info.name] = info;
        return true;
    }

    bool removeDataset(const std::string& name) {
        std::lock_guard<std::mutex> lock(mtx_);
        return datasets_.erase(name) > 0;
    }

    bool updateDataset(const DatasetInfo& info) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = datasets_.find(info.name);
        if (it == datasets_.end()) return false;
        it->second = info;
        return true;
    }

    std::optional<DatasetInfo> getDataset(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = datasets_.find(name);
        return (it != datasets_.end()) ? std::optional<DatasetInfo>(it->second) : std::nullopt;
    }

    bool exists(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        return datasets_.find(name) != datasets_.end();
    }

    std::vector<DatasetInfo> getAllDatasets() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<DatasetInfo> result;
        for (const auto& [n, d] : datasets_) result.push_back(d);
        return result;
    }

    std::vector<DatasetInfo> getDatasetsByTier(StorageTier tier) const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<DatasetInfo> result;
        for (const auto& [n, d] : datasets_) if (d.tier == tier) result.push_back(d);
        return result;
    }

    std::vector<DatasetInfo> getDatasetsByState(DatasetState state) const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<DatasetInfo> result;
        for (const auto& [n, d] : datasets_) if (d.state == state) result.push_back(d);
        return result;
    }

    std::vector<DatasetInfo> getDatasetsByVolume(const std::string& volume) const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<DatasetInfo> result;
        for (const auto& [n, d] : datasets_) if (d.volume == volume) result.push_back(d);
        return result;
    }

    std::vector<DatasetInfo> getMigrationCandidates(uint32_t age_days, uint64_t min_size) const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<DatasetInfo> result;
        auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(age_days * 24);
        for (const auto& [n, d] : datasets_) {
            if (d.tier == StorageTier::PRIMARY && d.state == DatasetState::ACTIVE && d.last_accessed < cutoff && d.size >= min_size)
                result.push_back(d);
        }
        return result;
    }

    size_t count() const { std::lock_guard<std::mutex> lock(mtx_); return datasets_.size(); }

    bool updateAccessTime(const std::string& name) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = datasets_.find(name);
        if (it == datasets_.end()) return false;
        it->second.last_accessed = std::chrono::system_clock::now();
        it->second.access_count++;
        return true;
    }

    bool setDatasetState(const std::string& name, DatasetState state) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = datasets_.find(name);
        if (it == datasets_.end()) return false;
        it->second.state = state;
        return true;
    }

    bool setDatasetTier(const std::string& name, StorageTier tier) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = datasets_.find(name);
        if (it == datasets_.end()) return false;
        it->second.tier = tier;
        if (tier != StorageTier::PRIMARY) it->second.migrated_time = std::chrono::system_clock::now();
        return true;
    }

private:
    std::map<std::string, DatasetInfo> datasets_;
    mutable std::mutex mtx_;
};

} // namespace dfsmshsm
#endif
