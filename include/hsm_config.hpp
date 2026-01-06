/**
 * @file hsm_config.hpp
 * @brief Configuration management for HSM
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 *
 * Enhanced configuration with:
 * - Structured configuration object
 * - Configuration validation
 * - Type-safe accessors
 * - Default value management
 */
#ifndef DFSMSHSM_HSM_CONFIG_HPP
#define DFSMSHSM_HSM_CONFIG_HPP

#include "hsm_types.hpp"
#include "result.hpp"
#include <map>
#include <mutex>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <algorithm>

namespace dfsmshsm {

//=============================================================================
// Structured Configuration
//=============================================================================

/**
 * @struct HSMConfig
 * @brief Structured HSM configuration with typed fields
 */
struct HSMConfig {
    // Singleton instance for backward compatibility
    static HSMConfig& instance() {
        static HSMConfig instance_;
        return instance_;
    }
    
    // Map-based interface for backward compatibility
    std::map<std::string, std::string> config_map_;
    
    void clear() {
        config_map_.clear();
    }
    
    void set(const std::string& key, const std::string& value) {
        config_map_[key] = value;
        setFromString(key, value);  // Also update struct fields
    }
    
    std::string get(const std::string& key, const std::string& defaultVal = "") const {
        auto it = config_map_.find(key);
        return it != config_map_.end() ? it->second : defaultVal;
    }
    
    int getInt(const std::string& key, int defaultVal = 0) const {
        auto it = config_map_.find(key);
        if (it != config_map_.end()) {
            try { return std::stoi(it->second); } catch (...) {}
        }
        return defaultVal;
    }
    
    bool getBool(const std::string& key, bool defaultVal = false) const {
        auto it = config_map_.find(key);
        if (it != config_map_.end()) {
            std::string val = it->second;
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            return val == "true" || val == "1" || val == "yes" || val == "on";
        }
        return defaultVal;
    }
    
    bool exists(const std::string& key) const {
        return config_map_.find(key) != config_map_.end();
    }
    
    // System limits
    size_t max_datasets = 1000000;
    size_t max_volumes = 10000;
    size_t max_concurrent_operations = 100;
    
    // Storage thresholds (percentages)
    int primary_high_threshold = 85;
    int primary_low_threshold = 75;
    int ml1_high_threshold = 90;
    int ml1_low_threshold = 80;
    int ml2_high_threshold = 95;
    int ml2_low_threshold = 85;
    
    // Migration settings
    int migration_age_days = 30;
    size_t migration_min_size = 104857600;  // 100 MB
    bool migration_enabled = true;
    bool compress_on_migrate = true;
    
    // Backup settings
    int backup_retention_days = 30;
    int backup_frequency_hours = 24;
    bool backup_incremental = true;
    bool backup_compress = true;
    int backup_max_versions = 5;
    
    // Feature flags
    bool enable_compression = true;
    bool enable_encryption = false;
    bool enable_deduplication = true;
    bool enable_snapshots = true;
    bool enable_replication = false;
    bool enable_audit_logging = true;
    
    // Performance tuning
    size_t cache_size_mb = 512;
    size_t buffer_size_kb = 64;
    int io_threads = 4;
    int worker_threads = 8;
    
    // Logging
    std::string log_level = "INFO";
    bool log_to_console = false;
    std::string log_file_path = "/var/log/dfsmshsm/hsm.log";
    
    // Volume configuration
    std::vector<std::string> primary_volumes;
    std::vector<std::string> ml1_volumes;
    std::vector<std::string> ml2_volumes;
    
    // Migration threshold accessor (percentage)
    [[nodiscard]] int getMigrationThreshold() const {
        return primary_high_threshold;
    }
    
    void setMigrationThreshold(int threshold) {
        primary_high_threshold = threshold;
    }
    
    // Primary volumes accessor
    [[nodiscard]] const std::vector<std::string>& getPrimaryVolumes() const {
        return primary_volumes;
    }
    
    void setPrimaryVolumes(const std::vector<std::string>& volumes) {
        primary_volumes = volumes;
    }
    
    // ML1 volumes accessor
    [[nodiscard]] const std::vector<std::string>& getML1Volumes() const {
        return ml1_volumes;
    }
    
    void setML1Volumes(const std::vector<std::string>& volumes) {
        ml1_volumes = volumes;
    }
    
    // ML2 volumes accessor
    [[nodiscard]] const std::vector<std::string>& getML2Volumes() const {
        return ml2_volumes;
    }
    
    void setML2Volumes(const std::vector<std::string>& volumes) {
        ml2_volumes = volumes;
    }
    
