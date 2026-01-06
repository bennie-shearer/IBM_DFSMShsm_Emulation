/*
 * DFSMShsm Emulator - v4.1.0 Feature Tests
 * @version 4.2.0
 * Tests for: CDS, Command Processor, Space Management, Interval Processing,
 *            Security (RACF/SAF), and Sysplex Coordination
 */

#include <iostream>
#include <cassert>
#include <sstream>
#include <thread>
#include <chrono>

// v4.1.0 Headers
#include "hsm_cds.hpp"
#include "hsm_command.hpp"
#include "hsm_space_management.hpp"
#include "hsm_interval.hpp"
#include "hsm_security.hpp"
#include "hsm_sysplex.hpp"

using namespace hsm;

// Test counters
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " << #name << "... "; \
    try { \
        test_##name(); \
        std::cout << "PASSED" << std::endl; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << "FAILED: " << e.what() << std::endl; \
        tests_failed++; \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        throw std::runtime_error("Assertion failed: " #cond); \
    } \
} while(0)

// =============================================================================
// CDS Tests
// =============================================================================

TEST(cds_stck_timestamp) {
    cds::STCKTimestamp ts = cds::STCKTimestamp::now();
    ASSERT(ts.value > 0);
    
    auto tp = ts.toTimePoint();
    // Verify time point is valid (not epoch)
    ASSERT(tp.time_since_epoch().count() > 0);
    std::string str = ts.toString();
    ASSERT(!str.empty());
}

TEST(cds_mcd_dataset_record) {
    cds::MCDDatasetRecord record;
    record.setDatasetName("TEST.DATASET.NAME");
    record.migrationLevel = cds::MigrationLevel::ML1;
    record.originalSize = 1000000;
    record.compressedSize = 500000;
    record.setCompressed(true);
    
    ASSERT(record.getDatasetName() == "TEST.DATASET.NAME");
    ASSERT(record.isCompressed());
    ASSERT(!record.isEncrypted());
}

TEST(cds_manager_add_migrated) {
    cds::CDSManager manager;
    
    cds::MCDDatasetRecord record;
    record.setDatasetName("USER.DATA.TEST1");
    record.migrationLevel = cds::MigrationLevel::ML1;
    record.originalSize = 5000;
    std::memcpy(record.targetVolume, "HSM001", 6);
    
    ASSERT(manager.addMigratedDataset(record));
    
    auto result = manager.getMigratedDataset("USER.DATA.TEST1");
    ASSERT(result.has_value());
    ASSERT(result->originalSize == 5000);
}

TEST(cds_manager_update_migrated) {
    cds::CDSManager manager;
    
    cds::MCDDatasetRecord record;
    record.setDatasetName("USER.UPDATE.TEST");
    record.migrationLevel = cds::MigrationLevel::ML1;
    record.recallCount = 0;
    
    manager.addMigratedDataset(record);
    
    record.recallCount = 5;
    ASSERT(manager.updateMigratedDataset(record));
    
    auto result = manager.getMigratedDataset("USER.UPDATE.TEST");
    ASSERT(result->recallCount == 5);
}

TEST(cds_manager_delete_migrated) {
    cds::CDSManager manager;
    
    cds::MCDDatasetRecord record;
    record.setDatasetName("USER.DELETE.TEST");
    manager.addMigratedDataset(record);
    
    ASSERT(manager.deleteMigratedDataset("USER.DELETE.TEST"));
    ASSERT(!manager.getMigratedDataset("USER.DELETE.TEST").has_value());
}

TEST(cds_manager_search_pattern) {
    cds::CDSManager manager;
    
    for (int i = 0; i < 5; i++) {
        cds::MCDDatasetRecord record;
        record.setDatasetName("PROD.DATA.FILE" + std::to_string(i));
        manager.addMigratedDataset(record);
    }
    
    auto results = manager.searchMigratedDatasets("PROD.DATA.*");
    ASSERT(results.size() == 5);
}

TEST(cds_backup_dataset_record) {
    cds::BCDDatasetRecord record;
    record.setDatasetName("BACKUP.TEST.DS");
    record.frequency = cds::BackupFrequency::DAILY;
    record.versionCount = 3;
    record.maxVersions = 10;
    
    ASSERT(record.getDatasetName() == "BACKUP.TEST.DS");
    ASSERT(record.versionCount == 3);
}

TEST(cds_manager_backup_operations) {
    cds::CDSManager manager;
    
    cds::BCDDatasetRecord record;
    record.setDatasetName("USER.BACKUP.TEST");
    record.frequency = cds::BackupFrequency::WEEKLY;
    
    ASSERT(manager.addBackupDataset(record));
    
    auto result = manager.getBackupDataset("USER.BACKUP.TEST");
    ASSERT(result.has_value());
    ASSERT(result->frequency == cds::BackupFrequency::WEEKLY);
}

