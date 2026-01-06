/*
 * DFSMShsm Emulator - v3.6.0 Feature Tests
 * @version 4.2.0
 * 
 * Comprehensive tests for all v3.6.0 features:
 * - SMS Integration
 * - Cloud Tiering
 * - ABARS Enhancement
 * - Common Recall Queue (CRQ)
 * - Fast Replication
 * - Audit & Compliance
 */

#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

#include "hsm_sms.hpp"
#include "hsm_cloud_tier.hpp"
#include "hsm_abars.hpp"
#include "hsm_crq.hpp"
#include "hsm_fast_replication.hpp"
#include "hsm_compliance.hpp"

using namespace dfsmshsm;

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    void test_##name(); \
    struct TestRunner_##name { \
        TestRunner_##name() { \
            tests_run++; \
            std::cout << "Running: " << #name << "... "; \
            try { \
                test_##name(); \
                tests_passed++; \
                std::cout << "PASSED\n"; \
            } catch (const std::exception& e) { \
                tests_failed++; \
                std::cout << "FAILED: " << e.what() << "\n"; \
            } \
        } \
    } test_runner_##name; \
    void test_##name()

#define ASSERT(cond) \
    if (!(cond)) { \
        throw std::runtime_error("Assertion failed: " #cond); \
    }

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        throw std::runtime_error("Assertion failed: " #a " == " #b); \
    }

// ============================================================================
// SMS Integration Tests
// ============================================================================

TEST(sms_storage_class_creation) {
    sms::StorageClass sc("SCHIGH", "High performance storage");
    sc.setPerformanceObjective(sms::PerformanceObjective::HIGH);
    sc.setAvailabilityObjective(sms::AvailabilityObjective::HIGH);
    
    ASSERT_EQ(sc.getName(), "SCHIGH");
    ASSERT(sc.isValid());
    ASSERT(sc.isActive());
}

TEST(sms_data_class_creation) {
    sms::DataClass dc("DCSEQ", "Sequential datasets");
    dc.setDataSetOrganization(sms::DataSetOrganization::PHYSICAL_SEQUENTIAL);
    dc.setRecordFormat(sms::RecordFormat::FIXED_BLOCKED);
    dc.setLogicalRecordLength(80);
    
    ASSERT_EQ(dc.getName(), "DCSEQ");
    ASSERT(dc.isValid());
    ASSERT_EQ(dc.getLogicalRecordLength(), 80u);
}

TEST(sms_management_class_creation) {
    sms::ManagementClass mc("MCSTD", "Standard management");
    mc.enableAutoBackup(sms::BackupFrequency::DAILY);
    mc.enableAutoMigration(30, 60);
    mc.setRetentionDays(365);
    
    ASSERT_EQ(mc.getName(), "MCSTD");
    ASSERT(mc.isValid());
    ASSERT(mc.getBackupPolicy().autoBackup);
    ASSERT(mc.getMigrationPolicy().autoMigrate);
}

TEST(sms_storage_group_creation) {
    sms::StorageGroup sg("SGPRIM", sms::StorageGroup::Type::POOL, "Primary pool");
    sg.addVolume("VOL001", 3339ULL * 1024 * 1024 * 1024);
    sg.addVolume("VOL002", 3339ULL * 1024 * 1024 * 1024);
    sg.setThresholdPercent(85);
    
    ASSERT_EQ(sg.getName(), "SGPRIM");
    ASSERT_EQ(sg.getVolumes().size(), 2u);
    ASSERT(sg.isValid());
}

TEST(sms_acs_routine_creation) {
    sms::ACSRoutine acs("ACSSTOR", sms::ACSRoutineType::STORAGE_CLASS, "Storage class selection");
    acs.addRule(10, "SYS1.*", "SCHIGH", "System datasets");
    acs.addRule(20, "*.TEMP.*", "SCLOW", "Temporary datasets");
    acs.setDefaultClass("SCSTD");
    
    ASSERT_EQ(acs.getName(), "ACSSTOR");
    ASSERT_EQ(acs.getRules().size(), 2u);
}

TEST(sms_manager_allocation) {
    sms::SMSManager manager;
    manager.createDefaultConfiguration();
    
    sms::AllocationRequest request;
    request.dataSetName = "SYS1.LINKLIB";
    request.userId = "TESTUSER";
    
    auto result = manager.processAllocation(request);
    
    ASSERT(result.success);
    ASSERT_EQ(result.storageClass, "SCHIGH");
    ASSERT_EQ(result.managementClass, "MCCRIT");
}

TEST(sms_configuration_validation) {
    sms::SMSConfiguration config("TEST");
    
    config.addStorageClass(sms::createHighPerformanceStorageClass("SCHIGH"));
    config.addDataClass(sms::DataClass("DCSEQ"));
    config.addManagementClass(sms::createStandardManagementClass("MCSTD"));
    
    sms::StorageGroup sg("SGTEST");
    sg.addVolume("VOL001", 1024ULL * 1024 * 1024);
    config.addStorageGroup(sg);
    
    std::vector<std::string> errors;
    bool valid = config.validate(errors);
    ASSERT(valid);
}

// ============================================================================
// Cloud Tiering Tests
// ============================================================================

TEST(cloud_s3_provider_creation) {
    auto provider = cloud::createS3Provider("AKIATEST", "secret123", "us-east-1", "test-bucket");
    
    ASSERT(provider != nullptr);
    ASSERT_EQ(provider->getName(), "Amazon S3");
}

TEST(cloud_s3_connection) {
    auto provider = cloud::createS3Provider("AKIATEST", "secret123", "us-east-1");
    
    ASSERT(provider->connect());
    provider->disconnect();
}

TEST(cloud_bucket_operations) {
    auto provider = cloud::createS3Provider("AKIATEST", "secret123");
    provider->connect();
    
    ASSERT(provider->createBucket("test-bucket-1"));
    ASSERT(provider->bucketExists("test-bucket-1"));
    
    auto buckets = provider->listBuckets();
    ASSERT(buckets.size() >= 1);
    
    ASSERT(provider->deleteBucket("test-bucket-1"));
    ASSERT(!provider->bucketExists("test-bucket-1"));
}

TEST(cloud_migration_upload) {
    auto provider = cloud::createS3Provider("AKIATEST", "secret123");
    provider->connect();
    provider->createBucket("archive-bucket");
    
    cloud::MigrationRequest request;
    request.requestId = "MIG001";
    request.datasetName = "USER.TEST.DATA";
    request.targetBucket = "archive-bucket";
    request.targetClass = cloud::CloudStorageClass::S3_GLACIER;
    request.compressBeforeUpload = true;
    
    auto result = provider->uploadObject(request, nullptr);
    
    ASSERT(result.isSuccess());
    ASSERT(result.bytesTransferred > 0);
}

TEST(cloud_migration_policy) {
    auto policy = cloud::CloudMigrationPolicy::createDefaultPolicy();
    
    ASSERT(policy.isEnabled());
    ASSERT(policy.getRules().size() > 0);
}

TEST(cloud_tier_manager) {
    cloud::CloudTierManager manager;
    
    auto s3Provider = cloud::createS3Provider("AKIATEST", "secret123");
    s3Provider->connect();
    manager.addProvider("s3", s3Provider);
    manager.setDefaultProvider(s3Provider);
    
    cloud::MigrationRequest request;
    request.requestId = "MIG002";
    request.datasetName = "TEST.MIGRATE.DATA";
    request.targetBucket = "archive";
    
    auto result = manager.migrateToCloud(request);
    
    ASSERT(result.isSuccess());
    ASSERT(manager.isInCloud("TEST.MIGRATE.DATA"));
}

TEST(cloud_cost_analysis) {
    cloud::CostAnalyzer analyzer;
    analyzer.setPricing(cloud::StoragePricing::getS3Standard());
    
    auto cost = analyzer.calculateStorageCost(100ULL * 1024 * 1024 * 1024, 30);
    
    ASSERT(cost.storageCost > 0);
    ASSERT(cost.totalCost > 0);
}

// ============================================================================
// ABARS Tests
// ============================================================================

TEST(abars_aggregate_group_creation) {
    abars::AggregateGroup group("SYSAGG", "System aggregate");
    group.addInclude("SYS1.*");
    group.addInclude("SYS2.*");
    group.addExclude("SYS1.DUMP*");
    
    ASSERT_EQ(group.getName(), "SYSAGG");
    ASSERT(group.isValid());
    ASSERT(group.isValid());
    ASSERT(group.getName() == "SYSAGG");
}

TEST(abars_control_file_parsing) {
    abars::ABARSControlFile cf;
    
    std::string content = R"(
DEFINE AGGGROUP(TESTGRP)
  DESCRIPTION('Test aggregate group')
  INCLUDE(DSN(TEST.*))
)";
    
    ASSERT(cf.parse(content));
    ASSERT(!cf.hasErrors());
}

