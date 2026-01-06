/**
 * @file hsm_lru_cache.hpp
 * @brief Enhanced LRU cache with bloom filters and statistics
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 *
 * Provides optimized caching with:
 * - LRU eviction with configurable size limits
 * - Bloom filter for fast negative lookups
 * - Cache statistics and hit rate tracking
 * - TTL-based expiration
 * - Thread-safe operations
 * - Write-through and write-back policies
 */
#ifndef DFSMSHSM_HSM_LRU_CACHE_HPP
#define DFSMSHSM_HSM_LRU_CACHE_HPP

#include "hsm_types.hpp"
#include "result.hpp"
#include <map>
#include <unordered_map>
#include <list>
#include <vector>
#include <optional>
#include <functional>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <bitset>
#include <array>

namespace dfsmshsm {

//=============================================================================
// Bloom Filter
//=============================================================================

/**
 * @class BloomFilter
 * @brief Probabilistic data structure for set membership testing
 *
 * False positive rate: (1 - e^(-kn/m))^k
 * where k = number of hash functions, n = elements, m = bits
 */
template<size_t BitSize = 65536>
class BloomFilter {
public:
    static constexpr size_t NUM_HASH_FUNCTIONS = 7;
    
    BloomFilter() : bits_(), count_(0) {}
    
    /**
     * @brief Add an element to the filter
     */
    void add(const std::string& key) {
        for (size_t i = 0; i < NUM_HASH_FUNCTIONS; ++i) {
            size_t index = hash(key, i) % BitSize;
            bits_[index] = true;
        }
        ++count_;
    }
    
    /**
     * @brief Check if an element might be in the set
     * @return true if possibly present, false if definitely not present
     */
    [[nodiscard]] bool mightContain(const std::string& key) const {
        for (size_t i = 0; i < NUM_HASH_FUNCTIONS; ++i) {
            size_t index = hash(key, i) % BitSize;
            if (!bits_[index]) return false;
        }
        return true;
    }
    
    /**
     * @brief Clear the filter
     */
    void clear() {
        bits_.reset();
        count_ = 0;
    }
    
    /**
     * @brief Get the approximate false positive rate
     */
    [[nodiscard]] double falsePositiveRate() const {
        if (count_ == 0) return 0.0;
        double k = static_cast<double>(NUM_HASH_FUNCTIONS);
        double n = static_cast<double>(count_);
        double m = static_cast<double>(BitSize);
        return std::pow(1.0 - std::exp(-k * n / m), k);
    }
    
    /**
     * @brief Get fill ratio (bits set / total bits)
     */
    [[nodiscard]] double fillRatio() const {
        return static_cast<double>(bits_.count()) / static_cast<double>(BitSize);
    }
    
    /**
     * @brief Get number of elements added
     */
    [[nodiscard]] size_t count() const { return count_; }
    
    /**
     * @brief Get optimal number of hash functions for given size and elements
     */
    [[nodiscard]] static size_t optimalHashFunctions(size_t bits, size_t elements) {
        if (elements == 0) return 1;
        return static_cast<size_t>(std::round(
            (static_cast<double>(bits) / static_cast<double>(elements)) * std::log(2.0)));
    }
    
    /**
     * @brief Get optimal bit size for given elements and false positive rate
     */
    [[nodiscard]] static size_t optimalBitSize(size_t elements, double falsePositiveRate) {
        if (elements == 0 || falsePositiveRate <= 0 || falsePositiveRate >= 1) return 1024;
        return static_cast<size_t>(
            -static_cast<double>(elements) * std::log(falsePositiveRate) / 
            (std::log(2.0) * std::log(2.0)));
    }

private:
    [[nodiscard]] size_t hash(const std::string& key, size_t seed) const {
        // MurmurHash-inspired mixing
        size_t h = seed;
        for (char c : key) {
            h ^= static_cast<size_t>(c);
            h *= 0x5bd1e995;
            h ^= h >> 15;
        }
        h ^= key.size();
        h *= 0x5bd1e995;
        h ^= h >> 15;
        return h;
    }
    
