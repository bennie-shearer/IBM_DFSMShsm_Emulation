/**
 * @file test_all.cpp
 * @brief Comprehensive test suite for DFSMShsm v4.1.0
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */

#include "test_framework.hpp"
#include "hsm_types.hpp"
#include "dataset_catalog.hpp"
#include "volume_manager.hpp"
#include "migration_manager.hpp"
#include "recall_manager.hpp"
#include "backup_manager.hpp"
#include "deduplication_manager.hpp"
#include "quota_manager.hpp"
#include "journal_manager.hpp"
#include "encryption_manager.hpp"
#include "snapshot_manager.hpp"
#include "replication_manager.hpp"
#include "scheduler.hpp"
#include "storage_tier.hpp"
#include "compression.hpp"
#include "cache_manager.hpp"
#include "thread_pool.hpp"
#include "metrics_exporter.hpp"
#include "rest_api.hpp"
#include "metrics_exporter.hpp"
#include "rest_api.hpp"
#include "hsm_engine.hpp"

#include <filesystem>
#include <thread>
#include <chrono>
#include <atomic>

using namespace dfsmshsm;
using namespace dfsmshsm::testing;

// Storage Tier Tests
TEST_CASE(StorageTierManager_Creation) {
    StorageTierManager tier("ML1", StorageTier::MIGRATION_1);
    REQUIRE(tier.start());
    REQUIRE_EQ(tier.getName(), "ML1");
    REQUIRE(tier.isOnline());
    tier.stop();
}

TEST_CASE(StorageTierManager_VolumeCreation) {
    StorageTierManager tier("PRIMARY", StorageTier::PRIMARY);
    REQUIRE(tier.start());
    REQUIRE(tier.createVolume("VOL001", 1048576));
    REQUIRE(tier.volumeExists("VOL001"));
    auto info = tier.getVolumeInfo("VOL001");
    REQUIRE(info.has_value());
    REQUIRE_EQ(info->name, "VOL001");
    REQUIRE_EQ(info->total_space, 1048576ULL);
    tier.stop();
}

TEST_CASE(StorageTierManager_SpaceManagement) {
    StorageTierManager tier("TEST", StorageTier::PRIMARY);
    REQUIRE(tier.start());
    REQUIRE(tier.createVolume("VOL001", 1048576));
    uint64_t total, used, avail;
    REQUIRE(tier.getSpaceInfo(total, used, avail));
    REQUIRE_EQ(total, 1048576ULL);
    REQUIRE_EQ(used, 0ULL);
    REQUIRE(tier.allocateSpace("VOL001", 1024));
    REQUIRE(tier.getSpaceInfo(total, used, avail));
    REQUIRE_EQ(used, 1024ULL);
    tier.stop();
}

TEST_CASE(StorageTierManager_MultipleVolumes) {
    StorageTierManager tier("MULTI", StorageTier::MIGRATION_1);
    REQUIRE(tier.start());
    REQUIRE(tier.createVolume("VOL001", 1048576));
    REQUIRE(tier.createVolume("VOL002", 2097152));
    auto vols = tier.getVolumes();
    REQUIRE_EQ(vols.size(), 2ULL);
    tier.stop();
}

// Catalog Tests
TEST_CASE(Catalog_AddDataset) {
    DatasetCatalog catalog;
    DatasetInfo ds;
    ds.name = "TEST.DATASET";
    ds.size = 1024;
    ds.tier = StorageTier::PRIMARY;
    REQUIRE(catalog.addDataset(ds));
    REQUIRE(catalog.exists("TEST.DATASET"));
    REQUIRE(!catalog.addDataset(ds));
}

TEST_CASE(Catalog_RemoveDataset) {
    DatasetCatalog catalog;
    DatasetInfo ds;
    ds.name = "TEST.DATASET";
    catalog.addDataset(ds);
    REQUIRE(catalog.removeDataset("TEST.DATASET"));
    REQUIRE(!catalog.exists("TEST.DATASET"));
}

TEST_CASE(Catalog_QueryByTier) {
    DatasetCatalog catalog;
    for (int i = 0; i < 5; ++i) {
        DatasetInfo ds;
        ds.name = "DS" + std::to_string(i);
        ds.tier = (i % 2 == 0) ? StorageTier::PRIMARY : StorageTier::MIGRATION_1;
        catalog.addDataset(ds);
    }
    auto primary = catalog.getDatasetsByTier(StorageTier::PRIMARY);
    REQUIRE_EQ(primary.size(), 3ULL);
}

