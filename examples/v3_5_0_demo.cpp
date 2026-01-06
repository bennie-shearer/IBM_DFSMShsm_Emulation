/*******************************************************************************
 * DFSMShsm Emulator - v3.5.0 Features Demo
 * 
 * Demonstrates all new features in version 3.5.0:
 * - Enhanced Logging/Tracing
 * - Performance Benchmarking
 * - Tape Pool Management
 * - Report Generation
 * - ABACKUP/ARECOVER Emulation
 * - Disaster Recovery
 *****************************************************************************
 * @version 4.2.0
 *****************************************************************************/

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

#include "hsm_logging.hpp"
#include "hsm_benchmark.hpp"
#include "hsm_tape_pool.hpp"
#include "hsm_reports.hpp"
#include "hsm_abackup.hpp"
#include "hsm_disaster_recovery.hpp"

using namespace dfsmshsm;

void printSection(const std::string& title) {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(70, '=') << std::endl;
}

// ============================================================================
// 1. Enhanced Logging/Tracing Demo
// ============================================================================
void demoLogging() {
    printSection("1. Enhanced Logging/Tracing");
    
    using namespace logging;
    
    // Create logger with console output
    auto logger = LogManager::instance().getLogger("hsm.demo");
    logger->setLevel(LogLevel::DEBUG);
    
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->setMinLevel(LogLevel::DEBUG);
    logger->addSink(console_sink);
    
    std::cout << "\n--- Basic Logging ---\n";
    logger->info("HSM system starting up");
    logger->debug("Initializing subsystems");
    logger->warn("Storage utilization at 85%");
    
    std::cout << "\n--- Tracing with Spans ---\n";
    {
        TraceSpan outer_span("migration_operation");
        logger->info("Starting migration [trace:" + 
                     TraceContext::getCurrentTraceId() + "]");
        
        {
            TraceSpan inner_span("compress_data");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            logger->debug("Compression complete in " + 
                         std::to_string(inner_span.elapsedMs()) + "ms");
        }
        
        logger->info("Migration complete in " + 
                     std::to_string(outer_span.elapsedMs()) + "ms");
    }
    
    std::cout << "\n--- JSON Logging ---\n";
    console_sink->setUseJSON(true);
    logger->info("JSON formatted log entry");
    console_sink->setUseJSON(false);
    
    std::cout << "\n--- Trace Analysis ---\n";
    TraceAnalyzer analyzer;
    
    for (int i = 0; i < 10; ++i) {
        TraceRecord record;
        record.trace_id = TraceContext::generateTraceId();
        record.operation = "migration";
        record.duration = std::chrono::milliseconds(100 + i * 10);
        record.status = (i < 9) ? LogLevel::INFO : LogLevel::ERROR;
        analyzer.recordTrace(record);
    }
    
    auto stats = analyzer.calculateStats("migration");
    std::cout << "  Trace Statistics:\n";
    std::cout << "    Total traces: " << stats.total_traces << "\n";
    std::cout << "    Avg duration: " << std::fixed << std::setprecision(2) 
              << stats.avg_duration_ms << " ms\n";
    std::cout << "    P95 duration: " << stats.p95_duration_ms << " ms\n";
    std::cout << "    Error count:  " << stats.error_count << "\n";
}

