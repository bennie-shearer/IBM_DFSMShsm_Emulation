/*******************************************************************************
 * DFSMShsm Emulator v3.5.0 - Comprehensive Examples
 * 
 * Demonstrates all v3.5.0 features:
 * - Enhanced Logging/Tracing
 * - Performance Benchmarking
 * - Tape Pool Management
 * - Report Generation
 * - ABACKUP/ARECOVER Emulation
 * - Disaster Recovery
 * 
 * Build: g++ -std=c++20 -I../include -o example_v350 example_v350_features.cpp -pthread
 *****************************************************************************
 * @version 4.2.0
 *****************************************************************************/

#include "hsm_logging.hpp"
#include "hsm_benchmark.hpp"
#include "hsm_tape_pool.hpp"
#include "hsm_reports.hpp"
#include "hsm_abackup.hpp"
#include "hsm_disaster_recovery.hpp"

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace dfsmshsm;

// ============================================================================
// 1. Enhanced Logging/Tracing Example
// ============================================================================

void demonstrateLogging() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  1. ENHANCED LOGGING AND TRACING\n";
    std::cout << std::string(70, '=') << "\n\n";
    
    // Create console sink
    auto console_sink = std::make_shared<logging::ConsoleSink>(true);
    console_sink->setMinLevel(logging::LogLevel::DEBUG);
    
    // Create file sink with rotation
    logging::RotationConfig rot_config;
    rot_config.max_file_size = 1024 * 1024;  // 1 MB
    rot_config.max_files = 3;
    rot_config.rotate_daily = true;
    
    auto file_sink = std::make_shared<logging::FileSink>("hsm.log", rot_config);
    file_sink->setUseJSON(true);  // JSON format for file
    
    // Setup log manager
    logging::LogManager::instance().addDefaultSink(console_sink);
    logging::LogManager::instance().setDefaultLevel(logging::LogLevel::DEBUG);
    
    // Get logger
    auto logger = logging::LogManager::instance().getLogger("hsm.migration");
    logger->addSink(file_sink);
    
    std::cout << "Logging with different levels:\n\n";
    
    // Basic logging
    logger->info("Migration service started");
    logger->debug("Processing batch of 100 files");
    logger->warn("High CPU usage detected: 85%");
    logger->error("Failed to connect to ML2 storage");
    
    // Logging with trace context
    std::cout << "\nDistributed tracing example:\n\n";
    
    {
        logging::TraceSpan span("migrate_dataset");
        span.addTag("dataset", "USER.DATA.FILE1");
        span.addTag("target", "ML2");
        
        logger->info("Starting dataset migration");
        
        {
            logging::TraceSpan child_span("compress_data");
            logger->debug("Compressing data block");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        {
            logging::TraceSpan child_span("write_tape");
            logger->debug("Writing to tape volume");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        std::cout << "Trace span completed in " << span.elapsedMs() << " ms\n";
    }
    
    // Trace analysis
    logging::TraceAnalyzer analyzer;
    
    logging::TraceRecord record;
    record.trace_id = logging::TraceContext::generateTraceId();
    record.operation = "migration";
    record.duration = std::chrono::milliseconds(150);
    record.status = logging::LogLevel::INFO;
    analyzer.recordTrace(record);
    
    auto stats = analyzer.calculateStats("migration");
    std::cout << "\nTrace Statistics:\n";
    std::cout << "  Total traces: " << stats.total_traces << "\n";
    std::cout << "  Avg duration: " << stats.avg_duration_ms << " ms\n";
    
    logger->flush();
    std::cout << "\n[OK] Logging demonstration complete\n";
}

// ============================================================================
// 2. Performance Benchmarking Example
// ============================================================================

void demonstrateBenchmarking() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  2. PERFORMANCE BENCHMARKING\n";
    std::cout << std::string(70, '=') << "\n\n";
    
    // Create benchmark suite
    auto suite = benchmark::createDefaultHSMBenchmarkSuite();
    
    std::cout << "Available benchmarks:\n";
    for (const auto& name : suite.getBenchmarkNames()) {
        std::cout << "  - " << name << "\n";
    }
    
    // Run individual benchmark
    std::cout << "\nRunning migration benchmark...\n";
    
    benchmark::HSMBenchmarks::MigrationBenchmarkConfig mig_config;
    mig_config.file_size = 1024 * 1024;  // 1 MB
    mig_config.file_count = 10;
    mig_config.use_compression = true;
    
    benchmark::BenchmarkRunner::Config runner_config;
    runner_config.iterations = 5;
    runner_config.warmup_iterations = 1;
    
    auto result = benchmark::HSMBenchmarks::benchmarkMigration(mig_config, runner_config);
    
    std::cout << "\nMigration Benchmark Results:\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  Iterations: " << result.iterations << "\n";
    std::cout << "  Total bytes: " << (result.total_bytes / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "  Timing:\n";
    std::cout << "    Mean:   " << result.time_stats_ms.mean << " ms\n";
    std::cout << "    Median: " << result.time_stats_ms.median << " ms\n";
    std::cout << "    P95:    " << result.time_stats_ms.p95 << " ms\n";
    std::cout << "    P99:    " << result.time_stats_ms.p99 << " ms\n";
    std::cout << "  Throughput: " << result.throughput_stats_mbps.mean << " MB/s\n";
    
    // Compression benchmark
    std::cout << "\nRunning compression benchmark...\n";
    
    benchmark::HSMBenchmarks::CompressionBenchmarkConfig comp_config;
    comp_config.block_size = 64 * 1024;
    comp_config.iterations = 100;
    comp_config.data_pattern = "text";
    
    auto comp_result = benchmark::HSMBenchmarks::benchmarkCompression(comp_config, runner_config);
    
    std::cout << "Compression Results:\n";
    std::cout << "  Mean time: " << comp_result.time_stats_ms.mean << " ms\n";
    std::cout << "  Throughput: " << comp_result.throughput_stats_mbps.mean << " MB/s\n";
    
    // Benchmark comparison
    std::cout << "\nComparing baseline vs current:\n";
    
    benchmark::BenchmarkResult baseline = result;
    baseline.time_stats_ms.mean *= 0.9;  // Simulate faster baseline
    
    benchmark::BenchmarkComparator comparator(10.0);  // 10% threshold
    auto comparisons = comparator.compare(baseline, result);
    
    for (const auto& comp : comparisons) {
        std::cout << "  " << comp.toString() << "\n";
    }
    
    std::cout << "\n[OK] Benchmarking demonstration complete\n";
}

// ============================================================================
// 3. Tape Pool Management Example
// ============================================================================

void demonstrateTapePoolManagement() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  3. TAPE POOL MANAGEMENT\n";
    std::cout << std::string(70, '=') << "\n\n";
    
    tapepool::TapePoolManager pool_manager;
    
    // Create tape pools
    tapepool::TapePool scratch_pool;
    scratch_pool.name = "SCRATCH01";
    scratch_pool.description = "Primary scratch pool";
    scratch_pool.default_media_type = tapepool::MediaType::LTO8;
    scratch_pool.auto_scratch_return = true;
    scratch_pool.default_retention_days = 30;
    scratch_pool.target_scratch_count = 100;
    
    pool_manager.createPool(scratch_pool);
    
    tapepool::TapePool backup_pool;
    backup_pool.name = "BACKUP01";
    backup_pool.description = "Backup tape pool";
    backup_pool.default_media_type = tapepool::MediaType::LTO8;
    backup_pool.default_retention_days = 90;
    
    pool_manager.createPool(backup_pool);
    
    std::cout << "Created tape pools:\n";
    for (const auto& name : pool_manager.getPoolNames()) {
        std::cout << "  - " << name << "\n";
    }
    
    // Generate test volumes
    std::cout << "\nGenerating test volumes...\n";
    pool_manager.generateTestVolumes("SCRATCH01", 50, tapepool::MediaType::LTO8);
    pool_manager.generateTestVolumes("BACKUP01", 20, tapepool::MediaType::LTO8);
    
    // Get pool statistics
    auto stats = pool_manager.getPoolStatistics("SCRATCH01");
    
    std::cout << "\nSCRATCH01 Pool Statistics:\n";
    std::cout << "  Total volumes: " << stats.total_volumes << "\n";
    std::cout << "  Scratch volumes: " << stats.scratch_volumes << "\n";
    std::cout << "  Private volumes: " << stats.private_volumes << "\n";
    std::cout << "  Total capacity: " << (stats.total_capacity_bytes / (1024.0 * 1024 * 1024 * 1024)) << " TB\n";
    std::cout << "  Utilization: " << std::fixed << std::setprecision(1) 
              << stats.utilization_percent << "%\n";
    
    // Allocate scratch volume
    std::cout << "\nAllocating scratch volume...\n";
    
    tapepool::AllocationRequest request;
    request.pool_name = "SCRATCH01";
    request.owner = "MIGRATION";
    request.space_needed = 5ULL * 1024 * 1024 * 1024;  // 5 GB
    request.retention_days = 30;
    
    auto result = pool_manager.allocateScratch(request);
    
    if (result.success) {
        std::cout << "  Allocated volume: " << result.volser << "\n";
        std::cout << "  Available space: " << (result.available_space / (1024.0 * 1024 * 1024)) << " GB\n";
    }
    
    // Check pool health
    std::cout << "\nPool Health Check:\n";
    auto health = pool_manager.checkPoolHealth("SCRATCH01");
    
    std::cout << "  Pool: " << health.pool_name << "\n";
    std::cout << "  Healthy: " << (health.healthy ? "YES" : "NO") << "\n";
    std::cout << "  Scratch %: " << std::fixed << std::setprecision(1) 
              << health.scratch_percentage << "%\n";
    
    for (const auto& warning : health.warnings) {
        std::cout << "  Warning: " << warning << "\n";
    }
    
    // Storage group management
    std::cout << "\nStorage Group Setup:\n";
    
    tapepool::StorageGroupManager sg_manager;
    
    tapepool::StorageGroup sg;
    sg.name = "HSMDATA";
    sg.description = "HSM migration data";
    sg.pool_names = {"SCRATCH01", "BACKUP01"};  // Priority order
    sg.overflow_to_next_pool = true;
    
    sg_manager.createGroup(sg);
    std::cout << "  Created storage group: " << sg.name << "\n";
    
    std::cout << "\n[OK] Tape pool management demonstration complete\n";
}