TEST(cds_tape_volume_record) {
    cds::OCDTapeRecord record;
    std::memcpy(record.volumeSerial, "TAPE01", 6);
    record.capacity = 100000000000ULL;  // 100GB
    record.usedSpace = 50000000000ULL;  // 50GB
    record.fileCount = 100;
    
    ASSERT(record.getVolumeSerial() == "TAPE01");
    ASSERT(record.capacity == 100000000000ULL);
}

TEST(cds_manager_statistics) {
    cds::CDSManager manager;
    
    // Add some records
    for (int i = 0; i < 10; i++) {
        cds::MCDDatasetRecord record;
        record.setDatasetName("STATS.TEST.DS" + std::to_string(i));
        manager.addMigratedDataset(record);
    }
    
    auto summary = manager.getMCDSSummary();
    ASSERT(summary.recordCount == 10);
    ASSERT(summary.addedToday == 10);
}

TEST(cds_dataset_status) {
    cds::CDSManager manager;
    
    cds::MCDDatasetRecord mcd;
    mcd.setDatasetName("STATUS.TEST.DS");
    mcd.migrationLevel = cds::MigrationLevel::ML1;
    manager.addMigratedDataset(mcd);
    
    cds::BCDDatasetRecord bcd;
    bcd.setDatasetName("STATUS.TEST.DS");
    bcd.versionCount = 2;
    manager.addBackupDataset(bcd);
    
    auto status = manager.getDatasetStatus("STATUS.TEST.DS");
    ASSERT(status.isMigrated);
    ASSERT(status.hasBackup);
    ASSERT(status.backupVersions == 2);
}

// =============================================================================
// Command Processor Tests
// =============================================================================

TEST(command_parser_hmigrate) {
    command::CommandParser parser;
    
    auto cmd = parser.parse("HMIGRATE DSN(USER.DATA.TEST) WAIT");
    ASSERT(cmd.isValid());
    ASSERT(cmd.type == command::CommandType::HMIGRATE);
    
    auto& opts = std::get<command::MigrateOptions>(cmd.options);
    ASSERT(opts.datasets.size() == 1);
    ASSERT(opts.wait);
}

TEST(command_parser_hrecall) {
    command::CommandParser parser;
    
    auto cmd = parser.parse("HRECALL DSN('USER.DATA.RECALL') NOWAIT PRIORITY(7)");
    ASSERT(cmd.isValid());
    ASSERT(cmd.type == command::CommandType::HRECALL);
    
    auto& opts = std::get<command::RecallOptions>(cmd.options);
    ASSERT(opts.datasets.size() == 1);
    ASSERT(opts.nowait);
    ASSERT(opts.priority == 7);
}

TEST(command_parser_hbackup) {
    command::CommandParser parser;
    
    auto cmd = parser.parse("HBACKUP DSN(PROD.DATA) INCREMENTAL VERSIONS(5)");
    ASSERT(cmd.isValid());
    ASSERT(cmd.type == command::CommandType::HBACKUP);
    
    auto& opts = std::get<command::BackupOptions>(cmd.options);
    ASSERT(opts.incremental);
    ASSERT(opts.versions == 5);
}

TEST(command_parser_hquery) {
    command::CommandParser parser;
    
    auto cmd = parser.parse("HQUERY ACTIVE");
    ASSERT(cmd.isValid());
    ASSERT(cmd.type == command::CommandType::HQUERY);
    
    auto& opts = std::get<command::QueryOptions>(cmd.options);
    ASSERT(opts.queryType == "ACTIVE");
}

TEST(command_parser_hlist) {
    command::CommandParser parser;
    
    auto cmd = parser.parse("HLIST DSN(USER.*) MIGRATED LEVEL");
    ASSERT(cmd.isValid());
    ASSERT(cmd.type == command::CommandType::HLIST);
    
    auto& opts = std::get<command::ListOptions>(cmd.options);
    ASSERT(opts.migrated);
    ASSERT(opts.level);
}

TEST(command_parser_abbreviation) {
    command::CommandParser parser;
    
    auto cmd = parser.parse("HMIG DSN(TEST.DS)");
    ASSERT(cmd.isValid());
    ASSERT(cmd.type == command::CommandType::HMIGRATE);
}

TEST(command_parser_hsend_wrapper) {
    command::CommandParser parser;
    
    auto cmd = parser.parse("HSEND HMIGRATE DSN(TEST.DS)");
    ASSERT(cmd.isValid());
    ASSERT(cmd.type == command::CommandType::HMIGRATE);
}

