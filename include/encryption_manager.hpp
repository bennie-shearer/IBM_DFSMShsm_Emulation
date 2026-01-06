/**
 * @file encryption_manager.hpp
 * @brief Data-at-rest encryption for HSM datasets
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 * 
 * Implements data encryption with:
 * - AES-256 simulation (XOR for emulation)
 * - Key management
 * - Transparent encryption/decryption
 */
#ifndef DFSMSHSM_ENCRYPTION_MANAGER_HPP
#define DFSMSHSM_ENCRYPTION_MANAGER_HPP

#include "hsm_types.hpp"
#include <map>
#include <mutex>
#include <atomic>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace dfsmshsm {

/**
 * @struct EncryptionKey
 * @brief Encryption key information
 */
struct EncryptionKey {
    std::string id;
    std::string name;
    EncryptionType type = EncryptionType::AES_256;
    std::vector<uint8_t> key_data;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point expires;
    uint64_t usage_count = 0;
    bool active = true;
    bool exportable = false;
};

/**
 * @struct EncryptionStatistics
 * @brief Encryption operation statistics
 */
struct EncryptionStatistics {
    uint64_t total_encryptions = 0;
    uint64_t total_decryptions = 0;
    uint64_t bytes_encrypted = 0;
    uint64_t bytes_decrypted = 0;
    uint64_t key_count = 0;
    uint64_t key_rotations = 0;
    uint64_t failures = 0;
};

/**
 * @class EncryptionManager
 * @brief Manages data encryption for HSM datasets
 */
class EncryptionManager {
public:
    EncryptionManager() : running_(false), next_key_id_(1) {}
    
    /**
     * @brief Start the encryption manager
     */
    bool start() {
        if (running_) return false;
        running_ = true;
        
        // Generate default master key if none exists
        if (keys_.empty()) {
            generateKey("master", EncryptionType::AES_256, 0);
        }
        
        return true;
    }
    
    /**
     * @brief Stop the encryption manager
     */
    void stop() { running_ = false; }
    
    /**
     * @brief Check if manager is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Generate a new encryption key
     * @param name Key name
     * @param type Encryption type
     * @param expires_days Days until expiration (0 = never)
     * @return Key ID
     */
    std::string generateKey(const std::string& name, 
                            EncryptionType type = EncryptionType::AES_256,
                            uint32_t expires_days = 365) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        EncryptionKey key;
        key.id = "KEY-" + std::to_string(next_key_id_++);
        key.name = name;
        key.type = type;
        key.created = std::chrono::system_clock::now();
        
        if (expires_days > 0) {
            key.expires = key.created + std::chrono::hours(24 * expires_days);
        } else {
            key.expires = std::chrono::system_clock::time_point::max();
        }
        