TEST(abars_activity_log) {
    abars::ActivityLog log("TESTLOG");
    
    log.logInfo("Starting backup");
    log.logDatasetProcessed("TEST.DATA1", 1024 * 1024, true);
    log.logDatasetProcessed("TEST.DATA2", 2 * 1024 * 1024, true);
    log.logWarning("Skipped empty dataset");
    
    ASSERT_EQ(log.getDatasetsProcessed(), 2u);
    ASSERT(log.getBytesProcessed() > 0);
    ASSERT_EQ(log.getWarningCount(), 1u);
    ASSERT_EQ(log.getErrorCount(), 0u);
}

TEST(abars_stack_manager) {
    abars::StackManager sm;
    
    uint32_t stack1 = sm.allocateStack("TAPE01", 10ULL * 1024 * 1024 * 1024);
    uint32_t stack2 = sm.allocateStack("TAPE02", 10ULL * 1024 * 1024 * 1024);
    
    ASSERT(stack1 > 0);
    ASSERT(stack2 > 0);
    ASSERT(stack2 != stack1);
    
    ASSERT(sm.writeToStack(stack1, "TEST.DATA1", 100 * 1024 * 1024));
    ASSERT(sm.writeToStack(stack1, "TEST.DATA2", 200 * 1024 * 1024));
    
    auto info = sm.getStackInfo(stack1);
    ASSERT(info.has_value());
    ASSERT_EQ(info->fileCount, 2u);
}

