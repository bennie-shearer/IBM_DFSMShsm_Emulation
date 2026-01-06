/*******************************************************************************
 * DFSMShsm Emulator - Tape Pool Management System
 * @version 4.2.0
 * 
 * Provides comprehensive tape pool management including scratch pool
 * allocation, pool statistics, volume expiration, and capacity planning.
 * 
 * Features:
 * - Multiple tape pool definitions (scratch, private, storage groups)
 * - Automatic scratch volume allocation
 * - Volume expiration and retention management
 * - Pool capacity tracking and thresholds
 * - Usage statistics and trending
 * - Pool rebalancing recommendations
 * - Integration with HSM migration/recall
 * 
 * Author: DFSMShsm Emulator Team
 * License: MIT
 ******************************************************************************/

#ifndef HSM_TAPE_POOL_HPP
#define HSM_TAPE_POOL_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <optional>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <random>
#include <sstream>
#include <iomanip>
#include <functional>
#include <queue>

namespace dfsmshsm {
namespace tapepool {

// ============================================================================
// Volume Status and Types
// ============================================================================

enum class VolumeStatus {
    SCRATCH,        // Available for allocation
    PRIVATE,        // Allocated to specific owner
    FULL,           // No space remaining
    FILLING,        // Currently being written
    EXPIRED,        // Past retention, awaiting return to scratch
    HELD,           // Held for retention period
    ERROR,          // I/O or hardware error
    CLEANING,       // Cleaning cartridge
    PENDING_MOUNT,  // Queued for mount
    MOUNTED,        // Currently mounted
    EXPORTED,       // Exported from library
    EJECTED         // Ejected from library
};

inline std::string volumeStatusToString(VolumeStatus status) {
    switch (status) {
        case VolumeStatus::SCRATCH:       return "SCRATCH";
        case VolumeStatus::PRIVATE:       return "PRIVATE";
        case VolumeStatus::FULL:          return "FULL";
        case VolumeStatus::FILLING:       return "FILLING";
        case VolumeStatus::EXPIRED:       return "EXPIRED";
        case VolumeStatus::HELD:          return "HELD";
        case VolumeStatus::ERROR:         return "ERROR";
        case VolumeStatus::CLEANING:      return "CLEANING";
        case VolumeStatus::PENDING_MOUNT: return "PENDING_MOUNT";
        case VolumeStatus::MOUNTED:       return "MOUNTED";
        case VolumeStatus::EXPORTED:      return "EXPORTED";
        case VolumeStatus::EJECTED:       return "EJECTED";
        default: return "UNKNOWN";
    }
}

enum class MediaType {
    LTO4,           // 800GB native
    LTO5,           // 1.5TB native
    LTO6,           // 2.5TB native
    LTO7,           // 6TB native
    LTO8,           // 12TB native
    LTO9,           // 18TB native
    TS1140,         // IBM 3592 4TB
    TS1150,         // IBM 3592 10TB
    TS1160,         // IBM 3592 20TB
    VIRTUAL,        // Virtual tape
    UNKNOWN
};

inline std::string mediaTypeToString(MediaType type) {
    switch (type) {
        case MediaType::LTO4:    return "LTO4";
        case MediaType::LTO5:    return "LTO5";
        case MediaType::LTO6:    return "LTO6";
        case MediaType::LTO7:    return "LTO7";
        case MediaType::LTO8:    return "LTO8";
        case MediaType::LTO9:    return "LTO9";
        case MediaType::TS1140:  return "TS1140";
        case MediaType::TS1150:  return "TS1150";
        case MediaType::TS1160:  return "TS1160";
        case MediaType::VIRTUAL: return "VIRTUAL";
        default: return "UNKNOWN";
    }
}

inline size_t getMediaCapacity(MediaType type) {
    switch (type) {
        case MediaType::LTO4:    return 800ULL * 1024 * 1024 * 1024;
        case MediaType::LTO5:    return 1500ULL * 1024 * 1024 * 1024;
        case MediaType::LTO6:    return 2500ULL * 1024 * 1024 * 1024;
        case MediaType::LTO7:    return 6000ULL * 1024 * 1024 * 1024;
        case MediaType::LTO8:    return 12000ULL * 1024 * 1024 * 1024;
        case MediaType::LTO9:    return 18000ULL * 1024 * 1024 * 1024;
        case MediaType::TS1140:  return 4000ULL * 1024 * 1024 * 1024;
        case MediaType::TS1150:  return 10000ULL * 1024 * 1024 * 1024;
        case MediaType::TS1160:  return 20000ULL * 1024 * 1024 * 1024;
        case MediaType::VIRTUAL: return 10000ULL * 1024 * 1024 * 1024;
        default: return 0;
    }
}

// ============================================================================
// Tape Volume
// ============================================================================

struct TapeVolume {
    std::string volser;              // Volume serial (6 chars)
    MediaType media_type;
    VolumeStatus status;
    std::string pool_name;           // Owning pool
    std::string storage_group;       // Storage group assignment
    std::string owner;               // Owner identifier
    
