/**
 * @file example_encryption.cpp
 * @brief Encryption at rest example
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */

#include "hsm_engine.hpp"
#include <iostream>

using namespace dfsmshsm;

int main() {
    std::cout << "=== DFSMShsm Encryption Example ===\n\n";
    
    HSMEngine engine;
    engine.initialize("./hsm_data_encrypt");
    engine.start();
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 107374182400ULL);
    
    // Create sensitive datasets
    engine.createDataset("PAYROLL.DATA", 1048576);
    engine.createDataset("HR.RECORDS", 2097152);
    engine.createDataset("PUBLIC.INFO", 524288);
    
    std::cout << "=== Encrypting Datasets ===\n";
    
    // Encrypt with auto-generated key
    auto r1 = engine.encryptDataset("PAYROLL.DATA");
    std::cout << "PAYROLL.DATA: " << statusToString(r1.status);
    if (r1.details.count("key_id")) {
        std::cout << " (Key: " << r1.details.at("key_id") << ")";
    }
    std::cout << "\n";
    
    // Encrypt with specific key
    std::string hr_key = engine.generateEncryptionKey("HR_Master_Key");
    auto r2 = engine.encryptDataset("HR.RECORDS", hr_key);
    std::cout << "HR.RECORDS: " << statusToString(r2.status) << " (Key: " << hr_key << ")\n\n";
    
    // Show dataset encryption status
    std::cout << "=== Dataset Status ===\n";
    for (const auto& ds : engine.getAllDatasets()) {
        std::cout << ds.name << ": " 
                  << (ds.is_encrypted ? "ENCRYPTED" : "PLAINTEXT") << "\n";
    }
    
    // Direct encryption manager access
    std::cout << "\n=== Key Management ===\n";
    auto& enc = engine.getEncryptionManager();
    
    // Generate additional keys
    auto key1 = enc.generateKey("Archive_Key", EncryptionType::AES_256, 365);
    auto key2 = enc.generateKey("Temp_Key", EncryptionType::AES_128, 30);
    std::cout << "Generated Archive_Key: " << key1 << " (expires in 365 days)\n";
    std::cout << "Generated Temp_Key: " << key2 << " (expires in 30 days)\n";
    
    // Show key info
    auto info = enc.getKeyInfo(hr_key);
    if (info) {
        std::cout << "\nHR_Master_Key info:\n";
        std::cout << "  Type: " << encryptionTypeToString(info->type) << "\n";
        std::cout << "  Active: " << (info->active ? "Yes" : "No") << "\n";
        std::cout << "  Usage count: " << info->usage_count << "\n";
    }
    
    // Key rotation
    std::cout << "\n=== Key Rotation ===\n";
    auto new_hr_key = engine.rotateEncryptionKey(hr_key);
    if (new_hr_key) {
        std::cout << "Rotated HR key: " << hr_key << " -> " << *new_hr_key << "\n";
        
        // Old key should be inactive
        auto old_info = enc.getKeyInfo(hr_key);
        std::cout << "Old key active: " << (old_info->active ? "Yes" : "No") << "\n";
    }
    
    // Decrypt dataset
    std::cout << "\n=== Decryption ===\n";
    auto r3 = engine.decryptDataset("PAYROLL.DATA");
    std::cout << "PAYROLL.DATA decrypted: " << statusToString(r3.status) << "\n";
    
    auto payroll = engine.getDataset("PAYROLL.DATA");
    std::cout << "Encryption status: " << (payroll->is_encrypted ? "ENCRYPTED" : "PLAINTEXT") << "\n";
    
    // Statistics
    std::cout << "\n" << enc.generateReport();
    
    engine.stop();
    return 0;
}