// Migration Tests
TEST_CASE(Migration_BasicMigration) {
    auto catalog = std::make_shared<DatasetCatalog>();
    auto volumes = std::make_shared<VolumeManager>();
    volumes->createVolume("PRIMARY01", StorageTier::PRIMARY, 10485760);
    volumes->createVolume("ML1VOL01", StorageTier::MIGRATION_1, 104857600);
    
    DatasetInfo ds;
    ds.name = "MIGRATE.TEST";
    ds.size = 1024;
    ds.volume = "PRIMARY01";
    ds.tier = StorageTier::PRIMARY;
    ds.state = DatasetState::ACTIVE;
    catalog->addDataset(ds);
    volumes->allocateSpace("PRIMARY01", 1024, "MIGRATE.TEST");
    
    MigrationManager migration(catalog, volumes);
    migration.start();
    auto result = migration.migrateDataset("MIGRATE.TEST", StorageTier::MIGRATION_1);
    REQUIRE(result.status == OperationStatus::COMPLETED);
    migration.stop();
}

// Backup Tests
TEST_CASE(Backup_Initialization) {
    auto catalog = std::make_shared<DatasetCatalog>();
    auto volumes = std::make_shared<VolumeManager>();
    BackupManager backup(catalog, volumes);
    REQUIRE(backup.start());
    REQUIRE(backup.isRunning());
    backup.stop();
    REQUIRE(!backup.isRunning());
}

TEST_CASE(Backup_BasicBackup) {
    auto catalog = std::make_shared<DatasetCatalog>();
    auto volumes = std::make_shared<VolumeManager>();
    volumes->createVolume("PRIMARY01", StorageTier::PRIMARY, 10485760);
    volumes->createVolume("BACKUP01", StorageTier::BACKUP, 104857600);
    
    DatasetInfo ds;
    ds.name = "BACKUP.TEST";
    ds.size = 1024;
    ds.volume = "PRIMARY01";
    catalog->addDataset(ds);
    
    BackupManager backup(catalog, volumes);
    backup.start();
    auto result = backup.backupDataset("BACKUP.TEST");
    REQUIRE(result.status == OperationStatus::COMPLETED);
    backup.stop();
}

// Recall Tests
TEST_CASE(Recall_Initialization) {
    auto catalog = std::make_shared<DatasetCatalog>();
    auto volumes = std::make_shared<VolumeManager>();
    RecallManager recall(catalog, volumes);
    REQUIRE(recall.start());
    REQUIRE(recall.isRunning());
    recall.stop();
}

TEST_CASE(Recall_BasicRecall) {
    auto catalog = std::make_shared<DatasetCatalog>();
    auto volumes = std::make_shared<VolumeManager>();
    volumes->createVolume("PRIMARY01", StorageTier::PRIMARY, 10485760);
    volumes->createVolume("ML1VOL01", StorageTier::MIGRATION_1, 104857600);
    
    DatasetInfo ds;
    ds.name = "RECALL.TEST";
    ds.size = 1024;
    ds.volume = "ML1VOL01";
    ds.tier = StorageTier::MIGRATION_1;
    ds.state = DatasetState::MIGRATED;
    catalog->addDataset(ds);
    volumes->allocateSpace("ML1VOL01", 1024, "RECALL.TEST");
    
    RecallManager recall(catalog, volumes);
    recall.start();
    auto result = recall.recallDataset("RECALL.TEST");
    REQUIRE(result.status == OperationStatus::COMPLETED);
    recall.stop();
}

// Compression Tests
TEST_CASE(Compression_RLE_BasicCompress) {
    std::vector<uint8_t> data = {1, 1, 1, 1, 1, 2, 2, 2, 2, 3};
    auto compressed = Compression::compressRLE(data);
    REQUIRE_LT(compressed.size(), data.size());
    auto decompressed = Compression::decompressRLE(compressed);
    REQUIRE(decompressed == data);
}

TEST_CASE(Compression_RLE_NoRuns) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
    auto compressed = Compression::compressRLE(data);
    auto decompressed = Compression::decompressRLE(compressed);
    REQUIRE(decompressed == data);
}