// ============================================================================
// 2. Performance Benchmarking Demo
// ============================================================================
void demoBenchmarking() {
    printSection("2. Performance Benchmarking");
    
    using namespace benchmark;
    
    std::cout << "\n--- Running Migration Benchmark ---\n";
    
    HSMBenchmarks::MigrationBenchmarkConfig mig_config;
    mig_config.file_size = 64 * 1024;  // 64 KB
    mig_config.file_count = 10;
    mig_config.use_compression = true;
    
    BenchmarkRunner::Config runner_config;
    runner_config.iterations = 5;
    runner_config.warmup_iterations = 1;
    
    auto mig_result = HSMBenchmarks::benchmarkMigration(mig_config, runner_config);
    
    std::cout << "  Migration Benchmark Results:\n";
    std::cout << "    Iterations:   " << mig_result.iterations << "\n";
    std::cout << "    Avg Time:     " << std::fixed << std::setprecision(2)
              << mig_result.time_stats_ms.mean << " ms\n";
    std::cout << "    Throughput:   " << mig_result.throughput_stats_mbps.mean << " MB/s\n";
    
    std::cout << "\n--- Running Compression Benchmark ---\n";
    
    HSMBenchmarks::CompressionBenchmarkConfig comp_config;
    comp_config.block_size = 32 * 1024;
    comp_config.iterations = 100;
    comp_config.algorithm = "LZ4";
    
    runner_config.iterations = 3;
    auto comp_result = HSMBenchmarks::benchmarkCompression(comp_config, runner_config);
    
    std::cout << "  Compression Benchmark Results:\n";
    std::cout << "    Avg Time:     " << comp_result.time_stats_ms.mean << " ms\n";
    std::cout << "    Throughput:   " << comp_result.throughput_stats_mbps.mean << " MB/s\n";
    
    std::cout << "\n--- Benchmark Suite ---\n";
    
    BenchmarkSuite suite;
    suite.addBenchmark("quick_io", []() {
        BenchmarkRunner::Config cfg;
        cfg.iterations = 3;
        return HSMBenchmarks::benchmarkIOThroughput(4096, 64 * 1024, true, cfg);
    });
    
    auto results = suite.runAll();
    std::cout << "  Suite completed with " << results.size() << " benchmarks\n";
    
    // Export example
    std::cout << "\n--- JSON Export ---\n";
    std::cout << mig_result.toJSON().substr(0, 200) << "...\n";
}

// ============================================================================
// 3. Tape Pool Management Demo
// ============================================================================
void demoTapePoolManagement() {
    printSection("3. Tape Pool Management");
    
    using namespace tapepool;
    
    TapePoolManager manager;
    
    // Create pools
    std::cout << "\n--- Creating Tape Pools ---\n";
    
    TapePool scratch_pool;
    scratch_pool.name = "SCRATCH";
    scratch_pool.description = "Primary scratch pool";
    scratch_pool.default_media_type = MediaType::LTO8;
    scratch_pool.target_scratch_count = 100;
    scratch_pool.auto_scratch_return = true;
    manager.createPool(scratch_pool);
    
    TapePool ml2_pool;
    ml2_pool.name = "ML2_POOL";
    ml2_pool.description = "ML2 migration target";
    ml2_pool.default_media_type = MediaType::LTO8;
    manager.createPool(ml2_pool);
    
    std::cout << "  Created pools: SCRATCH, ML2_POOL\n";
    
    // Generate test volumes
    std::cout << "\n--- Adding Volumes ---\n";
    manager.generateTestVolumes("SCRATCH", 20, MediaType::LTO8);
    manager.generateTestVolumes("ML2_POOL", 10, MediaType::LTO8);
    
    auto scratch_stats = manager.getPoolStatistics("SCRATCH");
    std::cout << "  SCRATCH pool: " << scratch_stats.total_volumes 
              << " volumes, " << scratch_stats.scratch_volumes << " scratch\n";
    
    // Allocate volumes
    std::cout << "\n--- Allocating Scratch Volumes ---\n";
    
    AllocationRequest request;
    request.pool_name = "SCRATCH";
    request.owner = "HSM_MIGRATION";
    request.retention_days = 30;
    
    for (int i = 0; i < 3; ++i) {
        auto result = manager.allocateScratch(request);
        if (result.success) {
            std::cout << "  Allocated: " << result.volser << "\n";
        }
    }
    
    // Pool health check
    std::cout << "\n--- Pool Health Check ---\n";
    auto health = manager.checkPoolHealth("SCRATCH");
    std::cout << "  SCRATCH pool healthy: " << (health.healthy ? "Yes" : "No") << "\n";
    std::cout << "  Scratch %: " << std::fixed << std::setprecision(1) 
              << health.scratch_percentage << "%\n";
    
    if (!health.warnings.empty()) {
        std::cout << "  Warnings:\n";
        for (const auto& w : health.warnings) {
            std::cout << "    - " << w << "\n";
        }
    }
    
    // Return to scratch
    std::cout << "\n--- Returning Volume to Scratch ---\n";
    auto allocated = manager.getVolumesByStatus(VolumeStatus::FILLING);
    if (!allocated.empty()) {
        manager.returnToScratch(allocated[0].volser);
        std::cout << "  Returned " << allocated[0].volser << " to scratch\n";
    }
}