TEST(abars_manager_backup) {
    abars::ABARSManager manager;
    
    abars::AggregateGroup group("USERDATA", "User data backup");
    group.addInclude("USER.*");
    manager.defineGroup(group);
    
    std::vector<std::string> datasets = {
        "USER.DATA1", "USER.DATA2", "USER.DATA3"
    };
    
    auto result = manager.executeABackup("USERDATA", datasets);
    
    ASSERT(result.success);
    ASSERT_EQ(result.datasetsProcessed, 3u);
    ASSERT_EQ(result.datasetsSuccessful, 3u);
    ASSERT(result.bytesProcessed > 0);
}

TEST(abars_manager_recovery) {
    abars::ABARSManager manager;
    
    abars::AggregateGroup group("RECOVERY", "Recovery test");
    group.addInclude("RECOVER.*");
    manager.defineGroup(group);
    
    std::vector<std::string> datasets = {"RECOVER.DATA1", "RECOVER.DATA2"};
    manager.executeABackup("RECOVERY", datasets);
    
    auto result = manager.executeARecover("RECOVERY");
    
    ASSERT(result.success);
    ASSERT_EQ(result.datasetsProcessed, 2u);
}

// ============================================================================
// Common Recall Queue Tests
// ============================================================================

TEST(crq_recall_request_creation) {
    crq::RecallRequest request("USER.MIGRATED.DATA");
    request.setPriority(crq::RecallPriority::HIGH);
    request.setRequestingUser("TESTUSER");
    
    ASSERT(!request.getRequestId().empty());
    ASSERT_EQ(request.getDatasetName(), "USER.MIGRATED.DATA");
}

