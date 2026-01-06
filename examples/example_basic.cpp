/**
 * @file example_basic.cpp
 * @brief Basic HSM operations example
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */

#include "hsm_engine.hpp"
#include <iostream>

using namespace dfsmshsm;

int main() {
    std::cout << "=== DFSMShsm Basic Example ===\n\n";
    
    HSMEngine engine;
    
    // Initialize
    if (!engine.initialize("./hsm_data")) {
        std::cerr << "Failed to initialize\n";
        return 1;
    }
    
    engine.start();
    std::cout << "Engine started (v" << HSMEngine::getVersion() << ")\n\n";
    
    // Create volumes
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 10737418240ULL);
    engine.createVolume("ML1VOL01", StorageTier::MIGRATION_1, 107374182400ULL);
    engine.createVolume("BACKUP01", StorageTier::BACKUP, 107374182400ULL);
    std::cout << "Created 3 volumes\n";
    
    // Create datasets
    auto r1 = engine.createDataset("USER.PAYROLL.DATA", 1048576);
    auto r2 = engine.createDataset("USER.ARCHIVE.DATA", 2097152);
    std::cout << "Created datasets: " << r1.message << ", " << r2.message << "\n\n";
    
    // List all datasets
    std::cout << "=== Datasets ===\n";
    for (const auto& ds : engine.getAllDatasets()) {
        std::cout << "  " << ds.name << " - " << tierToString(ds.tier) 
                  << " - " << formatBytes(ds.size) << "\n";
    }
    
    // Migrate a dataset
    std::cout << "\nMigrating USER.ARCHIVE.DATA to ML1...\n";
    auto migrate_result = engine.migrateDataset("USER.ARCHIVE.DATA", StorageTier::MIGRATION_1);
    std::cout << "Result: " << statusToString(migrate_result.status) << "\n";
    
    // Backup a dataset
    std::cout << "\nBacking up USER.PAYROLL.DATA...\n";
    auto backup_result = engine.backupDataset("USER.PAYROLL.DATA");
    std::cout << "Result: " << statusToString(backup_result.status) << "\n";
    
    // Show status
    std::cout << "\n" << engine.getStatusReport();
    
    engine.stop();
    std::cout << "\nEngine stopped. Goodbye!\n";
    
    return 0;
}