    // Convert to string representation
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << "=== HSM Configuration ===\n"
            << "Max Datasets: " << max_datasets << "\n"
            << "Max Volumes: " << max_volumes << "\n"
            << "Migration Threshold: " << primary_high_threshold << "%\n"
            << "Migration Age Days: " << migration_age_days << "\n"
            << "Migration Enabled: " << (migration_enabled ? "Yes" : "No") << "\n"
            << "Compression: " << (enable_compression ? "Enabled" : "Disabled") << "\n"
            << "Encryption: " << (enable_encryption ? "Enabled" : "Disabled") << "\n"
            << "Deduplication: " << (enable_deduplication ? "Enabled" : "Disabled") << "\n"
            << "Cache Size: " << cache_size_mb << " MB\n"
            << "IO Threads: " << io_threads << "\n"
            << "Worker Threads: " << worker_threads << "\n"
            << "Primary Volumes: " << primary_volumes.size() << "\n"
            << "ML1 Volumes: " << ml1_volumes.size() << "\n"
            << "ML2 Volumes: " << ml2_volumes.size() << "\n";
        return oss.str();
    }
    
    /**
     * @brief Set configuration value from string
     */
    void setFromString(const std::string& key, const std::string& value) {
        if (key == "max_datasets") max_datasets = std::stoull(value);
        else if (key == "max_volumes") max_volumes = std::stoull(value);
        else if (key == "max_concurrent_operations") max_concurrent_operations = std::stoull(value);
        else if (key == "primary_high_threshold") primary_high_threshold = std::stoi(value);
        else if (key == "primary_low_threshold") primary_low_threshold = std::stoi(value);
        else if (key == "ml1_high_threshold") ml1_high_threshold = std::stoi(value);
        else if (key == "ml1_low_threshold") ml1_low_threshold = std::stoi(value);
        else if (key == "ml2_high_threshold") ml2_high_threshold = std::stoi(value);
        else if (key == "ml2_low_threshold") ml2_low_threshold = std::stoi(value);
        else if (key == "migration_age_days") migration_age_days = std::stoi(value);
        else if (key == "migration_min_size") migration_min_size = std::stoull(value);
        else if (key == "migration_enabled") migration_enabled = parseBool(value);
        else if (key == "compress_on_migrate") compress_on_migrate = parseBool(value);
        else if (key == "backup_retention_days") backup_retention_days = std::stoi(value);
        else if (key == "backup_frequency_hours") backup_frequency_hours = std::stoi(value);
        else if (key == "backup_incremental") backup_incremental = parseBool(value);
        else if (key == "backup_compress") backup_compress = parseBool(value);
        else if (key == "backup_max_versions") backup_max_versions = std::stoi(value);
        else if (key == "enable_compression") enable_compression = parseBool(value);
        else if (key == "enable_encryption") enable_encryption = parseBool(value);
        else if (key == "enable_deduplication") enable_deduplication = parseBool(value);
        else if (key == "enable_snapshots") enable_snapshots = parseBool(value);
        else if (key == "enable_replication") enable_replication = parseBool(value);
        else if (key == "enable_audit_logging") enable_audit_logging = parseBool(value);
        else if (key == "cache_size_mb") cache_size_mb = std::stoull(value);
        else if (key == "buffer_size_kb") buffer_size_kb = std::stoull(value);
        else if (key == "io_threads") io_threads = std::stoi(value);
        else if (key == "worker_threads") worker_threads = std::stoi(value);
        else if (key == "log_level") log_level = value;
        else if (key == "log_to_console") log_to_console = parseBool(value);
        else if (key == "log_file_path") log_file_path = value;
    }
    
    /**
     * @brief Get default configuration
     */
    [[nodiscard]] static HSMConfig getDefaults() {
        return HSMConfig{};
    }
    