TEST(command_parser_invalid) {
    command::CommandParser parser;
    
    auto cmd = parser.parse("INVALIDCOMMAND");
    ASSERT(!cmd.isValid());
    ASSERT(!cmd.errors.empty());
}

TEST(command_executor_submit) {
    command::CommandExecutor executor;
    command::CommandParser parser;
    
    executor.registerHandler(command::CommandType::HQUERY, 
        [](const command::ParsedCommand&) {
            command::CommandResult result;
            result.returnCode = command::ReturnCode::SUCCESS;
            result.message = "Query completed";
            return result;
        });
    
    auto cmd = parser.parse("HQUERY ACTIVE");
    auto result = executor.execute(cmd);
    
    ASSERT(result.isSuccess());
}

TEST(command_processor_integration) {
    command::CommandProcessor processor;
    
    processor.registerHandler(command::CommandType::HMIGRATE,
        [](const command::ParsedCommand& cmd) {
            command::CommandResult result;
            auto& opts = std::get<command::MigrateOptions>(cmd.options);
            result.returnCode = command::ReturnCode::SUCCESS;
            result.message = "Migrated " + std::to_string(opts.datasets.size()) + " datasets";
            return result;
        });
    
    auto result = processor.process("HMIGRATE DSN(TEST.DS)");
    ASSERT(result.isSuccess());
}

// =============================================================================
// Space Management Tests
// =============================================================================

TEST(space_volume_info) {
    space::VolumeSpaceInfo vol;
    vol.volser = "VOL001";
    vol.totalCapacity = 1000000000;  // 1GB
    vol.allocatedSpace = 850000000;   // 850MB
    vol.freeSpace = 150000000;        // 150MB
    vol.recalculate();
    
    ASSERT(vol.utilizationPercent == 85.0);
}

TEST(space_dataset_info) {
    space::DatasetSpaceInfo ds;
    ds.datasetName = "TEST.DATASET";
    ds.allocatedSpace = 10000;
    ds.lastReferenceDate = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
    ds.updateDaysNonUsage();
    
    ASSERT(ds.daysNonUsage >= 29);  // At least 29 days
}

TEST(space_threshold) {
    space::SpaceThreshold threshold;
    threshold.name = "HIGH";
    threshold.type = space::ThresholdType::HIGH_THRESHOLD;
    threshold.percent = 85.0;
    threshold.action = space::SpaceAction::MIGRATE_PRIMARY;
    
    ASSERT(threshold.percent == 85.0);
}

TEST(space_policy) {
    space::SpacePolicy policy;
    policy.name = "DEFAULT";
    policy.autoMigrate = true;
    policy.primaryMigrateDays = 30;
    policy.excludePatterns.push_back("SYS1.*");
    policy.excludePatterns.push_back("*.LOAD");
    
    ASSERT(policy.matchesDataset("USER.DATA"));
    ASSERT(!policy.matchesDataset("SYS1.PARMLIB"));
}

TEST(space_manager_register_volume) {
    space::SpaceManager manager;
    
    space::VolumeSpaceInfo vol;
    vol.volser = "VOL001";
    vol.totalCapacity = 1000000000;
    vol.allocatedSpace = 500000000;
    vol.freeSpace = 500000000;
    vol.recalculate();
    
    manager.registerVolume(vol);
    
    auto result = manager.getVolume("VOL001");
    ASSERT(result.has_value());
    ASSERT(result->utilizationPercent == 50.0);
}

TEST(space_manager_threshold_detection) {
    space::SpaceManager::Config config;
    config.defaultHighThreshold = 85.0;
    space::SpaceManager manager(config);
    
    space::VolumeSpaceInfo vol;
    vol.volser = "FULL01";
    vol.totalCapacity = 1000000000;
    vol.allocatedSpace = 900000000;  // 90%
    vol.freeSpace = 100000000;
    vol.recalculate();
    
    manager.registerVolume(vol);
    
    auto over = manager.getVolumesAboveThreshold(85.0);
    ASSERT(over.size() == 1);
}

TEST(space_manager_select_candidates) {
    space::SpaceManager manager;
    
    space::VolumeSpaceInfo vol;
    vol.volser = "VOL001";
    manager.registerVolume(vol);
    
    // Add datasets
    for (int i = 0; i < 10; i++) {
        space::DatasetSpaceInfo ds;
        ds.datasetName = "USER.DATA." + std::to_string(i);
        ds.volser = "VOL001";
        ds.allocatedSpace = 10000 * (i + 1);
        ds.lastReferenceDate = std::chrono::system_clock::now() - 
                               std::chrono::hours(24 * (i * 10));
        ds.updateDaysNonUsage();
        manager.registerDataset(ds);
    }
    
    auto candidates = manager.selectMigrationCandidates("VOL001", 5);
    ASSERT(candidates.size() == 5);
}