    std::bitset<BitSize> bits_;
    size_t count_;
};

//=============================================================================
// Cache Entry
//=============================================================================

/**
 * @struct CacheEntry
 * @brief Entry in the LRU cache with metadata
 */
template<typename V>
struct CacheEntry {
    V value;
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point last_accessed;
    std::chrono::steady_clock::time_point expires;
    uint64_t access_count = 0;
    size_t size_bytes = 0;
    bool dirty = false;  // For write-back policy
    
    CacheEntry() : created(std::chrono::steady_clock::now()),
                   last_accessed(created),
                   expires(std::chrono::steady_clock::time_point::max()) {}
    
    explicit CacheEntry(V val, size_t size = 0,
                       std::chrono::seconds ttl = std::chrono::seconds::max())
        : value(std::move(val)),
          created(std::chrono::steady_clock::now()),
          last_accessed(created),
          size_bytes(size) {
        if (ttl != std::chrono::seconds::max()) {
            expires = created + ttl;
        } else {
            expires = std::chrono::steady_clock::time_point::max();
        }
    }
    
    [[nodiscard]] bool isExpired() const {
        return std::chrono::steady_clock::now() >= expires;
    }
    
    void touch() {
        last_accessed = std::chrono::steady_clock::now();
        ++access_count;
    }
};

//=============================================================================
// Cache Statistics
//=============================================================================

/**
 * @struct CacheStatistics
 * @brief Statistics for cache operations
 */
struct CacheStatistics {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> bloom_filter_rejections{0};  // Fast path misses
    std::atomic<uint64_t> insertions{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint64_t> expirations{0};
    std::atomic<uint64_t> updates{0};
    std::atomic<uint64_t> deletions{0};
    std::atomic<size_t> current_size{0};
    std::atomic<size_t> current_bytes{0};
    std::atomic<size_t> peak_size{0};
    std::atomic<size_t> peak_bytes{0};
    
    [[nodiscard]] double hitRate() const {
        uint64_t total = hits.load() + misses.load();
        return total > 0 ? static_cast<double>(hits.load()) / static_cast<double>(total) : 0.0;
    }
    
    [[nodiscard]] double bloomFilterEfficiency() const {
        uint64_t total_misses = misses.load();
        if (total_misses == 0) return 0.0;
        return static_cast<double>(bloom_filter_rejections.load()) / 
               static_cast<double>(total_misses);
    }
    
    void reset() {
        hits = 0;
        misses = 0;
        bloom_filter_rejections = 0;
        insertions = 0;
        evictions = 0;
        expirations = 0;
        updates = 0;
        deletions = 0;
    }
    
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2)
            << "=== Cache Statistics ===\n"
            << "Hits: " << hits.load() << "\n"
            << "Misses: " << misses.load() << "\n"
            << "Hit Rate: " << (hitRate() * 100.0) << "%\n"
            << "Bloom Filter Rejections: " << bloom_filter_rejections.load() << "\n"
            << "Bloom Filter Efficiency: " << (bloomFilterEfficiency() * 100.0) << "%\n"
            << "Insertions: " << insertions.load() << "\n"
            << "Updates: " << updates.load() << "\n"
            << "Evictions: " << evictions.load() << "\n"
            << "Expirations: " << expirations.load() << "\n"
            << "Deletions: " << deletions.load() << "\n"
            << "Current Size: " << current_size.load() << " entries\n"
            << "Current Bytes: " << current_bytes.load() << " bytes\n"
            << "Peak Size: " << peak_size.load() << " entries\n"
            << "Peak Bytes: " << peak_bytes.load() << " bytes\n";
        return oss.str();
    }
};

//=============================================================================
// Enhanced LRU Cache
//=============================================================================

/**
 * @enum CacheWritePolicy
 * @brief Write policy for the cache
 */
enum class CacheWritePolicy {
    WRITE_THROUGH,  // Write to backing store immediately
    WRITE_BACK      // Write to backing store on eviction
};

/**
 * @class EnhancedLRUCache
 * @brief LRU cache with bloom filter, TTL, and statistics
 */