    /**
     * @brief Convert to string map
     */
    [[nodiscard]] std::map<std::string, std::string> toMap() const {
        std::map<std::string, std::string> m;
        m["max_datasets"] = std::to_string(max_datasets);
        m["max_volumes"] = std::to_string(max_volumes);
        m["max_concurrent_operations"] = std::to_string(max_concurrent_operations);
        m["primary_high_threshold"] = std::to_string(primary_high_threshold);
        m["primary_low_threshold"] = std::to_string(primary_low_threshold);
        m["ml1_high_threshold"] = std::to_string(ml1_high_threshold);
        m["ml1_low_threshold"] = std::to_string(ml1_low_threshold);
        m["ml2_high_threshold"] = std::to_string(ml2_high_threshold);
        m["ml2_low_threshold"] = std::to_string(ml2_low_threshold);
        m["migration_age_days"] = std::to_string(migration_age_days);
        m["migration_min_size"] = std::to_string(migration_min_size);
        m["migration_enabled"] = migration_enabled ? "true" : "false";
        m["compress_on_migrate"] = compress_on_migrate ? "true" : "false";
        m["backup_retention_days"] = std::to_string(backup_retention_days);
        m["backup_frequency_hours"] = std::to_string(backup_frequency_hours);
        m["backup_incremental"] = backup_incremental ? "true" : "false";
        m["backup_compress"] = backup_compress ? "true" : "false";
        m["backup_max_versions"] = std::to_string(backup_max_versions);
        m["enable_compression"] = enable_compression ? "true" : "false";
        m["enable_encryption"] = enable_encryption ? "true" : "false";
        m["enable_deduplication"] = enable_deduplication ? "true" : "false";
        m["enable_snapshots"] = enable_snapshots ? "true" : "false";
        m["enable_replication"] = enable_replication ? "true" : "false";
        m["enable_audit_logging"] = enable_audit_logging ? "true" : "false";
        m["cache_size_mb"] = std::to_string(cache_size_mb);
        m["buffer_size_kb"] = std::to_string(buffer_size_kb);
        m["io_threads"] = std::to_string(io_threads);
        m["worker_threads"] = std::to_string(worker_threads);
        m["log_level"] = log_level;
        m["log_to_console"] = log_to_console ? "true" : "false";
        m["log_file_path"] = log_file_path;
        return m;
    }

private:
    static bool parseBool(const std::string& s) {
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower == "true" || lower == "1" || lower == "yes" || lower == "on";
    }
};

//=============================================================================
// Configuration Validator
//=============================================================================

/**
 * @class BasicConfigValidator
 * @brief Validates HSM configuration
 */
class BasicConfigValidator {
public:
    struct ValidationError {
        std::string field;
        std::string message;
    };
    
    /**
     * @brief Validate configuration
     */
    [[nodiscard]] static Result<void> validate(const HSMConfig& config) {
        std::vector<ValidationError> errors;
        
        // Validate thresholds
        if (config.primary_high_threshold <= config.primary_low_threshold) {
            errors.push_back({"primary_thresholds", 
                "High threshold must be greater than low threshold"});
        }
        if (config.ml1_high_threshold <= config.ml1_low_threshold) {
            errors.push_back({"ml1_thresholds",
                "High threshold must be greater than low threshold"});
        }
        if (config.ml2_high_threshold <= config.ml2_low_threshold) {
            errors.push_back({"ml2_thresholds",
                "High threshold must be greater than low threshold"});
        }
        
        // Validate threshold ranges
        if (config.primary_high_threshold > 100 || config.primary_high_threshold < 0) {
            errors.push_back({"primary_high_threshold", "Must be between 0 and 100"});
        }
        if (config.primary_low_threshold > 100 || config.primary_low_threshold < 0) {
            errors.push_back({"primary_low_threshold", "Must be between 0 and 100"});
        }
        
        // Validate limits
        if (config.max_datasets == 0) {
            errors.push_back({"max_datasets", "Must be greater than 0"});
        }
        if (config.max_volumes == 0) {
            errors.push_back({"max_volumes", "Must be greater than 0"});
        }
        if (config.max_concurrent_operations == 0) {
            errors.push_back({"max_concurrent_operations", "Must be greater than 0"});
        }
        
        // Validate positive values
        if (config.migration_age_days <= 0) {
            errors.push_back({"migration_age_days", "Must be positive"});
        }
        if (config.backup_retention_days <= 0) {
            errors.push_back({"backup_retention_days", "Must be positive"});
        }
        if (config.backup_frequency_hours <= 0) {
            errors.push_back({"backup_frequency_hours", "Must be positive"});
        }
        if (config.backup_max_versions <= 0) {
            errors.push_back({"backup_max_versions", "Must be positive"});
        }
        
        // Validate thread counts
        if (config.io_threads <= 0) {
            errors.push_back({"io_threads", "Must be positive"});
        }
        if (config.worker_threads <= 0) {
            errors.push_back({"worker_threads", "Must be positive"});
        }
        
        // Validate log level
        std::vector<std::string> validLevels = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
        std::string upperLevel = config.log_level;
        std::transform(upperLevel.begin(), upperLevel.end(), upperLevel.begin(), ::toupper);
        if (std::find(validLevels.begin(), validLevels.end(), upperLevel) == validLevels.end()) {
            errors.push_back({"log_level", "Invalid log level: " + config.log_level});
        }
        
        if (!errors.empty()) {
            std::ostringstream oss;
            oss << "Configuration validation failed:\n";
            for (const auto& err : errors) {
                oss << "  - " << err.field << ": " << err.message << "\n";
            }
            return Err<void>(ErrorCode::INVALID_ARGUMENT, oss.str());
        }
        
        return Ok();
    }
    
