/**
 * @file config_validator.hpp
 * @brief Configuration validation with comprehensive checks
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_CONFIG_VALIDATOR_HPP
#define DFSMSHSM_CONFIG_VALIDATOR_HPP

#include "hsm_types.hpp"
#include "result.hpp"
#include "hsm_config.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <regex>
#include <set>
#include <algorithm>

namespace dfsmshsm {

struct ValidationError {
    std::string key;
    std::string value;
    std::string message;
    std::string suggestion;
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Config error [" << key << "]: " << message;
        if (!value.empty()) oss << " (value: '" << value << "')";
        if (!suggestion.empty()) oss << " Suggestion: " << suggestion;
        return oss.str();
    }
};

struct ValidationResult {
    bool valid = true;
    std::vector<ValidationError> errors;
    std::vector<std::string> warnings;
    
    void addError(const std::string& key, const std::string& value,
                  const std::string& message, const std::string& suggestion = "") {
        valid = false;
        errors.push_back({key, value, message, suggestion});
    }
    
    void addWarning(const std::string& msg) { warnings.push_back(msg); }
    
    std::string summary() const {
        std::ostringstream oss;
        oss << "Validation " << (valid ? "PASSED" : "FAILED");
        if (!errors.empty()) oss << " (" << errors.size() << " errors)";
        if (!warnings.empty()) oss << " (" << warnings.size() << " warnings)";
        return oss.str();
    }
    
    std::string fullReport() const {
        std::ostringstream oss;
        oss << "=== Configuration Validation Report ===\n" << summary() << "\n\n";
        if (!errors.empty()) {
            oss << "ERRORS:\n";
            for (const auto& e : errors) oss << "  - " << e.toString() << "\n";
            oss << "\n";
        }
        if (!warnings.empty()) {
            oss << "WARNINGS:\n";
            for (const auto& w : warnings) oss << "  - " << w << "\n";
        }
        return oss.str();
    }
};

class ConfigValidator {
public:
    static ValidationResult validate(const HSMConfig& config) {
        ValidationResult result;
        validateMigrationConfig(config, result);
        validateBackupConfig(config, result);
        validateLoggingConfig(config, result);
        validateNetworkConfig(config, result);
        validateSecurityConfig(config, result);
        return result;
    }
    
    static Result<void> validateKey(const std::string& key, const std::string& value) {
        if (key == "migration.age_days") return validateIntRange(key, value, 1, 3650);
        if (key == "migration.min_size") return validateUInt64Range(key, value, 0, 1099511627776ULL);
        if (key == "migration.space_threshold") return validateIntRange(key, value, 1, 100);
        if (key == "migration.enabled" || key == "migration.compress") return validateBool(key, value);
        if (key == "backup.retention_days") return validateIntRange(key, value, 1, 36500);
        if (key == "backup.frequency_hours") return validateIntRange(key, value, 1, 8760);
        if (key == "backup.max_versions") return validateIntRange(key, value, 1, 1000);
        if (key == "backup.incremental" || key == "backup.compress") return validateBool(key, value);
        if (key == "log.level") return validateLogLevel(key, value);
        if (key == "log.console") return validateBool(key, value);
        if (key == "rest.port") return validateIntRange(key, value, 1, 65535);
        if (key == "rest.enabled") return validateBool(key, value);
        if (key == "encryption.algorithm") return validateEncryptionAlgorithm(key, value);
        if (key == "encryption.key_size") return validateKeySize(key, value);
        return Ok();
    }
    
    static ValidationResult checkRequired(const HSMConfig& config,
                                          const std::vector<std::string>& requiredKeys) {
        ValidationResult result;
        for (const auto& key : requiredKeys) {
            if (!config.exists(key)) {
                result.addError(key, "", "Required configuration key missing",
                               "Add '" + key + "' to configuration");
            }
        }
        return result;
    }

private:
    static void validateMigrationConfig(const HSMConfig& config, ValidationResult& result) {
        if (config.exists("migration.age_days")) {
            int val = config.getInt("migration.age_days");
            if (val < 1 || val > 3650) {
                result.addError("migration.age_days", std::to_string(val),
                    "Value out of range (1-3650)", "Use 1-3650 days");
            }
        }
        if (config.exists("migration.space_threshold")) {
            int val = config.getInt("migration.space_threshold");
            if (val < 1 || val > 100) {
                result.addError("migration.space_threshold", std::to_string(val),
                    "Percentage out of range (1-100)", "Use 1-100%");
            }
            if (val > 95) {
                result.addWarning("migration.space_threshold is very high (" + 
                    std::to_string(val) + "%)");
            }
        }
    }
    
    static void validateBackupConfig(const HSMConfig& config, ValidationResult& result) {
        if (config.exists("backup.retention_days")) {
            int val = config.getInt("backup.retention_days");
            if (val < 1) result.addError("backup.retention_days", std::to_string(val),
                "Must be at least 1 day", "Set to 1 or higher");
        }
        if (config.exists("backup.frequency_hours")) {
            int val = config.getInt("backup.frequency_hours");
            if (val < 1 || val > 8760) result.addError("backup.frequency_hours", 
                std::to_string(val), "Value out of range (1-8760)", "Use 1-8760 hours");
        }
        if (config.exists("backup.max_versions")) {
            int val = config.getInt("backup.max_versions");
            if (val < 1) result.addError("backup.max_versions", std::to_string(val),
                "Must keep at least 1 version", "Set to 1 or higher");
        }
    }
    
    static void validateLoggingConfig(const HSMConfig& config, ValidationResult& result) {
        if (config.exists("log.level")) {
            std::string level = config.get("log.level");
            std::set<std::string> validLevels = {"DEBUG", "INFO", "WARN", "WARNING", "ERROR", "FATAL"};
            std::string upperLevel = level;
            std::transform(upperLevel.begin(), upperLevel.end(), upperLevel.begin(), ::toupper);
            if (validLevels.find(upperLevel) == validLevels.end()) {
                result.addError("log.level", level, "Invalid log level", 
                    "Use DEBUG, INFO, WARN, ERROR, or FATAL");
            }
        }
    }
    
    static void validateNetworkConfig(const HSMConfig& config, ValidationResult& result) {
        if (config.exists("rest.port")) {
            int port = config.getInt("rest.port");
            if (port < 1 || port > 65535) result.addError("rest.port", std::to_string(port),
                "Invalid port number (1-65535)", "Use a valid port");
            if (port < 1024) result.addWarning("rest.port " + std::to_string(port) + 
                " requires root/admin privileges");
        }
    }
    
    static void validateSecurityConfig(const HSMConfig& config, ValidationResult& result) {
        if (config.exists("encryption.algorithm")) {
            std::string algo = config.get("encryption.algorithm");
            std::set<std::string> valid = {"AES-128", "AES-256", "XOR"};
            if (valid.find(algo) == valid.end()) {
                result.addError("encryption.algorithm", algo,
                    "Unknown encryption algorithm", "Use AES-128, AES-256, or XOR");
            }
            if (algo == "XOR") result.addWarning("XOR encryption is for testing only");
        }
        if (config.exists("encryption.key_size")) {
            int size = config.getInt("encryption.key_size");
            std::set<int> valid = {128, 192, 256};
            if (valid.find(size) == valid.end()) {
                result.addError("encryption.key_size", std::to_string(size),
                    "Invalid key size", "Use 128, 192, or 256 bits");
            }
        }
    }
    
    static Result<void> validateIntRange(const std::string& key, const std::string& value,
                                         int min, int max) {
        try {
            int val = std::stoi(value);
            if (val < min || val > max) return Err<void>(ErrorCode::CONFIG_INVALID,
                key + " must be between " + std::to_string(min) + "-" + std::to_string(max));
            return Ok();
        } catch (...) { return Err<void>(ErrorCode::CONFIG_INVALID, key + " must be an integer"); }
    }
    
    static Result<void> validateUInt64Range(const std::string& key, const std::string& value,
                                            uint64_t min, uint64_t max) {
        try {
            uint64_t val = std::stoull(value);
            if (val < min || val > max) return Err<void>(ErrorCode::CONFIG_INVALID,
                key + " out of range");
            return Ok();
        } catch (...) { return Err<void>(ErrorCode::CONFIG_INVALID, key + " must be a positive integer"); }
    }
    
    static Result<void> validateBool(const std::string& key, const std::string& value) {
        std::set<std::string> valid = {"true", "false", "1", "0", "yes", "no"};
        std::string lower = value;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (valid.find(lower) == valid.end()) return Err<void>(ErrorCode::CONFIG_INVALID,
            key + " must be true/false, yes/no, or 1/0");
        return Ok();
    }
    
    static Result<void> validateLogLevel(const std::string& key, const std::string& value) {
        std::set<std::string> valid = {"DEBUG", "INFO", "WARN", "WARNING", "ERROR", "FATAL"};
        std::string upper = value;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        if (valid.find(upper) == valid.end()) return Err<void>(ErrorCode::CONFIG_INVALID,
            key + " must be DEBUG, INFO, WARN, ERROR, or FATAL");
        return Ok();
    }
    
    static Result<void> validateEncryptionAlgorithm(const std::string& key, const std::string& value) {
        std::set<std::string> valid = {"AES-128", "AES-256", "XOR", "NONE"};
        if (valid.find(value) == valid.end()) return Err<void>(ErrorCode::CONFIG_INVALID,
            key + " must be AES-128, AES-256, XOR, or NONE");
        return Ok();
    }
    
    static Result<void> validateKeySize(const std::string& key, const std::string& value) {
        try {
            int size = std::stoi(value);
            std::set<int> valid = {128, 192, 256};
            if (valid.find(size) == valid.end()) return Err<void>(ErrorCode::CONFIG_INVALID,
                key + " must be 128, 192, or 256");
            return Ok();
        } catch (...) { return Err<void>(ErrorCode::CONFIG_INVALID, key + " must be an integer"); }
    }
};

} // namespace dfsmshsm
#endif
