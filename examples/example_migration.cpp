/**
 * @file example_migration.cpp
 * @brief Migration and recall operations example
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */

#include "hsm_engine.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace dfsmshsm;

int main() {
    std::cout << "=== DFSMShsm Migration Example ===\n\n";
    
    HSMEngine engine;
    engine.initialize("./hsm_data_migration");
    engine.start();
    
    // Create tiered volumes
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 10737418240ULL);
    engine.createVolume("ML1VOL01", StorageTier::MIGRATION_1, 107374182400ULL);
    engine.createVolume("ML2VOL01", StorageTier::MIGRATION_2, 536870912000ULL);
    engine.createVolume("TAPEVOL01", StorageTier::TAPE, 1099511627776ULL);
    
    // Create test datasets
    for (int i = 1; i <= 5; ++i) {
        engine.createDataset("TEST.DATA" + std::to_string(i), 1024 * 1024 * i);
    }
    
    std::cout << "Created 5 test datasets on PRIMARY tier\n\n";
    
    // Migrate datasets progressively
    std::cout << "=== Progressive Migration ===\n";
    
    auto migrate_and_show = [&](const std::string& name, StorageTier tier) {
        auto result = engine.migrateDataset(name, tier);
        auto ds = engine.getDataset(name);
        std::cout << name << " -> " << tierToString(tier) 
                  << ": " << statusToString(result.status) << "\n";
    };
    
    migrate_and_show("TEST.DATA1", StorageTier::MIGRATION_1);
    migrate_and_show("TEST.DATA2", StorageTier::MIGRATION_1);
    migrate_and_show("TEST.DATA3", StorageTier::MIGRATION_2);
    migrate_and_show("TEST.DATA4", StorageTier::TAPE);
    
    // Show tier distribution
    std::cout << "\n=== Tier Distribution ===\n";
    for (auto tier : {StorageTier::PRIMARY, StorageTier::MIGRATION_1, 
                      StorageTier::MIGRATION_2, StorageTier::TAPE}) {
        auto ds = engine.getDatasetsByTier(tier);
        std::cout << tierToString(tier) << ": " << ds.size() << " datasets\n";
    }
    
    // Recall a dataset
    std::cout << "\n=== Recall Operation ===\n";
    std::cout << "Recalling TEST.DATA4 from TAPE...\n";
    auto recall_result = engine.recallDataset("TEST.DATA4");
    std::cout << "Result: " << statusToString(recall_result.status) << "\n";
    
    auto recalled = engine.getDataset("TEST.DATA4");
    std::cout << "New tier: " << tierToString(recalled->tier) << "\n";
    
    // Set migration policy
    std::cout << "\n=== Migration Policy ===\n";
    MigrationPolicy policy;
    policy.age_threshold_days = 30;
    policy.space_pressure_percent = 80;
    policy.enabled = true;
    engine.setMigrationPolicy(policy);
    std::cout << "Set policy: Migrate from PRIMARY after 30 days\n";
    
    engine.stop();
    return 0;
}