    // Capacity tracking
    size_t capacity;                 // Total capacity in bytes
    size_t used_space;               // Used space in bytes
    size_t available_space;          // Available space
    size_t file_count;               // Number of files
    
    // Location
    std::string library;             // Library name
    std::string slot;                // Slot location
    std::string drive;               // Current drive (if mounted)
    
    // Timestamps
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point last_mounted;
    std::chrono::system_clock::time_point last_written;
    std::chrono::system_clock::time_point expiration;
    
    // Retention
    int retention_days;              // Retention period
    bool retention_expired;
    
    // Statistics
    uint64_t mount_count;
    uint64_t read_count;
    uint64_t write_count;
    uint64_t error_count;
    
    // Error tracking
    std::string last_error;
    std::chrono::system_clock::time_point last_error_time;
    
    TapeVolume()
        : media_type(MediaType::UNKNOWN)
        , status(VolumeStatus::SCRATCH)
        , capacity(0)
        , used_space(0)
        , available_space(0)
        , file_count(0)
        , retention_days(0)
        , retention_expired(false)
        , mount_count(0)
        , read_count(0)
        , write_count(0)
        , error_count(0) {}
    
    double utilizationPercent() const {
        if (capacity == 0) return 0;
        return (static_cast<double>(used_space) / capacity) * 100.0;
    }
    
    bool isScratch() const {
        return status == VolumeStatus::SCRATCH;
    }
    
    bool isAvailable() const {
        return status == VolumeStatus::SCRATCH || 
               (status == VolumeStatus::FILLING && available_space > 0);
    }
    
    bool isExpired() const {
        if (retention_days <= 0) return false;
        auto now = std::chrono::system_clock::now();
        return now >= expiration;
    }
    
    std::string toSummary() const {
        std::ostringstream oss;
        oss << volser << " | " << mediaTypeToString(media_type)
            << " | " << volumeStatusToString(status)
            << " | " << std::fixed << std::setprecision(1) 
            << utilizationPercent() << "% used"
            << " | " << file_count << " files";
        return oss.str();
    }
};

// ============================================================================
// Tape Pool Definition
// ============================================================================

struct PoolThresholds {
    double low_scratch_warning = 10.0;    // Warning when scratch < 10%
    double low_scratch_critical = 5.0;    // Critical when scratch < 5%
    double high_utilization_warning = 80.0;
    double high_utilization_critical = 90.0;
    size_t min_scratch_count = 10;
    size_t max_private_volumes = 0;       // 0 = unlimited
};

struct TapePool {
    std::string name;
    std::string description;
    MediaType default_media_type;
    
    // Pool configuration
    bool auto_scratch_return;            // Auto-return expired to scratch
    int default_retention_days;
    size_t target_scratch_count;         // Target scratch pool size
    PoolThresholds thresholds;
    
    // Volume tracking (by volser)
    std::set<std::string> volume_sers;
    
    // Statistics
    size_t total_volumes;
    size_t scratch_count;
    size_t private_count;
    size_t full_count;
    size_t error_count;
    size_t total_capacity;
    size_t used_capacity;
    
    TapePool()
        : default_media_type(MediaType::LTO8)
        , auto_scratch_return(true)
        , default_retention_days(30)
        , target_scratch_count(100)
        , total_volumes(0)
        , scratch_count(0)
        , private_count(0)
        , full_count(0)
        , error_count(0)
        , total_capacity(0)
        , used_capacity(0) {}
    
    double scratchPercentage() const {
        if (total_volumes == 0) return 0;
        return (static_cast<double>(scratch_count) / total_volumes) * 100.0;
    }
    
    double utilizationPercentage() const {
        if (total_capacity == 0) return 0;
        return (static_cast<double>(used_capacity) / total_capacity) * 100.0;
    }
    
    bool isLowOnScratch() const {
        return scratchPercentage() < thresholds.low_scratch_warning;
    }
    