template<typename K, typename V, size_t BloomBits = 65536>
class EnhancedLRUCache {
public:
    using WriteBackFunc = std::function<void(const K&, const V&)>;
    using LoadFunc = std::function<std::optional<V>(const K&)>;
    using SizeFunc = std::function<size_t(const V&)>;
    
    struct Config {
        size_t max_entries = 10000;
        size_t max_bytes = 100 * 1024 * 1024;  // 100 MB
        std::chrono::seconds default_ttl = std::chrono::seconds::max();
        CacheWritePolicy write_policy = CacheWritePolicy::WRITE_THROUGH;
        bool use_bloom_filter = true;
        double eviction_threshold = 0.9;  // Evict when 90% full
    };
    
    explicit EnhancedLRUCache(const Config& config = Config{})
        : config_(config) {}
    
    /**
     * @brief Set callback for write-back on eviction
     */
    void setWriteBackFunc(WriteBackFunc func) {
        write_back_func_ = std::move(func);
    }
    
    /**
     * @brief Set callback for loading missing entries
     */
    void setLoadFunc(LoadFunc func) {
        load_func_ = std::move(func);
    }
    
    /**
     * @brief Set function to calculate value size
     */
    void setSizeFunc(SizeFunc func) {
        size_func_ = std::move(func);
    }
    
    /**
     * @brief Get a value from the cache
     */
    [[nodiscard]] std::optional<V> get(const K& key) {
        // Fast path: bloom filter check
        if (config_.use_bloom_filter) {
            std::string keyStr = keyToString(key);
            if (!bloom_filter_.mightContain(keyStr)) {
                ++stats_.misses;
                ++stats_.bloom_filter_rejections;
                return tryLoad(key);
            }
        }
        
        std::unique_lock lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            ++stats_.misses;
            lock.unlock();
            return tryLoad(key);
        }
        
        // Check expiration
        if (it->second->isExpired()) {
            evictEntry(it);
            ++stats_.expirations;
            ++stats_.misses;
            lock.unlock();
            return tryLoad(key);
        }
        
        // Move to front (most recently used)
        it->second->touch();
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
        