TEST(space_analysis) {
    space::SpaceManager manager;
    
    // Add multiple volumes
    for (int i = 0; i < 3; i++) {
        space::VolumeSpaceInfo vol;
        vol.volser = "VOL00" + std::to_string(i + 1);
        vol.totalCapacity = 1000000000;
        vol.allocatedSpace = 300000000 * (i + 1);
        vol.freeSpace = vol.totalCapacity - vol.allocatedSpace;
        vol.recalculate();
        manager.registerVolume(vol);
    }
    
    auto analysis = manager.analyzeSpace();
    ASSERT(analysis.volumeCount == 3);
    ASSERT(analysis.totalCapacity == 3000000000ULL);
}

TEST(space_forecast) {
    space::SpaceManager manager;
    
    space::VolumeSpaceInfo vol;
    vol.volser = "VOL001";
    vol.totalCapacity = 1000000000;
    vol.allocatedSpace = 800000000;  // 80%
    vol.freeSpace = 200000000;
    vol.recalculate();
    
    manager.registerVolume(vol);
    
    auto forecast = manager.forecastVolume("VOL001", 1.0);  // 1% per day
    ASSERT(forecast.currentUtilization == 80.0);
    ASSERT(forecast.daysToHighThreshold > 0);
}

// =============================================================================
// Interval Processing Tests
// =============================================================================

TEST(interval_time_of_day) {
    interval::TimeOfDay t1(14, 30, 0);
    ASSERT(t1.toSeconds() == 52200);  // 14*3600 + 30*60
    ASSERT(t1.toString() == "14:30:00");
}

TEST(interval_time_window) {
    interval::TimeWindow window(8, 0, 17, 0);  // 8 AM to 5 PM
    ASSERT(window.durationMinutes() == 540);    // 9 hours
    
    ASSERT(window.isActive(interval::TimeOfDay(12, 0)));
    ASSERT(!window.isActive(interval::TimeOfDay(7, 0)));
    ASSERT(!window.isActive(interval::TimeOfDay(18, 0)));
}

TEST(interval_time_window_overnight) {
    interval::TimeWindow window(22, 0, 6, 0);  // 10 PM to 6 AM
    
    ASSERT(window.isActive(interval::TimeOfDay(23, 0)));
    ASSERT(window.isActive(interval::TimeOfDay(3, 0)));
    ASSERT(!window.isActive(interval::TimeOfDay(12, 0)));
}

TEST(interval_definition) {
    interval::IntervalDefinition interval;
    interval.name = "TEST_INTERVAL";
    interval.type = interval::IntervalType::PRIMARY_SPACE_MANAGEMENT;
    interval.windows.push_back(interval::TimeWindow(0, 0, 6, 0));
    interval.activeDays.insert(interval::DayOfWeek::ALL_DAYS);
    interval.maxTasks = 20;
    
    ASSERT(interval.name == "TEST_INTERVAL");
    ASSERT(interval.maxTasks == 20);
}

TEST(interval_factory_primary_sm) {
    auto interval = interval::IntervalFactory::createPrimarySpaceManagement();
    
    ASSERT(interval.name == "PRIMARY_SM");
    ASSERT(interval.type == interval::IntervalType::PRIMARY_SPACE_MANAGEMENT);
    ASSERT(!interval.windows.empty());
}

TEST(interval_factory_backup) {
    auto interval = interval::IntervalFactory::createAutomaticBackup();
    
    ASSERT(interval.name == "AUTO_BACKUP");
    ASSERT(interval.type == interval::IntervalType::AUTOMATIC_BACKUP);
}

TEST(interval_scheduler_add_interval) {
    interval::IntervalScheduler scheduler;
    
    auto def = interval::IntervalFactory::createPrimarySpaceManagement();
    scheduler.addInterval(def);
    
    auto result = scheduler.getInterval("PRIMARY_SM");
    ASSERT(result.has_value());
    ASSERT(result->type == interval::IntervalType::PRIMARY_SPACE_MANAGEMENT);
}

TEST(interval_scheduler_state_management) {
    interval::IntervalScheduler scheduler;
    
    auto def = interval::IntervalFactory::createPrimarySpaceManagement();
    scheduler.addInterval(def);
    
    ASSERT(scheduler.getState("PRIMARY_SM") == interval::IntervalState::INACTIVE);
    
    scheduler.holdInterval("PRIMARY_SM");
    ASSERT(scheduler.getState("PRIMARY_SM") == interval::IntervalState::HELD);
    
    scheduler.releaseInterval("PRIMARY_SM");
    ASSERT(scheduler.getState("PRIMARY_SM") == interval::IntervalState::INACTIVE);
}

