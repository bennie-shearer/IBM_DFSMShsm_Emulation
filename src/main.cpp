/**
 * @file main.cpp
 * @brief DFSMShsm Emulator main entry point
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */

#include "hsm_engine.hpp"
#include "command_processor.hpp"
#include <iostream>
#include <string>
#include <filesystem>

using namespace dfsmshsm;

void printBanner() {
    std::cout << R"(
+===============================================================+
|        DFSMShsm - Hierarchical Storage Management Emulator    |
|                         Version 4.1.0                         |
|                    Author: Bennie Shearer                     |
|         Copyright (c) 2025 Bennie Shearer - MIT License        |
+===============================================================+
|  New in v4.1.0:                                               |
|  * Data Deduplication      * Quota Management                 |
|  * Write-Ahead Journaling  * Encryption at Rest               |
|  * Point-in-Time Snapshots * Cross-Tier Replication           |
|  * Task Scheduling         * Enhanced Monitoring              |
+===============================================================+
)" << "\n";
}

int main(int argc, char* argv[]) {
    printBanner();
    
    // Determine data directory
    std::filesystem::path data_dir = "./hsm_data";
    if (argc > 1) {
        data_dir = argv[1];
    }
    
    std::cout << "Initializing HSM engine at: " << data_dir << "\n\n";
    
    // Initialize engine
    HSMEngine engine;
    if (!engine.initialize(data_dir)) {
        std::cerr << "Failed to initialize HSM engine\n";
        return 1;
    }
    
    // Create default volumes
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 10737418240ULL);     // 10GB
    engine.createVolume("PRIMARY02", StorageTier::PRIMARY, 10737418240ULL);     // 10GB
    engine.createVolume("ML1VOL01", StorageTier::MIGRATION_1, 107374182400ULL); // 100GB
    engine.createVolume("ML2VOL01", StorageTier::MIGRATION_2, 536870912000ULL); // 500GB
    engine.createVolume("TAPEVOL01", StorageTier::TAPE, 1099511627776ULL);      // 1TB
    engine.createVolume("BACKUP01", StorageTier::BACKUP, 107374182400ULL);      // 100GB
    
    // Start engine
    if (!engine.start()) {
        std::cerr << "Failed to start HSM engine\n";
        return 1;
    }
    
    std::cout << "HSM engine started successfully.\n";
    std::cout << "Type 'HELP' for available commands, 'QUIT' to exit.\n\n";
    
    // Command processing loop
    CommandProcessor processor(&engine);
    std::string line;
    
    while (true) {
        std::cout << "HSM> ";
        std::cout.flush();
        
        if (!std::getline(std::cin, line)) {
            break;
        }
        
        if (line.empty()) {
            continue;
        }
        
        std::string result = processor.process(line);
        
        if (result == "QUIT") {
            std::cout << "Shutting down HSM engine...\n";
            break;
        }
        
        std::cout << result << "\n";
    }
    
    engine.stop();
    std::cout << "Goodbye!\n";
    
    return 0;
}