    bool isCriticalOnScratch() const {
        return scratchPercentage() < thresholds.low_scratch_critical ||
               scratch_count < thresholds.min_scratch_count;
    }
};

// ============================================================================
// Pool Statistics
// ============================================================================

struct PoolStatistics {
    std::string pool_name;
    std::chrono::system_clock::time_point timestamp;
    
    // Volume counts
    size_t total_volumes;
    size_t scratch_volumes;
    size_t private_volumes;
    size_t full_volumes;
    size_t filling_volumes;
    size_t error_volumes;
    size_t expired_volumes;
    
    // Capacity
    size_t total_capacity_bytes;
    size_t used_capacity_bytes;
    size_t available_capacity_bytes;
    double utilization_percent;
    
    // Activity
    uint64_t mounts_today;
    uint64_t mounts_this_week;
    uint64_t bytes_written_today;
    uint64_t bytes_read_today;
    
    // File counts
    size_t total_files;
    double avg_files_per_volume;
    
    std::string toJSON() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "{\n";
        oss << "  \"pool_name\": \"" << pool_name << "\",\n";
        oss << "  \"total_volumes\": " << total_volumes << ",\n";
        oss << "  \"scratch_volumes\": " << scratch_volumes << ",\n";
        oss << "  \"private_volumes\": " << private_volumes << ",\n";
        oss << "  \"full_volumes\": " << full_volumes << ",\n";
        oss << "  \"utilization_percent\": " << utilization_percent << ",\n";
        oss << "  \"total_capacity_gb\": " << (total_capacity_bytes / (1024.0 * 1024 * 1024)) << ",\n";
        oss << "  \"used_capacity_gb\": " << (used_capacity_bytes / (1024.0 * 1024 * 1024)) << ",\n";
        oss << "  \"total_files\": " << total_files << "\n";
        oss << "}";
        return oss.str();
    }
};

// ============================================================================
// Allocation Request and Result
// ============================================================================

struct AllocationRequest {
    std::string pool_name;
    std::string storage_group;
    std::string owner;
    MediaType preferred_media;
    size_t space_needed;
    int retention_days;
    bool prefer_empty;              // Prefer empty volumes
    bool allow_partial;             // Allow partially filled volumes
    
    AllocationRequest()
        : preferred_media(MediaType::UNKNOWN)
        , space_needed(0)
        , retention_days(30)
        , prefer_empty(true)
        , allow_partial(true) {}
};

struct AllocationResult {
    bool success;
    std::string volser;
    std::string error_message;
    size_t available_space;
    
    AllocationResult() : success(false), available_space(0) {}
    
    static AllocationResult Success(const std::string& vol, size_t space) {
        AllocationResult r;
        r.success = true;
        r.volser = vol;
        r.available_space = space;
        return r;
    }
    
    static AllocationResult Failure(const std::string& msg) {
        AllocationResult r;
        r.success = false;
        r.error_message = msg;
        return r;
    }
};

// ============================================================================
// Tape Pool Manager
// ============================================================================

class TapePoolManager {
public:
    TapePoolManager() = default;
    
    // Pool management
    void createPool(const TapePool& pool) {
        std::lock_guard<std::mutex> lock(mutex_);
        pools_[pool.name] = pool;
    }
    
    bool deletePool(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pools_.find(name);
        if (it == pools_.end()) return false;
        
        // Can only delete empty pools
        if (!it->second.volume_sers.empty()) {
            return false;
        }
        
        pools_.erase(it);
        return true;
    }
    
    std::optional<TapePool> getPool(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pools_.find(name);
        if (it != pools_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<std::string> getPoolNames() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& [name, pool] : pools_) {
            names.push_back(name);
        }
        return names;
    }
    
    // Volume management
    void addVolume(const TapeVolume& volume) {
        std::lock_guard<std::mutex> lock(mutex_);
        volumes_[volume.volser] = volume;
        
        // Add to pool
        auto pool_it = pools_.find(volume.pool_name);
        if (pool_it != pools_.end()) {
            pool_it->second.volume_sers.insert(volume.volser);
            updatePoolStats(pool_it->second);
        }
    }
    
    bool removeVolume(const std::string& volser) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = volumes_.find(volser);
        if (it == volumes_.end()) return false;
        
        // Remove from pool
        auto pool_it = pools_.find(it->second.pool_name);
        if (pool_it != pools_.end()) {
            pool_it->second.volume_sers.erase(volser);
            updatePoolStats(pool_it->second);
        }
        
