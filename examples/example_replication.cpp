/**
 * @file example_replication.cpp
 * @brief Cross-tier replication example
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */

#include "hsm_engine.hpp"
#include <iostream>

using namespace dfsmshsm;

int main() {
    std::cout << "=== DFSMShsm Replication Example ===\n\n";
    
    HSMEngine engine;
    engine.initialize("./hsm_data_replication");
    engine.start();
    
    // Create volumes on different tiers
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 107374182400ULL);
    engine.createVolume("BACKUP01", StorageTier::BACKUP, 107374182400ULL);
    engine.createVolume("BACKUP02", StorageTier::BACKUP, 107374182400ULL);
    engine.createVolume("ML1VOL01", StorageTier::MIGRATION_1, 536870912000ULL);
    
    // Create critical datasets
    engine.createDataset("CRITICAL.DATA", 10485760);
    engine.createDataset("IMPORTANT.DATA", 5242880);
    std::cout << "Created datasets on PRIMARY\n\n";
    
    // Create synchronous replica
    std::cout << "=== Synchronous Replication ===\n";
    auto rep1 = engine.replicateDataset("CRITICAL.DATA", StorageTier::BACKUP, true);
    if (rep1) {
        std::cout << "Replica 1: " << *rep1 << " (sync to BACKUP)\n";
    }
    
    // Create asynchronous replica
    std::cout << "\n=== Asynchronous Replication ===\n";
    auto rep2 = engine.replicateDataset("CRITICAL.DATA", StorageTier::MIGRATION_1, false);
    std::cout << "Replica 2: queued for async replication to ML1\n";
    
    // Create second backup replica
    auto rep3 = engine.replicateDataset("CRITICAL.DATA", StorageTier::BACKUP, true);
    if (rep3) {
        std::cout << "Replica 3: " << *rep3 << " (second BACKUP copy)\n";
    }
    
    // Set replication policy
    std::cout << "\n=== Replication Policy ===\n";
    ReplicationPolicy policy;
    policy.replica_count = 2;
    policy.target_tiers = {StorageTier::BACKUP, StorageTier::MIGRATION_1};
    policy.sync_replication = false;
    policy.sync_interval_seconds = 3600;  // Sync every hour
    policy.verify_replicas = true;
    policy.enabled = true;
    
    engine.setReplicationPolicy("IMPORTANT.DATA", policy);
    std::cout << "Set policy for IMPORTANT.DATA:\n";
    std::cout << "  Replica count: 2\n";
    std::cout << "  Targets: BACKUP, ML1\n";
    std::cout << "  Sync interval: 3600s\n";
    
    // List replicas
    std::cout << "\n=== Replica List ===\n";
    auto replicas = engine.getDatasetReplicas("CRITICAL.DATA");
    for (const auto& r : replicas) {
        std::cout << "  " << r.id << ":\n";
        std::cout << "    Target: " << r.target_volume << " (" << tierToString(r.target_tier) << ")\n";
        std::cout << "    Consistent: " << (r.is_consistent ? "Yes" : "No") << "\n";
    }
    
    // Sync a replica
    if (rep1) {
        std::cout << "\n=== Sync Replica ===\n";
        bool synced = engine.syncReplica(*rep1);
        std::cout << "Synced " << *rep1 << ": " << (synced ? "OK" : "FAILED") << "\n";
    }
    
    // Verify consistency
    std::cout << "\n=== Consistency Verification ===\n";
    auto& repl = engine.getReplicationManager();
    for (const auto& r : replicas) {
        bool consistent = repl.verifyConsistency(r.id);
        std::cout << "  " << r.id << ": " << (consistent ? "CONSISTENT" : "INCONSISTENT") << "\n";
    }
    
    // Delete a replica
    if (rep3) {
        std::cout << "\n=== Delete Replica ===\n";
        bool deleted = engine.deleteReplica(*rep3);
        std::cout << "Deleted " << *rep3 << ": " << (deleted ? "OK" : "FAILED") << "\n";
    }
    
    // Final state
    std::cout << "\n=== Final Replica Count ===\n";
    replicas = engine.getDatasetReplicas("CRITICAL.DATA");
    std::cout << "CRITICAL.DATA has " << replicas.size() << " replica(s)\n";
    
    // Statistics
    std::cout << "\n" << repl.generateReport();
    
    engine.stop();
    return 0;
}
