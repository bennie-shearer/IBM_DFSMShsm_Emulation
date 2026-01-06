/**
 * @file volume_manager.hpp
 * @brief Volume management across storage tiers
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_VOLUME_MANAGER_HPP
#define DFSMSHSM_VOLUME_MANAGER_HPP

#include "hsm_types.hpp"
#include <map>
#include <mutex>
#include <optional>

namespace dfsmshsm {

class VolumeManager {
public:
    VolumeManager() = default;

    bool createVolume(const std::string& name, StorageTier tier, uint64_t size) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (volumes_.find(name) != volumes_.end()) return false;
        VolumeInfo v; v.name = name; v.tier = tier; v.total_space = size; v.available_space = size; v.online = true;
        volumes_[name] = v;
        return true;
    }

    bool deleteVolume(const std::string& name) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = volumes_.find(name);
        if (it == volumes_.end() || !it->second.datasets.empty()) return false;
        volumes_.erase(it);
        return true;
    }

    std::optional<VolumeInfo> getVolume(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = volumes_.find(name);
        return (it != volumes_.end()) ? std::optional<VolumeInfo>(it->second) : std::nullopt;
    }

    std::vector<VolumeInfo> getAllVolumes() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<VolumeInfo> result;
        for (const auto& [n, v] : volumes_) result.push_back(v);
        return result;
    }

    std::vector<VolumeInfo> getVolumesByTier(StorageTier tier) const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<VolumeInfo> result;
        for (const auto& [n, v] : volumes_) if (v.tier == tier) result.push_back(v);
        return result;
    }

    bool allocateSpace(const std::string& name, uint64_t size, const std::string& dataset) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = volumes_.find(name);
        if (it == volumes_.end() || it->second.available_space < size) return false;
        it->second.used_space += size; it->second.available_space -= size;
        it->second.datasets.push_back(dataset); it->second.dataset_count++;
        return true;
    }

    bool freeSpace(const std::string& name, uint64_t size, const std::string& dataset) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = volumes_.find(name);
        if (it == volumes_.end() || it->second.used_space < size) return false;
        it->second.used_space -= size; it->second.available_space += size;
        auto& ds = it->second.datasets;
        ds.erase(std::remove(ds.begin(), ds.end(), dataset), ds.end());
        it->second.dataset_count = static_cast<uint32_t>(ds.size());
        return true;
    }

    std::string findVolumeWithSpace(StorageTier tier, uint64_t size) const {
        std::lock_guard<std::mutex> lock(mtx_);
        for (const auto& [n, v] : volumes_) if (v.tier == tier && v.online && v.available_space >= size) return n;
        return "";
    }

    bool setOnline(const std::string& name, bool online) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = volumes_.find(name);
        if (it == volumes_.end()) return false;
        it->second.online = online;
        return true;
    }

private:
    std::map<std::string, VolumeInfo> volumes_;
    mutable std::mutex mtx_;
};

} // namespace dfsmshsm
#endif