// ============================================================================
// 4. Report Generation Demo
// ============================================================================
void demoReportGeneration() {
    printSection("4. Report Generation");
    
    using namespace reports;
    
    // Create data collector and add sample data
    auto collector = std::make_shared<ReportDataCollector>();
    
    std::cout << "\n--- Collecting Sample Data ---\n";
    
    auto now = std::chrono::system_clock::now();
    
    // Add migration records
    for (int i = 0; i < 20; ++i) {
        MigrationRecord mig;
        mig.dataset_name = "SYS" + std::to_string(i % 5) + ".DATA.SET" + std::to_string(i);
        mig.source_volume = "VOL001";
        mig.target_volume = "ML2001";
        mig.bytes_migrated = 1000000 * (i + 1);
        mig.timestamp = now - std::chrono::hours(i);
        mig.duration = std::chrono::milliseconds(100 * (i + 1));
        mig.compressed = true;
        mig.compression_ratio = 2.0 + (i % 3) * 0.5;
        mig.success = (i != 5);  // One failure
        mig.migration_level = (i % 2 == 0) ? 1 : 2;
        collector->addMigration(mig);
    }
    
    // Add recall records
    for (int i = 0; i < 10; ++i) {
        RecallRecord rec;
        rec.dataset_name = "USER.DATA" + std::to_string(i);
        rec.source_volume = "ML2002";
        rec.bytes_recalled = 500000 * (i + 1);
        rec.timestamp = now - std::chrono::hours(i * 2);
        rec.duration = std::chrono::milliseconds(200 * (i + 1));
        rec.wait_time = std::chrono::milliseconds(50 * i);
        rec.success = true;
        collector->addRecall(rec);
    }
    
    std::cout << "  Added 20 migration records\n";
    std::cout << "  Added 10 recall records\n";
    
    // Generate reports
    ReportGenerator generator(collector);
    auto range = ReportTimeRange::lastNDays(7);
    
    std::cout << "\n--- Migration Activity Report (Text) ---\n";
    auto mig_report = generator.createMigrationReport();
    mig_report->setTimeRange(range);
    mig_report->setFormat(ReportFormat::TEXT);
    std::cout << mig_report->generate();
    
    std::cout << "\n--- Dashboard (Text) ---\n";
    auto dashboard = generator.createDashboard();
    dashboard->setTimeRange(range);
    dashboard->setFormat(ReportFormat::TEXT);
    std::cout << dashboard->generate();
    
    std::cout << "\n--- CSV Export Sample ---\n";
    mig_report->setFormat(ReportFormat::CSV);
    std::string csv = mig_report->generate();
    // Show first 3 lines
    std::istringstream iss(csv);
    std::string line;
    int line_count = 0;
    while (std::getline(iss, line) && line_count < 3) {
        std::cout << line << "\n";
        line_count++;
    }
    std::cout << "  ... (" << (std::count(csv.begin(), csv.end(), '\n') - 2) 
              << " more rows)\n";
}