        ++stats_.hits;
        return it->second->value;
    }
    
    /**
     * @brief Get a value or compute it
     */
    template<typename Factory>
    [[nodiscard]] V getOrCompute(const K& key, Factory&& factory) {
        auto result = get(key);
        if (result) return *result;
        
        V value = factory();
        put(key, value);
        return value;
    }
    
    /**
     * @brief Put a value in the cache
     */
    template<typename Duration = std::chrono::seconds>
    void put(const K& key, const V& value, 
             Duration ttl = Duration::max()) {
        std::chrono::seconds ttl_sec;
        if (ttl == Duration::max()) {
            ttl_sec = config_.default_ttl;
        } else {
            ttl_sec = std::chrono::duration_cast<std::chrono::seconds>(ttl);
            if (ttl_sec.count() == 0 && ttl > Duration::zero()) {
                ttl_sec = std::chrono::seconds(1);  // Minimum 1 second if non-zero
            }
        }
        
        size_t value_size = size_func_ ? size_func_(value) : sizeof(V);
        
        std::unique_lock lock(mutex_);
        
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            // Update existing entry
            stats_.current_bytes -= it->second->size_bytes;
            it->second->value = value;
            it->second->size_bytes = value_size;
            it->second->touch();
            it->second->dirty = true;
            if (ttl_sec != std::chrono::seconds::max()) {
                it->second->expires = std::chrono::steady_clock::now() + ttl_sec;
            }
            stats_.current_bytes += value_size;
            ++stats_.updates;
            
            // Move to front
            lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
        } else {
            // Ensure space
            ensureSpace(value_size);
            
            // Add to bloom filter
            if (config_.use_bloom_filter) {
                bloom_filter_.add(keyToString(key));
            }
            
            // Create new entry
            lru_list_.emplace_front(value, value_size, ttl_sec);
            cache_[key] = lru_list_.begin();
            
            stats_.current_size = cache_.size();
            stats_.current_bytes += value_size;
            if (stats_.current_size > stats_.peak_size) {
                stats_.peak_size.store(stats_.current_size.load());
            }
            if (stats_.current_bytes > stats_.peak_bytes) {
                stats_.peak_bytes.store(stats_.current_bytes.load());
            }
            ++stats_.insertions;
        }
        
        // Write-through policy
        if (config_.write_policy == CacheWritePolicy::WRITE_THROUGH && write_back_func_) {
            lock.unlock();
            write_back_func_(key, value);
        }
    }
    
    /**
     * @brief Remove an entry from the cache
     */
    bool remove(const K& key) {
        std::unique_lock lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) return false;
        
        // Write back if dirty
        if (config_.write_policy == CacheWritePolicy::WRITE_BACK &&
            it->second->dirty && write_back_func_) {
            write_back_func_(key, it->second->value);
        }
        
        stats_.current_bytes -= it->second->size_bytes;
        lru_list_.erase(it->second);
        cache_.erase(it);
        stats_.current_size = cache_.size();
        ++stats_.deletions;
        
        return true;
    }
    
    /**
     * @brief Check if key exists (without updating LRU order)
     */
    [[nodiscard]] bool contains(const K& key) const {
        if (config_.use_bloom_filter) {
            if (!bloom_filter_.mightContain(keyToString(key))) {
                return false;
            }
        }
        
        std::shared_lock lock(mutex_);
        auto it = cache_.find(key);
        if (it == cache_.end()) return false;
        return !it->second->isExpired();
    }
    
    /**
     * @brief Clear all entries
     */
    void clear() {
        std::unique_lock lock(mutex_);
        
        // Write back all dirty entries
        if (config_.write_policy == CacheWritePolicy::WRITE_BACK && write_back_func_) {
            for (const auto& [key, iter] : cache_) {
                if (iter->dirty) {
                    write_back_func_(key, iter->value);
                }
            }
        }
        
        cache_.clear();
        lru_list_.clear();
        bloom_filter_.clear();
        stats_.current_size = 0;
        stats_.current_bytes = 0;
    }
    
    /**
     * @brief Flush dirty entries to backing store
     */
    void flush() {
        if (!write_back_func_) return;
        
        std::shared_lock lock(mutex_);
        for (const auto& [key, iter] : cache_) {
            if (iter->dirty) {
                write_back_func_(key, iter->value);
                iter->dirty = false;
            }
        }
    }
    
    /**
     * @brief Remove expired entries
     */
    size_t purgeExpired() {
        std::unique_lock lock(mutex_);
        
        size_t count = 0;
        auto it = lru_list_.begin();
        while (it != lru_list_.end()) {
            if (it->isExpired()) {
                // Find key for this entry
                K key_to_remove{};
                for (const auto& [k, v] : cache_) {
                    if (v == it) {
                        key_to_remove = k;
                        break;
                    }
                }
                
                stats_.current_bytes -= it->size_bytes;
                it = lru_list_.erase(it);
                cache_.erase(key_to_remove);
                ++count;
                ++stats_.expirations;
            } else {
                ++it;
            }
        }
        
        stats_.current_size = cache_.size();
        return count;
    }
    
    /**
     * @brief Get current size
     */
    [[nodiscard]] size_t size() const {
        std::shared_lock lock(mutex_);
        return cache_.size();
    }
    
    /**
     * @brief Get current byte usage
     */
    [[nodiscard]] size_t bytes() const {
        return stats_.current_bytes.load();
    }
    
    /**
     * @brief Check if cache is empty
     */
    [[nodiscard]] bool empty() const {
        std::shared_lock lock(mutex_);
        return cache_.empty();
    }
    
    /**
     * @brief Get statistics
     */
    [[nodiscard]] const CacheStatistics& statistics() const { return stats_; }
    
    /**
     * @brief Get configuration
     */
    [[nodiscard]] const Config& config() const { return config_; }
    
    /**
     * @brief Get bloom filter false positive rate
     */
    [[nodiscard]] double bloomFilterFPR() const {
        return bloom_filter_.falsePositiveRate();
    }
    
    /**
     * @brief Get all keys (for debugging/iteration)
     */
    [[nodiscard]] std::vector<K> keys() const {
        std::shared_lock lock(mutex_);
        std::vector<K> result;
        result.reserve(cache_.size());
        for (const auto& [k, v] : cache_) {
            result.push_back(k);
        }
        return result;
    }
    
    /**
     * @brief Generate report
     */
    [[nodiscard]] std::string generateReport() const {
        std::ostringstream oss;
        oss << stats_.toString()
            << "\nBloom Filter:\n"
            << "  Fill Ratio: " << std::fixed << std::setprecision(2) 
            << (bloom_filter_.fillRatio() * 100.0) << "%\n"
            << "  False Positive Rate: " << (bloom_filter_.falsePositiveRate() * 100.0) << "%\n"
            << "  Elements Added: " << bloom_filter_.count() << "\n";
        return oss.str();
    }