        // Generate random key data
        size_t key_size = getKeySize(type);
        key.key_data.resize(key_size);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 255);
        
        for (auto& byte : key.key_data) {
            byte = static_cast<uint8_t>(dist(gen));
        }
        
        keys_[key.id] = key;
        ++stats_.key_count;
        
        return key.id;
    }
    
    /**
     * @brief Import an existing key
     */
    bool importKey(const std::string& id, const std::string& name,
                   EncryptionType type, const std::vector<uint8_t>& key_data) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        if (key_data.size() != getKeySize(type)) {
            return false;
        }
        
        EncryptionKey key;
        key.id = id;
        key.name = name;
        key.type = type;
        key.key_data = key_data;
        key.created = std::chrono::system_clock::now();
        key.expires = std::chrono::system_clock::time_point::max();
        
        keys_[id] = key;
        ++stats_.key_count;
        
        return true;
    }
    
    /**
     * @brief Get key information (without key data)
     */
    std::optional<EncryptionKey> getKeyInfo(const std::string& key_id) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = keys_.find(key_id);
        if (it == keys_.end()) return std::nullopt;
        
        EncryptionKey info = it->second;
        info.key_data.clear();  // Don't expose key data
        return info;
    }
    
    /**
     * @brief Encrypt data
     * @param data Data to encrypt
     * @param key_id Key ID to use
     * @return Encrypted data
     */
    std::optional<std::vector<uint8_t>> encrypt(const std::vector<uint8_t>& data,
                                                 const std::string& key_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = keys_.find(key_id);
        if (it == keys_.end() || !it->second.active) {
            ++stats_.failures;
            return std::nullopt;
        }
        
        auto& key = it->second;
        
        // Check if key has expired
        auto now = std::chrono::system_clock::now();
        if (now >= key.expires) {
            ++stats_.failures;
            return std::nullopt;
        }
        
        std::vector<uint8_t> result;
        
        switch (key.type) {
            case EncryptionType::XOR_SIMPLE:
            case EncryptionType::AES_128:
            case EncryptionType::AES_256:
                result = xorEncrypt(data, key.key_data);
                break;
            case EncryptionType::NONE:
                result = data;
                break;
        }
        
        key.usage_count++;
        stats_.total_encryptions++;
        stats_.bytes_encrypted += data.size();
        
        return result;
    }
    
    /**
     * @brief Decrypt data
     * @param data Data to decrypt
     * @param key_id Key ID to use
     * @return Decrypted data
     */
    std::optional<std::vector<uint8_t>> decrypt(const std::vector<uint8_t>& data,
                                                 const std::string& key_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = keys_.find(key_id);
        if (it == keys_.end()) {
            ++stats_.failures;
            return std::nullopt;
        }
        
        auto& key = it->second;
        
        std::vector<uint8_t> result;
        
        switch (key.type) {
            case EncryptionType::XOR_SIMPLE:
            case EncryptionType::AES_128:
            case EncryptionType::AES_256:
                // XOR is symmetric
                result = xorEncrypt(data, key.key_data);
                break;
            case EncryptionType::NONE:
                result = data;
                break;
        }
        
        key.usage_count++;
        stats_.total_decryptions++;
        stats_.bytes_decrypted += data.size();
        
        return result;
    }
    
    /**
     * @brief Rotate a key
     * @param old_key_id Current key ID
     * @return New key ID
     */
    std::optional<std::string> rotateKey(const std::string& old_key_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = keys_.find(old_key_id);
        if (it == keys_.end()) return std::nullopt;
        
        // Mark old key as inactive
        it->second.active = false;
        
        // Generate new key with same name
        std::string new_id = "KEY-" + std::to_string(next_key_id_++);
        
        EncryptionKey new_key;
        new_key.id = new_id;
        new_key.name = it->second.name + " (rotated)";
        new_key.type = it->second.type;
        new_key.created = std::chrono::system_clock::now();
        new_key.expires = new_key.created + std::chrono::hours(24 * 365);
        
        // Generate new key data
        size_t key_size = getKeySize(new_key.type);
        new_key.key_data.resize(key_size);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 255);
        
        for (auto& byte : new_key.key_data) {
            byte = static_cast<uint8_t>(dist(gen));
        }
        
        keys_[new_id] = new_key;
        ++stats_.key_rotations;
        ++stats_.key_count;
        
        return new_id;
    }
    
    /**
     * @brief Delete a key
     */
    bool deleteKey(const std::string& key_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = keys_.find(key_id);
        if (it == keys_.end()) return false;
        
        // Securely clear key data
        std::fill(it->second.key_data.begin(), it->second.key_data.end(), 0);
        keys_.erase(it);
        --stats_.key_count;
        
        return true;
    }
    
    /**
     * @brief Get all key IDs
     */
    std::vector<std::string> getAllKeyIds() const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        std::vector<std::string> result;
        for (const auto& [id, key] : keys_) {
            result.push_back(id);
        }
        return result;
    }
    
    /**
     * @brief Get encryption statistics
     */
    EncryptionStatistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return stats_;
    }
    
    /**
     * @brief Check if dataset is encrypted with specified key
     */
    bool verifyEncryption(const std::vector<uint8_t>& encrypted_data,
                          const std::string& key_id) {
        // Verify key exists
        auto it = keys_.find(key_id);
        if (it == keys_.end()) {
            return false;
        }
        
        // Verify encrypted data has minimum size for header/IV
        if (encrypted_data.size() < 16) {
            return false;
        }
        
        // In a real implementation, this would verify encryption headers
        // For now, verify data is not all zeros (likely unencrypted)
        bool hasNonZero = false;
        for (size_t i = 0; i < std::min(encrypted_data.size(), size_t(16)); ++i) {
            if (encrypted_data[i] != 0) {
                hasNonZero = true;
                break;
            }
        }
        
        return hasNonZero;
    }
    
    /**
     * @brief Generate encryption report
     */
    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        
        oss << "=== Encryption Statistics ===\n"
            << "Total Encryptions: " << stats_.total_encryptions << "\n"
            << "Total Decryptions: " << stats_.total_decryptions << "\n"
            << "Bytes Encrypted: " << formatBytes(stats_.bytes_encrypted) << "\n"
            << "Bytes Decrypted: " << formatBytes(stats_.bytes_decrypted) << "\n"
            << "Key Count: " << stats_.key_count << "\n"
            << "Key Rotations: " << stats_.key_rotations << "\n"
            << "Failures: " << stats_.failures << "\n\n"
            << "Keys:\n";
        
        for (const auto& [id, key] : keys_) {
            oss << "  " << id << " (" << key.name << "): "
                << encryptionTypeToString(key.type)
                << (key.active ? " [ACTIVE]" : " [INACTIVE]") << "\n";
        }
        
        return oss.str();
    }

private:
    size_t getKeySize(EncryptionType type) const {
        switch (type) {
            case EncryptionType::AES_128: return 16;
            case EncryptionType::AES_256: return 32;
            case EncryptionType::XOR_SIMPLE: return 32;
            default: return 0;
        }
    }
    
    std::vector<uint8_t> xorEncrypt(const std::vector<uint8_t>& data,
                                     const std::vector<uint8_t>& key) {
        std::vector<uint8_t> result(data.size());
        
        for (size_t i = 0; i < data.size(); ++i) {
            result[i] = data[i] ^ key[i % key.size()];
        }
        
        return result;
    }
    
    std::atomic<bool> running_;
    std::atomic<uint64_t> next_key_id_;
    mutable std::mutex mtx_;
    std::map<std::string, EncryptionKey> keys_;
    EncryptionStatistics stats_;
};

} // namespace dfsmshsm
#endif // DFSMSHSM_ENCRYPTION_MANAGER_HPP