    /**
     * @brief Validate a single field
     */
    [[nodiscard]] static Result<void> validateField(const std::string& field, 
                                                     const std::string& value) {
        HSMConfig config;
        try {
            config.setFromString(field, value);
        } catch (const std::exception& e) {
            return Err<void>(ErrorCode::INVALID_ARGUMENT, 
                "Invalid value for " + field + ": " + e.what());
        }
        return Ok();
    }
};

//=============================================================================
// Legacy Configuration Manager (for backward compatibility)
//=============================================================================

/**
 * @class HSMConfigManager
 * @brief Singleton configuration manager (legacy interface)
 */
class HSMConfigManager {
public:
    static HSMConfigManager& instance() { 
        static HSMConfigManager c; 
        return c; 
    }

    bool loadFromFile(const std::filesystem::path& filepath) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ifstream file(filepath);
        if (!file) return false;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = trim(line.substr(0, pos));
                std::string val = trim(line.substr(pos + 1));
                config_map_[key] = val;
                config_.setFromString(key, val);
            }
        }
        return true;
    }

    bool saveToFile(const std::filesystem::path& filepath) const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ofstream file(filepath);
        if (!file) return false;
        file << "# DFSMShsm Configuration v4.1.0\n\n";
        auto configMap = config_.toMap();
        for (const auto& [k, v] : configMap) {
            file << k << "=" << v << "\n";
        }
        return true;
    }

    void set(const std::string& key, const std::string& value) { 
        std::lock_guard<std::mutex> lock(mtx_); 
        config_map_[key] = value;
        config_.setFromString(key, value);
    }
    
    std::string get(const std::string& key, const std::string& def = "") const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = config_map_.find(key);
        return (it != config_map_.end()) ? it->second : def;
    }

    int getInt(const std::string& key, int def = 0) const { 
        try { return std::stoi(get(key, std::to_string(def))); } 
        catch (...) { return def; } 
    }
    
    uint64_t getUInt64(const std::string& key, uint64_t def = 0) const { 
        try { return std::stoull(get(key, std::to_string(def))); } 
        catch (...) { return def; } 
    }
    
    double getDouble(const std::string& key, double def = 0.0) const { 
        try { return std::stod(get(key, std::to_string(def))); } 
        catch (...) { return def; } 
    }
    
    bool getBool(const std::string& key, bool def = false) const { 
        std::string v = get(key, def ? "true" : "false"); 
        return v == "true" || v == "1" || v == "yes"; 
    }

    bool exists(const std::string& key) const { 
        std::lock_guard<std::mutex> lock(mtx_); 
        return config_map_.find(key) != config_map_.end(); 
    }
    
    void remove(const std::string& key) { 
        std::lock_guard<std::mutex> lock(mtx_); 
        config_map_.erase(key); 
    }
    
    void clear() { 
        std::lock_guard<std::mutex> lock(mtx_); 
        config_map_.clear();
        config_ = HSMConfig::getDefaults();
    }

    [[nodiscard]] const HSMConfig& getConfig() const { return config_; }
    [[nodiscard]] HSMConfig& getConfig() { return config_; }

    MigrationPolicy getMigrationPolicy() const {
        MigrationPolicy p;
        p.age_threshold_days = static_cast<uint32_t>(config_.migration_age_days);
        p.size_threshold_bytes = config_.migration_min_size;
        p.space_pressure_percent = static_cast<uint32_t>(config_.primary_high_threshold);
        p.enabled = config_.migration_enabled;
        p.compress_on_migrate = config_.compress_on_migrate;
        return p;
    }

    BackupPolicy getBackupPolicy() const {
        BackupPolicy p;
        p.retention_days = static_cast<uint32_t>(config_.backup_retention_days);
        p.frequency_hours = static_cast<uint32_t>(config_.backup_frequency_hours);
        p.incremental = config_.backup_incremental;
        p.compress = config_.backup_compress;
        p.backup_volume = "";
        p.max_versions = static_cast<uint32_t>(config_.backup_max_versions);
        return p;
    }

    void setDefaults() {
        std::lock_guard<std::mutex> lock(mtx_);
        config_ = HSMConfig::getDefaults();
        auto m = config_.toMap();
        for (const auto& [k, v] : m) {
            config_map_[k] = v;
        }
    }

private:
    HSMConfigManager() { setDefaults(); }
    
    static std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\r\n");
        auto end = s.find_last_not_of(" \t\r\n");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    }
    
    mutable std::mutex mtx_;
    std::map<std::string, std::string> config_map_;
    HSMConfig config_;
};

} // namespace dfsmshsm
#endif
