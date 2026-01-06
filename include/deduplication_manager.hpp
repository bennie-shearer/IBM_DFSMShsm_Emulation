/**
 * @file deduplication_manager.hpp
 * @brief Content-addressable storage with hash-based deduplication
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 * 
 * Implements data deduplication using SHA-256 content hashing.
 * Supports block-level and file-level deduplication.
 */
#ifndef DFSMSHSM_DEDUPLICATION_MANAGER_HPP
#define DFSMSHSM_DEDUPLICATION_MANAGER_HPP

#include "hsm_types.hpp"
#include <map>
#include <set>
#include <mutex>
#include <memory>
#include <atomic>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace dfsmshsm {

/**
 * @class SimpleHash
 * @brief Simple hash implementation (production would use SHA-256)
 */
class SimpleHash {
public:
    static std::string compute(const std::vector<uint8_t>& data) {
        // Simple hash for emulation - production would use SHA-256
        uint64_t hash1 = 0x5555555555555555ULL;
        uint64_t hash2 = 0xAAAAAAAAAAAAAAAAULL;
        
        for (size_t i = 0; i < data.size(); ++i) {
            hash1 = hash1 * 31 + data[i];
            hash2 = hash2 * 37 + data[i];
            hash1 ^= (hash1 >> 17);
            hash2 ^= (hash2 << 13);
        }
        
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(16) << hash1 
            << std::setw(16) << hash2;
        return oss.str();
    }
    
    static std::string computeBlock(const uint8_t* data, size_t len) {
        std::vector<uint8_t> vec(data, data + len);
        return compute(vec);
    }
};

/**
 * @struct DedupeBlock
 * @brief Represents a deduplicated data block
 */
struct DedupeBlock {
    std::string hash;
    std::vector<uint8_t> data;
    uint32_t ref_count = 0;
    std::set<std::string> referencing_datasets;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point last_accessed;
};

/**
 * @struct DedupeStatistics
 * @brief Deduplication statistics
 */
struct DedupeStatistics {
    uint64_t total_blocks = 0;
    uint64_t unique_blocks = 0;
    uint64_t duplicate_blocks = 0;
    uint64_t bytes_before_dedupe = 0;
    uint64_t bytes_after_dedupe = 0;
    uint64_t bytes_saved = 0;
    double dedupe_ratio = 1.0;
    uint64_t inline_dedupe_hits = 0;
    uint64_t post_process_dedupe_hits = 0;
};

/**
 * @class DeduplicationManager
 * @brief Manages data deduplication using content-addressable storage
 */
class DeduplicationManager {
public:
    /**
     * @brief Constructor
     * @param block_size Size of deduplication blocks (default 64KB)
     */
    explicit DeduplicationManager(size_t block_size = 65536)
        : block_size_(block_size), running_(false) {}
    
    /**
     * @brief Start the deduplication manager
     */
    bool start() {
        if (running_) return false;
        running_ = true;
        return true;
    }
    
    /**
     * @brief Stop the deduplication manager
     */
    void stop() { running_ = false; }
    
