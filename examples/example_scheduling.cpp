/**
 * @file example_scheduling.cpp
 * @brief Task scheduling example
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */

#include "hsm_engine.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace dfsmshsm;

int main() {
    std::cout << "=== DFSMShsm Scheduling Example ===\n\n";
    
    HSMEngine engine;
    engine.initialize("./hsm_data_scheduling");
    engine.start();
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 107374182400ULL);
    engine.createVolume("ML1VOL01", StorageTier::MIGRATION_1, 536870912000ULL);
    engine.createVolume("BACKUP01", StorageTier::BACKUP, 107374182400ULL);
    
    // Create test datasets
    for (int i = 1; i <= 5; ++i) {
        engine.createDataset("SCHEDULE.TEST" + std::to_string(i), 1048576);
    }
    
    // Access scheduler directly
    auto& sched = engine.getScheduler();
    
    // Counter for callback tracking
    std::atomic<int> backup_count{0};
    std::atomic<int> health_count{0};
    
    // Schedule daily backup
    std::cout << "=== Scheduling Tasks ===\n";
    
    auto task1 = sched.scheduleTask("DailyBackup", OperationType::OP_BACKUP,
        ScheduleFrequency::DAILY,
        [&backup_count](const ScheduledTask& task) {
            ++backup_count;
            std::cout << "  [Executing] " << task.name << " (run #" << backup_count << ")\n";
            return true;
        });
    std::cout << "Scheduled DailyBackup: " << task1 << "\n";
    
    // Schedule hourly health check
    auto task2 = sched.scheduleTask("HourlyHealthCheck", OperationType::OP_HEALTH_CHECK,
        ScheduleFrequency::HOURLY,
        [&health_count](const ScheduledTask& task) {
            ++health_count;
            std::cout << "  [Executing] " << task.name << " (run #" << health_count << ")\n";
            return true;
        });
    std::cout << "Scheduled HourlyHealthCheck: " << task2 << "\n";
    
    // Schedule weekly migration with CRON
    auto task3 = sched.scheduleCron("WeeklyMigration", OperationType::OP_MIGRATE,
        "0 2 * * 0",  // Sundays at 2:00 AM
        [](const ScheduledTask& task) {
            std::cout << "  [Executing] " << task.name << " (weekly)\n";
            return true;
        });
    std::cout << "Scheduled WeeklyMigration (CRON: 0 2 * * 0): " << task3 << "\n";
    
    // Schedule one-time task
    auto now = std::chrono::system_clock::now();
    auto one_min = now + std::chrono::minutes(1);
    auto task4 = sched.scheduleOnce("OneTimeCleanup", OperationType::OP_DELETE,
        one_min,
        [](const ScheduledTask& task) {
            std::cout << "  [Executing] " << task.name << " (one-time)\n";
            return true;
        });
    std::cout << "Scheduled OneTimeCleanup: " << task4 << " (runs in 1 minute)\n\n";
    
    // List scheduled tasks
    std::cout << "=== Scheduled Tasks ===\n";
    for (const auto& task : engine.getScheduledTasks()) {
        std::cout << "  " << task.id << ": " << task.name << "\n";
        std::cout << "    Operation: " << operationToString(task.operation) << "\n";
        std::cout << "    Frequency: " << frequencyToString(task.frequency) << "\n";
        std::cout << "    Enabled: " << (task.enabled ? "Yes" : "No") << "\n";
        std::cout << "    Run count: " << task.run_count << "\n";
    }
    
    // Manually trigger tasks
    std::cout << "\n=== Manual Trigger ===\n";
    sched.triggerTask(task1);
    sched.triggerTask(task2);
    std::cout << "Triggered DailyBackup and HourlyHealthCheck\n";
    
    // Disable a task
    std::cout << "\n=== Disable Task ===\n";
    sched.setTaskEnabled(task2, false);
    auto task_info = sched.getTask(task2);
    std::cout << task2 << " enabled: " << (task_info->enabled ? "Yes" : "No") << "\n";
    
    // Re-enable
    sched.setTaskEnabled(task2, true);
    task_info = sched.getTask(task2);
    std::cout << task2 << " enabled: " << (task_info->enabled ? "Yes" : "No") << " (re-enabled)\n";
    
    // Cancel a task
    std::cout << "\n=== Cancel Task ===\n";
    bool cancelled = engine.cancelTask(task4);
    std::cout << "Cancelled OneTimeCleanup: " << (cancelled ? "OK" : "FAILED") << "\n";
    
    // Final task list
    std::cout << "\n=== Final Task List ===\n";
    for (const auto& task : engine.getScheduledTasks()) {
        std::cout << "  " << task.name << " (" << frequencyToString(task.frequency) << ")\n";
    }
    
    // Statistics
    std::cout << "\n" << sched.generateReport();
    
    std::cout << "\nTotal callbacks executed:\n";
    std::cout << "  Backups: " << backup_count << "\n";
    std::cout << "  Health checks: " << health_count << "\n";
    
    engine.stop();
    return 0;
}
