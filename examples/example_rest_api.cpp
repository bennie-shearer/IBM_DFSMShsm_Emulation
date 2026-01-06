/**
 * @file example_rest_api.cpp
 * @brief REST API interface example
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#include "hsm_engine.hpp"
#include "rest_api.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace dfsmshsm;

int main() {
    std::cout << "DFSMShsm v4.1.0 - REST API Interface Demo\n";
    std::cout << "==========================================\n\n";

    std::cout << "=== Initializing HSM Engine ===\n";
    HSMEngine engine;
    std::filesystem::path data_dir = "./hsm_rest_demo";
    engine.initialize(data_dir);
    engine.start();
    
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 10737418240ULL);
    engine.createDataset("USER.DATA.FILE1", 1048576);
    engine.createDataset("USER.DATA.FILE2", 2097152);
    engine.createDataset("PROD.DATABASE", 5242880);
    std::cout << "Created 3 test datasets\n";

    std::cout << "\n=== Starting REST API Server ===\n";
    uint16_t port = 8080;
    HSMRestAPI api(port);
    
    api.setHandlers(
        [&engine]() { return engine.getAllDatasets(); },
        [&engine](const std::string& n, uint64_t s) { return engine.createDataset(n, s); },
        [&engine](const std::string& n) { return engine.deleteDataset(n); },
        [&engine](const std::string& n, StorageTier t) { return engine.migrateDataset(n, t); },
        [&engine](const std::string& n) { return engine.recallDataset(n); },
        [&engine](const std::string& n) { return engine.backupDataset(n); },
        [&engine]() { return engine.getStatusReport(); }
    );
    
    if (!api.start()) {
        std::cerr << "Failed to start REST API on port " << port << "\n";
        engine.stop();
        return 1;
    }
    std::cout << "REST API started on port " << api.getPort() << "\n";

    std::cout << "\n=== API ENDPOINTS ===\n";
    std::cout << "GET  /api/v1           - API documentation\n";
    std::cout << "GET  /api/v1/health    - Health check\n";
    std::cout << "GET  /api/v1/datasets  - List datasets\n";
    std::cout << "POST /api/v1/datasets  - Create dataset\n";
    std::cout << "GET  /metrics          - Prometheus metrics\n";

    std::cout << "\n=== CURL EXAMPLES ===\n";
    std::cout << "curl http://localhost:" << port << "/api/v1/health\n";
    std::cout << "curl http://localhost:" << port << "/api/v1/datasets\n";
    std::cout << "curl http://localhost:" << port << "/metrics\n";

    std::cout << "\nPress Enter to stop...";
    std::cin.get();

    api.stop();
    engine.stop();
    std::filesystem::remove_all(data_dir);
    std::cout << "REST API Demo Complete\n";
    return 0;
}