TEST_CASE(CompressedBuffer_Usage) {
    CompressedBuffer buffer;
    std::vector<uint8_t> data = {1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2};
    buffer.setData(data, CompressionType::RLE);
    REQUIRE_LT(buffer.compressedSize(), buffer.originalSize());
    REQUIRE(buffer.getData() == data);
}

// Deduplication Tests (v4.1.0)
TEST_CASE(Dedupe_StoreAndRetrieve) {
    DeduplicationManager dedupe;
    REQUIRE(dedupe.start());
    std::vector<uint8_t> data(10000, 'A');
    auto hashes = dedupe.storeWithDedupe("DATASET1", data);
    REQUIRE(!hashes.empty());
    auto retrieved = dedupe.retrieve("DATASET1");
    REQUIRE(retrieved.has_value());
    REQUIRE(*retrieved == data);
    dedupe.stop();
}

TEST_CASE(Dedupe_DuplicateDetection) {
    DeduplicationManager dedupe;
    dedupe.start();
    std::vector<uint8_t> data(10000, 'B');
    dedupe.storeWithDedupe("DATASET1", data);
    dedupe.storeWithDedupe("DATASET2", data);
    auto stats = dedupe.getStatistics();
    REQUIRE_GT(stats.duplicate_blocks, 0ULL);
    dedupe.stop();
}

TEST_CASE(Dedupe_RemoveDataset) {
    DeduplicationManager dedupe;
    dedupe.start();
    std::vector<uint8_t> data(1000, 'C');
    dedupe.storeWithDedupe("DATASET1", data);
    REQUIRE(dedupe.removeDataset("DATASET1"));
    auto retrieved = dedupe.retrieve("DATASET1");
    REQUIRE(!retrieved.has_value());
    dedupe.stop();
}

TEST_CASE(Dedupe_Integrity) {
    DeduplicationManager dedupe;
    dedupe.start();
    std::vector<uint8_t> data(5000, 'F');
    dedupe.storeWithDedupe("VERIFY", data);
    REQUIRE(dedupe.verifyIntegrity("VERIFY"));
    dedupe.stop();
}

// Quota Tests (v4.1.0)
TEST_CASE(Quota_SetAndGet) {
    QuotaManager quota;
    quota.start();
    REQUIRE(quota.setQuota("USER1", false, 1048576, 100));
    auto info = quota.getQuota("USER1", false);
    REQUIRE(info.has_value());
    REQUIRE_EQ(info->space_limit, 1048576ULL);
    quota.stop();
}

TEST_CASE(Quota_CheckWithinLimit) {
    QuotaManager quota;
    quota.start();
    quota.setQuota("USER1", false, 1048576);
    auto result = quota.checkQuota("USER1", "", 1024, 1);
    REQUIRE(result == QuotaCheckResult::OK);
    quota.stop();
}

TEST_CASE(Quota_CheckExceedsLimit) {
    QuotaManager quota;
    quota.start();
    quota.setQuota("USER1", false, 1048576);
    quota.updateUsage("USER1", "", 1000000, 0);
    auto result = quota.checkQuota("USER1", "", 100000, 1);
    REQUIRE(result == QuotaCheckResult::HARD_EXCEEDED);
    quota.stop();
}

TEST_CASE(Quota_UsageTracking) {
    QuotaManager quota;
    quota.start();
    quota.setQuota("USER1", false, 1048576);
    quota.updateUsage("USER1", "", 1024, 1);
    quota.updateUsage("USER1", "", 2048, 2);
    auto usage = quota.getUsage("USER1", false);
    REQUIRE(usage.has_value());
    REQUIRE_EQ(usage->space_used, 3072ULL);
    quota.stop();
}

// Journal Tests (v4.1.0)
TEST_CASE(Journal_BeginAndCommit) {
    std::filesystem::path test_dir = "/tmp/hsm_journal_test";
    std::filesystem::create_directories(test_dir);
    JournalManager journal(test_dir);
    REQUIRE(journal.start());
    uint64_t txn = journal.beginTransaction();
    REQUIRE_GT(txn, 0ULL);
    journal.write(txn, JournalEntryType::DATA_WRITE, "TEST.DS");
    REQUIRE(journal.commit(txn));
    journal.stop();
    std::filesystem::remove_all(test_dir);
}