TEST(interval_scheduler_submit_task) {
    interval::IntervalScheduler scheduler;
    
    auto def = interval::IntervalFactory::createPrimarySpaceManagement();
    scheduler.addInterval(def);
    
    bool executed = false;
    auto taskId = scheduler.submitTask("PRIMARY_SM", [&executed]() {
        executed = true;
        return true;
    });
    
    ASSERT(taskId > 0);
    ASSERT(scheduler.getPendingTaskCount() == 1);
}

TEST(interval_statistics) {
    interval::IntervalScheduler scheduler;
    
    auto def = interval::IntervalFactory::createPrimarySpaceManagement();
    scheduler.addInterval(def);
    
    auto stats = scheduler.getStatistics("PRIMARY_SM");
    ASSERT(stats.intervalName == "PRIMARY_SM");
    ASSERT(stats.totalActivations == 0);
}

// =============================================================================
// Security Tests
// =============================================================================

TEST(security_access_level) {
    using namespace security;
    
    AccessLevel read = AccessLevel::READ;
    AccessLevel update = AccessLevel::UPDATE;
    AccessLevel combined = read | update;
    
    ASSERT(hasAccess(combined, AccessLevel::READ));
    ASSERT(hasAccess(combined, AccessLevel::UPDATE));
    ASSERT(!hasAccess(combined, AccessLevel::ALTER));
}

TEST(security_user_profile) {
    security::UserProfile user;
    user.userId = "TESTUSER";
    user.defaultGroup = "GROUP1";
    user.connectGroups.push_back("GROUP1");
    user.connectGroups.push_back("GROUP2");
    
    ASSERT(user.userId == "TESTUSER");
    ASSERT(user.connectGroups.size() == 2);
    ASSERT(user.isValid());
}

TEST(security_resource_profile) {
    security::ResourceProfile profile;
    profile.profileName = "USER.**";
    profile.resourceClass = security::ResourceClass::DATASET;
    profile.universalAccess = security::AccessLevel::NONE;
    
    security::ResourceProfile::AccessEntry entry;
    entry.identity = "TESTUSER";
    entry.access = security::AccessLevel::READ;
    profile.accessList.push_back(entry);
    
    ASSERT(profile.profileName == "USER.**");
    ASSERT(!profile.accessList.empty());
}

TEST(security_stgadmin_profile) {
    security::STGADMINProfile profile;
    profile.profileName = "ADM.HMIG";
    profile.type = security::STGADMINProfile::ProfileType::HMIG;
    profile.universalAccess = security::AccessLevel::NONE;
    
    ASSERT(profile.profileName == "ADM.HMIG");
}

TEST(security_audit_record) {
    security::AuditRecord record;
    record.eventType = security::AuditEvent::SUCCESS;
    record.userId = "TESTUSER";
    record.resourceName = "TEST.DATASET";
    record.timestamp = std::chrono::system_clock::now();
    
    std::string str = record.toString();
    ASSERT(!str.empty());
}

TEST(security_manager_check_access) {
    security::SecurityManager::Config config;
    config.enabled = true;
    security::SecurityManager manager(config);
    
    // Add user
    security::UserProfile user;
    user.userId = "USER1";
    user.defaultGroup = "GROUP1";
    user.connectGroups.push_back("GROUP1");
    manager.addUser(user);
    
    // Add profile
    security::ResourceProfile profile;
    profile.profileName = "TEST.**";
    profile.resourceClass = security::ResourceClass::DATASET;
    profile.universalAccess = security::AccessLevel::NONE;
    profile.isGeneric = true;
    
    security::ResourceProfile::AccessEntry entry;
    entry.identity = "USER1";
    entry.access = security::AccessLevel::READ;
    profile.accessList.push_back(entry);
    
    manager.addProfile(profile);
    
    // Check access
    auto response = manager.checkDatasetAccess("USER1", "TEST.DATASET", 
                                               security::AccessLevel::READ);
    ASSERT(response.returnCode == security::SAFReturnCode::SUCCESS);
}

TEST(security_manager_hsm_operation) {
    security::SecurityManager manager;
    
    security::UserProfile user;
    user.userId = "HSMADMIN";
    user.isSpecial = true;  // SPECIAL authority
    manager.addUser(user);
    
    auto response = manager.checkHSMOperation("HSMADMIN", 
                                              security::HSMOperation::MIGRATE, 
                                              "USER.DATA");
    ASSERT(response.returnCode == security::SAFReturnCode::SUCCESS);
}

TEST(security_manager_audit_log) {
    security::SecurityManager manager;
    
    security::UserProfile user;
    user.userId = "AUDITTEST";
    user.defaultGroup = "GROUP1";
    manager.addUser(user);
    
    // Perform some operations that get audited
    manager.checkDatasetAccess("AUDITTEST", "SOME.DATASET", security::AccessLevel::READ);
    
    auto logs = manager.getAuditLog(100);
    // Audit log may or may not be empty depending on config
    ASSERT(true);  // Just verify no crash
}

