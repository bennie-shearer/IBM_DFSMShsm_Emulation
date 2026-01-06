/**
 * @file quota_manager.hpp
 * @brief User and group quota management with enforcement
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 * 
 * Implements hierarchical quota management with:
 * - User-level quotas
 * - Group-level quotas
 * - Soft limits with grace periods
 * - Hard limit enforcement
 */
#ifndef DFSMSHSM_QUOTA_MANAGER_HPP
#define DFSMSHSM_QUOTA_MANAGER_HPP

#include "hsm_types.hpp"
#include "event_system.hpp"
#include <map>
#include <mutex>
#include <atomic>
#include <sstream>
#include <iomanip>

namespace dfsmshsm {

/**
 * @struct QuotaUsage
 * @brief Current quota usage information
 */
struct QuotaUsage {
    std::string id;
    bool is_group = false;
    uint64_t space_limit = 0;
    uint64_t space_used = 0;
    uint64_t space_available = 0;
    uint64_t inode_limit = 0;
    uint64_t inode_used = 0;
    uint64_t inode_available = 0;
    double space_percent = 0.0;
    double inode_percent = 0.0;
    QuotaCheckResult status = QuotaCheckResult::OK;
    std::chrono::system_clock::time_point grace_expires;
};

/**
 * @class QuotaManager
 * @brief Manages storage quotas for users and groups
 */
class QuotaManager {
public:
    QuotaManager() : running_(false) {}
    
    /**
     * @brief Start the quota manager
     */
    bool start() {
        if (running_) return false;
        running_ = true;
        return true;
    }
    
    /**
     * @brief Stop the quota manager
     */
    void stop() { running_ = false; }
    
    /**
     * @brief Check if manager is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Set quota for a user or group
     * @param id User or group ID
     * @param is_group True if this is a group quota
     * @param space_limit Maximum space in bytes (0 = unlimited)
     * @param inode_limit Maximum number of datasets (0 = unlimited)
     * @param soft_limit Soft limit for space (warning threshold)
     * @param grace_days Grace period after soft limit exceeded
     */
    bool setQuota(const std::string& id, bool is_group, 
                  uint64_t space_limit, uint64_t inode_limit = 0,
                  uint64_t soft_limit = 0, uint32_t grace_days = 7) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        QuotaInfo quota;
        quota.id = id;
        quota.is_group = is_group;
        quota.space_limit = space_limit;
        quota.inode_limit = inode_limit;
        quota.soft_limit = soft_limit > 0 ? soft_limit : 
                           static_cast<uint64_t>(space_limit * 0.9);  // 90% default
        quota.grace_period_days = grace_days;
        quota.enabled = true;
        
        if (is_group) {
            group_quotas_[id] = quota;
        } else {
            user_quotas_[id] = quota;
        }
        
        return true;
    }
    