        volumes_.erase(it);
        return true;
    }
    
    std::optional<TapeVolume> getVolume(const std::string& volser) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = volumes_.find(volser);
        if (it != volumes_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    void updateVolume(const TapeVolume& volume) {
        std::lock_guard<std::mutex> lock(mutex_);
        volumes_[volume.volser] = volume;
        
        auto pool_it = pools_.find(volume.pool_name);
        if (pool_it != pools_.end()) {
            updatePoolStats(pool_it->second);
        }
    }
    
    // Scratch allocation
    AllocationResult allocateScratch(const AllocationRequest& request) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Find pool
        auto pool_it = pools_.find(request.pool_name);
        if (pool_it == pools_.end()) {
            return AllocationResult::Failure("Pool not found: " + request.pool_name);
        }
        
        TapePool& pool = pool_it->second;
        
        // Collect candidate volumes
        std::vector<std::string> candidates;
        
        for (const auto& volser : pool.volume_sers) {
            auto vol_it = volumes_.find(volser);
            if (vol_it == volumes_.end()) continue;
            
            TapeVolume& vol = vol_it->second;
            
            // Check media type
            if (request.preferred_media != MediaType::UNKNOWN &&
                vol.media_type != request.preferred_media) {
                continue;
            }
            
            // Check availability
            if (vol.status == VolumeStatus::SCRATCH) {
                candidates.push_back(volser);
            } else if (request.allow_partial && 
                       vol.status == VolumeStatus::FILLING &&
                       vol.available_space >= request.space_needed) {
                candidates.push_back(volser);
            }
        }
        
        if (candidates.empty()) {
            return AllocationResult::Failure("No suitable scratch volumes available");
        }
        
        // Sort candidates (prefer empty, then by available space)
        std::sort(candidates.begin(), candidates.end(), 
            [this, &request](const std::string& a, const std::string& b) {
                const auto& va = volumes_[a];
                const auto& vb = volumes_[b];
                
                if (request.prefer_empty) {
                    bool a_empty = va.status == VolumeStatus::SCRATCH;
                    bool b_empty = vb.status == VolumeStatus::SCRATCH;
                    if (a_empty != b_empty) return a_empty;
                }
                
                return va.available_space > vb.available_space;
            });
        
        // Allocate first candidate
        std::string selected = candidates[0];
        TapeVolume& vol = volumes_[selected];
        
        vol.status = VolumeStatus::FILLING;
        vol.owner = request.owner;
        vol.storage_group = request.storage_group;
        vol.retention_days = request.retention_days;
        
        // Set expiration
        if (request.retention_days > 0) {
            vol.expiration = std::chrono::system_clock::now() + 
                             std::chrono::hours(24 * request.retention_days);
        }
        
        updatePoolStats(pool);
        
        return AllocationResult::Success(selected, vol.available_space);
    }
    
    // Return volume to scratch
    bool returnToScratch(const std::string& volser) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = volumes_.find(volser);
        if (it == volumes_.end()) return false;
        
        TapeVolume& vol = it->second;
        vol.status = VolumeStatus::SCRATCH;
        vol.owner.clear();
        vol.storage_group.clear();
        vol.used_space = 0;
        vol.available_space = vol.capacity;
        vol.file_count = 0;
        vol.retention_expired = false;
        
        auto pool_it = pools_.find(vol.pool_name);
        if (pool_it != pools_.end()) {
            updatePoolStats(pool_it->second);
        }
        
