/**
 * @file lifecycle_manager.hpp
 * @brief Data lifecycle management with automated policies
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 * 
 * Implements lifecycle management:
 * - Automated tier transitions
 * - Expiration policies
 * - Retention management
 */
#ifndef DFSMSHSM_LIFECYCLE_MANAGER_HPP
#define DFSMSHSM_LIFECYCLE_MANAGER_HPP

#include "hsm_types.hpp"
#include "event_system.hpp"
#include "result.hpp"
#include <map>
#include <mutex>
#include <atomic>
#include <regex>
#include <sstream>
#include <iomanip>

namespace dfsmshsm {

// Forward declaration
class HSMEngine;

/**
 * @struct LifecycleStatistics
 * @brief Lifecycle management statistics
 */
struct LifecycleStatistics {
    uint64_t datasets_processed = 0;
    uint64_t tier_transitions = 0;
    uint64_t archival_operations = 0;
    uint64_t expiration_deletions = 0;
    uint64_t bytes_transitioned = 0;
    uint64_t bytes_archived = 0;
    uint64_t bytes_expired = 0;
};

/**
 * @struct LifecycleCandidate
 * @brief Dataset identified as a candidate for lifecycle action
 */
struct LifecycleCandidate {
    std::string dataset_name;
    OperationType action;
    std::string reason;
    StorageTier current_tier;
    StorageTier target_tier;
    uint64_t size;
};

/**
 * @class LifecycleManager
 * @brief Manages data lifecycle policies
 */
class LifecycleManager {
public:
    LifecycleManager() : running_(false), engine_(nullptr) {}
    
    explicit LifecycleManager(HSMEngine& engine) 
        : running_(false), engine_(&engine) {}
    
    /**
     * @brief Start the lifecycle manager
     */
    bool start() {
        if (running_) return false;
        running_ = true;
        return true;
    }
    
    /**
     * @brief Stop the lifecycle manager
     */
    void stop() { running_ = false; }
    
    /**
     * @brief Check if manager is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Add a lifecycle policy (alias for createPolicy)
     */
    [[nodiscard]] Result<void> addPolicy(const LifecyclePolicy& policy) {
        if (createPolicy(policy)) {
            return Ok();
        }
        return Err<void>(ErrorCode::ALREADY_EXISTS, 
            "Policy already exists: " + policy.name);
    }
    
    /**
     * @brief Create a lifecycle policy
     */
    bool createPolicy(const LifecyclePolicy& policy) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        if (policies_.find(policy.name) != policies_.end()) {
            return false;  // Already exists
        }
        
        policies_[policy.name] = policy;
        return true;
    }
    
    /**
     * @brief Update a lifecycle policy
     */
    bool updatePolicy(const LifecyclePolicy& policy) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = policies_.find(policy.name);
        if (it == policies_.end()) return false;
        