    /**
     * @brief Remove quota for a user or group
     */
    bool removeQuota(const std::string& id, bool is_group) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        if (is_group) {
            return group_quotas_.erase(id) > 0;
        } else {
            return user_quotas_.erase(id) > 0;
        }
    }
    
    /**
     * @brief Get quota information
     */
    std::optional<QuotaInfo> getQuota(const std::string& id, bool is_group) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto& quotas = is_group ? group_quotas_ : user_quotas_;
        auto it = quotas.find(id);
        return it != quotas.end() ? std::optional<QuotaInfo>(it->second) : std::nullopt;
    }
    
    /**
     * @brief Check if operation would exceed quota
     * @param user_id User ID
     * @param group_id Group ID (optional)
     * @param additional_space Additional space needed
     * @param additional_inodes Additional datasets needed
     * @return Quota check result
     */
    QuotaCheckResult checkQuota(const std::string& user_id,
                                 const std::string& group_id,
                                 uint64_t additional_space,
                                 uint64_t additional_inodes = 1) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        // Check user quota
        auto user_result = checkQuotaInternal(user_id, false, 
                                               additional_space, additional_inodes);
        if (user_result == QuotaCheckResult::HARD_EXCEEDED ||
            user_result == QuotaCheckResult::GRACE_EXPIRED) {
            return user_result;
        }
        
        // Check group quota if specified
        if (!group_id.empty()) {
            auto group_result = checkQuotaInternal(group_id, true,
                                                    additional_space, additional_inodes);
            if (group_result == QuotaCheckResult::HARD_EXCEEDED ||
                group_result == QuotaCheckResult::GRACE_EXPIRED) {
                return group_result;
            }
            
            // Return worst case between user and group
            if (group_result == QuotaCheckResult::SOFT_EXCEEDED) {
                return QuotaCheckResult::SOFT_EXCEEDED;
            }
        }
        
        return user_result;
    }
    
    /**
     * @brief Update usage after dataset creation
     * @param user_id User ID
     * @param group_id Group ID (optional)
     * @param space_delta Space change (positive for add, negative for remove)
     * @param inode_delta Inode change
     */
    void updateUsage(const std::string& user_id,
                     const std::string& group_id,
                     int64_t space_delta,
                     int64_t inode_delta = 0) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        updateUsageInternal(user_id, false, space_delta, inode_delta);
        if (!group_id.empty()) {
            updateUsageInternal(group_id, true, space_delta, inode_delta);
        }
    }
    
    /**
     * @brief Get quota usage for a user or group
     */
    std::optional<QuotaUsage> getUsage(const std::string& id, bool is_group) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto& quotas = is_group ? group_quotas_ : user_quotas_;
        auto it = quotas.find(id);
        if (it == quotas.end()) return std::nullopt;
        
        QuotaUsage usage;
        usage.id = id;
        usage.is_group = is_group;
        usage.space_limit = it->second.space_limit;
        usage.space_used = it->second.space_used;
        usage.space_available = it->second.space_limit > it->second.space_used ?
                                it->second.space_limit - it->second.space_used : 0;
        usage.inode_limit = it->second.inode_limit;
        usage.inode_used = it->second.inode_used;
        usage.inode_available = it->second.inode_limit > it->second.inode_used ?
                                it->second.inode_limit - it->second.inode_used : 0;
        
        if (it->second.space_limit > 0) {
            usage.space_percent = 100.0 * static_cast<double>(it->second.space_used) /
                                  static_cast<double>(it->second.space_limit);
        }
        if (it->second.inode_limit > 0) {
            usage.inode_percent = 100.0 * static_cast<double>(it->second.inode_used) /
                                  static_cast<double>(it->second.inode_limit);
        }
        
        // Determine status
        if (it->second.space_used >= it->second.space_limit) {
            usage.status = QuotaCheckResult::HARD_EXCEEDED;
        } else if (it->second.space_used >= it->second.soft_limit) {
            auto now = std::chrono::system_clock::now();
            auto grace_end = it->second.soft_exceeded_time + 
                            std::chrono::hours(24 * it->second.grace_period_days);
            if (now >= grace_end) {
                usage.status = QuotaCheckResult::GRACE_EXPIRED;
            } else {
                usage.status = QuotaCheckResult::SOFT_EXCEEDED;
                usage.grace_expires = grace_end;
            }
        }
        
        return usage;
    }
    
    /**
     * @brief Get all user quotas
     */
    std::vector<QuotaInfo> getAllUserQuotas() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<QuotaInfo> result;
        for (const auto& [id, quota] : user_quotas_) {
            result.push_back(quota);
        }
        return result;
    }
    
    /**
     * @brief Get all group quotas
     */
    std::vector<QuotaInfo> getAllGroupQuotas() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<QuotaInfo> result;
        for (const auto& [id, quota] : group_quotas_) {
            result.push_back(quota);
        }
        return result;
    }
    
    /**
     * @brief Enable or disable quota enforcement
     */
    void setEnforcement(bool enabled) { enforcement_enabled_ = enabled; }
    
    /**
     * @brief Check if enforcement is enabled
     */
    bool isEnforcementEnabled() const { return enforcement_enabled_; }
    
    /**
     * @brief Generate quota report
     */
    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        
        oss << "=== Quota Report ===\n\n";
        oss << "Enforcement: " << (enforcement_enabled_ ? "ENABLED" : "DISABLED") << "\n\n";
        
        oss << "User Quotas (" << user_quotas_.size() << "):\n";
        for (const auto& [id, q] : user_quotas_) {
            double pct = q.space_limit > 0 ? 
                        100.0 * q.space_used / q.space_limit : 0.0;
            oss << "  " << id << ": " << formatBytes(q.space_used) << " / "
                << formatBytes(q.space_limit) << " (" << std::fixed 
                << std::setprecision(1) << pct << "%)\n";
        }
        
        oss << "\nGroup Quotas (" << group_quotas_.size() << "):\n";
        for (const auto& [id, q] : group_quotas_) {
            double pct = q.space_limit > 0 ? 
                        100.0 * q.space_used / q.space_limit : 0.0;
            oss << "  " << id << ": " << formatBytes(q.space_used) << " / "
                << formatBytes(q.space_limit) << " (" << std::fixed 
                << std::setprecision(1) << pct << "%)\n";
        }
        
        return oss.str();
    }
    
    /**
     * @brief Run quota check on all users/groups
     * @return Number of quota violations found
     */
    size_t runQuotaCheck() {
        std::lock_guard<std::mutex> lock(mtx_);
        size_t violations = 0;
        
        auto check_quotas = [&](const std::map<std::string, QuotaInfo>& quotas, 
                                bool is_group) {
            for (const auto& [id, q] : quotas) {
                if (!q.enabled) continue;
                
                if (q.space_limit > 0 && q.space_used > q.space_limit) {
                    ++violations;
                    EventSystem::instance().emit(
                        EventType::QUOTA_EXCEEDED,
                        "QuotaManager",
                        (is_group ? "Group " : "User ") + id + " exceeded space quota",
                        "", "");
                } else if (q.soft_limit > 0 && q.space_used > q.soft_limit) {
                    EventSystem::instance().emit(
                        EventType::QUOTA_WARNING,
                        "QuotaManager",
                        (is_group ? "Group " : "User ") + id + " exceeded soft limit",
                        "", "");
                }
            }
        };
        
        check_quotas(user_quotas_, false);
        check_quotas(group_quotas_, true);
        
        return violations;
    }

