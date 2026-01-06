/**
 * @file example_quota.cpp
 * @brief Quota management example
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */

#include "hsm_engine.hpp"
#include <iostream>

using namespace dfsmshsm;

int main() {
    std::cout << "=== DFSMShsm Quota Management Example ===\n\n";
    
    HSMEngine engine;
    engine.initialize("./hsm_data_quota");
    engine.start();
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 107374182400ULL);
    
    // Set user quotas
    std::cout << "=== Setting User Quotas ===\n";
    engine.setUserQuota("JOHN", 10485760, 10);   // 10MB, 10 files
    engine.setUserQuota("JANE", 52428800, 50);   // 50MB, 50 files
    engine.setGroupQuota("FINANCE", 104857600);  // 100MB for group
    std::cout << "JOHN: 10MB limit, 10 file limit\n";
    std::cout << "JANE: 50MB limit, 50 file limit\n";
    std::cout << "FINANCE group: 100MB limit\n\n";
    
    // Create datasets as different users
    std::cout << "=== Creating Datasets ===\n";
    
    auto r1 = engine.createDataset("JOHN.DATA1", 2097152, "", "JOHN", "FINANCE");
    std::cout << "JOHN.DATA1 (2MB): " << statusToString(r1.status) << "\n";
    
    auto r2 = engine.createDataset("JOHN.DATA2", 5242880, "", "JOHN", "FINANCE");
    std::cout << "JOHN.DATA2 (5MB): " << statusToString(r2.status) << "\n";
    
    auto r3 = engine.createDataset("JOHN.DATA3", 5242880, "", "JOHN", "FINANCE");
    std::cout << "JOHN.DATA3 (5MB): " << statusToString(r3.status);
    if (r3.status == OperationStatus::FAILED) {
        std::cout << " - " << r3.message;
    }
    std::cout << "\n\n";
    
    // Show quota usage
    std::cout << "=== Quota Usage ===\n";
    auto john_usage = engine.getUserQuotaUsage("JOHN");
    if (john_usage) {
        std::cout << "JOHN:\n";
        std::cout << "  Space: " << formatBytes(john_usage->space_used) 
                  << " / " << formatBytes(john_usage->space_limit)
                  << " (" << std::fixed << std::setprecision(1) 
                  << john_usage->space_percent << "%)\n";
        std::cout << "  Files: " << john_usage->inode_used 
                  << " / " << john_usage->inode_limit << "\n";
        std::cout << "  Status: " << quotaResultToString(john_usage->status) << "\n";
    }
    
    // Check quota before operation
    std::cout << "\n=== Pre-Check Quota ===\n";
    auto check = engine.checkQuota("JOHN", "FINANCE", 2097152);
    std::cout << "Check for 2MB allocation: " << quotaResultToString(check) << "\n";
    
    check = engine.checkQuota("JOHN", "FINANCE", 10485760);
    std::cout << "Check for 10MB allocation: " << quotaResultToString(check) << "\n";
    
    // Direct quota manager access for soft limits
    std::cout << "\n=== Soft Limit Example ===\n";
    auto& quota = engine.getQuotaManager();
    quota.setQuota("MARY", false, 10485760, 0, 5242880);  // 5MB soft, 10MB hard
    quota.updateUsage("MARY", "", 6291456, 1);  // 6MB used - over soft
    
    auto mary_check = quota.checkQuota("MARY", "", 1048576, 0);
    std::cout << "MARY (6MB used, 5MB soft, 10MB hard)\n";
    std::cout << "  Status: " << quotaResultToString(mary_check) << "\n";
    std::cout << "  (Soft limit exceeded, grace period active)\n";
    
    // Generate report
    std::cout << "\n" << quota.generateReport();
    
    engine.stop();
    return 0;
}