// ============================================================================
// 4. Report Generation Example
// ============================================================================

void demonstrateReportGeneration() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  4. REPORT GENERATION\n";
    std::cout << std::string(70, '=') << "\n\n";
    
    // Create data collector and add sample data
    auto collector = std::make_shared<reports::ReportDataCollector>();
    
    // Add migration records
    for (int i = 0; i < 50; ++i) {
        reports::MigrationRecord rec;
        rec.dataset_name = "USER.DATA.FILE" + std::to_string(i);
        rec.source_volume = "VOL001";
        rec.target_volume = "T0000" + std::to_string(i % 10);
        rec.bytes_migrated = (i + 1) * 1024 * 1024;
        rec.timestamp = std::chrono::system_clock::now() - std::chrono::hours(i);
        rec.duration = std::chrono::milliseconds(100 + i * 10);
        rec.compressed = true;
        rec.compression_ratio = 1.8;
        rec.success = (i % 10 != 7);  // Some failures
        rec.migration_level = (i % 2) + 1;
        
        collector->addMigration(rec);
    }
    
    // Add recall records
    for (int i = 0; i < 20; ++i) {
        reports::RecallRecord rec;
        rec.dataset_name = "USER.DATA.RECALL" + std::to_string(i);
        rec.source_volume = "T0000" + std::to_string(i % 5);
        rec.bytes_recalled = (i + 1) * 512 * 1024;
        rec.timestamp = std::chrono::system_clock::now() - std::chrono::hours(i * 2);
        rec.duration = std::chrono::milliseconds(500 + i * 50);
        rec.wait_time = std::chrono::milliseconds(2000 + i * 100);
        rec.success = true;
        
        collector->addRecall(rec);
    }
    
    // Create report generator
    reports::ReportGenerator generator(collector);
    
    // Generate migration report
    std::cout << "Migration Activity Report (Text):\n";
    std::cout << std::string(50, '-') << "\n";
    
    auto mig_report = generator.createMigrationReport();
    mig_report->setTimeRange(reports::ReportTimeRange::lastNDays(7));
    mig_report->setFormat(reports::ReportFormat::TEXT);
    
    std::cout << mig_report->generate() << "\n";
    
    // Generate JSON report
    std::cout << "Recall Activity Report (JSON excerpt):\n";
    std::cout << std::string(50, '-') << "\n";
    
    auto recall_report = generator.createRecallReport();
    recall_report->setTimeRange(reports::ReportTimeRange::lastNDays(7));
    recall_report->setFormat(reports::ReportFormat::JSON);
    
    std::string json = recall_report->generate();
    std::cout << json.substr(0, std::min(size_t(500), json.size())) << "...\n\n";
    
    // Generate HTML dashboard
    std::cout << "Dashboard Report (HTML excerpt):\n";
    std::cout << std::string(50, '-') << "\n";
    
    auto dashboard = generator.createDashboard();
    dashboard->setTimeRange(reports::ReportTimeRange::lastNDays(7));
    dashboard->setFormat(reports::ReportFormat::HTML);
    
    std::string html = dashboard->generate();
    std::cout << html.substr(0, std::min(size_t(400), html.size())) << "...\n";
    
    std::cout << "\n[OK] Report generation demonstration complete\n";
}

