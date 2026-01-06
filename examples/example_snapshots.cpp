/**
 * @file example_snapshots.cpp
 * @brief Point-in-time snapshots example
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
    std::cout << "=== DFSMShsm Snapshots Example ===\n\n";
    
    HSMEngine engine;
    engine.initialize("./hsm_data_snapshots");
    engine.start();
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 107374182400ULL);
    
    // Create a dataset
    engine.createDataset("PROD.DATABASE", 10485760);
    std::cout << "Created PROD.DATABASE (10MB)\n\n";
    
    // Create snapshots at different points
    std::cout << "=== Creating Snapshots ===\n";
    
    auto snap1 = engine.createSnapshot("PROD.DATABASE", "Initial state");
    std::cout << "Snapshot 1: " << (snap1 ? *snap1 : "FAILED") << "\n";
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto snap2 = engine.createSnapshot("PROD.DATABASE", "After update 1", 30);
    std::cout << "Snapshot 2: " << (snap2 ? *snap2 : "FAILED") << " (30 day retention)\n";
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto snap3 = engine.createSnapshot("PROD.DATABASE", "Before maintenance");
    std::cout << "Snapshot 3: " << (snap3 ? *snap3 : "FAILED") << "\n\n";
    
    // List snapshots
    std::cout << "=== Snapshot List ===\n";
    auto snaps = engine.getDatasetSnapshots("PROD.DATABASE");
    for (const auto& s : snaps) {
        std::cout << "  " << s.id << " (v" << s.version << "): " << s.description << "\n";
    }
    
    // Set retention policy
    std::cout << "\n=== Retention Policy ===\n";
    auto& snap_mgr = engine.getSnapshotManager();
    
    SnapshotPolicy policy;
    policy.hourly_keep = 24;   // Keep 24 hourly snapshots
    policy.daily_keep = 7;     // Keep 7 daily snapshots
    policy.weekly_keep = 4;    // Keep 4 weekly snapshots
    policy.monthly_keep = 12;  // Keep 12 monthly snapshots
    policy.enabled = true;
    
    snap_mgr.setPolicy("PROD.DATABASE", policy);
    std::cout << "Set retention: 24 hourly, 7 daily, 4 weekly, 12 monthly\n";
    
    // Compare snapshots
    if (snap1 && snap3) {
        std::cout << "\n=== Snapshot Comparison ===\n";
        std::cout << "Comparing " << *snap1 << " vs " << *snap3 << "\n";
        auto diff = snap_mgr.compareSnapshots(*snap1, *snap3);
        std::cout << diff << "\n";
    }
    
    // Restore from snapshot
    std::cout << "\n=== Restore Operation ===\n";
    if (snap2) {
        std::cout << "Restoring from " << *snap2 << "...\n";
        auto result = engine.restoreFromSnapshot(*snap2);
        std::cout << "Result: " << statusToString(result.status) << "\n";
    }
    
    // Delete a snapshot
    std::cout << "\n=== Delete Snapshot ===\n";
    if (snap1) {
        bool deleted = engine.deleteSnapshot(*snap1);
        std::cout << "Deleted " << *snap1 << ": " << (deleted ? "OK" : "FAILED") << "\n";
    }
    
    // Final snapshot list
    std::cout << "\n=== Final Snapshot List ===\n";
    snaps = engine.getDatasetSnapshots("PROD.DATABASE");
    for (const auto& s : snaps) {
        std::cout << "  " << s.id << " (v" << s.version << "): " << s.description << "\n";
    }
    
    // Statistics
    std::cout << "\n" << snap_mgr.generateReport();
    
    engine.stop();
    return 0;
}
