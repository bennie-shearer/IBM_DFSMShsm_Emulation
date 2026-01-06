/*******************************************************************************
 * DFSMShsm Emulator - v3.5.0 Feature Tests
 * 
 * Tests for:
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
#include <cassert>
#include <sstream>
#include <thread>
#include <chrono>

#include "hsm_logging.hpp"
#include "hsm_benchmark.hpp"
#include "hsm_tape_pool.hpp"
#include "hsm_reports.hpp"
#include "hsm_abackup.hpp"
#include "hsm_disaster_recovery.hpp"

using namespace dfsmshsm;

int tests_run = 0;
int tests_passed = 0;

#define TEST(name) \
    void test_##name(); \
    struct test_##name##_runner { \
        test_##name##_runner() { \
            std::cout << "Running: " #name << "... "; \
            tests_run++; \
            try { \
                test_##name(); \
                tests_passed++; \
                std::cout << "PASSED" << std::endl; \
            } catch (const std::exception& e) { \
                std::cout << "FAILED: " << e.what() << std::endl; \
            } \
        } \
    } test_##name##_instance; \
    void test_##name()

#define ASSERT(cond) \
    if (!(cond)) throw std::runtime_error("Assertion failed: " #cond)

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " == " #b)

// ============================================================================
// Logging Tests
// ============================================================================

TEST(logging_levels) {
    using namespace logging;
    
    ASSERT_EQ(logLevelToString(LogLevel::TRACE), "TRACE");
    ASSERT_EQ(logLevelToString(LogLevel::DEBUG), "DEBUG");
    ASSERT_EQ(logLevelToString(LogLevel::INFO), "INFO");
    ASSERT_EQ(logLevelToString(LogLevel::WARN), "WARN");
    ASSERT_EQ(logLevelToString(LogLevel::ERROR), "ERROR");
    ASSERT_EQ(logLevelToString(LogLevel::FATAL), "FATAL");
    
    ASSERT_EQ(stringToLogLevel("TRACE"), LogLevel::TRACE);
    ASSERT_EQ(stringToLogLevel("DEBUG"), LogLevel::DEBUG);
    ASSERT_EQ(stringToLogLevel("INFO"), LogLevel::INFO);
}

TEST(logging_entry_format) {
    using namespace logging;
    
    LogEntry entry;
    entry.level = LogLevel::INFO;
    entry.logger_name = "test";
    entry.message = "Test message";
    entry.sequence_number = 1;
    
    std::string text = entry.formatPlainText();
    ASSERT(text.find("[INFO]") != std::string::npos);
    ASSERT(text.find("[test]") != std::string::npos);
    ASSERT(text.find("Test message") != std::string::npos);
    
    std::string json = entry.formatJSON();
    ASSERT(json.find("\"level\":\"INFO\"") != std::string::npos);
    ASSERT(json.find("\"message\":\"Test message\"") != std::string::npos);
}

TEST(logging_console_sink) {
    using namespace logging;
    
    auto sink = std::make_shared<ConsoleSink>();
    sink->setMinLevel(LogLevel::WARN);
    
    ASSERT_EQ(sink->getMinLevel(), LogLevel::WARN);
    
    LogEntry entry;
    entry.level = LogLevel::INFO;
    entry.message = "Should be filtered";
    sink->write(entry);  // Should not output (below threshold)
}

TEST(logging_logger) {
    using namespace logging;
    
    auto logger = std::make_shared<Logger>("test_logger");
    logger->setLevel(LogLevel::DEBUG);
    
    ASSERT_EQ(logger->getName(), "test_logger");
    ASSERT_EQ(logger->getLevel(), LogLevel::DEBUG);
    
    // Add a callback sink to capture output
    std::vector<LogEntry> captured;
    auto callback_sink = std::make_shared<CallbackSink>(
        [&captured](const LogEntry& entry) {
            captured.push_back(entry);
        });
    logger->addSink(callback_sink);
    
    logger->info("Test info");
    logger->warn("Test warn");
    
    ASSERT_EQ(captured.size(), 2);
    ASSERT_EQ(captured[0].level, LogLevel::INFO);
    ASSERT_EQ(captured[1].level, LogLevel::WARN);
}

TEST(logging_trace_context) {
    using namespace logging;
    
    std::string trace_id = TraceContext::generateTraceId();
    ASSERT_EQ(trace_id.length(), 32);  // 16 bytes = 32 hex chars
    
    std::string span_id = TraceContext::generateSpanId();
    ASSERT_EQ(span_id.length(), 16);  // 8 bytes = 16 hex chars
    
    TraceContext::setCurrentTraceId("test_trace_123");
    ASSERT_EQ(TraceContext::getCurrentTraceId(), "test_trace_123");
    
    TraceContext::clear();
    ASSERT(TraceContext::getCurrentTraceId().empty());
}

TEST(logging_trace_span) {
    using namespace logging;
    
    {
        TraceSpan span("outer_operation");
        ASSERT_EQ(span.getName(), "outer_operation");
        ASSERT(!span.getSpanId().empty());
        
        std::string outer_span_id = span.getSpanId();
        
        {
            TraceSpan inner("inner_operation");
            ASSERT(!inner.getSpanId().empty());
            ASSERT(inner.getSpanId() != outer_span_id);
        }
    }
}

TEST(logging_log_manager) {
    using namespace logging;
    
    auto& manager = LogManager::instance();
    
    auto logger1 = manager.getLogger("service1");
    auto logger2 = manager.getLogger("service2");
    auto logger1_again = manager.getLogger("service1");
    
    ASSERT(logger1.get() == logger1_again.get());  // Same instance
    ASSERT(logger1.get() != logger2.get());
}

// ============================================================================
// Benchmark Tests
// ============================================================================

TEST(benchmark_scoped_timer) {
    using namespace benchmark;
    
    ScopedTimer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    timer.stop();
    
    // Lenient bounds for cross-platform timing
    ASSERT(timer.elapsedMs() >= 5);
    ASSERT(timer.elapsedMs() < 150);  // Sanity check
}

TEST(benchmark_statistics) {
    using namespace benchmark;
    
    std::vector<double> values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto stats = Statistics::calculate(values);
    
    ASSERT_EQ(stats.count, 10);
    ASSERT_EQ(stats.min, 1.0);
    ASSERT_EQ(stats.max, 10.0);
    ASSERT(std::abs(stats.mean - 5.5) < 0.01);
    ASSERT(std::abs(stats.median - 5.5) < 0.01);
    ASSERT_EQ(stats.sum, 55.0);
}

TEST(benchmark_sample) {
    using namespace benchmark;
    
    BenchmarkSample sample;
    sample.elapsed_time = std::chrono::seconds(1);
    sample.bytes_processed = 10 * 1024 * 1024;  // 10 MB
    sample.operations_count = 100;
    
    ASSERT(std::abs(sample.throughputMBps() - 10.0) < 0.1);
    ASSERT_EQ(sample.operationsPerSecond(), 100.0);
}

TEST(benchmark_runner_simple) {
    using namespace benchmark;
    
    BenchmarkRunner runner;
    BenchmarkRunner::Config config;
    config.iterations = 5;
    config.warmup_iterations = 1;
    runner.setConfig(config);
    
    int call_count = 0;
    auto result = runner.runTimed("simple_test", [&call_count]() {
        call_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });
    
    ASSERT_EQ(result.name, "simple_test");
    ASSERT_EQ(result.iterations, 5);
    ASSERT_EQ(call_count, 6);  // 1 warmup + 5 iterations
    ASSERT(result.time_stats_ms.mean > 0);
}

TEST(benchmark_result_export) {
    using namespace benchmark;
    
    BenchmarkResult result;
    result.name = "test_benchmark";
    result.iterations = 10;
    result.time_stats_ms.mean = 5.5;
    result.time_stats_ms.min = 4.0;
    result.time_stats_ms.max = 7.0;
    result.throughput_stats_mbps.mean = 100.0;
    
    std::string json = result.toJSON();
    ASSERT(json.find("\"name\": \"test_benchmark\"") != std::string::npos);
    ASSERT(json.find("\"iterations\": 10") != std::string::npos);
    
    std::string csv = result.toCSVRow();
    ASSERT(csv.find("test_benchmark") != std::string::npos);
}

TEST(benchmark_suite) {
    using namespace benchmark;
    
    BenchmarkSuite suite;
    
    suite.addBenchmark("fast", []() {
        BenchmarkResult r;
        r.name = "fast";
        r.iterations = 1;
        return r;
    });
    
    suite.addBenchmark("slow", []() {
        BenchmarkResult r;
        r.name = "slow";
        r.iterations = 2;
        return r;
    });
    
    auto names = suite.getBenchmarkNames();
    ASSERT_EQ(names.size(), 2);
    
    auto results = suite.runAll();
    ASSERT_EQ(results.size(), 2);
    ASSERT_EQ(results["fast"].name, "fast");
    ASSERT_EQ(results["slow"].name, "slow");
}

// ============================================================================
// Tape Pool Tests
// ============================================================================

TEST(tape_pool_volume_status) {
    using namespace tapepool;
    
    ASSERT_EQ(volumeStatusToString(VolumeStatus::SCRATCH), "SCRATCH");
    ASSERT_EQ(volumeStatusToString(VolumeStatus::PRIVATE), "PRIVATE");
    ASSERT_EQ(volumeStatusToString(VolumeStatus::FULL), "FULL");
}

TEST(tape_pool_media_type) {
    using namespace tapepool;
    
    ASSERT_EQ(mediaTypeToString(MediaType::LTO8), "LTO8");
    ASSERT(getMediaCapacity(MediaType::LTO8) > 0);
    ASSERT(getMediaCapacity(MediaType::LTO9) > getMediaCapacity(MediaType::LTO8));
}

TEST(tape_pool_volume) {
    using namespace tapepool;
    
    TapeVolume vol;
    vol.volser = "T00001";
    vol.media_type = MediaType::LTO8;
    vol.status = VolumeStatus::SCRATCH;
    vol.capacity = 12000ULL * 1024 * 1024 * 1024;
    vol.used_space = 0;
    vol.available_space = vol.capacity;
    
    ASSERT(vol.isScratch());
    ASSERT(vol.isAvailable());
    ASSERT(vol.utilizationPercent() == 0);
    
    vol.used_space = vol.capacity / 2;
    ASSERT(std::abs(vol.utilizationPercent() - 50.0) < 0.1);
}

TEST(tape_pool_manager_create_pool) {
    using namespace tapepool;
    
    TapePoolManager manager;
    
    TapePool pool;
    pool.name = "SCRATCH1";
    pool.description = "Primary scratch pool";
    pool.default_media_type = MediaType::LTO8;
    pool.target_scratch_count = 100;
    
    manager.createPool(pool);
    
    auto retrieved = manager.getPool("SCRATCH1");
    ASSERT(retrieved.has_value());
    ASSERT_EQ(retrieved->name, "SCRATCH1");
    ASSERT_EQ(retrieved->default_media_type, MediaType::LTO8);
}

TEST(tape_pool_manager_add_volume) {
    using namespace tapepool;
    
    TapePoolManager manager;
    
    TapePool pool;
    pool.name = "TEST_POOL";
    manager.createPool(pool);
    
    TapeVolume vol;
    vol.volser = "VOL001";
    vol.pool_name = "TEST_POOL";
    vol.status = VolumeStatus::SCRATCH;
    vol.capacity = 1000000;
    vol.available_space = 1000000;
    
    manager.addVolume(vol);
    
    auto retrieved = manager.getVolume("VOL001");
    ASSERT(retrieved.has_value());
    ASSERT_EQ(retrieved->volser, "VOL001");
    
    auto pool_vols = manager.getPoolVolumes("TEST_POOL");
    ASSERT_EQ(pool_vols.size(), 1);
}

TEST(tape_pool_allocation) {
    using namespace tapepool;
    
    TapePoolManager manager;
    
    TapePool pool;
    pool.name = "ALLOC_POOL";
    manager.createPool(pool);
    
    // Add scratch volumes
    manager.generateTestVolumes("ALLOC_POOL", 10, MediaType::LTO8);
    
    auto stats = manager.getPoolStatistics("ALLOC_POOL");
    ASSERT_EQ(stats.scratch_volumes, 10);
    
    // Allocate
    AllocationRequest request;
    request.pool_name = "ALLOC_POOL";
    request.owner = "USER1";
    request.retention_days = 30;
    
    auto result = manager.allocateScratch(request);
    ASSERT(result.success);
    ASSERT(!result.volser.empty());
    
    // Check volume status changed
    auto vol = manager.getVolume(result.volser);
    ASSERT(vol.has_value());
    ASSERT_EQ(vol->status, VolumeStatus::FILLING);
    ASSERT_EQ(vol->owner, "USER1");
}

TEST(tape_pool_return_to_scratch) {
    using namespace tapepool;
    
    TapePoolManager manager;
    
    TapePool pool;
    pool.name = "RETURN_POOL";
    manager.createPool(pool);
    manager.generateTestVolumes("RETURN_POOL", 5, MediaType::LTO8);
    
    // Allocate then return
    AllocationRequest request;
    request.pool_name = "RETURN_POOL";
    auto result = manager.allocateScratch(request);
    ASSERT(result.success);
    
    bool returned = manager.returnToScratch(result.volser);
    ASSERT(returned);
    
    auto vol = manager.getVolume(result.volser);
    ASSERT_EQ(vol->status, VolumeStatus::SCRATCH);
}

TEST(tape_pool_health_check) {
    using namespace tapepool;
    
    TapePoolManager manager;
    
    TapePool pool;
    pool.name = "HEALTH_POOL";
    pool.thresholds.min_scratch_count = 5;
    manager.createPool(pool);
    manager.generateTestVolumes("HEALTH_POOL", 10, MediaType::LTO8);
    
    auto health = manager.checkPoolHealth("HEALTH_POOL");
    ASSERT(health.healthy);
    ASSERT(health.warnings.empty());
}

// ============================================================================
// Report Tests
// ============================================================================

TEST(reports_time_range) {
    using namespace reports;
    
    auto today = ReportTimeRange::today();
    ASSERT(today.start < today.end);
    ASSERT_EQ(today.period, ReportPeriod::DAILY);
    
    auto week = ReportTimeRange::thisWeek();
    ASSERT(week.start < week.end);
}

TEST(reports_data_collector) {
    using namespace reports;
    
    ReportDataCollector collector;
    
    MigrationRecord mig;
    mig.dataset_name = "TEST.DATA";
    mig.bytes_migrated = 1000000;
    mig.timestamp = std::chrono::system_clock::now();
    mig.success = true;
    collector.addMigration(mig);
    
    auto range = ReportTimeRange::today();
    auto migrations = collector.getMigrations(range);
    ASSERT_EQ(migrations.size(), 1);
    ASSERT_EQ(migrations[0].dataset_name, "TEST.DATA");
}

TEST(reports_migration_statistics) {
    using namespace reports;
    
    std::vector<MigrationRecord> records;
    for (int i = 0; i < 10; ++i) {
        MigrationRecord r;
        r.bytes_migrated = 1000 * (i + 1);
        r.duration = std::chrono::milliseconds(100 * (i + 1));
        r.success = (i < 8);  // 8 success, 2 failures
        r.compressed = true;
        r.compression_ratio = 2.0;
        r.migration_level = (i % 2 == 0) ? 1 : 2;
        records.push_back(r);
    }
    
    auto stats = MigrationStatistics::calculate(records);
    ASSERT_EQ(stats.total_count, 10);
    ASSERT_EQ(stats.success_count, 8);
    ASSERT_EQ(stats.failure_count, 2);
    ASSERT(stats.total_bytes == 55000);  // Sum 1000+2000+...+10000
}

TEST(reports_migration_report_text) {
    using namespace reports;
    
    auto collector = std::make_shared<ReportDataCollector>();
    
    MigrationRecord mig;
    mig.dataset_name = "SYS1.LINKLIB";
    mig.bytes_migrated = 50000000;
    mig.timestamp = std::chrono::system_clock::now();
    mig.duration = std::chrono::milliseconds(500);
    mig.success = true;
    collector->addMigration(mig);
    
    MigrationActivityReport report(*collector);
    report.setTimeRange(ReportTimeRange::today());
    report.setFormat(ReportFormat::TEXT);
    
    std::string output = report.generate();
    ASSERT(output.find("Migration Activity Report") != std::string::npos);
    ASSERT(output.find("Total Migrations") != std::string::npos);
}

TEST(reports_migration_report_json) {
    using namespace reports;
    
    auto collector = std::make_shared<ReportDataCollector>();
    
    MigrationRecord mig;
    mig.dataset_name = "USER.DATA";
    mig.bytes_migrated = 1000000;
    mig.timestamp = std::chrono::system_clock::now();
    mig.success = true;
    collector->addMigration(mig);
    
    MigrationActivityReport report(*collector);
    report.setFormat(ReportFormat::JSON);
    
    std::string json = report.generate();
    ASSERT(json.find("\"title\"") != std::string::npos);
    ASSERT(json.find("\"statistics\"") != std::string::npos);
    ASSERT(json.find("\"records\"") != std::string::npos);
}

TEST(reports_dashboard) {
    using namespace reports;
    
    auto collector = std::make_shared<ReportDataCollector>();
    
    // Add some test data
    for (int i = 0; i < 5; ++i) {
        MigrationRecord mig;
        mig.bytes_migrated = 1000000;
        mig.timestamp = std::chrono::system_clock::now();
        mig.success = true;
        collector->addMigration(mig);
        
        RecallRecord rec;
        rec.bytes_recalled = 500000;
        rec.timestamp = std::chrono::system_clock::now();
        rec.success = true;
        collector->addRecall(rec);
    }
    
    DashboardReport dashboard(*collector);
    dashboard.setFormat(ReportFormat::TEXT);
    
    std::string output = dashboard.generate();
    ASSERT(output.find("MIGRATION ACTIVITY") != std::string::npos);
    ASSERT(output.find("RECALL ACTIVITY") != std::string::npos);
}

TEST(reports_generator) {
    using namespace reports;
    
    auto collector = std::make_shared<ReportDataCollector>();
    ReportGenerator generator(collector);
    
    auto mig_report = generator.createMigrationReport();
    ASSERT(mig_report != nullptr);
    
    auto recall_report = generator.createRecallReport();
    ASSERT(recall_report != nullptr);
    
    auto storage_report = generator.createStorageReport();
    ASSERT(storage_report != nullptr);
    
    auto dashboard = generator.createDashboard();
    ASSERT(dashboard != nullptr);
}

// ============================================================================
// ABACKUP/ARECOVER Tests
// ============================================================================

TEST(abackup_aggregate_group) {
    using namespace abackup;
    
    AggregateGroup group;
    group.name = "PRODGRP";
    group.volume_patterns.push_back("PROD*");
    group.dataset_patterns.push_back("SYS1.*");
    group.default_backup_type = BackupType::FULL;
    group.retention_days = 30;
    
    ASSERT(group.matchesVolume("PROD01"));
    ASSERT(group.matchesVolume("PROD99"));
    ASSERT(!group.matchesVolume("TEST01"));
    
    ASSERT(group.matchesDataset("SYS1.LINKLIB"));
    ASSERT(!group.matchesDataset("USER.DATA"));
}

TEST(abackup_backup_version) {
    using namespace abackup;
    
    BackupVersion backup;
    backup.backup_id = "BK001";
    backup.type = BackupType::FULL;
    backup.status = BackupStatus::COMPLETED;
    backup.total_bytes = 1000000;
    backup.compressed_bytes = 500000;
    backup.started = std::chrono::system_clock::now() - std::chrono::hours(1);
    backup.completed = std::chrono::system_clock::now();
    backup.expiration = std::chrono::system_clock::now() + std::chrono::hours(24 * 30);
    
    ASSERT(std::abs(backup.compressionRatio() - 2.0) < 0.01);
    ASSERT(!backup.isExpired());
    ASSERT(backup.duration().count() > 0);
}

TEST(abackup_catalog) {
    using namespace abackup;
    
    BackupCatalog catalog;
    
    BackupVersion backup;
    backup.backup_id = "CAT_BK001";
    backup.group_name = "TEST_GROUP";
    backup.type = BackupType::FULL;
    backup.status = BackupStatus::COMPLETED;
    backup.version_number = 1;
    
    catalog.addBackup(backup);
    
    auto retrieved = catalog.getBackup("CAT_BK001");
    ASSERT(retrieved.has_value());
    ASSERT_EQ(retrieved->group_name, "TEST_GROUP");
    
    auto group_backups = catalog.getGroupBackups("TEST_GROUP");
    ASSERT_EQ(group_backups.size(), 1);
    
    auto latest = catalog.getLatestBackup("TEST_GROUP");
    ASSERT(latest.has_value());
    ASSERT_EQ(latest->backup_id, "CAT_BK001");
}

TEST(abackup_manager_define_group) {
    using namespace abackup;
    
    ABackupManager manager;
    
    AggregateGroup group;
    group.name = "DAILY_BACKUP";
    group.volume_patterns.push_back("DASD*");
    group.retention_days = 14;
    
    manager.defineGroup(group);
    
    auto retrieved = manager.getGroup("DAILY_BACKUP");
    ASSERT(retrieved.has_value());
    ASSERT_EQ(retrieved->name, "DAILY_BACKUP");
    
    auto groups = manager.listGroups();
    ASSERT_EQ(groups.size(), 1);
}

TEST(abackup_execute_backup) {
    using namespace abackup;
    
    ABackupManager manager;
    
    AggregateGroup group;
    group.name = "BACKUP_TEST";
    manager.defineGroup(group);
    
    ABackupOptions options;
    options.group_name = "BACKUP_TEST";
    options.type = BackupType::FULL;
    options.compress = true;
    options.retention_days = 7;
    
    auto result = manager.executeABackup(options);
    ASSERT(result.success);
    ASSERT(!result.backup_id.empty());
    ASSERT(result.bytes_processed > 0);
    
    // Verify backup in catalog
    auto backup = manager.getBackupInfo(result.backup_id);
    ASSERT(backup.has_value());
    ASSERT_EQ(backup->status, BackupStatus::COMPLETED);
}

TEST(abackup_incremental_chain) {
    using namespace abackup;
    
    ABackupManager manager;
    
    AggregateGroup group;
    group.name = "INCR_TEST";
    manager.defineGroup(group);
    
    // Create full backup
    ABackupOptions full_opts;
    full_opts.group_name = "INCR_TEST";
    full_opts.type = BackupType::FULL;
    auto full_result = manager.executeABackup(full_opts);
    ASSERT(full_result.success);
    
    // Create incremental backup
    ABackupOptions incr_opts;
    incr_opts.group_name = "INCR_TEST";
    incr_opts.type = BackupType::INCREMENTAL;
    auto incr_result = manager.executeABackup(incr_opts);
    ASSERT(incr_result.success);
    
    // Check chain
    auto backups = manager.listBackups("INCR_TEST");
    ASSERT_EQ(backups.size(), 2);
    ASSERT_EQ(backups[0].version_number, 2);  // Newest first
    ASSERT_EQ(backups[1].version_number, 1);
}

TEST(abackup_recovery) {
    using namespace abackup;
    
    ABackupManager manager;
    
    AggregateGroup group;
    group.name = "RECOVER_TEST";
    manager.defineGroup(group);
    
    // Create backup
    ABackupOptions backup_opts;
    backup_opts.group_name = "RECOVER_TEST";
    auto backup_result = manager.executeABackup(backup_opts);
    ASSERT(backup_result.success);
    
    // Recover
    ARecoverOptions recover_opts;
    recover_opts.backup_id = backup_result.backup_id;
    recover_opts.verify_only = false;
    
    auto recover_result = manager.executeARecover(recover_opts);
    ASSERT(recover_result.success);
    ASSERT(recover_result.validation_passed);
}

// ============================================================================
// Disaster Recovery Tests
// ============================================================================

TEST(dr_site_roles) {
    using namespace dr;
    
    ASSERT_EQ(siteRoleToString(SiteRole::PRIMARY), "PRIMARY");
    ASSERT_EQ(siteRoleToString(SiteRole::SECONDARY), "SECONDARY");
    ASSERT_EQ(siteStatusToString(SiteStatus::ONLINE), "ONLINE");
}

TEST(dr_site_definition) {
    using namespace dr;
    
    Site site;
    site.site_id = "NYC";
    site.name = "New York Data Center";
    site.role = SiteRole::PRIMARY;
    site.status = SiteStatus::ONLINE;
    site.total_capacity = 1000000000;
    site.used_capacity = 500000000;
    site.cpu_utilization = 50;
    site.memory_utilization = 60;
    site.storage_utilization = 50;
    
    ASSERT(site.isHealthy());
    ASSERT(!site.canAcceptFailover());  // Primary can't accept failover
    ASSERT(site.availableCapacity() > 0);
}

TEST(dr_manager_register_sites) {
    using namespace dr;
    
    DisasterRecoveryManager manager;
    
    Site primary;
    primary.site_id = "SITE_A";
    primary.name = "Primary Site";
    primary.role = SiteRole::PRIMARY;
    primary.status = SiteStatus::ONLINE;
    manager.registerSite(primary);
    
    Site secondary;
    secondary.site_id = "SITE_B";
    secondary.name = "DR Site";
    secondary.role = SiteRole::SECONDARY;
    secondary.status = SiteStatus::ONLINE;
    secondary.total_capacity = 1000000;
    manager.registerSite(secondary);
    
    auto sites = manager.listSites();
    ASSERT_EQ(sites.size(), 2);
    
    auto primary_site = manager.getPrimarySite();
    ASSERT(primary_site.has_value());
    ASSERT_EQ(primary_site->site_id, "SITE_A");
}

TEST(dr_replication_link) {
    using namespace dr;
    
    DisasterRecoveryManager manager;
    
    Site primary;
    primary.site_id = "PRI";
    primary.role = SiteRole::PRIMARY;
    primary.status = SiteStatus::ONLINE;
    manager.registerSite(primary);
    
    Site secondary;
    secondary.site_id = "SEC";
    secondary.role = SiteRole::SECONDARY;
    secondary.status = SiteStatus::ONLINE;
    manager.registerSite(secondary);
    
    ReplicationLink link;
    link.link_id = "LINK_PRI_SEC";
    link.source_site = "PRI";
    link.target_site = "SEC";
    link.mode = ReplicationMode::ASYNCHRONOUS;
    link.state = ReplicationState::ACTIVE;
    
    manager.createReplicationLink(link);
    
    auto retrieved = manager.getReplicationLink("LINK_PRI_SEC");
    ASSERT(retrieved.has_value());
    ASSERT_EQ(retrieved->source_site, "PRI");
    ASSERT(retrieved->isHealthy());
}

TEST(dr_recovery_point) {
    using namespace dr;
    
    DisasterRecoveryManager manager;
    
    Site site;
    site.site_id = "RP_SITE";
    site.role = SiteRole::PRIMARY;
    manager.registerSite(site);
    
    std::string rp_id = manager.createRecoveryPoint("RP_SITE", "Test recovery point");
    ASSERT(!rp_id.empty());
    
    auto rp = manager.getRecoveryPoint(rp_id);
    ASSERT(rp.has_value());
    ASSERT_EQ(rp->site_id, "RP_SITE");
    ASSERT(!rp->isExpired());
    
    auto site_rps = manager.getRecoveryPointsForSite("RP_SITE");
    ASSERT_EQ(site_rps.size(), 1);
}

TEST(dr_failover) {
    using namespace dr;
    
    DisasterRecoveryManager manager;
    
    // Setup sites
    Site primary;
    primary.site_id = "MAIN";
    primary.role = SiteRole::PRIMARY;
    primary.status = SiteStatus::ONLINE;
    primary.objectives.rpo = std::chrono::hours(1);
    primary.objectives.rto = std::chrono::hours(4);
    manager.registerSite(primary);
    
    Site secondary;
    secondary.site_id = "DR";
    secondary.role = SiteRole::SECONDARY;
    secondary.status = SiteStatus::ONLINE;
    secondary.total_capacity = 1000000;
    secondary.total_capacity = 900000;
    manager.registerSite(secondary);
    
    // Create replication link
    ReplicationLink link;
    link.link_id = "MAIN_DR";
    link.source_site = "MAIN";
    link.target_site = "DR";
    link.state = ReplicationState::ACTIVE;
    link.replication_lag = std::chrono::milliseconds(100);
    manager.createReplicationLink(link);
    
    // Execute failover
    DisasterRecoveryManager::FailoverOptions options;
    options.target_site = "DR";
    options.type = FailoverEvent::Type::PLANNED;
    options.initiated_by = "test";
    
    auto result = manager.initiateFailover(options);
    ASSERT(result.success);
    ASSERT(!result.event_id.empty());
    
    // Verify roles switched
    auto new_primary = manager.getPrimarySite();
    ASSERT(new_primary.has_value());
    ASSERT_EQ(new_primary->site_id, "DR");
}

TEST(dr_status_report) {
    using namespace dr;
    
    DisasterRecoveryManager manager;
    
    Site primary;
    primary.site_id = "P1";
    primary.role = SiteRole::PRIMARY;
    primary.status = SiteStatus::ONLINE;
    manager.registerSite(primary);
    
    Site secondary;
    secondary.site_id = "S1";
    secondary.role = SiteRole::SECONDARY;
    secondary.status = SiteStatus::ONLINE;
    manager.registerSite(secondary);
    
    ReplicationLink link;
    link.link_id = "L1";
    link.source_site = "P1";
    link.target_site = "S1";
    link.state = ReplicationState::ACTIVE;
    manager.createReplicationLink(link);
    
    auto report = manager.getStatusReport();
    ASSERT(report.healthy);
    ASSERT_EQ(report.total_sites, 2);
    ASSERT_EQ(report.online_sites, 2);
    ASSERT_EQ(report.primary_site, "P1");
    ASSERT_EQ(report.secondary_sites.size(), 1);
}

TEST(dr_test_execution) {
    using namespace dr;
    
    DisasterRecoveryManager manager;
    
    Site primary;
    primary.site_id = "PROD";
    primary.role = SiteRole::PRIMARY;
    primary.status = SiteStatus::ONLINE;
    primary.objectives.rpo = std::chrono::hours(1);
    manager.registerSite(primary);
    
    Site dr_site;
    dr_site.site_id = "DR";
    dr_site.role = SiteRole::SECONDARY;
    dr_site.status = SiteStatus::ONLINE;
    dr_site.total_capacity = 1000000;
    manager.registerSite(dr_site);
    
    // Create recovery point
    manager.createRecoveryPoint("DR", "Test RP");
    
    // Run DR test
    auto result = manager.runDRTest("DR", false);
    ASSERT(result.rpo_validated);  // Has recent RP
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n=== DFSMShsm Emulator v3.5.0 Tests ===\n\n";
    
    // Tests run automatically via static initialization
    
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Tests run:    " << tests_run << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << (tests_run - tests_passed) << std::endl;
    
    if (tests_passed == tests_run) {
        std::cout << "\n*** ALL TESTS PASSED ***" << std::endl;
        return 0;
    } else {
        std::cout << "\n*** SOME TESTS FAILED ***" << std::endl;
        return 1;
    }
}