TEST(crq_host_manager) {
    crq::HostManager hm;
    
    crq::HostInfo host1;
    host1.hostName = "HOST1";
    host1.maxConcurrentRecalls = 10;
    host1.status = crq::HostStatus::ACTIVE;
    
    crq::HostInfo host2;
    host2.hostName = "HOST2";
    host2.maxConcurrentRecalls = 15;
    host2.status = crq::HostStatus::ACTIVE;
    
    ASSERT(hm.registerHost(host1));
    ASSERT(hm.registerHost(host2));
    
    ASSERT_EQ(hm.getHostCount(), 2u);
}

TEST(crq_manager_operations) {
    crq::CRQManager manager;
    
    crq::HostInfo host;
    host.hostName = "HOST1";
    host.maxConcurrentRecalls = 10;
    host.status = crq::HostStatus::ACTIVE;
    manager.getHostManager().registerHost(host);
    
    auto& queue = manager.getQueue();
    crq::RecallRequest req("MIGRATED.DATA1");
    req.setPriority(crq::RecallPriority::NORMAL);
    
    auto id = queue.enqueue(req);
    ASSERT(!id.empty());
    
    auto stats = queue.getStatistics();
    ASSERT(stats.totalRequests > 0);
}

// ============================================================================
// Fast Replication Tests
// ============================================================================

TEST(fastrep_copy_pair_creation) {
    replication::CopyPair pair("SOURCE.DSN", "TARGET.DSN");
    pair.setCopyType(replication::CopyType::FLASHCOPY);
    pair.setSourceSize(100 * 1024 * 1024);
    
    ASSERT(!pair.getPairId().empty());
    ASSERT_EQ(pair.getSourceDsn(), "SOURCE.DSN");
    ASSERT_EQ(pair.getTargetDsn(), "TARGET.DSN");
}

TEST(fastrep_copy_pair_lifecycle) {
    replication::CopyPair pair("SRC.DATA", "TGT.DATA");
    pair.setSourceSize(50 * 1024 * 1024);
    
    pair.establish();
    pair.startCopy();
    ASSERT(pair.isActive());
    
    pair.updateProgress(500, 28 * 1024 * 1024);
    pair.completeCopy();
    ASSERT(pair.isComplete());
}

TEST(fastrep_consistency_group) {
    replication::ConsistencyGroup group("DBGROUP", "Database consistency group");
    group.setReplicationMode(replication::ReplicationMode::SYNCHRONOUS);
    
    ASSERT(group.addPair("PAIR1"));
    ASSERT(group.addPair("PAIR2"));
    ASSERT(group.addPair("PAIR3"));
    
    ASSERT(group.containsPair("PAIR2"));
    ASSERT(!group.containsPair("PAIR99"));
    
    group.enable();
    ASSERT(group.isEnabled());
}

TEST(fastrep_flashcopy_manager) {
    replication::FlashCopyManager manager;
    
    auto result = manager.establishFlashCopy("SOURCE.DATA", "TARGET.DATA");
    
    ASSERT(result.success);
    ASSERT(!result.pairId.empty());
}

TEST(fastrep_manager_with_groups) {
    replication::FlashCopyManager manager;
    
    manager.createConsistencyGroup("TESTGRP", "Test group");
    
    replication::FlashCopyOptions options;
    options.consistencyGroup = "TESTGRP";
    
    auto result = manager.establishFlashCopy("GRP.SOURCE", "GRP.TARGET", options);
    
    ASSERT(result.success);
}

TEST(fastrep_replication_manager) {
    replication::ReplicationManager manager;
    
    auto stats = manager.getStatistics();
    ASSERT_EQ(stats.copyOperations, 0u);
    
    // Test report generation
    auto report = manager.generateSummaryReport();
    ASSERT(!report.empty());
}

// ============================================================================
// Compliance Tests
// ============================================================================

TEST(compliance_retention_policy_creation) {
    auto policy = compliance::createStandardRetentionPolicy("STDRET", 7);
    
    ASSERT(!policy.getPolicyId().empty());
    ASSERT_EQ(policy.getPolicyName(), "STDRET");
    ASSERT(policy.isActive());
}