TEST_CASE(Journal_Rollback) {
    std::filesystem::path test_dir = "/tmp/hsm_journal_test2";
    std::filesystem::create_directories(test_dir);
    JournalManager journal(test_dir);
    journal.start();
    uint64_t txn = journal.beginTransaction();
    journal.write(txn, JournalEntryType::DATA_WRITE, "TEST.DS");
    REQUIRE(journal.rollback(txn));
    auto stats = journal.getStatistics();
    REQUIRE_EQ(stats.rolled_back_transactions, 1ULL);
    journal.stop();
    std::filesystem::remove_all(test_dir);
}

// Encryption Tests (v4.1.0)
TEST_CASE(Encryption_GenerateKey) {
    EncryptionManager encryption;
    encryption.start();
    std::string key_id = encryption.generateKey("TestKey");
    REQUIRE(!key_id.empty());
    auto info = encryption.getKeyInfo(key_id);
    REQUIRE(info.has_value());
    REQUIRE_EQ(info->name, "TestKey");
    encryption.stop();
}

TEST_CASE(Encryption_EncryptDecrypt) {
    EncryptionManager encryption;
    encryption.start();
    std::string key_id = encryption.generateKey("TestKey");
    std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto encrypted = encryption.encrypt(data, key_id);
    REQUIRE(encrypted.has_value());
    REQUIRE_NE(*encrypted, data);
    auto decrypted = encryption.decrypt(*encrypted, key_id);
    REQUIRE(decrypted.has_value());
    REQUIRE(*decrypted == data);
    encryption.stop();
}

TEST_CASE(Encryption_KeyRotation) {
    EncryptionManager encryption;
    encryption.start();
    std::string old_key = encryption.generateKey("OldKey");
    auto new_key = encryption.rotateKey(old_key);
    REQUIRE(new_key.has_value());
    REQUIRE_NE(*new_key, old_key);
    encryption.stop();
}

// Snapshot Tests (v4.1.0)
TEST_CASE(Snapshot_Create) {
    SnapshotManager snapshots;
    snapshots.start();
    auto snap_id = snapshots.createSnapshot("DATASET1", "Test snapshot");
    REQUIRE(snap_id.has_value());
    auto info = snapshots.getSnapshot(*snap_id);
    REQUIRE(info.has_value());
    REQUIRE_EQ(info->dataset_name, "DATASET1");
    snapshots.stop();
}

TEST_CASE(Snapshot_Delete) {
    SnapshotManager snapshots;
    snapshots.start();
    auto snap_id = snapshots.createSnapshot("DATASET1", "To delete");
    REQUIRE(snapshots.deleteSnapshot(*snap_id));
    auto info = snapshots.getSnapshot(*snap_id);
    REQUIRE(!info.has_value());
    snapshots.stop();
}

TEST_CASE(Snapshot_Restore) {
    SnapshotManager snapshots;
    snapshots.start();
    auto snap_id = snapshots.createSnapshot("DATASET1", "Restore test");
    REQUIRE(snapshots.restoreFromSnapshot(*snap_id));
    auto stats = snapshots.getStatistics();
    REQUIRE_EQ(stats.restores_performed, 1ULL);
    snapshots.stop();
}

// Replication Tests (v4.1.0)
TEST_CASE(Replication_CreateReplica) {
    ReplicationManager replication;
    replication.start();
    auto rep_id = replication.createReplica("DATASET1", "VOL1", "VOL2", StorageTier::MIGRATION_1, true);
    REQUIRE(rep_id.has_value());
    auto info = replication.getReplica(*rep_id);
    REQUIRE(info.has_value());
    REQUIRE_EQ(info->dataset_name, "DATASET1");
    replication.stop();
}

TEST_CASE(Replication_SyncReplica) {
    ReplicationManager replication;
    replication.start();
    auto rep_id = replication.createReplica("DATASET1", "VOL1", "VOL2", StorageTier::BACKUP, true);
    REQUIRE(replication.syncReplica(*rep_id));
    replication.stop();
}

// Scheduler Tests (v4.1.0)
TEST_CASE(Scheduler_AddTask) {
    Scheduler scheduler;
    scheduler.start();
    std::string task_id = scheduler.scheduleTask("TestTask", OperationType::OP_BACKUP,
                                                  ScheduleFrequency::DAILY,
                                                  [](const ScheduledTask&) { return true; });
    REQUIRE(!task_id.empty());
    auto task = scheduler.getTask(task_id);
    REQUIRE(task.has_value());
    REQUIRE_EQ(task->name, "TestTask");
    scheduler.stop();
}

