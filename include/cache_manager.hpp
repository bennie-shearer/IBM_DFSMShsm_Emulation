/**
 * @file cache_manager.hpp
 * @brief LRU cache for dataset metadata and data
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_CACHE_MANAGER_HPP
#define DFSMSHSM_CACHE_MANAGER_HPP

#include "hsm_types.hpp"
#include <unordered_map>
#include <list>
#include <mutex>
#include <optional>
#include <atomic>
#include <iomanip>
#include <sstream>

namespace dfsmshsm {

template<typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(size_t cap) : cap_(cap) {}
    void put(const K& key, const V& val) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = cache_.find(key);
        if (it != cache_.end()) { order_.erase(it->second.it); order_.push_front(key); it->second.val = val; it->second.it = order_.begin(); return; }
        while (cache_.size() >= cap_ && !order_.empty()) { cache_.erase(order_.back()); order_.pop_back(); ++evict_; }
        order_.push_front(key); cache_[key] = {val, order_.begin()};
    }
    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = cache_.find(key);
        if (it == cache_.end()) { ++miss_; return std::nullopt; }
        order_.erase(it->second.it); order_.push_front(key); it->second.it = order_.begin();
        ++hit_; return it->second.val;
    }
    bool remove(const K& key) { std::lock_guard<std::mutex> lock(mtx_); auto it = cache_.find(key); if (it == cache_.end()) return false; order_.erase(it->second.it); cache_.erase(it); return true; }
    void clear() { std::lock_guard<std::mutex> lock(mtx_); cache_.clear(); order_.clear(); }
    size_t size() const { std::lock_guard<std::mutex> lock(mtx_); return cache_.size(); }
    size_t capacity() const { return cap_; }
    uint64_t hits() const { return hit_; }
    uint64_t misses() const { return miss_; }
    double hitRatio() const { uint64_t t = hit_ + miss_; return t > 0 ? double(hit_) / double(t) : 0.0; }
private:
    struct Entry { V val; typename std::list<K>::iterator it; };
    size_t cap_;
    std::unordered_map<K, Entry> cache_;
    std::list<K> order_;
    mutable std::mutex mtx_;
    std::atomic<uint64_t> hit_{0}, miss_{0}, evict_{0};
};

class CacheManager {
public:
    CacheManager(size_t mc = 10000, size_t dc = 100) : meta_(mc), data_(dc) {}
    void cacheMetadata(const std::string& n, const DatasetInfo& i) { meta_.put(n, i); }
    std::optional<DatasetInfo> getMetadata(const std::string& n) { return meta_.get(n); }
    void invalidateMetadata(const std::string& n) { meta_.remove(n); }
    void cacheData(const std::string& n, const std::vector<uint8_t>& d) { if (d.size() <= max_data_) { cached_ += d.size(); data_.put(n, d); } }
    std::optional<std::vector<uint8_t>> getData(const std::string& n) { return data_.get(n); }
    void invalidateData(const std::string& n) { auto d = data_.get(n); if (d) cached_ -= d->size(); data_.remove(n); }
    void invalidate(const std::string& n) { invalidateMetadata(n); invalidateData(n); }
    void clear() { meta_.clear(); data_.clear(); cached_ = 0; }
    std::string generateReport() const {
        std::ostringstream oss;
        oss << "=== Cache Statistics ===\n"
            << "Metadata: " << meta_.size() << "/" << meta_.capacity() << " (hit: " << std::fixed << std::setprecision(1) << (meta_.hitRatio()*100) << "%)\n"
            << "Data: " << data_.size() << "/" << data_.capacity() << " (hit: " << (data_.hitRatio()*100) << "%)\n";
        return oss.str();
    }
private:
    LRUCache<std::string, DatasetInfo> meta_;
    LRUCache<std::string, std::vector<uint8_t>> data_;
    std::atomic<uint64_t> cached_{0};
    size_t max_data_ = 104857600;
};

} // namespace dfsmshsm
#endif
