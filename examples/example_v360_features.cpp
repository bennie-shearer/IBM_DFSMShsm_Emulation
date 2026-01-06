/**
 * @file example_v360_features.cpp
 * @brief Minimal demo of DFSMShsm v3.6.0 feature headers
 * @version 4.2.0
 */

#include <iostream>

// v3.6.0 Feature Headers
#include "hsm_sms.hpp"
#include "hsm_cloud_tier.hpp"
#include "hsm_abars.hpp"
#include "hsm_crq.hpp"
#include "hsm_fast_replication.hpp"

using namespace dfsmshsm;

int main() {
    std::cout << "========================================\n";
    std::cout << "DFSMShsm v3.6.0 Features\n";
    std::cout << "========================================\n\n";
    
    // SMS Integration
    std::cout << "SMS Integration:\n";
    sms::StorageClass sc("FAST", "High-performance storage");
    sms::DataClass dc("SEQDATA", "Sequential data");
    sms::ManagementClass mc("STANDARD", "Standard management");
    std::cout << "  Storage Class: " << sc.getName() << "\n";
    std::cout << "  Data Class: " << dc.getName() << "\n";
    std::cout << "  Management Class: " << mc.getName() << "\n\n";
    
    // Cloud Tiering
    std::cout << "Cloud Tiering:\n";
    cloud::CloudTierManager cloudMgr;
    cloud::CostAnalyzer analyzer;
    std::cout << "  CloudTierManager created\n";
    std::cout << "  CostAnalyzer created\n\n";
    
    // ABARS
    std::cout << "ABARS Enhancement:\n";
    abars::ABARSManager abarsMgr;
    abars::ABackupOptions backupOpts;
    backupOpts.compress = true;
    std::cout << "  ABARSManager created\n";
    std::cout << "  Compression: enabled\n\n";
    
    // CRQ
    std::cout << "Common Recall Queue:\n";
    crq::CRQManager crqMgr;
    auto id = crqMgr.submitRecall("USER.DATA", crq::RecallPriority::NORMAL, "USER1");
    std::cout << "  Submitted recall: " << id << "\n\n";
    
    // Fast Replication
    std::cout << "Fast Replication:\n";
    replication::ReplicationManager repMgr;
    std::cout << "  ReplicationManager created\n";
    
    std::cout << "\n========================================\n";
    std::cout << "All v3.6.0 headers compiled successfully!\n";
    std::cout << "========================================\n";
    
    return 0;
}