private:
    using ListType = std::list<CacheEntry<V>>;
    using ListIterator = typename ListType::iterator;
    using CacheMap = std::unordered_map<K, ListIterator>;
    
    std::string keyToString(const K& key) const {
        if constexpr (std::is_same_v<K, std::string>) {
            return key;
        } else if constexpr (std::is_arithmetic_v<K>) {
            return std::to_string(key);
        } else {
            std::ostringstream oss;
            oss << key;
            return oss.str();
        }
    }
    
    std::optional<V> tryLoad(const K& key) {
        if (!load_func_) return std::nullopt;
        
        auto value = load_func_(key);
        if (value) {
            put(key, *value);
        }
        return value;
    }
    
    void ensureSpace(size_t needed_bytes) {
        // Check entry limit
        while (cache_.size() >= config_.max_entries) {
            evictLRU();
        }
        
        // Check byte limit
        while (stats_.current_bytes + needed_bytes > config_.max_bytes && !cache_.empty()) {
            evictLRU();
        }
    }
    
    void evictLRU() {
        if (lru_list_.empty()) return;
        
        auto& entry = lru_list_.back();
        
        // Find key for this entry
        K key_to_remove{};
        for (const auto& [k, v] : cache_) {
            if (v == std::prev(lru_list_.end())) {
                key_to_remove = k;
                break;
            }
        }
        
        // Write back if dirty
        if (config_.write_policy == CacheWritePolicy::WRITE_BACK &&
            entry.dirty && write_back_func_) {
            write_back_func_(key_to_remove, entry.value);
        }
        
        stats_.current_bytes -= entry.size_bytes;
        cache_.erase(key_to_remove);
        lru_list_.pop_back();
        stats_.current_size = cache_.size();
        ++stats_.evictions;
    }
    
    void evictEntry(typename CacheMap::iterator it) {
        auto& entry = *it->second;
        
        // Write back if dirty
        if (config_.write_policy == CacheWritePolicy::WRITE_BACK &&
            entry.dirty && write_back_func_) {
            write_back_func_(it->first, entry.value);
        }
        
        stats_.current_bytes -= entry.size_bytes;
        lru_list_.erase(it->second);
        cache_.erase(it);
        stats_.current_size = cache_.size();
        ++stats_.evictions;
    }
    
    Config config_;
    mutable std::shared_mutex mutex_;
    CacheMap cache_;
    ListType lru_list_;
    BloomFilter<BloomBits> bloom_filter_;
    CacheStatistics stats_;
    WriteBackFunc write_back_func_;
    LoadFunc load_func_;
    SizeFunc size_func_;
};

//=============================================================================
// Specialized Caches
//=============================================================================

/**
 * @class DatasetCache
 * @brief Specialized cache for DatasetInfo entries
 */
class DatasetCache : public EnhancedLRUCache<std::string, DatasetInfo> {
public:
    DatasetCache() : EnhancedLRUCache<std::string, DatasetInfo>(defaultConfig()) {
        setSizeFunc([](const DatasetInfo& ds) {
            return sizeof(DatasetInfo) + ds.name.size() + ds.volume.size() +
                   ds.owner.size() + ds.encryption_key_id.size();
        });
    }
    
private:
    static Config defaultConfig() {
        Config cfg;
        cfg.max_entries = 100000;
        cfg.max_bytes = 500 * 1024 * 1024;  // 500 MB
        cfg.default_ttl = std::chrono::seconds(3600);  // 1 hour
        cfg.use_bloom_filter = true;
        return cfg;
    }
};

