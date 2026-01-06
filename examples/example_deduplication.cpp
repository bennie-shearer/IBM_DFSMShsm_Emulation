/**
 * @file example_deduplication.cpp
 * @brief Data deduplication example
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */

#include "hsm_engine.hpp"
#include <iostream>
#include <vector>

using namespace dfsmshsm;

int main() {
    std::cout << "=== DFSMShsm Deduplication Example ===\n\n";
    
    HSMEngine engine;
    engine.initialize("./hsm_data_dedupe");
    engine.start();
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 107374182400ULL);
    
    // Create test data with duplicates
    std::cout << "Creating test data with duplicates...\n\n";
    
    // Data pattern: repeated blocks
    std::vector<uint8_t> data1(100000, 'A');  // 100KB of 'A'
    std::vector<uint8_t> data2(100000, 'A');  // Same as data1
    std::vector<uint8_t> data3(100000, 'B');  // Different pattern
    
    // Mix data2 with some unique content
    for (size_t i = 50000; i < 60000; ++i) data2[i] = 'X';
    
    // Store with deduplication
    engine.createDataset("DEDUPE.TEST1", data1.size());
    engine.createDataset("DEDUPE.TEST2", data2.size());
    engine.createDataset("DEDUPE.TEST3", data3.size());
    
    engine.deduplicateDataset("DEDUPE.TEST1", data1);
    engine.deduplicateDataset("DEDUPE.TEST2", data2);
    engine.deduplicateDataset("DEDUPE.TEST3", data3);
    
    // Show statistics
    std::cout << "=== Deduplication Statistics ===\n";
    auto stats = engine.getDedupeStatistics();
    std::cout << "Total blocks: " << stats.total_blocks << "\n";
    std::cout << "Unique blocks: " << stats.unique_blocks << "\n";
    std::cout << "Duplicate blocks: " << stats.duplicate_blocks << "\n";
    std::cout << "Bytes before: " << formatBytes(stats.bytes_before_dedupe) << "\n";
    std::cout << "Bytes after: " << formatBytes(stats.bytes_after_dedupe) << "\n";
    std::cout << "Bytes saved: " << formatBytes(stats.bytes_saved) << "\n";
    std::cout << "Dedupe ratio: " << std::fixed << std::setprecision(2) 
              << stats.dedupe_ratio << "x\n\n";
    
    // Check shared blocks
    auto& dedupe = engine.getDedupeManager();
    size_t shared = dedupe.getSharedBlockCount("DEDUPE.TEST1", "DEDUPE.TEST2");
    std::cout << "Shared blocks between TEST1 and TEST2: " << shared << "\n";
    
    // Verify integrity
    std::cout << "\n=== Integrity Verification ===\n";
    for (const auto* ds : {"DEDUPE.TEST1", "DEDUPE.TEST2", "DEDUPE.TEST3"}) {
        bool valid = dedupe.verifyIntegrity(ds);
        std::cout << ds << ": " << (valid ? "VALID" : "CORRUPTED") << "\n";
    }
    
    // Retrieve and verify data
    std::cout << "\n=== Data Retrieval ===\n";
    auto retrieved = dedupe.retrieve("DEDUPE.TEST1");
    if (retrieved && *retrieved == data1) {
        std::cout << "DEDUPE.TEST1 data verified successfully\n";
    }
    
    // Garbage collection
    std::cout << "\n=== Garbage Collection ===\n";
    engine.deleteDataset("DEDUPE.TEST3");
    size_t cleaned = engine.runDedupeGarbageCollection();
    std::cout << "Cleaned " << cleaned << " orphaned blocks\n";
    
    engine.stop();
    return 0;
}