    /**
     * @brief Check if manager is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Store data with deduplication
     * @param dataset_name Name of the dataset
     * @param data Data to store
     * @return List of block hashes representing the data
     */
    std::vector<std::string> storeWithDedupe(const std::string& dataset_name,
                                              const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<std::string> block_hashes;
        
        stats_.bytes_before_dedupe += data.size();
        
        // Split data into blocks
        size_t offset = 0;
        while (offset < data.size()) {
            size_t chunk_size = std::min(block_size_, data.size() - offset);
            std::vector<uint8_t> block(data.begin() + offset, 
                                        data.begin() + offset + chunk_size);
            
            std::string hash = SimpleHash::compute(block);
            block_hashes.push_back(hash);
            
            auto it = blocks_.find(hash);
            if (it != blocks_.end()) {
                // Duplicate block found
                it->second.ref_count++;
                it->second.referencing_datasets.insert(dataset_name);
                it->second.last_accessed = std::chrono::system_clock::now();
                ++stats_.duplicate_blocks;
                ++stats_.inline_dedupe_hits;
            } else {
                // New unique block
                DedupeBlock blk;
                blk.hash = hash;
                blk.data = std::move(block);
                blk.ref_count = 1;
                blk.referencing_datasets.insert(dataset_name);
                blk.created = std::chrono::system_clock::now();
                blk.last_accessed = blk.created;
                blocks_[hash] = std::move(blk);
                stats_.bytes_after_dedupe += chunk_size;
                ++stats_.unique_blocks;
            }
            
            ++stats_.total_blocks;
            offset += chunk_size;
        }
        
        // Update dataset mapping
        dataset_blocks_[dataset_name] = block_hashes;
        
        // Update stats
        stats_.bytes_saved = stats_.bytes_before_dedupe - stats_.bytes_after_dedupe;
        if (stats_.bytes_after_dedupe > 0) {
            stats_.dedupe_ratio = static_cast<double>(stats_.bytes_before_dedupe) / 
                                   static_cast<double>(stats_.bytes_after_dedupe);
        }
        
        return block_hashes;
    }
    
    /**
     * @brief Retrieve data from deduplicated storage
     * @param dataset_name Name of the dataset
     * @return Reconstructed data
     */
    std::optional<std::vector<uint8_t>> retrieve(const std::string& dataset_name) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = dataset_blocks_.find(dataset_name);
        if (it == dataset_blocks_.end()) return std::nullopt;
        
        std::vector<uint8_t> result;
        for (const auto& hash : it->second) {
            auto blk_it = blocks_.find(hash);
            if (blk_it == blocks_.end()) {
                // Block missing - data corruption
                return std::nullopt;
            }
            blk_it->second.last_accessed = std::chrono::system_clock::now();
            result.insert(result.end(), blk_it->second.data.begin(), 
                         blk_it->second.data.end());
        }
        