TEST(compliance_sec_17a4_policy) {
    auto policy = compliance::createSEC17a4Policy("SEC17A4");
    
    ASSERT_EQ(policy.getDefaultPeriod().years, 6u);
}

TEST(compliance_retention_record_creation) {
    auto policy = compliance::createStandardRetentionPolicy("TEST", 5);
    auto record = policy.createRecord("USER.IMPORTANT.DATA", "ADMIN");
    
    ASSERT(!record.recordId.empty());
    ASSERT_EQ(record.datasetName, "USER.IMPORTANT.DATA");
    ASSERT(!record.isExpired());
}

TEST(compliance_legal_hold_manager) {
    compliance::LegalHoldManager manager;
    
    std::string holdId = manager.createHold("CASE2024-001", "LIT-2024-001", "LEGAL_DEPT");
    ASSERT(!holdId.empty());
    
    ASSERT(manager.activateHold(holdId));
    ASSERT(manager.addDatasetToHold(holdId, "USER.CASE.DATA1"));
    ASSERT(manager.addDatasetToHold(holdId, "USER.CASE.DATA2"));
    
    ASSERT(manager.isDatasetUnderHold("USER.CASE.DATA1"));
    ASSERT(!manager.isDatasetUnderHold("USER.OTHER.DATA"));
    
    auto holds = manager.getHoldsForDataset("USER.CASE.DATA1");
    ASSERT_EQ(holds.size(), 1u);
}

TEST(compliance_worm_volume_manager) {
    compliance::WORMVolumeManager manager;
    
    std::string volser = manager.createVolume("WORM01", 10ULL * 1024 * 1024 * 1024);
    ASSERT_EQ(volser, "WORM01");
    
    ASSERT(manager.writeToVolume("WORM01", "ARCHIVE.DATA1", 100 * 1024 * 1024));
    ASSERT(manager.writeToVolume("WORM01", "ARCHIVE.DATA2", 200 * 1024 * 1024));
    
    auto info = manager.getVolumeInfo("WORM01");
    ASSERT(info.has_value());
    ASSERT_EQ(info->fileCount, 2u);
    ASSERT(info->canWrite());
    
    ASSERT(manager.sealVolume("WORM01"));
    
    info = manager.getVolumeInfo("WORM01");
    ASSERT(!info->canWrite());
}

TEST(compliance_audit_log) {
    compliance::AuditLog log("TEST");
    
    log.log(compliance::AuditEventType::DATA_CREATE, compliance::AuditSeverity::INFO,
           "TESTUSER", "NEW.DATASET", "CREATE", "SUCCESS");
    
    log.logDataAccess("TESTUSER", "SENSITIVE.DATA", 
                     compliance::AuditEventType::DATA_READ, true);
    
    log.logComplianceViolation("UNRESTRICTED.DATA", 
                               "Missing retention policy",
                               compliance::ComplianceStandard::SEC_17A4);
    
    ASSERT_EQ(log.size(), 3u);
    
    auto entries = log.getEntries(10);
    ASSERT_EQ(entries.size(), 3u);
}

TEST(compliance_manager_retention) {
    compliance::ComplianceManager manager;
    
    auto policy = compliance::createStandardRetentionPolicy("CORPRET", 7);
    manager.addRetentionPolicy(policy);
    
    std::string recordId = manager.applyRetention("USER.CRITICAL.DATA", 
                                                   policy.getPolicyId(), 
                                                   "ADMIN");
    ASSERT(!recordId.empty());
    
    ASSERT(!manager.canDeleteDataset("USER.CRITICAL.DATA"));
}

TEST(compliance_manager_legal_hold) {
    compliance::ComplianceManager manager;
    
    auto& lhm = manager.getLegalHoldManager();
    
    std::string holdId = compliance::createLegalHold(
        lhm, "INVESTIGATION", "INV-2024", "LEGAL",
        {"EVIDENCE.DATA1", "EVIDENCE.DATA2"});
    
    ASSERT(!holdId.empty());
    ASSERT(!manager.canDeleteDataset("EVIDENCE.DATA1"));
}

