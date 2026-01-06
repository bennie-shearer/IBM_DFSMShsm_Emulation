/**
 * @file storage_tier.hpp
 * @brief Storage tier management
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_STORAGE_TIER_HPP
#define DFSMSHSM_STORAGE_TIER_HPP

#include "hsm_types.hpp"
#include <map>
#include <mutex>
#include <optional>

namespace dfsmshsm {

class StorageTierManager {
public:
    StorageTierManager(const std::string& name, StorageTier level) : name_(name), level_(level), online_(false) {}
    
    bool start() { std::lock_guard<std::mutex> lock(mtx_); online_ = true; return true; }
    void stop() { std::lock_guard<std::mutex> lock(mtx_); online_ = false; }
    bool isOnline() const { std::lock_guard<std::mutex> lock(mtx_); return online_; }
    
    std::string getName() const { return name_; }
    StorageTier getLevel() const { return level_; }

    bool createVolume(const std::string& name, uint64_t size) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (volumes_.find(name) != volumes_.end()) return false;
        VolumeInfo v; v.name = name; v.total_space = size; v.available_space = size; v.online = true;
        volumes_[name] = v;
        return true;
    }

    bool volumeExists(const std::string& name) const { std::lock_guard<std::mutex> lock(mtx_); return volumes_.find(name) != volumes_.end(); }

    std::optional<VolumeInfo> getVolumeInfo(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = volumes_.find(name);
        return (it != volumes_.end()) ? std::optional<VolumeInfo>(it->second) : std::nullopt;
    }

    bool getSpaceInfo(uint64_t& total, uint64_t& used, uint64_t& available) const {
        std::lock_guard<std::mutex> lock(mtx_);
        total = used = available = 0;
        for (const auto& [n, v] : volumes_) { total += v.total_space; used += v.used_space; available += v.available_space; }
        return true;
    }

    bool allocateSpace(const std::string& volume, uint64_t size) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = volumes_.find(volume);
        if (it == volumes_.end() || it->second.available_space < size) return false;
        it->second.used_space += size; it->second.available_space -= size;
        return true;
    }

    bool freeSpace(const std::string& volume, uint64_t size) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = volumes_.find(volume);
        if (it == volumes_.end() || it->second.used_space < size) return false;
        it->second.used_space -= size; it->second.available_space += size;
        return true;
    }

    std::vector<std::string> getVolumes() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<std::string> result;
        for (const auto& [n, v] : volumes_) result.push_back(n);
        return result;
    }

    std::string findVolumeWithSpace(uint64_t size) const {
        std::lock_guard<std::mutex> lock(mtx_);
        for (const auto& [n, v] : volumes_) if (v.online && v.available_space >= size) return n;
        return "";
    }

private:
    std::string name_;
    StorageTier level_;
    bool online_;
    std::map<std::string, VolumeInfo> volumes_;
    mutable std::mutex mtx_;
};

} // namespace dfsmshsm
#endif