TEST_CASE(Scheduler_CancelTask) {
    Scheduler scheduler;
    scheduler.start();
    std::string task_id = scheduler.scheduleTask("TestTask", OperationType::OP_MIGRATE,
                                                  ScheduleFrequency::HOURLY,
                                                  [](const ScheduledTask&) { return true; });
    REQUIRE(scheduler.cancelTask(task_id));
    auto task = scheduler.getTask(task_id);
    REQUIRE(!task.has_value());
    scheduler.stop();
}

TEST_CASE(Scheduler_TriggerTask) {
    Scheduler scheduler;
    scheduler.start();
    std::atomic<bool> executed{false};
    std::string task_id = scheduler.scheduleTask("TestTask", OperationType::OP_HEALTH_CHECK,
                                                  ScheduleFrequency::MONTHLY,
                                                  [&executed](const ScheduledTask&) { executed = true; return true; });
    REQUIRE(scheduler.triggerTask(task_id));
    REQUIRE(executed.load());
    scheduler.stop();
}

// Cache Tests
TEST_CASE(LRUCache_BasicOperations) {
    LRUCache<std::string, int> cache(3);
    cache.put("a", 1);
    cache.put("b", 2);
    cache.put("c", 3);
    REQUIRE_EQ(*cache.get("a"), 1);
    REQUIRE_EQ(*cache.get("b"), 2);
    REQUIRE_EQ(*cache.get("c"), 3);
}

TEST_CASE(LRUCache_Eviction) {
    LRUCache<std::string, int> cache(2);
    cache.put("a", 1);
    cache.put("b", 2);
    cache.put("c", 3);
    REQUIRE(!cache.get("a").has_value());
    REQUIRE_EQ(*cache.get("b"), 2);
}

// Thread Pool Tests
TEST_CASE(ThreadPool_BasicSubmit) {
    ThreadPool pool(2);
    auto future = pool.submit([]() { return 42; });
    REQUIRE_EQ(future.get(), 42);
}

TEST_CASE(ThreadPool_MultipleTasks) {
    ThreadPool pool(4);
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.submit([i]() { return i * 2; }));
    }
    for (int i = 0; i < 10; ++i) {
        REQUIRE_EQ(futures[i].get(), i * 2);
    }
}

// HSM Engine Integration Tests
TEST_CASE(Engine_Initialize) {
    std::filesystem::path test_dir = "/tmp/hsm_engine_test";
    HSMEngine engine;
    REQUIRE(engine.initialize(test_dir));
    REQUIRE(engine.start());
    REQUIRE(engine.isRunning());
    engine.stop();
    std::filesystem::remove_all(test_dir);
}

TEST_CASE(Engine_CreateDeleteDataset) {
    std::filesystem::path test_dir = "/tmp/hsm_engine_test2";
    HSMEngine engine;
    engine.initialize(test_dir);
    engine.start();
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 10485760);
    auto result = engine.createDataset("TEST.DATA", 1024);
    REQUIRE(result.status == OperationStatus::COMPLETED);
    auto ds = engine.getDataset("TEST.DATA");
    REQUIRE(ds.has_value());
    auto del_result = engine.deleteDataset("TEST.DATA");
    REQUIRE(del_result.status == OperationStatus::COMPLETED);
    engine.stop();
    std::filesystem::remove_all(test_dir);
}

TEST_CASE(Engine_QuotaEnforcement) {
    std::filesystem::path test_dir = "/tmp/hsm_engine_test3";
    HSMEngine engine;
    engine.initialize(test_dir);
    engine.start();
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 10485760);
    engine.setUserQuota("USER1", 2048);
    auto result1 = engine.createDataset("DS1", 1024, "", "USER1");
    REQUIRE(result1.status == OperationStatus::COMPLETED);
    auto result2 = engine.createDataset("DS2", 2048, "", "USER1");
    REQUIRE(result2.status == OperationStatus::FAILED);
    engine.stop();
    std::filesystem::remove_all(test_dir);
}

TEST_CASE(Engine_Snapshot) {
    std::filesystem::path test_dir = "/tmp/hsm_engine_test4";
    HSMEngine engine;
    engine.initialize(test_dir);
    engine.start();
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 10485760);
    engine.createDataset("SNAP.TEST", 1024);
    auto snap_id = engine.createSnapshot("SNAP.TEST", "Test");
    REQUIRE(snap_id.has_value());
    engine.stop();
    std::filesystem::remove_all(test_dir);
}