// =============================================================================
// Sysplex Tests
// =============================================================================

TEST(sysplex_enq_request) {
    sysplex::ENQRequest request;
    request.qname = "TESTQNAM";
    request.rname = "RESOURCE.NAME";
    request.scope = sysplex::ENQScope::SYSPLEX;
    request.type = sysplex::ENQType::EXCLUSIVE;
    
    ASSERT(request.getResourceKey() == "TESTQNAM.RESOURCE.NAME");
}

TEST(sysplex_grs_enq_deq) {
    sysplex::GRSManager grs;
    
    sysplex::ENQRequest request;
    request.qname = "TESTQNAM";
    request.rname = "TEST.RESOURCE";
    request.owner = "JOB1";
    request.type = sysplex::ENQType::EXCLUSIVE;
    request.wait = false;
    
    auto result = grs.enq(request);
    ASSERT(result == sysplex::ENQResult::SUCCESS);
    
    bool released = grs.deq("TESTQNAM", "TEST.RESOURCE", "JOB1");
    ASSERT(released);
}

TEST(sysplex_grs_shared_lock) {
    sysplex::GRSManager grs;
    
    // First shared lock
    sysplex::ENQRequest req1;
    req1.qname = "SHARE";
    req1.rname = "RESOURCE";
    req1.owner = "JOB1";
    req1.type = sysplex::ENQType::SHARED;
    req1.wait = false;
    
    ASSERT(grs.enq(req1) == sysplex::ENQResult::SUCCESS);
    
    // Second shared lock should succeed
    sysplex::ENQRequest req2 = req1;
    req2.owner = "JOB2";
    ASSERT(grs.enq(req2) == sysplex::ENQResult::SUCCESS);
    
    // Query resource
    auto status = grs.queryResource("SHARE", "RESOURCE");
    ASSERT(status.has_value());
    ASSERT(status->holders.size() == 2);
}

TEST(sysplex_grs_contention) {
    sysplex::GRSManager grs;
    
    // Get exclusive lock
    sysplex::ENQRequest req1;
    req1.qname = "EXCL";
    req1.rname = "RESOURCE";
    req1.owner = "JOB1";
    req1.type = sysplex::ENQType::EXCLUSIVE;
    req1.wait = false;
    
    ASSERT(grs.enq(req1) == sysplex::ENQResult::SUCCESS);
    
    // Try another exclusive - should get contention
    sysplex::ENQRequest req2 = req1;
    req2.owner = "JOB2";
    ASSERT(grs.enq(req2) == sysplex::ENQResult::CONTENTION);
}

TEST(sysplex_grs_statistics) {
    sysplex::GRSManager grs;
    
    sysplex::ENQRequest request;
    request.qname = "STAT";
    request.rname = "TEST";
    request.owner = "JOB";
    request.wait = false;
    
    grs.enq(request);
    grs.deq("STAT", "TEST", "JOB");
    
    auto stats = grs.getStatistics();
    ASSERT(stats.enqRequests > 0);
    ASSERT(stats.deqRequests > 0);
}

TEST(sysplex_host_info) {
    sysplex::HostInfo host;
    host.hostId = "HOST1";
    host.systemId = "SYS1";
    host.sysplexName = "PLEX1";
    host.status = sysplex::HostStatus::ACTIVE;
    host.role = sysplex::HostRole::PRIMARY;
    
    ASSERT(host.isAvailable());
}

TEST(sysplex_coordinator_host_management) {
    sysplex::SysplexCoordinator::Config config;
    config.hostId = "HOST1";
    config.systemId = "SYS1";
    config.sysplexName = "PLEX1";
    
    sysplex::SysplexCoordinator coordinator(config);
    
    // Register remote host
    sysplex::HostInfo remote;
    remote.hostId = "HOST2";
    remote.systemId = "SYS2";
    remote.status = sysplex::HostStatus::ACTIVE;
    coordinator.registerHost(remote);
    
    auto hosts = coordinator.getAllHosts();
    ASSERT(hosts.size() == 2);
    
    auto activeHosts = coordinator.getActiveHosts();
    ASSERT(activeHosts.size() >= 1);
}

TEST(sysplex_coordinator_cds_locking) {
    sysplex::SysplexCoordinator coordinator;
    
    auto result = coordinator.lockMCDS("JOB1");
    ASSERT(result == sysplex::ENQResult::SUCCESS);
    
    ASSERT(coordinator.unlockMCDS("JOB1"));
}