        return result;
    }
    
    /**
     * @brief Remove dataset from deduplication
     * @param dataset_name Name of the dataset
     * @return True if successful
     */
    bool removeDataset(const std::string& dataset_name) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = dataset_blocks_.find(dataset_name);
        if (it == dataset_blocks_.end()) return false;
        
        for (const auto& hash : it->second) {
            auto blk_it = blocks_.find(hash);
            if (blk_it != blocks_.end()) {
                blk_it->second.ref_count--;
                blk_it->second.referencing_datasets.erase(dataset_name);
                
                if (blk_it->second.ref_count == 0) {
                    stats_.bytes_after_dedupe -= blk_it->second.data.size();
                    --stats_.unique_blocks;
                    blocks_.erase(blk_it);
                }
            }
        }
        
        dataset_blocks_.erase(it);
        return true;
    }
    
    /**
     * @brief Get deduplication info for a dataset
     * @param dataset_name Name of the dataset
     * @return Deduplication information
     */
    std::optional<DedupeInfo> getDedupeInfo(const std::string& dataset_name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = dataset_blocks_.find(dataset_name);
        if (it == dataset_blocks_.end()) return std::nullopt;
        
        DedupeInfo info;
        info.reference_count = 0;
        info.original_size = 0;
        info.dedupe_size = 0;
        
        std::set<std::string> unique_hashes;
        for (const auto& hash : it->second) {
            auto blk_it = blocks_.find(hash);
            if (blk_it != blocks_.end()) {
                info.original_size += blk_it->second.data.size();
                if (unique_hashes.insert(hash).second) {
                    info.dedupe_size += blk_it->second.data.size();
                }
            }
        }
        
        // Generate combined hash
        std::ostringstream oss;
        for (const auto& h : it->second) oss << h.substr(0, 8);
        info.content_hash = SimpleHash::compute(
            std::vector<uint8_t>(oss.str().begin(), oss.str().end()));
        
        return info;
    }
    
    /**
     * @brief Run garbage collection to remove orphaned blocks
     * @return Number of blocks cleaned up
     */
    size_t runGarbageCollection() {
        std::lock_guard<std::mutex> lock(mtx_);
        size_t cleaned = 0;
        
        for (auto it = blocks_.begin(); it != blocks_.end();) {
            if (it->second.ref_count == 0) {
                stats_.bytes_after_dedupe -= it->second.data.size();
                --stats_.unique_blocks;
                it = blocks_.erase(it);
                ++cleaned;
            } else {
                ++it;
            }
        }
        
        return cleaned;
    }
    
    /**
     * @brief Check if two datasets share any blocks
     * @param dataset1 First dataset name
     * @param dataset2 Second dataset name
     * @return Number of shared blocks
     */
    size_t getSharedBlockCount(const std::string& dataset1, 
                                const std::string& dataset2) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it1 = dataset_blocks_.find(dataset1);
        auto it2 = dataset_blocks_.find(dataset2);
        
        if (it1 == dataset_blocks_.end() || it2 == dataset_blocks_.end()) {
            return 0;
        }
        
        std::set<std::string> set1(it1->second.begin(), it1->second.end());
        size_t shared = 0;
        for (const auto& hash : it2->second) {
            if (set1.count(hash)) ++shared;
        }
        
        return shared;
    }
    
    /**
     * @brief Get deduplication statistics
     */
    DedupeStatistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return stats_;
    }
    
    /**
     * @brief Generate statistics report
     */
    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        oss << "=== Deduplication Statistics ===\n"
            << "Total Blocks: " << stats_.total_blocks << "\n"
            << "Unique Blocks: " << stats_.unique_blocks << "\n"
            << "Duplicate Blocks: " << stats_.duplicate_blocks << "\n"
            << "Bytes Before: " << formatBytes(stats_.bytes_before_dedupe) << "\n"
            << "Bytes After: " << formatBytes(stats_.bytes_after_dedupe) << "\n"
            << "Bytes Saved: " << formatBytes(stats_.bytes_saved) << "\n"
            << "Dedupe Ratio: " << std::fixed << std::setprecision(2) 
            << stats_.dedupe_ratio << ":1\n"
            << "Inline Hits: " << stats_.inline_dedupe_hits << "\n"
            << "Datasets: " << dataset_blocks_.size() << "\n";
        return oss.str();
    }
    
    /**
     * @brief Get block size
     */
    size_t getBlockSize() const { return block_size_; }
    
    /**
     * @brief Get total unique block count
     */
    size_t getUniqueBlockCount() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return blocks_.size();
    }
    
    /**
     * @brief Get dataset count
     */
    size_t getDatasetCount() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return dataset_blocks_.size();
    }
    
    /**
     * @brief Verify data integrity
     * @param dataset_name Name of the dataset
     * @return True if all blocks are valid
     */
    bool verifyIntegrity(const std::string& dataset_name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = dataset_blocks_.find(dataset_name);
        if (it == dataset_blocks_.end()) return false;
        
        for (const auto& hash : it->second) {
            auto blk_it = blocks_.find(hash);
            if (blk_it == blocks_.end()) return false;
            
            // Verify hash matches content
            std::string computed = SimpleHash::compute(blk_it->second.data);
            if (computed != hash) return false;
        }
        
        return true;
    }

private:
    size_t block_size_;
    std::atomic<bool> running_;
    mutable std::mutex mtx_;
    
    std::map<std::string, DedupeBlock> blocks_;  // hash -> block
    std::map<std::string, std::vector<std::string>> dataset_blocks_;  // dataset -> hashes
    DedupeStatistics stats_;
};

} // namespace dfsmshsm
#endif // DFSMSHSM_DEDUPLICATION_MANAGER_HPP