TEST(compliance_check) {
    compliance::ComplianceManager manager;
    
    auto policy = compliance::createSEC17a4Policy("SEC17A4");
    manager.addRetentionPolicy(policy);
    
    manager.getWORMManager().createVolume("WORM01", 10ULL * 1024 * 1024 * 1024);
    
    auto result = manager.checkCompliance(compliance::ComplianceStandard::SEC_17A4);
    
    ASSERT(result.totalChecks > 0);
    ASSERT(result.getComplianceScore() > 0);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(integration_sms_cloud_migration) {
    sms::SMSManager smsManager;
    smsManager.createDefaultConfiguration();
    
    cloud::CloudTierManager cloudManager;
    auto s3Provider = cloud::createS3Provider("AKIATEST", "secret123");
    s3Provider->connect();
    cloudManager.addProvider("s3", s3Provider);
    cloudManager.setDefaultProvider(s3Provider);
    
    sms::AllocationRequest allocReq;
    allocReq.dataSetName = "USER.ARCHIVE.DATA";
    allocReq.userId = "TESTUSER";
    
    auto allocResult = smsManager.processAllocation(allocReq);
    ASSERT(allocResult.success);
    
    cloud::MigrationRequest migReq;
    migReq.datasetName = "USER.ARCHIVE.DATA";
    migReq.targetBucket = "archive";
    
    auto migResult = cloudManager.migrateToCloud(migReq);
    ASSERT(migResult.isSuccess());
}

TEST(integration_abars_compliance) {
    abars::ABARSManager abarsManager;
    compliance::ComplianceManager compManager;
    
    auto policy = compliance::createStandardRetentionPolicy("BACKUP", 7);
    compManager.addRetentionPolicy(policy);
    
    abars::AggregateGroup group("REGULATED", "Regulated data");
    group.addInclude("REGULATED.*");
    abarsManager.defineGroup(group);
    
    std::vector<std::string> datasets = {"REGULATED.DATA1", "REGULATED.DATA2"};
    auto result = abarsManager.executeABackup("REGULATED", datasets);
    
    ASSERT(result.success);
    
    for (const auto& dsn : datasets) {
        std::string recordId = compManager.applyRetention(dsn, policy.getPolicyId(), "SYSTEM");
        ASSERT(!recordId.empty());
    }
    
    ASSERT(!compManager.canDeleteDataset("REGULATED.DATA1"));
}

TEST(integration_fastrep_with_crq) {
    replication::FlashCopyManager frManager;
    crq::CRQManager crqManager;
    
    crq::HostInfo host;
    host.hostName = "HOST1";
    host.maxConcurrentRecalls = 10;
    host.status = crq::HostStatus::ACTIVE;
    crqManager.getHostManager().registerHost(host);
    
    auto fcResult = frManager.establishFlashCopy("PROD.DATA", "DR.DATA");
    ASSERT(fcResult.success);
    
    crq::RecallRequest req("RECOVERED.DATA");
    req.setPriority(crq::RecallPriority::CRITICAL);
    
    auto& queue = crqManager.getQueue();
    auto recallId = queue.enqueue(req);
    ASSERT(!recallId.empty());
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "DFSMShsm v3.6.0 Feature Tests\n";
    std::cout << "========================================\n\n";
    
    // Tests run automatically via static initialization
    
    std::cout << "\n========================================\n";
    std::cout << "Test Results:\n";
    std::cout << "  Total:  " << tests_run << "\n";
    std::cout << "  Passed: " << tests_passed << "\n";
    std::cout << "  Failed: " << tests_failed << "\n";
    std::cout << "========================================\n";
    
    if (tests_failed > 0) {
        std::cout << "\n*** SOME TESTS FAILED ***\n";
        return 1;
    }
    
    std::cout << "\n*** ALL TESTS PASSED ***\n";
    return 0;
}