// ============================================================================
// 5. ABACKUP/ARECOVER Example
// ============================================================================

void demonstrateABackup() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  5. ABACKUP/ARECOVER EMULATION\n";
    std::cout << std::string(70, '=') << "\n\n";
    
    abackup::ABackupManager backup_manager;
    
    // Define aggregate group
    abackup::AggregateGroup group;
    group.name = "PROD_DATA";
    group.description = "Production data aggregate group";
    group.volume_patterns = {"VOL*", "USR*"};
    group.dataset_patterns = {"PROD.*", "USER.*"};
    group.default_backup_type = abackup::BackupType::FULL;
    group.retention_days = 30;
    group.versions_to_keep = 3;
    group.compress_backup = true;
    group.target_pool = "BACKUP01";
    
    backup_manager.defineGroup(group);
    
    std::cout << "Defined aggregate group: " << group.name << "\n";
    std::cout << "  Volume patterns: ";
    for (const auto& p : group.volume_patterns) std::cout << p << " ";
    std::cout << "\n  Retention: " << group.retention_days << " days\n";
    std::cout << "  Versions to keep: " << group.versions_to_keep << "\n\n";
    
    // Execute ABACKUP
    std::cout << "Executing ABACKUP...\n";
    
    abackup::ABackupOptions backup_opts;
    backup_opts.group_name = "PROD_DATA";
    backup_opts.type = abackup::BackupType::FULL;
    backup_opts.compress = true;
    backup_opts.verify_after = true;
    backup_opts.retention_days = 30;
    
    auto backup_result = backup_manager.executeABackup(backup_opts, 
        [](const std::string& msg, double pct) {
            std::cout << "  [" << std::fixed << std::setprecision(0) 
                      << pct << "%] " << msg << "\n";
        });
    
    if (backup_result.success) {
        std::cout << "\nBackup completed successfully!\n";
        std::cout << "  Backup ID: " << backup_result.backup_id << "\n";
        std::cout << "  Data processed: " << (backup_result.bytes_processed / (1024.0 * 1024.0)) << " MB\n";
        std::cout << "  Data written: " << (backup_result.bytes_written / (1024.0 * 1024.0)) << " MB\n";
        std::cout << "  Duration: " << backup_result.duration.count() << " seconds\n";
    }
    
    // List backups
    std::cout << "\nBackup catalog for " << group.name << ":\n";
    auto backups = backup_manager.listBackups("PROD_DATA");
    for (const auto& b : backups) {
        std::cout << "  " << b.toSummary() << "\n";
    }
    
    // Execute ARECOVER (validation only)
    std::cout << "\nExecuting ARECOVER (validation only)...\n";
    
    abackup::ARecoverOptions recover_opts;
    recover_opts.backup_id = backup_result.backup_id;
    recover_opts.verify_only = true;
    
    auto recover_result = backup_manager.executeARecover(recover_opts,
        [](const std::string& msg, double pct) {
            std::cout << "  [" << std::fixed << std::setprecision(0) 
                      << pct << "%] " << msg << "\n";
        });
    
    std::cout << "\nRecovery validation: " 
              << (recover_result.validation_passed ? "PASSED" : "FAILED") << "\n";
    
    // Catalog statistics
    auto catalog = backup_manager.getCatalog();
    auto stats = catalog->getStatistics();
    
    std::cout << "\nBackup Catalog Statistics:\n";
    std::cout << "  Total backups: " << stats.total_backups << "\n";
    std::cout << "  Completed: " << stats.completed_backups << "\n";
    std::cout << "  Total size: " << (stats.total_backup_bytes / (1024.0 * 1024.0)) << " MB\n";
    
    std::cout << "\n[OK] ABACKUP/ARECOVER demonstration complete\n";
}