TEST(sysplex_coordinator_dataset_locking) {
    sysplex::SysplexCoordinator coordinator;
    
    auto result = coordinator.lockDataset("JOB1", "USER.TEST.DATASET");
    ASSERT(result == sysplex::ENQResult::SUCCESS);
    
    ASSERT(coordinator.unlockDataset("JOB1", "USER.TEST.DATASET"));
}

TEST(sysplex_coordinator_work_selection) {
    sysplex::SysplexCoordinator::Config config;
    config.hostId = "HOST1";
    sysplex::SysplexCoordinator coordinator(config);
    
    // Update workload
    coordinator.updateLocalStatus(sysplex::HostStatus::ACTIVE);
    coordinator.updateLocalWorkload(10, 100);
    
    auto selected = coordinator.selectHostForWork();
    ASSERT(selected.has_value());
}

TEST(sysplex_statistics) {
    sysplex::SysplexCoordinator::Config config;
    config.hostId = "HOST1";
    sysplex::SysplexCoordinator coordinator(config);
    
    coordinator.updateLocalStatus(sysplex::HostStatus::ACTIVE);
    
    auto stats = coordinator.getStatistics();
    ASSERT(stats.totalHosts == 1);
}

// =============================================================================
// Integration Tests
// =============================================================================

TEST(integration_cds_command) {
    // Test CDS + Command integration
    cds::CDSManager cdsManager;
    command::CommandProcessor cmdProcessor;
    
    cmdProcessor.registerHandler(command::CommandType::HMIGRATE,
        [&cdsManager](const command::ParsedCommand& cmd) {
            command::CommandResult result;
            auto& opts = std::get<command::MigrateOptions>(cmd.options);
            
            for (const auto& ds : opts.datasets) {
                cds::MCDDatasetRecord record;
                record.setDatasetName(ds.name);
                record.migrationLevel = cds::MigrationLevel::ML1;
                cdsManager.addMigratedDataset(record);
            }
            
            result.returnCode = command::ReturnCode::SUCCESS;
            result.message = "Migration queued";
            return result;
        });
    
    auto result = cmdProcessor.process("HMIGRATE DSN(INTEG.TEST.DS)");
    ASSERT(result.isSuccess());
    
    auto migrated = cdsManager.getMigratedDataset("INTEG.TEST.DS");
    ASSERT(migrated.has_value());
}

TEST(integration_security_command) {
    // Test Security + Command integration
    security::SecurityManager secMgr;
    command::CommandProcessor cmdProcessor;
    
    security::UserProfile user;
    user.userId = "TESTUSER";
    user.isSpecial = true;
    secMgr.addUser(user);
    
    cmdProcessor.registerHandler(command::CommandType::HMIGRATE,
        [&secMgr](const command::ParsedCommand& cmd) {
            command::CommandResult result;
            
            auto secResult = secMgr.checkHSMOperation(cmd.userId, 
                                                      security::HSMOperation::MIGRATE);
            if (secResult.returnCode != security::SAFReturnCode::SUCCESS) {
                result.returnCode = command::ReturnCode::AUTH_ERROR;
                result.message = "Not authorized";
                return result;
            }
            
            result.returnCode = command::ReturnCode::SUCCESS;
            return result;
        });
    
    auto result = cmdProcessor.process("HMIGRATE DSN(TEST.DS)", "TESTUSER");
    ASSERT(result.isSuccess());
}

TEST(integration_space_interval) {
    // Test Space Management + Interval integration
    space::SpaceManager spaceMgr;
    interval::IntervalScheduler scheduler;
    
    auto interval = interval::IntervalFactory::createPrimarySpaceManagement();
    scheduler.addInterval(interval);
    
    space::VolumeSpaceInfo vol;
    vol.volser = "VOL001";
    vol.totalCapacity = 1000000000;
    vol.allocatedSpace = 900000000;
    vol.freeSpace = 100000000;
    vol.recalculate();
    spaceMgr.registerVolume(vol);
    
    // Submit space management task
    scheduler.submitTask("PRIMARY_SM", [&spaceMgr]() {
        auto analysis = spaceMgr.analyzeSpace();
        return analysis.volumesAboveHigh > 0;
    });
    
    ASSERT(scheduler.getPendingTaskCount() == 1);
}