        it->second = policy;
        return true;
    }
    
    /**
     * @brief Delete a lifecycle policy
     */
    bool deletePolicy(const std::string& name) {
        std::lock_guard<std::mutex> lock(mtx_);
        return policies_.erase(name) > 0;
    }
    
    /**
     * @brief Get a lifecycle policy
     */
    std::optional<LifecyclePolicy> getPolicy(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = policies_.find(name);
        return it != policies_.end() ? std::optional<LifecyclePolicy>(it->second) : std::nullopt;
    }
    
    /**
     * @brief Get all lifecycle policies
     */
    std::vector<LifecyclePolicy> getAllPolicies() const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        std::vector<LifecyclePolicy> result;
        for (const auto& [name, policy] : policies_) {
            result.push_back(policy);
        }
        return result;
    }
    
    /**
     * @brief Get all policies (alias for getAllPolicies)
     */
    std::vector<LifecyclePolicy> getPolicies() const {
        return getAllPolicies();
    }
    
    /**
     * @brief Evaluate and return candidates for lifecycle actions (alias)
     */
    std::vector<LifecycleCandidate> evaluateCandidates() {
        return evaluatePolicies();
    }
    
    /**
     * @brief Assign a policy to a dataset
     */
    void assignPolicy(const std::string& dataset_name, const std::string& policy_name) {
        std::lock_guard<std::mutex> lock(mtx_);
        dataset_policies_[dataset_name] = policy_name;
    }
    
    /**
     * @brief Remove policy assignment from a dataset
     */
    void unassignPolicy(const std::string& dataset_name) {
        std::lock_guard<std::mutex> lock(mtx_);
        dataset_policies_.erase(dataset_name);
    }
    
    /**
     * @brief Get policy assigned to a dataset
     */
    std::optional<std::string> getAssignedPolicy(const std::string& dataset_name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = dataset_policies_.find(dataset_name);
        return it != dataset_policies_.end() ? std::optional<std::string>(it->second) : std::nullopt;
    }
    
    /**
     * @brief Find matching policy for a dataset by pattern
     */
    std::optional<LifecyclePolicy> findMatchingPolicy(const std::string& dataset_name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        // Check explicit assignment first
        auto assign_it = dataset_policies_.find(dataset_name);
        if (assign_it != dataset_policies_.end()) {
            auto policy_it = policies_.find(assign_it->second);
            if (policy_it != policies_.end()) {
                return policy_it->second;
            }
        }
        
        // Check pattern matching
        for (const auto& [name, policy] : policies_) {
            if (!policy.enabled) continue;
            
            for (const auto& pattern : policy.apply_to_patterns) {
                try {
                    std::regex re(pattern);
                    if (std::regex_match(dataset_name, re)) {
                        return policy;
                    }
                } catch (...) {
                    // Invalid regex, try simple wildcard
                    if (matchWildcard(dataset_name, pattern)) {
                        return policy;
                    }
                }
            }
        }
        
        return std::nullopt;
    }
    
    /**
     * @brief Evaluate all policies and return candidates for action
     * @return Vector of lifecycle candidates
     */
    std::vector<LifecycleCandidate> evaluatePolicies() {
        std::vector<LifecycleCandidate> candidates;
        
        // If no engine, return empty
        if (!engine_) {
            return candidates;
        }
        
        // This would typically iterate through datasets from the engine
        // For now, return empty as the engine integration is optional
        return candidates;
    }
    
    /**
     * @brief Evaluate lifecycle actions for a dataset
     * @return List of recommended actions
     */
    std::vector<std::pair<OperationType, std::string>> evaluateDataset(
            const DatasetInfo& dataset) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        std::vector<std::pair<OperationType, std::string>> actions;
        
        auto policy_opt = findMatchingPolicyUnlocked(dataset.name);
        if (!policy_opt || !policy_opt->enabled) {
            return actions;
        }
        
        const auto& policy = *policy_opt;
        auto now = std::chrono::system_clock::now();
        auto age_days = std::chrono::duration_cast<std::chrono::hours>(
            now - dataset.last_accessed).count() / 24;
        
        // Check for tier transition
        if (dataset.tier == StorageTier::PRIMARY && 
            age_days >= policy.tier_down_days) {
            actions.emplace_back(OperationType::OP_MIGRATE, 
                "Age " + std::to_string(age_days) + " days exceeds threshold");
        }
        
        // Check for archival
        if (dataset.tier != StorageTier::ARCHIVE && 
            age_days >= policy.archive_days) {
            actions.emplace_back(OperationType::OP_MIGRATE,
                "Age " + std::to_string(age_days) + " days exceeds archive threshold");
        }
        
        // Check for expiration
        if (policy.expiration_days > 0 && age_days >= policy.expiration_days) {
            if (policy.delete_on_expiration) {
                actions.emplace_back(OperationType::OP_EXPIRE,
                    "Dataset expired after " + std::to_string(age_days) + " days");
            }
        }
        
        // Check explicit expiration time
        if (dataset.expiration_time != std::chrono::system_clock::time_point{} &&
            now >= dataset.expiration_time) {
            actions.emplace_back(OperationType::OP_EXPIRE, "Explicit expiration reached");
        }
        
        return actions;
    }
    
    /**
     * @brief Run lifecycle evaluation on all datasets
     * @param datasets List of datasets to evaluate
     * @return Map of dataset name to recommended actions
     */
    std::map<std::string, std::vector<std::pair<OperationType, std::string>>> 
    evaluateAll(const std::vector<DatasetInfo>& datasets) {
        std::map<std::string, std::vector<std::pair<OperationType, std::string>>> results;
        
        for (const auto& ds : datasets) {
            auto actions = evaluateDataset(ds);
            if (!actions.empty()) {
                results[ds.name] = actions;
                ++stats_.datasets_processed;
            }
        }
        
        return results;
    }
    
    /**
     * @brief Record a lifecycle action
     */
    void recordAction(OperationType action, uint64_t bytes) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        switch (action) {
            case OperationType::OP_MIGRATE:
                ++stats_.tier_transitions;
                stats_.bytes_transitioned += bytes;
                break;
            case OperationType::OP_EXPIRE:
                ++stats_.expiration_deletions;
                stats_.bytes_expired += bytes;
                break;
            default:
                break;
        }
    }
    
    /**
     * @brief Get lifecycle statistics
     */
    LifecycleStatistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return stats_;
    }
    
    /**
     * @brief Generate lifecycle report
     */
    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        
        oss << "=== Lifecycle Statistics ===\n"
            << "Datasets Processed: " << stats_.datasets_processed << "\n"
            << "Tier Transitions: " << stats_.tier_transitions << "\n"
            << "Archival Operations: " << stats_.archival_operations << "\n"
            << "Expiration Deletions: " << stats_.expiration_deletions << "\n"
            << "Bytes Transitioned: " << formatBytes(stats_.bytes_transitioned) << "\n"
            << "Bytes Archived: " << formatBytes(stats_.bytes_archived) << "\n"
            << "Bytes Expired: " << formatBytes(stats_.bytes_expired) << "\n\n"
            << "Policies (" << policies_.size() << "):\n";
        
        for (const auto& [name, policy] : policies_) {
            oss << "  " << name << ": "
                << (policy.enabled ? "[ENABLED]" : "[DISABLED]")
                << " tier_down=" << policy.tier_down_days << "d"
                << " archive=" << policy.archive_days << "d"
                << " expire=" << policy.expiration_days << "d\n";
        }
        
        oss << "\nAssignments (" << dataset_policies_.size() << "):\n";
        for (const auto& [ds, policy] : dataset_policies_) {
            oss << "  " << ds << " -> " << policy << "\n";
        }
        
        return oss.str();
    }