// ============================================================================
// 6. Disaster Recovery Example
// ============================================================================

void demonstrateDisasterRecovery() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  6. DISASTER RECOVERY\n";
    std::cout << std::string(70, '=') << "\n\n";
    
    dr::DisasterRecoveryManager dr_manager;
    
    // Define primary site
    dr::Site primary;
    primary.site_id = "SITE_NYC";
    primary.name = "New York Data Center";
    primary.location = "New York, NY";
    primary.role = dr::SiteRole::PRIMARY;
    primary.status = dr::SiteStatus::ONLINE;
    primary.total_capacity = 100ULL * 1024 * 1024 * 1024 * 1024;  // 100 TB
    primary.used_capacity = 60ULL * 1024 * 1024 * 1024 * 1024;    // 60 TB
    primary.last_heartbeat = std::chrono::system_clock::now();
    
    dr_manager.registerSite(primary);
    
    // Define secondary site
    dr::Site secondary;
    secondary.site_id = "SITE_CHI";
    secondary.name = "Chicago Data Center";
    secondary.location = "Chicago, IL";
    secondary.role = dr::SiteRole::SECONDARY;
    secondary.status = dr::SiteStatus::ONLINE;
    secondary.total_capacity = 100ULL * 1024 * 1024 * 1024 * 1024;
    secondary.used_capacity = 60ULL * 1024 * 1024 * 1024 * 1024;
    secondary.last_heartbeat = std::chrono::system_clock::now();
    
    dr_manager.registerSite(secondary);
    
    std::cout << "Configured DR Sites:\n";
    for (const auto& site : dr_manager.listSites()) {
        std::cout << "  " << site.toSummary() << "\n";
    }
    
    // Create replication link
    dr::ReplicationLink link;
    link.link_id = "REPL_NYC_CHI";
    link.source_site = "SITE_NYC";
    link.target_site = "SITE_CHI";
    link.mode = dr::ReplicationMode::SYNCHRONOUS;
    link.state = dr::ReplicationState::ACTIVE;
    link.replication_lag = std::chrono::milliseconds(50);
    link.bytes_replicated = 60ULL * 1024 * 1024 * 1024 * 1024;
    link.transactions_replicated = 1000000;
    
    dr_manager.createReplicationLink(link);
    
    std::cout << "\nReplication Link:\n";
    std::cout << "  " << link.link_id << ": " 
              << link.source_site << " -> " << link.target_site << "\n";
    std::cout << "  Mode: " << dr::replicationModeToString(link.mode) << "\n";
    std::cout << "  Lag: " << link.replication_lag.count() << " ms\n";
    
    // Create recovery points
    std::cout << "\nCreating recovery points...\n";
    
    auto rp1 = dr_manager.createRecoveryPoint("SITE_CHI", "Hourly checkpoint");
    auto rp2 = dr_manager.createRecoveryPoint("SITE_CHI", "Daily backup point");
    
    auto points = dr_manager.getRecoveryPointsForSite("SITE_CHI");
    std::cout << "Recovery Points:\n";
    for (const auto& pt : points) {
        std::cout << "  " << pt.point_id << " - " << pt.description << "\n";
    }
    
    // Run DR test
    std::cout << "\nRunning DR Test...\n";
    auto test_result = dr_manager.runDRTest("SITE_CHI");
    
    std::cout << "DR Test Result:\n";
    std::cout << "  Test ID: " << test_result.test_id << "\n";
    std::cout << "  Success: " << (test_result.success ? "YES" : "NO") << "\n";
    std::cout << "  RPO Validated: " << (test_result.rpo_validated ? "YES" : "NO") << "\n";
    std::cout << "  RTO Validated: " << (test_result.rto_validated ? "YES" : "NO") << "\n";
    
    if (!test_result.issues_found.empty()) {
        std::cout << "  Issues:\n";
        for (const auto& issue : test_result.issues_found) {
            std::cout << "    - " << issue << "\n";
        }
    }
    
    // Get DR status
    auto primary_site = dr_manager.getPrimarySite();
    auto sites = dr_manager.listSites();
    
    std::cout << "\nDR Status:\n";
    std::cout << "  Primary Site: " << (primary_site ? primary_site->site_id : "None") << "\n";
    std::cout << "  Total Sites: " << sites.size() << "\n";
    
    // Simulate failover
    std::cout << "\nSimulating planned failover...\n";
    
    dr::DisasterRecoveryManager::FailoverOptions fo_options;
    fo_options.target_site = "SITE_CHI";
    fo_options.force = true;  // Force for demo
    fo_options.type = dr::FailoverEvent::Type::PLANNED;
    fo_options.initiated_by = "DR_TEST";
    
    auto fo_result = dr_manager.initiateFailover(fo_options);
    
    if (fo_result.success) {
        std::cout << "  Failover successful!\n";
        std::cout << "  Event ID: " << fo_result.event_id << "\n";
    } else {
        std::cout << "  Failover status: " << fo_result.error_message << "\n";
    }
    
    std::cout << "\n[OK] Disaster recovery demonstration complete\n";
}

// ============================================================================
// Main Program
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "+======================================================================+\n";
    std::cout << "|        DFSMShsm Emulator v3.5.0 - Feature Demonstration              |\n";
    std::cout << "+======================================================================+\n";
    
    try {
        demonstrateLogging();
        demonstrateBenchmarking();
        demonstrateTapePoolManagement();
        demonstrateReportGeneration();
        demonstrateABackup();
        demonstrateDisasterRecovery();
        
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "  ALL DEMONSTRATIONS COMPLETED SUCCESSFULLY\n";
        std::cout << std::string(70, '=') << "\n\n";
        
        std::cout << "v3.5.0 Features Summary:\n";
        std::cout << "  1. Enhanced Logging - Structured logging with trace correlation\n";
        std::cout << "  2. Benchmarking - Performance measurement and comparison\n";
        std::cout << "  3. Tape Pools - Scratch allocation and capacity management\n";
        std::cout << "  4. Reports - Activity reports in multiple formats\n";
        std::cout << "  5. ABACKUP - Aggregate backup and recovery\n";
        std::cout << "  6. Disaster Recovery - Site failover and replication\n";
        std::cout << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