private:
    QuotaCheckResult checkQuotaInternal(const std::string& id, bool is_group,
                                         uint64_t additional_space,
                                         uint64_t additional_inodes) {
        auto& quotas = is_group ? group_quotas_ : user_quotas_;
        auto it = quotas.find(id);
        
        if (it == quotas.end() || !it->second.enabled) {
            return QuotaCheckResult::NO_QUOTA;
        }
        
        const auto& q = it->second;
        
        // Check space limit
        if (q.space_limit > 0) {
            uint64_t new_usage = q.space_used + additional_space;
            
            if (new_usage > q.space_limit) {
                return QuotaCheckResult::HARD_EXCEEDED;
            }
            
            if (new_usage > q.soft_limit) {
                auto now = std::chrono::system_clock::now();
                
                // Check if grace period has expired
                if (q.space_used > q.soft_limit) {
                    auto grace_end = q.soft_exceeded_time +
                                    std::chrono::hours(24 * q.grace_period_days);
                    if (now >= grace_end) {
                        return QuotaCheckResult::GRACE_EXPIRED;
                    }
                }
                
                return QuotaCheckResult::SOFT_EXCEEDED;
            }
        }
        
        // Check inode limit
        if (q.inode_limit > 0) {
            if (q.inode_used + additional_inodes > q.inode_limit) {
                return QuotaCheckResult::HARD_EXCEEDED;
            }
        }
        
        return QuotaCheckResult::OK;
    }
    
    void updateUsageInternal(const std::string& id, bool is_group,
                             int64_t space_delta, int64_t inode_delta) {
        auto& quotas = is_group ? group_quotas_ : user_quotas_;
        auto it = quotas.find(id);
        
        if (it == quotas.end()) return;
        
        auto& q = it->second;
        
        // Update space usage
        if (space_delta > 0) {
            q.space_used += static_cast<uint64_t>(space_delta);
        } else if (space_delta < 0) {
            uint64_t abs_delta = static_cast<uint64_t>(-space_delta);
            q.space_used = q.space_used > abs_delta ? q.space_used - abs_delta : 0;
        }
        
        // Update inode usage
        if (inode_delta > 0) {
            q.inode_used += static_cast<uint64_t>(inode_delta);
        } else if (inode_delta < 0) {
            uint64_t abs_delta = static_cast<uint64_t>(-inode_delta);
            q.inode_used = q.inode_used > abs_delta ? q.inode_used - abs_delta : 0;
        }
        
        // Track soft limit crossing
        if (q.soft_limit > 0 && q.space_used > q.soft_limit) {
            if (q.soft_exceeded_time == std::chrono::system_clock::time_point{}) {
                q.soft_exceeded_time = std::chrono::system_clock::now();
            }
        } else {
            q.soft_exceeded_time = std::chrono::system_clock::time_point{};
        }
    }
    
    std::atomic<bool> running_;
    std::atomic<bool> enforcement_enabled_{true};
    mutable std::mutex mtx_;
    std::map<std::string, QuotaInfo> user_quotas_;
    std::map<std::string, QuotaInfo> group_quotas_;
};

} // namespace dfsmshsm
#endif // DFSMSHSM_QUOTA_MANAGER_HPP
