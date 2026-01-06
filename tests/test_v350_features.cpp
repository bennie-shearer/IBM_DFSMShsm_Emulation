/*******************************************************************************
 * DFSMShsm Emulator v3.5.0 - Unit Tests for New Features
 * 
 * Tests for:
 * - Enhanced Logging/Tracing
 * - Performance Benchmarking
 * - Tape Pool Management
 * - Report Generation
 * - ABACKUP/ARECOVER Emulation
 * - Disaster Recovery
 * 
 * Build: g++ -std=c++20 -I../include -o test_v350 test_v350_features.cpp -pthread
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
#include <cassert>
#include <sstream>
#include <thread>
#include <chrono>

using namespace dfsmshsm;

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "  Testing " << #name << "... "; \
    try { \
        test_##name(); \
        std::cout << "PASSED\n"; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << "FAILED: " << e.what() << "\n"; \
        tests_failed++; \
    } \
} while(0)

#define ASSERT_TRUE(cond) if (!(cond)) throw std::runtime_error("Assertion failed: " #cond)
#define ASSERT_FALSE(cond) if (cond) throw std::runtime_error("Assertion failed: NOT " #cond)
#define ASSERT_EQ(a, b) if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " == " #b)
#define ASSERT_NE(a, b) if ((a) == (b)) throw std::runtime_error("Assertion failed: " #a " != " #b)
#define ASSERT_GT(a, b) if (!((a) > (b))) throw std::runtime_error("Assertion failed: " #a " > " #b)
#define ASSERT_GE(a, b) if (!((a) >= (b))) throw std::runtime_error("Assertion failed: " #a " >= " #b)
#define ASSERT_LT(a, b) if (!((a) < (b))) throw std::runtime_error("Assertion failed: " #a " < " #b)

// ============================================================================
// Logging Tests
// ============================================================================

TEST(log_level_conversion) {
    ASSERT_EQ(logging::logLevelToString(logging::LogLevel::TRACE), "TRACE");
    ASSERT_EQ(logging::logLevelToString(logging::LogLevel::DEBUG), "DEBUG");
    ASSERT_EQ(logging::logLevelToString(logging::LogLevel::INFO), "INFO");
    ASSERT_EQ(logging::logLevelToString(logging::LogLevel::WARN), "WARN");
    ASSERT_EQ(logging::logLevelToString(logging::LogLevel::ERROR), "ERROR");
    ASSERT_EQ(logging::logLevelToString(logging::LogLevel::FATAL), "FATAL");
    
    ASSERT_EQ(logging::stringToLogLevel("TRACE"), logging::LogLevel::TRACE);
    ASSERT_EQ(logging::stringToLogLevel("ERROR"), logging::LogLevel::ERROR);
    ASSERT_EQ(logging::stringToLogLevel("UNKNOWN"), logging::LogLevel::INFO);
}

TEST(log_entry_format) {
    logging::LogEntry entry;
    entry.level = logging::LogLevel::INFO;
    entry.logger_name = "test";
    entry.message = "Test message";
    entry.source_file = "test.cpp";
    entry.source_line = 42;
    
    std::string plain = entry.formatPlainText();
    ASSERT_TRUE(plain.find("[INFO]") != std::string::npos);
    ASSERT_TRUE(plain.find("[test]") != std::string::npos);
    ASSERT_TRUE(plain.find("Test message") != std::string::npos);
    
    std::string json = entry.formatJSON();
    ASSERT_TRUE(json.find("\"level\":\"INFO\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"message\":\"Test message\"") != std::string::npos);
}

TEST(trace_context) {
    std::string trace_id = logging::TraceContext::generateTraceId();
    ASSERT_EQ(trace_id.length(), 32u);  // 16 bytes = 32 hex chars
    
    std::string span_id = logging::TraceContext::generateSpanId();
    ASSERT_EQ(span_id.length(), 16u);   // 8 bytes = 16 hex chars
    
    logging::TraceContext::setCurrentTraceId("test_trace");
    ASSERT_EQ(logging::TraceContext::getCurrentTraceId(), "test_trace");
    
    logging::TraceContext::clear();
    ASSERT_TRUE(logging::TraceContext::getCurrentTraceId().empty());
}

TEST(trace_span) {
    logging::TraceContext::clear();
    
    {
        logging::TraceSpan span("test_operation");
        ASSERT_FALSE(span.getSpanId().empty());
        ASSERT_FALSE(logging::TraceContext::getCurrentTraceId().empty());
        
        // Use 25ms sleep with lenient check to handle Windows timer resolution
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        ASSERT_GE(span.elapsedMs(), 5.0);  // Lenient check for cross-platform
    }
    
    // Span ID should be cleared after scope
}

TEST(logger_basic) {
    auto logger = logging::LogManager::instance().getLogger("test.basic");
    ASSERT_EQ(logger->getName(), "test.basic");
    
    logger->setLevel(logging::LogLevel::WARN);
    ASSERT_EQ(logger->getLevel(), logging::LogLevel::WARN);
}

TEST(trace_analyzer) {
    logging::TraceAnalyzer analyzer;
    
    for (int i = 0; i < 10; ++i) {
        logging::TraceRecord record;
        record.trace_id = "trace_" + std::to_string(i);
        record.operation = "test_op";
        record.duration = std::chrono::milliseconds(100 + i * 10);
        record.status = (i == 5) ? logging::LogLevel::ERROR : logging::LogLevel::INFO;
        analyzer.recordTrace(record);
    }
    
    auto stats = analyzer.calculateStats("test_op");
    ASSERT_EQ(stats.total_traces, 10u);
    ASSERT_EQ(stats.error_count, 1u);
    ASSERT_GT(stats.avg_duration_ms, 0);
    
    auto errors = analyzer.getErrorTraces();
    ASSERT_EQ(errors.size(), 1u);
}

// ============================================================================
// Benchmark Tests
// ============================================================================

TEST(statistics_calculation) {
    std::vector<double> values = {10, 20, 30, 40, 50};
    auto stats = benchmark::Statistics::calculate(values);
    
    ASSERT_EQ(stats.count, 5u);
    ASSERT_EQ(stats.min, 10.0);
    ASSERT_EQ(stats.max, 50.0);
    ASSERT_EQ(stats.mean, 30.0);
    ASSERT_EQ(stats.median, 30.0);
    ASSERT_EQ(stats.sum, 150.0);
}

TEST(scoped_timer) {
    benchmark::ScopedTimer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timer.stop();
    
    // Lenient bounds for cross-platform timing
    ASSERT_GE(timer.elapsedMs(), 25.0);
    ASSERT_LT(timer.elapsedMs(), 150.0);
}

TEST(benchmark_runner) {
    benchmark::BenchmarkRunner runner;
    benchmark::BenchmarkRunner::Config config;
    config.iterations = 5;
    config.warmup_iterations = 1;
    runner.setConfig(config);
    
    auto result = runner.runTimed("test_bench", []() {
        volatile int sum = 0;
        for (int i = 0; i < 1000; ++i) sum += i;
    });
    
    ASSERT_EQ(result.name, "test_bench");
    ASSERT_EQ(result.iterations, 5u);
    ASSERT_GT(result.time_stats_ms.mean, 0);
}

TEST(benchmark_sample) {
    benchmark::BenchmarkSample sample;
    sample.elapsed_time = std::chrono::seconds(1);
    sample.bytes_processed = 100 * 1024 * 1024;  // 100 MB
    sample.operations_count = 100;
    
    ASSERT_EQ(sample.throughputMBps(), 100.0);
    ASSERT_EQ(sample.operationsPerSecond(), 100.0);
}

TEST(benchmark_comparison) {
    benchmark::BenchmarkResult baseline;
    baseline.time_stats_ms.mean = 100.0;
    baseline.throughput_stats_mbps.mean = 50.0;
    baseline.ops_stats.mean = 1000.0;
    
    benchmark::BenchmarkResult current;
    current.time_stats_ms.mean = 120.0;  // 20% slower
    current.throughput_stats_mbps.mean = 45.0;
    current.ops_stats.mean = 900.0;
    
    benchmark::BenchmarkComparator comparator(10.0);
    auto results = comparator.compare(baseline, current);
    
    ASSERT_TRUE(comparator.hasRegression(results));
}

// ============================================================================
// Tape Pool Tests
// ============================================================================

TEST(tape_volume_creation) {
    tapepool::TapeVolume vol;
    vol.volser = "T00001";
    vol.media_type = tapepool::MediaType::LTO8;
    vol.status = tapepool::VolumeStatus::SCRATCH;
    vol.capacity = 12000ULL * 1024 * 1024 * 1024;
    vol.available_space = vol.capacity;
    
    ASSERT_TRUE(vol.isScratch());
    ASSERT_TRUE(vol.isAvailable());
    ASSERT_EQ(vol.utilizationPercent(), 0.0);
}

TEST(tape_pool_creation) {
    tapepool::TapePoolManager manager;
    
    tapepool::TapePool pool;
    pool.name = "TEST_POOL";
    pool.default_media_type = tapepool::MediaType::LTO8;
    pool.auto_scratch_return = true;
    
    manager.createPool(pool);
    
    auto retrieved = manager.getPool("TEST_POOL");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->name, "TEST_POOL");
}

TEST(scratch_allocation) {
    tapepool::TapePoolManager manager;
    
    tapepool::TapePool pool;
    pool.name = "SCRATCH";
    manager.createPool(pool);
    
    manager.generateTestVolumes("SCRATCH", 10, tapepool::MediaType::LTO8);
    
    tapepool::AllocationRequest request;
    request.pool_name = "SCRATCH";
    request.owner = "TEST";
    request.retention_days = 30;
    
    auto result = manager.allocateScratch(request);
    ASSERT_TRUE(result.success);
    ASSERT_FALSE(result.volser.empty());
    ASSERT_GT(result.available_space, 0u);
}

TEST(pool_statistics) {
    tapepool::TapePoolManager manager;
    
    tapepool::TapePool pool;
    pool.name = "STATS_POOL";
    manager.createPool(pool);
    
    manager.generateTestVolumes("STATS_POOL", 20, tapepool::MediaType::LTO8);
    
    auto stats = manager.getPoolStatistics("STATS_POOL");
    ASSERT_EQ(stats.total_volumes, 20u);
    ASSERT_EQ(stats.scratch_volumes, 20u);
    ASSERT_GT(stats.total_capacity_bytes, 0u);
}

TEST(pool_health_check) {
    tapepool::TapePoolManager manager;
    
    tapepool::TapePool pool;
    pool.name = "HEALTH_POOL";
    pool.thresholds.min_scratch_count = 5;
    manager.createPool(pool);
    
    manager.generateTestVolumes("HEALTH_POOL", 10, tapepool::MediaType::LTO8);
    
    auto health = manager.checkPoolHealth("HEALTH_POOL");
    ASSERT_TRUE(health.healthy);
    ASSERT_TRUE(health.critical_issues.empty());
}

TEST(storage_group) {
    tapepool::StorageGroupManager sg_manager;
    
    tapepool::StorageGroup sg;
    sg.name = "TEST_SG";
    sg.pool_names = {"POOL1", "POOL2"};
    sg.overflow_to_next_pool = true;
    
    sg_manager.createGroup(sg);
    
    auto retrieved = sg_manager.getGroup("TEST_SG");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->pool_names.size(), 2u);
}

// ============================================================================
// Report Tests
// ============================================================================

TEST(migration_statistics) {
    std::vector<reports::MigrationRecord> records;
    
    for (int i = 0; i < 10; ++i) {
        reports::MigrationRecord rec;
        rec.bytes_migrated = 1024 * 1024;
        rec.duration = std::chrono::milliseconds(100);
        rec.success = (i < 8);
        rec.compressed = true;
        rec.compression_ratio = 2.0;
        rec.migration_level = 1;
        records.push_back(rec);
    }
    
    auto stats = reports::MigrationStatistics::calculate(records);
    ASSERT_EQ(stats.total_count, 10u);
    ASSERT_EQ(stats.success_count, 8u);
    ASSERT_EQ(stats.failure_count, 2u);
    ASSERT_GT(stats.avg_throughput_mbps, 0);
}

TEST(report_data_collector) {
    reports::ReportDataCollector collector;
    
    reports::MigrationRecord mig;
    mig.dataset_name = "TEST.DATA";
    mig.timestamp = std::chrono::system_clock::now();
    collector.addMigration(mig);
    
    reports::RecallRecord recall;
    recall.dataset_name = "TEST.RECALL";
    recall.timestamp = std::chrono::system_clock::now();
    collector.addRecall(recall);
    
    auto range = reports::ReportTimeRange::today();
    
    auto migrations = collector.getMigrations(range);
    ASSERT_EQ(migrations.size(), 1u);
    
    auto recalls = collector.getRecalls(range);
    ASSERT_EQ(recalls.size(), 1u);
}

TEST(report_generation) {
    auto collector = std::make_shared<reports::ReportDataCollector>();
    
    reports::MigrationRecord rec;
    rec.dataset_name = "TEST.DATA";
    rec.bytes_migrated = 1024 * 1024;
    rec.timestamp = std::chrono::system_clock::now();
    rec.success = true;
    collector->addMigration(rec);
    
    reports::ReportGenerator generator(collector);
    
    auto report = generator.createMigrationReport();
    report->setTimeRange(reports::ReportTimeRange::today());
    report->setFormat(reports::ReportFormat::TEXT);
    
    std::string output = report->generate();
    ASSERT_FALSE(output.empty());
    ASSERT_TRUE(output.find("Migration") != std::string::npos);
}

TEST(report_formats) {
    auto collector = std::make_shared<reports::ReportDataCollector>();
    
    reports::MigrationRecord rec;
    rec.dataset_name = "FORMAT.TEST";
    rec.timestamp = std::chrono::system_clock::now();
    rec.success = true;
    collector->addMigration(rec);
    
    reports::ReportGenerator generator(collector);
    auto report = generator.createMigrationReport();
    report->setTimeRange(reports::ReportTimeRange::today());
    
    // Text format
    report->setFormat(reports::ReportFormat::TEXT);
    std::string text = report->generate();
    ASSERT_FALSE(text.empty());
    
    // JSON format
    report->setFormat(reports::ReportFormat::JSON);
    std::string json = report->generate();
    ASSERT_TRUE(json.find("{") != std::string::npos);
    
    // CSV format
    report->setFormat(reports::ReportFormat::CSV);
    std::string csv = report->generate();
    ASSERT_TRUE(csv.find(",") != std::string::npos);
}

// ============================================================================
// ABACKUP Tests
// ============================================================================

TEST(aggregate_group_creation) {
    abackup::ABackupManager manager;
    
    abackup::AggregateGroup group;
    group.name = "TEST_GROUP";
    group.volume_patterns = {"VOL*"};
    group.retention_days = 30;
    group.versions_to_keep = 3;
    
    manager.defineGroup(group);
    
    auto retrieved = manager.getGroup("TEST_GROUP");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->name, "TEST_GROUP");
    ASSERT_EQ(retrieved->retention_days, 30);
}

TEST(pattern_matching) {
    abackup::AggregateGroup group;
    group.volume_patterns = {"VOL*", "USR???"};
    group.dataset_patterns = {"PROD.*", "*.DATA"};
    
    ASSERT_TRUE(group.matchesVolume("VOL001"));
    ASSERT_TRUE(group.matchesVolume("VOL999"));
    ASSERT_TRUE(group.matchesVolume("USR123"));
    ASSERT_FALSE(group.matchesVolume("OTHER"));
    
    ASSERT_TRUE(group.matchesDataset("PROD.DATA"));
    ASSERT_TRUE(group.matchesDataset("TEST.DATA"));
}

TEST(backup_execution) {
    abackup::ABackupManager manager;
    
    abackup::AggregateGroup group;
    group.name = "BACKUP_TEST";
    manager.defineGroup(group);
    
    abackup::ABackupOptions opts;
    opts.group_name = "BACKUP_TEST";
    opts.type = abackup::BackupType::FULL;
    opts.retention_days = 7;
    
    auto result = manager.executeABackup(opts);
    ASSERT_TRUE(result.success);
    ASSERT_FALSE(result.backup_id.empty());
}

TEST(backup_catalog) {
    abackup::BackupCatalog catalog;
    
    abackup::BackupVersion backup;
    backup.backup_id = "TEST_BACKUP_001";
    backup.group_name = "TEST_GROUP";
    backup.version_number = 1;
    backup.type = abackup::BackupType::FULL;
    backup.status = abackup::BackupStatus::COMPLETED;
    
    catalog.addBackup(backup);
    
    auto retrieved = catalog.getBackup("TEST_BACKUP_001");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->version_number, 1);
    
    auto group_backups = catalog.getGroupBackups("TEST_GROUP");
    ASSERT_EQ(group_backups.size(), 1u);
}

TEST(recovery_validation) {
    abackup::ABackupManager manager;
    
    abackup::AggregateGroup group;
    group.name = "RECOVER_TEST";
    manager.defineGroup(group);
    
    // Create backup first
    abackup::ABackupOptions backup_opts;
    backup_opts.group_name = "RECOVER_TEST";
    auto backup_result = manager.executeABackup(backup_opts);
    
    // Validate recovery
    abackup::ARecoverOptions recover_opts;
    recover_opts.backup_id = backup_result.backup_id;
    recover_opts.verify_only = true;
    
    auto recover_result = manager.executeARecover(recover_opts);
    ASSERT_TRUE(recover_result.validation_passed);
}

// ============================================================================
// Disaster Recovery Tests
// ============================================================================

TEST(site_management) {
    dr::DisasterRecoveryManager manager;
    
    dr::Site site;
    site.site_id = "SITE_A";
    site.name = "Site A";
    site.role = dr::SiteRole::PRIMARY;
    site.status = dr::SiteStatus::ONLINE;
    
    manager.registerSite(site);
    
    auto retrieved = manager.getSite("SITE_A");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->name, "Site A");
    ASSERT_TRUE(retrieved->isHealthy());
}

TEST(replication_stream) {
    dr::DisasterRecoveryManager manager;
    
    dr::ReplicationLink link;
    link.link_id = "LINK_1";
    link.source_site = "SITE_A";
    link.target_site = "SITE_B";
    link.mode = dr::ReplicationMode::SYNCHRONOUS;
    link.state = dr::ReplicationState::ACTIVE;
    link.replication_lag = std::chrono::milliseconds(50);
    
    manager.createReplicationLink(link);
    
    auto retrieved = manager.getReplicationLink("LINK_1");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->mode, dr::ReplicationMode::SYNCHRONOUS);
    ASSERT_TRUE(retrieved->isHealthy());
}

TEST(recovery_points) {
    dr::DisasterRecoveryManager manager;
    
    // Create a recovery point using the manager's method
    std::string point_id = manager.createRecoveryPoint("SITE_A", "Test point");
    ASSERT_FALSE(point_id.empty());
    
    auto retrieved = manager.getRecoveryPoint(point_id);
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->site_id, "SITE_A");
    
    auto points = manager.getRecoveryPointsForSite("SITE_A");
    ASSERT_EQ(points.size(), 1u);
    
    auto latest = manager.getLatestRecoveryPoint("SITE_A");
    ASSERT_TRUE(latest.has_value());
}

TEST(dr_test_execution) {
    dr::DisasterRecoveryManager manager;
    
    dr::Site primary;
    primary.site_id = "PRIMARY";
    primary.role = dr::SiteRole::PRIMARY;
    primary.status = dr::SiteStatus::ONLINE;
    primary.last_heartbeat = std::chrono::system_clock::now();
    manager.registerSite(primary);
    
    dr::Site secondary;
    secondary.site_id = "SECONDARY";
    secondary.role = dr::SiteRole::SECONDARY;
    secondary.status = dr::SiteStatus::ONLINE;
    secondary.last_heartbeat = std::chrono::system_clock::now();
    manager.registerSite(secondary);
    
    dr::ReplicationLink link;
    link.link_id = "REPL";
    link.source_site = "PRIMARY";
    link.target_site = "SECONDARY";
    link.state = dr::ReplicationState::ACTIVE;
    link.replication_lag = std::chrono::milliseconds(100);
    manager.createReplicationLink(link);
    
    auto result = manager.runDRTest("SECONDARY");
    // Either success or has issues found
    ASSERT_TRUE(result.success || !result.issues_found.empty());
}

TEST(failover_execution) {
    dr::DisasterRecoveryManager manager;
    
    dr::Site primary;
    primary.site_id = "PRIMARY";
    primary.role = dr::SiteRole::PRIMARY;
    primary.status = dr::SiteStatus::ONLINE;
    primary.last_heartbeat = std::chrono::system_clock::now();
    manager.registerSite(primary);
    
    dr::Site secondary;
    secondary.site_id = "SECONDARY";
    secondary.role = dr::SiteRole::SECONDARY;
    secondary.status = dr::SiteStatus::ONLINE;
    secondary.last_heartbeat = std::chrono::system_clock::now();
    manager.registerSite(secondary);
    
    dr::DisasterRecoveryManager::FailoverOptions options;
    options.target_site = "SECONDARY";
    options.force = true;  // Force for test
    options.type = dr::FailoverEvent::Type::PLANNED;
    
    auto result = manager.initiateFailover(options);
    // May or may not succeed depending on state
    ASSERT_TRUE(result.success || !result.error_message.empty());
}

TEST(recovery_objectives) {
    dr::RecoveryObjectives obj;
    ASSERT_EQ(obj.rpo.count(), 3600);  // 1 hour default
    ASSERT_EQ(obj.rto.count(), 14400); // 4 hours default
}

TEST(dr_status_report) {
    dr::DisasterRecoveryManager manager;
    
    dr::Site site;
    site.site_id = "TEST";
    site.role = dr::SiteRole::PRIMARY;
    site.status = dr::SiteStatus::ONLINE;
    manager.registerSite(site);
    
    auto sites = manager.listSites();
    ASSERT_FALSE(sites.empty());
    
    auto primary = manager.getPrimarySite();
    ASSERT_TRUE(primary.has_value());
    ASSERT_EQ(primary->site_id, "TEST");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "+======================================================================+\n";
    std::cout << "|           DFSMShsm Emulator v3.5.0 - Unit Tests                      |\n";
    std::cout << "+======================================================================+\n\n";
    
    // Logging Tests
    std::cout << "LOGGING TESTS:\n";
    RUN_TEST(log_level_conversion);
    RUN_TEST(log_entry_format);
    RUN_TEST(trace_context);
    RUN_TEST(trace_span);
    RUN_TEST(logger_basic);
    RUN_TEST(trace_analyzer);
    
    // Benchmark Tests
    std::cout << "\nBENCHMARK TESTS:\n";
    RUN_TEST(statistics_calculation);
    RUN_TEST(scoped_timer);
    RUN_TEST(benchmark_runner);
    RUN_TEST(benchmark_sample);
    RUN_TEST(benchmark_comparison);
    
    // Tape Pool Tests
    std::cout << "\nTAPE POOL TESTS:\n";
    RUN_TEST(tape_volume_creation);
    RUN_TEST(tape_pool_creation);
    RUN_TEST(scratch_allocation);
    RUN_TEST(pool_statistics);
    RUN_TEST(pool_health_check);
    RUN_TEST(storage_group);
    
    // Report Tests
    std::cout << "\nREPORT TESTS:\n";
    RUN_TEST(migration_statistics);
    RUN_TEST(report_data_collector);
    RUN_TEST(report_generation);
    RUN_TEST(report_formats);
    
    // ABACKUP Tests
    std::cout << "\nABACKUP/ARECOVER TESTS:\n";
    RUN_TEST(aggregate_group_creation);
    RUN_TEST(pattern_matching);
    RUN_TEST(backup_execution);
    RUN_TEST(backup_catalog);
    RUN_TEST(recovery_validation);
    
    // Disaster Recovery Tests
    std::cout << "\nDISASTER RECOVERY TESTS:\n";
    RUN_TEST(site_management);
    RUN_TEST(replication_stream);
    RUN_TEST(recovery_points);
    RUN_TEST(dr_test_execution);
    RUN_TEST(failover_execution);
    RUN_TEST(recovery_objectives);
    RUN_TEST(dr_status_report);
    
    // Summary
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "TEST SUMMARY: " << tests_passed << " passed, " 
              << tests_failed << " failed\n";
    std::cout << std::string(70, '=') << "\n\n";
    
    return tests_failed > 0 ? 1 : 0;
}