// ============================================================================
// 5. ABACKUP/ARECOVER Demo
// ============================================================================
void demoABackup() {
    printSection("5. ABACKUP/ARECOVER Emulation");
    
    using namespace abackup;
    
    ABackupManager manager;
    
    // Define aggregate group
    std::cout << "\n--- Defining Aggregate Group ---\n";
    
    AggregateGroup group;
    group.name = "PROD_DAILY";
    group.description = "Production daily backup";
    group.volume_patterns.push_back("DASD*");
    group.dataset_patterns.push_back("SYS1.*");
    group.dataset_patterns.push_back("PROD.*");
    group.default_backup_type = BackupType::FULL;
    group.retention_days = 30;
    group.versions_to_keep = 7;
    group.compress_backup = true;
    
    manager.defineGroup(group);
    std::cout << "  Defined group: " << group.name << "\n";
    std::cout << "  Patterns: SYS1.*, PROD.*\n";
    std::cout << "  Retention: " << group.retention_days << " days\n";
    
    // Execute ABACKUP
    std::cout << "\n--- Executing ABACKUP ---\n";
    
    ABackupOptions backup_opts;
    backup_opts.group_name = "PROD_DAILY";
    backup_opts.type = BackupType::FULL;
    backup_opts.compress = true;
    backup_opts.verify_after = true;
    backup_opts.comment = "Weekly full backup";
    
    auto backup_result = manager.executeABackup(backup_opts, 
        [](const std::string& msg, double pct) {
            std::cout << "  [" << std::setw(3) << static_cast<int>(pct) 
                      << "%] " << msg << "\n";
        });
    
    if (backup_result.success) {
        std::cout << "\n  Backup successful!\n";
        std::cout << "  Backup ID: " << backup_result.backup_id << "\n";
        std::cout << "  Bytes processed: " << backup_result.bytes_processed << "\n";
        std::cout << "  Bytes written: " << backup_result.bytes_written << "\n";
        std::cout << "  Files backed up: " << backup_result.files_backed_up << "\n";
    }
    
    // Create incremental backup
    std::cout << "\n--- Creating Incremental Backup ---\n";
    
    backup_opts.type = BackupType::INCREMENTAL;
    auto incr_result = manager.executeABackup(backup_opts);
    
    if (incr_result.success) {
        std::cout << "  Incremental backup: " << incr_result.backup_id << "\n";
    }
    
    // List backups
    std::cout << "\n--- Listing Backups ---\n";
    auto backups = manager.listBackups("PROD_DAILY");
    for (const auto& b : backups) {
        std::cout << "  " << b.toSummary() << "\n";
    }
    
    // Execute ARECOVER (verify only)
    std::cout << "\n--- Executing ARECOVER (Verify Only) ---\n";
    
    ARecoverOptions recover_opts;
    recover_opts.backup_id = backup_result.backup_id;
    recover_opts.verify_only = true;
    
    auto recover_result = manager.executeARecover(recover_opts,
        [](const std::string& msg, double pct) {
            std::cout << "  [" << std::setw(3) << static_cast<int>(pct) 
                      << "%] " << msg << "\n";
        });
    
    std::cout << "\n  Validation " 
              << (recover_result.validation_passed ? "PASSED" : "FAILED") << "\n";
}