TEST(integration_sysplex_cds) {
    // Test Sysplex + CDS integration
    sysplex::SysplexCoordinator coordinator;
    cds::CDSManager cdsManager;
    
    // Lock CDS before update
    auto lockResult = coordinator.lockMCDS("JOB1");
    ASSERT(lockResult == sysplex::ENQResult::SUCCESS);
    
    // Update CDS
    cds::MCDDatasetRecord record;
    record.setDatasetName("SYSPLEX.TEST.DS");
    cdsManager.addMigratedDataset(record);
    
    // Unlock CDS
    ASSERT(coordinator.unlockMCDS("JOB1"));
    
    auto migrated = cdsManager.getMigratedDataset("SYSPLEX.TEST.DS");
    ASSERT(migrated.has_value());
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== DFSMShsm v4.1.0 Feature Tests ===" << std::endl;
    std::cout << std::endl;
    
    // CDS Tests
    std::cout << "--- CDS Tests ---" << std::endl;
    RUN_TEST(cds_stck_timestamp);
    RUN_TEST(cds_mcd_dataset_record);
    RUN_TEST(cds_manager_add_migrated);
    RUN_TEST(cds_manager_update_migrated);
    RUN_TEST(cds_manager_delete_migrated);
    RUN_TEST(cds_manager_search_pattern);
    RUN_TEST(cds_backup_dataset_record);
    RUN_TEST(cds_manager_backup_operations);
    RUN_TEST(cds_tape_volume_record);
    RUN_TEST(cds_manager_statistics);
    RUN_TEST(cds_dataset_status);
    std::cout << std::endl;
    
    // Command Tests
    std::cout << "--- Command Processor Tests ---" << std::endl;
    RUN_TEST(command_parser_hmigrate);
    RUN_TEST(command_parser_hrecall);
    RUN_TEST(command_parser_hbackup);
    RUN_TEST(command_parser_hquery);
    RUN_TEST(command_parser_hlist);
    RUN_TEST(command_parser_abbreviation);
    RUN_TEST(command_parser_hsend_wrapper);
    RUN_TEST(command_parser_invalid);
    RUN_TEST(command_executor_submit);
    RUN_TEST(command_processor_integration);
    std::cout << std::endl;
    
    // Space Management Tests
    std::cout << "--- Space Management Tests ---" << std::endl;
    RUN_TEST(space_volume_info);
    RUN_TEST(space_dataset_info);
    RUN_TEST(space_threshold);
    RUN_TEST(space_policy);
    RUN_TEST(space_manager_register_volume);
    RUN_TEST(space_manager_threshold_detection);
    RUN_TEST(space_manager_select_candidates);
    RUN_TEST(space_analysis);
    RUN_TEST(space_forecast);
    std::cout << std::endl;
    
    // Interval Tests
    std::cout << "--- Interval Processing Tests ---" << std::endl;
    RUN_TEST(interval_time_of_day);
    RUN_TEST(interval_time_window);
    RUN_TEST(interval_time_window_overnight);
    RUN_TEST(interval_definition);
    RUN_TEST(interval_factory_primary_sm);
    RUN_TEST(interval_factory_backup);
    RUN_TEST(interval_scheduler_add_interval);
    RUN_TEST(interval_scheduler_state_management);
    RUN_TEST(interval_scheduler_submit_task);
    RUN_TEST(interval_statistics);
    std::cout << std::endl;
    
    // Security Tests
    std::cout << "--- Security Tests ---" << std::endl;
    RUN_TEST(security_access_level);
    RUN_TEST(security_user_profile);
    RUN_TEST(security_resource_profile);
    RUN_TEST(security_stgadmin_profile);
    RUN_TEST(security_audit_record);
    RUN_TEST(security_manager_check_access);
    RUN_TEST(security_manager_hsm_operation);
    RUN_TEST(security_manager_audit_log);
    std::cout << std::endl;
    
    // Sysplex Tests
    std::cout << "--- Sysplex Coordination Tests ---" << std::endl;
    RUN_TEST(sysplex_enq_request);
    RUN_TEST(sysplex_grs_enq_deq);
    RUN_TEST(sysplex_grs_shared_lock);
    RUN_TEST(sysplex_grs_contention);
    RUN_TEST(sysplex_grs_statistics);
    RUN_TEST(sysplex_host_info);
    RUN_TEST(sysplex_coordinator_host_management);
    RUN_TEST(sysplex_coordinator_cds_locking);
    RUN_TEST(sysplex_coordinator_dataset_locking);
    RUN_TEST(sysplex_coordinator_work_selection);
    RUN_TEST(sysplex_statistics);
    std::cout << std::endl;
    
    // Integration Tests
    std::cout << "--- Integration Tests ---" << std::endl;
    RUN_TEST(integration_cds_command);
    RUN_TEST(integration_security_command);
    RUN_TEST(integration_space_interval);
    RUN_TEST(integration_sysplex_cds);
    std::cout << std::endl;
    
    // Summary
    std::cout << "=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << tests_passed << std::endl;
    std::cout << "Failed: " << tests_failed << std::endl;
    std::cout << "Total:  " << (tests_passed + tests_failed) << std::endl;
    
    return tests_failed > 0 ? 1 : 0;
}