/**
 * @class VolumeCache
 * @brief Specialized cache for VolumeInfo entries
 */
class VolumeCache : public EnhancedLRUCache<std::string, VolumeInfo> {
public:
    VolumeCache() : EnhancedLRUCache<std::string, VolumeInfo>(defaultConfig()) {
        setSizeFunc([](const VolumeInfo& vol) {
            return sizeof(VolumeInfo) + vol.name.size() + 
                   vol.device_type.size() + vol.storage_group.size();
        });
    }
    
private:
    static Config defaultConfig() {
        Config cfg;
        cfg.max_entries = 10000;
        cfg.max_bytes = 50 * 1024 * 1024;  // 50 MB
        cfg.default_ttl = std::chrono::seconds(300);  // 5 minutes
        cfg.use_bloom_filter = true;
        return cfg;
    }
};

//=============================================================================
// Multi-Level Cache
//=============================================================================

/**
 * @class MultiLevelCache
 * @brief Two-level cache (L1 fast/small, L2 larger/slower)
 */
template<typename K, typename V>
class MultiLevelCache {
public:
    struct Config {
        size_t l1_max_entries = 1000;
        size_t l2_max_entries = 10000;
        std::chrono::seconds l1_ttl = std::chrono::seconds(60);
        std::chrono::seconds l2_ttl = std::chrono::seconds(3600);
    };
    
    explicit MultiLevelCache(const Config& config = Config{}) {
        typename EnhancedLRUCache<K, V>::Config l1_config;
        l1_config.max_entries = config.l1_max_entries;
        l1_config.default_ttl = config.l1_ttl;
        l1_ = std::make_unique<EnhancedLRUCache<K, V>>(l1_config);
        
        typename EnhancedLRUCache<K, V>::Config l2_config;
        l2_config.max_entries = config.l2_max_entries;
        l2_config.default_ttl = config.l2_ttl;
        l2_ = std::make_unique<EnhancedLRUCache<K, V>>(l2_config);
    }
    
    [[nodiscard]] std::optional<V> get(const K& key) {
        // Try L1 first
        auto result = l1_->get(key);
        if (result) {
            ++l1_hits_;
            return result;
        }
        
        // Try L2
        result = l2_->get(key);
        if (result) {
            ++l2_hits_;
            // Promote to L1
            l1_->put(key, *result);
            return result;
        }
        
        ++misses_;
        return std::nullopt;
    }
    
    void put(const K& key, const V& value) {
        l1_->put(key, value);
        l2_->put(key, value);
    }
    
    void remove(const K& key) {
        l1_->remove(key);
        l2_->remove(key);
    }
    
    void clear() {
        l1_->clear();
        l2_->clear();
    }
    
    [[nodiscard]] std::string generateReport() const {
        std::ostringstream oss;
        uint64_t total = l1_hits_ + l2_hits_ + misses_;
        oss << "=== Multi-Level Cache Statistics ===\n"
            << "L1 Hits: " << l1_hits_ << " ("
            << (total > 0 ? (100.0 * l1_hits_ / total) : 0) << "%)\n"
            << "L2 Hits: " << l2_hits_ << " ("
            << (total > 0 ? (100.0 * l2_hits_ / total) : 0) << "%)\n"
            << "Misses: " << misses_ << " ("
            << (total > 0 ? (100.0 * misses_ / total) : 0) << "%)\n"
            << "\n--- L1 Cache ---\n" << l1_->generateReport()
            << "\n--- L2 Cache ---\n" << l2_->generateReport();
        return oss.str();
    }

private:
    std::unique_ptr<EnhancedLRUCache<K, V>> l1_;
    std::unique_ptr<EnhancedLRUCache<K, V>> l2_;
    std::atomic<uint64_t> l1_hits_{0};
    std::atomic<uint64_t> l2_hits_{0};
    std::atomic<uint64_t> misses_{0};
};

} // namespace dfsmshsm
#endif // DFSMSHSM_HSM_LRU_CACHE_HPP