        return true;
    }
    
    // Process expired volumes
    std::vector<std::string> processExpiredVolumes() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> processed;
        
        auto now = std::chrono::system_clock::now();
        
        for (auto& [volser, vol] : volumes_) {
            if (vol.retention_days > 0 && 
                vol.status == VolumeStatus::HELD &&
                now >= vol.expiration) {
                
                vol.status = VolumeStatus::EXPIRED;
                vol.retention_expired = true;
                
                // Auto-return to scratch if pool configured
                auto pool_it = pools_.find(vol.pool_name);
                if (pool_it != pools_.end() && pool_it->second.auto_scratch_return) {
                    vol.status = VolumeStatus::SCRATCH;
                    vol.owner.clear();
                    vol.used_space = 0;
                    vol.available_space = vol.capacity;
                    vol.file_count = 0;
                    updatePoolStats(pool_it->second);
                }
                
                processed.push_back(volser);
            }
        }
        
        return processed;
    }
    
    // Get pool statistics
    PoolStatistics getPoolStatistics(const std::string& pool_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        PoolStatistics stats;
        stats.pool_name = pool_name;
        stats.timestamp = std::chrono::system_clock::now();
        
        auto pool_it = pools_.find(pool_name);
        if (pool_it == pools_.end()) {
            return stats;
        }
        
        const TapePool& pool = pool_it->second;
        
        stats.total_volumes = pool.total_volumes;
        stats.scratch_volumes = pool.scratch_count;
        stats.private_volumes = pool.private_count;
        stats.full_volumes = pool.full_count;
        stats.error_volumes = pool.error_count;
        stats.total_capacity_bytes = pool.total_capacity;
        stats.used_capacity_bytes = pool.used_capacity;
        stats.available_capacity_bytes = pool.total_capacity - pool.used_capacity;
        stats.utilization_percent = pool.utilizationPercentage();
        
        // Calculate file statistics
        for (const auto& volser : pool.volume_sers) {
            auto vol_it = volumes_.find(volser);
            if (vol_it != volumes_.end()) {
                stats.total_files += vol_it->second.file_count;
                
                if (vol_it->second.status == VolumeStatus::FILLING) {
                    stats.filling_volumes++;
                }
                if (vol_it->second.isExpired()) {
                    stats.expired_volumes++;
                }
            }
        }
        
        if (stats.total_volumes > 0) {
            stats.avg_files_per_volume = 
                static_cast<double>(stats.total_files) / stats.total_volumes;
        }
        
        return stats;
    }
    
    // Get all volumes in pool
    std::vector<TapeVolume> getPoolVolumes(const std::string& pool_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TapeVolume> result;
        
        auto pool_it = pools_.find(pool_name);
        if (pool_it == pools_.end()) {
            return result;
        }
        
        for (const auto& volser : pool_it->second.volume_sers) {
            auto vol_it = volumes_.find(volser);
            if (vol_it != volumes_.end()) {
                result.push_back(vol_it->second);
            }
        }
        
        return result;
    }
    
    // Get volumes by status
    std::vector<TapeVolume> getVolumesByStatus(VolumeStatus status) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TapeVolume> result;
        
        for (const auto& [volser, vol] : volumes_) {
            if (vol.status == status) {
                result.push_back(vol);
            }
        }
        
        return result;
    }
    
    // Get volumes expiring soon
    std::vector<TapeVolume> getVolumesExpiringSoon(int days) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TapeVolume> result;
        
        auto threshold = std::chrono::system_clock::now() + 
                         std::chrono::hours(24 * days);
        
        for (const auto& [volser, vol] : volumes_) {
            if (vol.retention_days > 0 && 
                vol.expiration <= threshold &&
                vol.status != VolumeStatus::SCRATCH) {
                result.push_back(vol);
            }
        }
        
        std::sort(result.begin(), result.end(),
            [](const TapeVolume& a, const TapeVolume& b) {
                return a.expiration < b.expiration;
            });
        
        return result;
    }
    
    // Check pool health
    struct PoolHealthReport {
        std::string pool_name;
        bool healthy;
        std::vector<std::string> warnings;
        std::vector<std::string> critical_issues;
        double scratch_percentage;
        double utilization_percentage;
        size_t error_volume_count;
    };
    
    PoolHealthReport checkPoolHealth(const std::string& pool_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        PoolHealthReport report;
        report.pool_name = pool_name;
        report.healthy = true;
        
        auto pool_it = pools_.find(pool_name);
        if (pool_it == pools_.end()) {
            report.healthy = false;
            report.critical_issues.push_back("Pool not found");
            return report;
        }
        
        const TapePool& pool = pool_it->second;
        report.scratch_percentage = pool.scratchPercentage();
        report.utilization_percentage = pool.utilizationPercentage();
        report.error_volume_count = pool.error_count;
        
        // Check scratch levels
        if (pool.isCriticalOnScratch()) {
            report.healthy = false;
            report.critical_issues.push_back(
                "Critical: Scratch pool below minimum threshold");
        } else if (pool.isLowOnScratch()) {
            report.warnings.push_back("Low scratch volume count");
        }
        
        // Check utilization
        if (report.utilization_percentage >= pool.thresholds.high_utilization_critical) {
            report.healthy = false;
            report.critical_issues.push_back(
                "Critical: Pool utilization exceeds " + 
                std::to_string(static_cast<int>(pool.thresholds.high_utilization_critical)) + "%");
        } else if (report.utilization_percentage >= pool.thresholds.high_utilization_warning) {
            report.warnings.push_back("High pool utilization");
        }
        
        // Check error volumes
        if (pool.error_count > 0) {
            report.warnings.push_back(
                std::to_string(pool.error_count) + " volumes in error state");
        }
        
        return report;
    }
    
    // Generate scratch volumes for testing
    void generateTestVolumes(const std::string& pool_name, 
                             int count, 
                             MediaType media_type) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto pool_it = pools_.find(pool_name);
        if (pool_it == pools_.end()) return;
        
        static int vol_counter = 0;
        
        for (int i = 0; i < count; ++i) {
            TapeVolume vol;
            
            // Generate volser (e.g., "T00001")
            std::ostringstream oss;
            oss << "T" << std::setfill('0') << std::setw(5) << (++vol_counter);
            vol.volser = oss.str();
            
            vol.media_type = media_type;
            vol.status = VolumeStatus::SCRATCH;
            vol.pool_name = pool_name;
            vol.capacity = getMediaCapacity(media_type);
            vol.available_space = vol.capacity;
            vol.created = std::chrono::system_clock::now();
            
            volumes_[vol.volser] = vol;
            pool_it->second.volume_sers.insert(vol.volser);
        }
        
        updatePoolStats(pool_it->second);
    }
    