// ============================================================================
// 6. Disaster Recovery Demo
// ============================================================================
void demoDisasterRecovery() {
    printSection("6. Disaster Recovery");
    
    using namespace dr;
    
    DisasterRecoveryManager manager;
    
    // Configure sites
    std::cout << "\n--- Configuring Sites ---\n";
    
    Site primary;
    primary.site_id = "NYC";
    primary.name = "New York Data Center";
    primary.location = "New York, NY";
    primary.role = SiteRole::PRIMARY;
    primary.status = SiteStatus::ONLINE;
    primary.total_capacity = 100000000000;  // 100 GB
    primary.used_capacity = 50000000000;    // 50 GB
    primary.cpu_utilization = 45;
    primary.memory_utilization = 60;
    primary.storage_utilization = 50;
    primary.objectives.rpo = std::chrono::minutes(15);
    primary.objectives.rto = std::chrono::hours(1);
    primary.last_heartbeat = std::chrono::system_clock::now();
    manager.registerSite(primary);
    
    Site secondary;
    secondary.site_id = "CHI";
    secondary.name = "Chicago DR Site";
    secondary.location = "Chicago, IL";
    secondary.role = SiteRole::SECONDARY;
    secondary.status = SiteStatus::ONLINE;
    secondary.total_capacity = 100000000000;
    secondary.used_capacity = 50000000000;
    secondary.cpu_utilization = 20;
    secondary.memory_utilization = 30;
    secondary.storage_utilization = 50;
    secondary.last_heartbeat = std::chrono::system_clock::now();
    manager.registerSite(secondary);
    
    std::cout << "  Primary: " << primary.name << " (" << primary.site_id << ")\n";
    std::cout << "  Secondary: " << secondary.name << " (" << secondary.site_id << ")\n";
    
    // Configure replication
    std::cout << "\n--- Configuring Replication ---\n";
    
    ReplicationLink link;
    link.link_id = "NYC_CHI_ASYNC";
    link.source_site = "NYC";
    link.target_site = "CHI";
    link.mode = ReplicationMode::ASYNCHRONOUS;
    link.state = ReplicationState::ACTIVE;
    link.replication_lag = std::chrono::milliseconds(500);
    link.bytes_replicated = 10000000000;
    link.transactions_replicated = 50000;
    manager.createReplicationLink(link);
    
    std::cout << "  Link: " << link.link_id << "\n";
    std::cout << "  Mode: " << replicationModeToString(link.mode) << "\n";
    std::cout << "  Lag: " << link.replication_lag.count() << " ms\n";
    
    // Create recovery points
    std::cout << "\n--- Creating Recovery Points ---\n";
    
    auto rp1 = manager.createRecoveryPoint("CHI", "Hourly checkpoint");
    auto rp2 = manager.createRecoveryPoint("CHI", "Pre-maintenance checkpoint");
    
    std::cout << "  Created: " << rp1 << "\n";
    std::cout << "  Created: " << rp2 << "\n";
    
    // Get status report
    std::cout << "\n--- DR Status Report ---\n";
    auto report = manager.getStatusReport();
    
    std::cout << "  Overall Health: " << (report.healthy ? "HEALTHY" : "DEGRADED") << "\n";
    std::cout << "  Primary Site: " << report.primary_site << "\n";
    std::cout << "  Secondary Sites: " << report.secondary_sites.size() << "\n";
    std::cout << "  Online Sites: " << report.online_sites << "/" << report.total_sites << "\n";
    std::cout << "  Healthy Links: " << report.healthy_links << "/" << report.total_links << "\n";
    std::cout << "  Max Replication Lag: " << report.max_replication_lag.count() << " ms\n";
    
    // Run DR test
    std::cout << "\n--- Running DR Test ---\n";
    auto test_result = manager.runDRTest("CHI", false);
    
    std::cout << "  Test ID: " << test_result.test_id << "\n";
    std::cout << "  Overall: " << (test_result.success ? "PASSED" : "FAILED") << "\n";
    std::cout << "  RPO Validated: " << (test_result.rpo_validated ? "Yes" : "No") << "\n";
    std::cout << "  Data Integrity: " << (test_result.data_integrity_ok ? "OK" : "ISSUES") << "\n";
    
    if (!test_result.issues_found.empty()) {
        std::cout << "  Issues:\n";
        for (const auto& issue : test_result.issues_found) {
            std::cout << "    - " << issue << "\n";
        }
    }
    std::cout << "  Recommendation: " << test_result.recommendations << "\n";
    
    // Execute planned failover
    std::cout << "\n--- Executing Planned Failover ---\n";
    
    DisasterRecoveryManager::FailoverOptions fo_opts;
    fo_opts.target_site = "CHI";
    fo_opts.type = FailoverEvent::Type::PLANNED;
    fo_opts.create_recovery_point = true;
    fo_opts.initiated_by = "demo_user";
    
    auto fo_result = manager.initiateFailover(fo_opts);
    
    std::cout << "  Failover " << (fo_result.success ? "SUCCEEDED" : "FAILED") << "\n";
    std::cout << "  Event ID: " << fo_result.event_id << "\n";
    std::cout << "  Recovery Time: " << fo_result.recovery_time.count() << " seconds\n";
    std::cout << "  Data Loss: " << fo_result.data_loss.count() << " seconds\n";
    
    // Verify new primary
    auto new_primary = manager.getPrimarySite();
    if (new_primary) {
        std::cout << "\n  New Primary Site: " << new_primary->site_id 
                  << " (" << new_primary->name << ")\n";
    }
    
    // Show failover history
    std::cout << "\n--- Failover History ---\n";
    auto history = manager.getFailoverHistory(5);
    for (const auto& event : history) {
        std::cout << "  " << event.event_id 
                  << " | " << event.source_site << " -> " << event.target_site
                  << " | Duration: " << event.duration().count() << "s\n";
    }
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "\n";
    std::cout << "+======================================================================+\n";
    std::cout << "|           DFSMShsm Emulator v3.5.0 - Feature Demonstration           |\n";
    std::cout << "+======================================================================+\n";
    
    try {
        demoLogging();
        demoBenchmarking();
        demoTapePoolManagement();
        demoReportGeneration();
        demoABackup();
        demoDisasterRecovery();
        
        printSection("Demo Complete");
        std::cout << "\nAll v3.5.0 features demonstrated successfully!\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