TEST_CASE(Engine_Encryption) {
    std::filesystem::path test_dir = "/tmp/hsm_engine_test5";
    HSMEngine engine;
    engine.initialize(test_dir);
    engine.start();
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 10485760);
    engine.createDataset("ENC.TEST", 1024);
    auto result = engine.encryptDataset("ENC.TEST");
    REQUIRE(result.status == OperationStatus::COMPLETED);
    auto ds = engine.getDataset("ENC.TEST");
    REQUIRE(ds->is_encrypted);
    engine.stop();
    std::filesystem::remove_all(test_dir);
}

TEST_CASE(Engine_Report) {
    std::filesystem::path test_dir = "/tmp/hsm_engine_test6";
    HSMEngine engine;
    engine.initialize(test_dir);
    engine.start();
    std::string report = engine.generateFullReport();
    REQUIRE(!report.empty());
    REQUIRE(report.find("v4.1.0") != std::string::npos);
    engine.stop();
    std::filesystem::remove_all(test_dir);
}

// LZ4 Compression Tests (v4.1.0)
TEST_CASE(Compression_LZ4_Basic) {
    std::vector<uint8_t> data;
    for (int i = 0; i < 100; ++i) for (uint8_t j = 0; j < 10; ++j) data.push_back(j);
    auto compressed = Compression::compressLZ4(data);
    auto decompressed = Compression::decompressLZ4(compressed);
    REQUIRE(decompressed == data);
}

TEST_CASE(Compression_ZSTD_Basic) {
    std::vector<uint8_t> data;
    for (int i = 0; i < 50; ++i) for (uint8_t j = 0; j < 20; ++j) data.push_back((uint8_t)(65 + (j % 10)));
    auto compressed = Compression::compressZSTD(data);
    auto decompressed = Compression::decompressZSTD(compressed);
    REQUIRE(decompressed == data);
}

TEST_CASE(Compression_GenericInterface) {
    std::vector<uint8_t> data;
    for (int i = 0; i < 100; ++i) data.push_back(i % 5);
    auto lz4 = Compression::compress(data, CompressionType::LZ4);
    REQUIRE(Compression::decompress(lz4, CompressionType::LZ4) == data);
    auto zstd = Compression::compress(data, CompressionType::ZSTD);
    REQUIRE(Compression::decompress(zstd, CompressionType::ZSTD) == data);
}

// Metrics Tests (v4.1.0)
TEST_CASE(Metrics_Counter) {
    MetricsRegistry::instance().reset();
    MetricsRegistry::instance().registerCounter("test_counter", "Test");
    MetricsRegistry::instance().incrementCounter("test_counter", 6.0);
    std::string out = MetricsRegistry::instance().exportPrometheus();
    REQUIRE(out.find("test_counter") != std::string::npos);
}

TEST_CASE(Metrics_Gauge) {
    MetricsRegistry::instance().reset();
    MetricsRegistry::instance().registerGauge("test_gauge", "Test");
    MetricsRegistry::instance().setGauge("test_gauge", 42.5);
    std::string out = MetricsRegistry::instance().exportPrometheus();
    REQUIRE(out.find("test_gauge") != std::string::npos);
}

TEST_CASE(Metrics_Histogram) {
    MetricsRegistry::instance().reset();
    MetricsRegistry::instance().registerHistogram("test_hist", "Test", {0.1, 0.5, 1.0});
    MetricsRegistry::instance().observeHistogram("test_hist", 0.3);
    std::string out = MetricsRegistry::instance().exportPrometheus();
    REQUIRE(out.find("test_hist_bucket") != std::string::npos);
}

// REST API Tests (v4.1.0)
TEST_CASE(REST_HttpResponse) {
    HttpResponse resp;
    resp.setJSON(R"({"ok":true})");
    REQUIRE(resp.content_type == "application/json");
    REQUIRE(resp.body.find("ok") != std::string::npos);
}

TEST_CASE(REST_RouteRegistration) {
    RESTServer server(0);
    server.get("/test", [](const HttpRequest&) { HttpResponse r; r.setJSON("{}"); return r; }, "Test");
    auto routes = server.getRoutes();
    REQUIRE(!routes.empty());
    REQUIRE(routes[0].pattern == "/test");
}

int main(int argc, char* argv[]) {
    std::string filter;
    if (argc > 1) filter = argv[1];
    return TestRunner::instance().run(filter);
}
