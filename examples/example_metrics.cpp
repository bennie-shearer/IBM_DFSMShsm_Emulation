/**
 * @file example_metrics.cpp
 * @brief Prometheus/Grafana metrics export example
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#include "hsm_engine.hpp"
#include "metrics_exporter.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace dfsmshsm;

int main() {
    std::cout << "DFSMShsm v4.1.0 - Prometheus/Grafana Metrics Demo\n";
    std::cout << "=================================================\n\n";

    std::cout << "=== Initializing HSM Metrics ===\n";
    HSMMetrics::instance().initialize();

    std::cout << "\n=== Simulating Operations ===\n";
    for (int i = 0; i < 5; ++i) {
        HSMMetrics::instance().recordDatasetCreated();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cout << "Created 5 datasets\n";
    
    HSMMetrics::instance().recordMigration(1048576);
    HSMMetrics::instance().recordMigration(2097152);
    std::cout << "Recorded 2 migrations\n";
    
    HSMMetrics::instance().recordRecall();
    HSMMetrics::instance().recordBackup(5242880);
    std::cout << "Recorded recall and backup\n";
    
    HSMMetrics::instance().updateStorageMetrics(10737418240ULL, 3221225472ULL);
    HSMMetrics::instance().updateDedupeRatio(1.75);
    HSMMetrics::instance().updateCompressionRatio(0.65);
    HSMMetrics::instance().updateHealth(true);
    HSMMetrics::instance().updateUptime(86400);

    std::cout << "\n=== PROMETHEUS FORMAT ===\n";
    std::cout << HSMMetrics::instance().exportPrometheus();

    std::cout << "\n=== JSON FORMAT ===\n";
    std::cout << HSMMetrics::instance().exportJSON();

    std::cout << "\n=== Grafana Integration ===\n";
    std::cout << "Configure Prometheus scrape:\n";
    std::cout << "  - job_name: 'dfsmshsm'\n";
    std::cout << "    static_configs:\n";
    std::cout << "      - targets: ['localhost:8080']\n";
    return 0;
}