private:
    bool matchWildcard(const std::string& str, const std::string& pattern) const {
        // Simple wildcard matching (* and ?)
        size_t si = 0, pi = 0;
        size_t star_p = std::string::npos, star_s = 0;
        
        while (si < str.size()) {
            if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == str[si])) {
                ++si; ++pi;
            } else if (pi < pattern.size() && pattern[pi] == '*') {
                star_p = pi++; star_s = si;
            } else if (star_p != std::string::npos) {
                pi = star_p + 1; si = ++star_s;
            } else {
                return false;
            }
        }
        
        while (pi < pattern.size() && pattern[pi] == '*') ++pi;
        return pi == pattern.size();
    }
    
    std::optional<LifecyclePolicy> findMatchingPolicyUnlocked(
            const std::string& dataset_name) const {
        // Check explicit assignment first
        auto assign_it = dataset_policies_.find(dataset_name);
        if (assign_it != dataset_policies_.end()) {
            auto policy_it = policies_.find(assign_it->second);
            if (policy_it != policies_.end()) {
                return policy_it->second;
            }
        }
        
        // Check pattern matching
        for (const auto& [name, policy] : policies_) {
            if (!policy.enabled) continue;
            
            for (const auto& pattern : policy.apply_to_patterns) {
                if (matchWildcard(dataset_name, pattern)) {
                    return policy;
                }
            }
        }
        
        return std::nullopt;
    }
    
    std::atomic<bool> running_;
    HSMEngine* engine_;
    mutable std::mutex mtx_;
    std::map<std::string, LifecyclePolicy> policies_;
    std::map<std::string, std::string> dataset_policies_;  // dataset -> policy_name
    LifecycleStatistics stats_;
};

} // namespace dfsmshsm
#endif // DFSMSHSM_LIFECYCLE_MANAGER_HPP