private:
    void updatePoolStats(TapePool& pool) {
        pool.total_volumes = 0;
        pool.scratch_count = 0;
        pool.private_count = 0;
        pool.full_count = 0;
        pool.error_count = 0;
        pool.total_capacity = 0;
        pool.used_capacity = 0;
        
        for (const auto& volser : pool.volume_sers) {
            auto it = volumes_.find(volser);
            if (it == volumes_.end()) continue;
            
            const TapeVolume& vol = it->second;
            pool.total_volumes++;
            pool.total_capacity += vol.capacity;
            pool.used_capacity += vol.used_space;
            
            switch (vol.status) {
                case VolumeStatus::SCRATCH:
                    pool.scratch_count++;
                    break;
                case VolumeStatus::PRIVATE:
                case VolumeStatus::FILLING:
                case VolumeStatus::HELD:
                    pool.private_count++;
                    break;
                case VolumeStatus::FULL:
                    pool.full_count++;
                    break;
                case VolumeStatus::ERROR:
                    pool.error_count++;
                    break;
                default:
                    break;
            }
        }
    }
    
    std::map<std::string, TapePool> pools_;
    std::map<std::string, TapeVolume> volumes_;
    mutable std::mutex mutex_;
};

// ============================================================================
// Storage Group Management
// ============================================================================

struct StorageGroup {
    std::string name;
    std::string description;
    std::vector<std::string> pool_names;  // Pools in priority order
    
    // Allocation policy
    bool overflow_to_next_pool;
    size_t min_available_space;
    
    StorageGroup()
        : overflow_to_next_pool(true)
        , min_available_space(0) {}
};

class StorageGroupManager {
public:
    void createGroup(const StorageGroup& group) {
        std::lock_guard<std::mutex> lock(mutex_);
        groups_[group.name] = group;
    }
    
    std::optional<StorageGroup> getGroup(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = groups_.find(name);
        if (it != groups_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<std::string> getGroupNames() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& [name, group] : groups_) {
            names.push_back(name);
        }
        return names;
    }
    
    // Allocate from storage group (tries pools in order)
    AllocationResult allocateFromGroup(
            const std::string& group_name,
            TapePoolManager& pool_manager,
            const AllocationRequest& base_request) {
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = groups_.find(group_name);
        if (it == groups_.end()) {
            return AllocationResult::Failure("Storage group not found: " + group_name);
        }
        
        const StorageGroup& group = it->second;
        
        for (const auto& pool_name : group.pool_names) {
            AllocationRequest request = base_request;
            request.pool_name = pool_name;
            request.storage_group = group_name;
            
            AllocationResult result = pool_manager.allocateScratch(request);
            if (result.success) {
                return result;
            }
            
            if (!group.overflow_to_next_pool) {
                break;
            }
        }
        
        return AllocationResult::Failure(
            "No volumes available in storage group: " + group_name);
    }
    
private:
    std::map<std::string, StorageGroup> groups_;
    mutable std::mutex mutex_;
};

} // namespace tapepool
} // namespace dfsmshsm

#endif // HSM_TAPE_POOL_HPP
