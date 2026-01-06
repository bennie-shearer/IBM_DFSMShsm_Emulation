# IBM DFSMShsm (Hierarchy Storage Manager) Emulation -  API Reference
Version 4.2.0

## Table of Contents

1. [Overview](#overview)
2. [HSMEngine API](#hsmengine-api)
3. [Dataset Operations](#dataset-operations)
4. [Migration & Recall](#migration--recall)
5. [Backup & Recovery](#backup--recovery)
6. [Deduplication API](#deduplication-api)
7. [Quota Management API](#quota-management-api)
8. [Journaling API](#journaling-api)
9. [Encryption API](#encryption-api)
10. [Snapshot API](#snapshot-api)
11. [Replication API](#replication-api)
12. [Scheduling API](#scheduling-api)
13. [Logging API (v3.5.0)](#logging-api)
14. [Benchmark API (v3.5.0)](#benchmark-api)
15. [Tape Pool API (v3.5.0)](#tape-pool-api)
16. [Reports API (v3.5.0)](#reports-api)
17. [ABACKUP API (v3.5.0)](#abackup-api)
18. [Disaster Recovery API (v3.5.0)](#disaster-recovery-api)
19. [Types & Enumerations](#types--enumerations)

---

## Overview

DFSMShsm v4.2.0 is a comprehensive Hierarchical Storage Management (HSM) emulator using modern C++20.

```cpp
#include "hsm_engine.hpp"
using namespace dfsmshsm;
```

---

## HSMEngine API

### Lifecycle Methods

```cpp
bool initialize(const std::filesystem::path& data_dir);
bool start();
void stop();
bool isRunning() const;
static std::string getVersion();  // "4.2.0"
```

---

## Dataset Operations

```cpp
OperationResult createDataset(const std::string& name, uint64_t size,
    const std::string& volume = "", const std::string& owner = "SYSTEM",
    const std::string& group = "");
OperationResult deleteDataset(const std::string& name);
std::optional<DatasetInfo> getDataset(const std::string& name) const;
std::vector<DatasetInfo> getAllDatasets() const;
std::vector<DatasetInfo> getDatasetsByTier(StorageTier tier) const;
std::vector<DatasetInfo> getDatasetsByState(DatasetState state) const;
```

---

## Migration & Recall

```cpp
OperationResult migrateDataset(const std::string& name, StorageTier target);
OperationResult runAutomaticMigration();
void setMigrationPolicy(const MigrationPolicy& policy);
OperationResult recallDataset(const std::string& name);
```

---

## Backup & Recovery

```cpp
OperationResult backupDataset(const std::string& name);
OperationResult recoverDataset(const std::string& name);
OperationResult runAutomaticBackup();
void setBackupPolicy(const BackupPolicy& policy);
```

---

## Deduplication API

**New in v4.0.2** - Content-addressable storage with block-level deduplication.

```cpp
OperationResult deduplicateDataset(const std::string& name, const std::vector<uint8_t>& data);
std::optional<DedupeInfo> getDedupeInfo(const std::string& name) const;
size_t runDedupeGarbageCollection();
DedupeStatistics getDedupeStatistics() const;
```

### DeduplicationManager Direct Access

```cpp
DeduplicationManager& dedupe = engine.getDedupeManager();
std::vector<std::string> hashes = dedupe.storeWithDedupe("DATASET", data);
std::optional<std::vector<uint8_t>> data = dedupe.retrieve("DATASET");
size_t shared = dedupe.getSharedBlockCount("DS1", "DS2");
bool valid = dedupe.verifyIntegrity("DATASET");
```

---

## Quota Management API

**New in v4.0.2** - User and group quota enforcement.

```cpp
bool setUserQuota(const std::string& user_id, uint64_t space_limit, uint64_t inode_limit = 0);
bool setGroupQuota(const std::string& group_id, uint64_t space_limit, uint64_t inode_limit = 0);
std::optional<QuotaUsage> getUserQuotaUsage(const std::string& user_id) const;
QuotaCheckResult checkQuota(const std::string& user_id, const std::string& group_id, uint64_t space_needed);
```

### QuotaCheckResult

```cpp
enum class QuotaCheckResult { OK, SOFT_EXCEEDED, HARD_EXCEEDED, GRACE_EXPIRED, NO_QUOTA };
```

---

## Journaling API

**New in v4.0.2** - Write-ahead logging for crash recovery.

```cpp
uint64_t beginTransaction();
bool commitTransaction(uint64_t txn_id);
bool rollbackTransaction(uint64_t txn_id);
void journalCheckpoint();
```

### JournalManager Direct Access

```cpp
JournalManager& journal = engine.getJournalManager();
uint64_t txn = journal.beginTransaction();
journal.write(txn, JournalEntryType::DATA_WRITE, "DATASET.NAME", "data");
journal.commit(txn);
size_t recovered = journal.recover();
```

---

## Encryption API

**New in v4.0.2** - Data-at-rest encryption with key management.

```cpp
OperationResult encryptDataset(const std::string& name, const std::string& key_id = "");
OperationResult decryptDataset(const std::string& name);
std::string generateEncryptionKey(const std::string& name);
std::optional<std::string> rotateEncryptionKey(const std::string& old_key_id);
```

### EncryptionManager Direct Access

```cpp
EncryptionManager& enc = engine.getEncryptionManager();
std::string key_id = enc.generateKey("MyKey", EncryptionType::AES_256);
auto encrypted = enc.encrypt(data, key_id);
auto decrypted = enc.decrypt(*encrypted, key_id);
```

---

## Snapshot API

**New in v4.0.2** - Point-in-time snapshots with retention policies.

```cpp
std::optional<std::string> createSnapshot(const std::string& name, 
    const std::string& description = "", uint32_t retention_days = 0);
bool deleteSnapshot(const std::string& snapshot_id);
OperationResult restoreFromSnapshot(const std::string& snapshot_id);
std::vector<SnapshotInfo> getDatasetSnapshots(const std::string& name) const;
```

### SnapshotManager Direct Access

```cpp
SnapshotManager& snaps = engine.getSnapshotManager();
SnapshotPolicy policy{.hourly_keep=24, .daily_keep=7, .weekly_keep=4};
snaps.setPolicy("DATASET", policy);
auto diff = snaps.compareSnapshots(snap1_id, snap2_id);
```

---

## Replication API

**New in v4.0.2** - Cross-tier data replication.

```cpp
std::optional<std::string> replicateDataset(const std::string& name, 
    StorageTier target_tier, bool synchronous = false);
bool deleteReplica(const std::string& replica_id);
bool syncReplica(const std::string& replica_id);
std::vector<ReplicaInfo> getDatasetReplicas(const std::string& name) const;
void setReplicationPolicy(const std::string& name, const ReplicationPolicy& policy);
```

---

## Scheduling API

**New in v4.0.2** - Automated task scheduling with CRON support.

```cpp
std::string scheduleTask(const std::string& name, OperationType operation, ScheduleFrequency frequency);
bool cancelTask(const std::string& task_id);
std::vector<ScheduledTask> getScheduledTasks() const;
```

### Scheduler Direct Access

```cpp
Scheduler& sched = engine.getScheduler();
std::string id = sched.scheduleTask("DailyBackup", OperationType::OP_BACKUP,
    ScheduleFrequency::DAILY, [](const ScheduledTask&) { return true; });
sched.scheduleCron("WeeklyMigration", OperationType::OP_MIGRATE, "0 2 * * 0", callback);
sched.triggerTask(id);
```

---

## Types & Enumerations

### StorageTier

```cpp
enum class StorageTier { PRIMARY, MIGRATION_1, MIGRATION_2, TAPE, BACKUP, CLOUD };
```

### DatasetState

```cpp
enum class DatasetState { ACTIVE, MIGRATED, ARCHIVED, DELETED, CORRUPTED, 
    ENCRYPTED, REPLICATED, SNAPSHOT, DEDUPLICATED };
```

### OperationType

```cpp
enum class OperationType { OP_MIGRATE, OP_RECALL, OP_BACKUP, OP_RECOVER, OP_DELETE,
    OP_ARCHIVE, OP_COMPRESS, OP_HEALTH_CHECK, OP_ENCRYPT, OP_DECRYPT,
    OP_REPLICATE, OP_SNAPSHOT, OP_DEDUPE, OP_EXPIRE, OP_QUOTA_CHECK };
```

### HSMReturnCode

```cpp
enum class HSMReturnCode { SUCCESS=0, ERROR_NOT_FOUND=4, ERROR_ALREADY_EXISTS=8,
    ERROR_NO_SPACE=12, ERROR_IO_ERROR=16, ERROR_PERMISSION=20,
    ERROR_QUOTA_EXCEEDED=24, ERROR_ENCRYPTION=28, ERROR_REPLICATION=32, ERROR_INTERNAL=99 };
```

---

## Logging API

```cpp
#include "hsm_logging.hpp"
using namespace dfsmshsm::logging;

// Log levels
enum class LogLevel { TRACE, DEBUG, INFO, WARN, ERROR, FATAL, OFF };

// Get logger
auto logger = LogManager::instance().getLogger("hsm.component");
logger->setLevel(LogLevel::DEBUG);

// Add sinks
auto console = std::make_shared<ConsoleSink>(true);
auto file = std::make_shared<FileSink>("hsm.log", RotationConfig{});
logger->addSink(console);
logger->addSink(file);

// Logging
logger->info("Message");
logger->error("Error occurred");
logger->logWithContext(LogLevel::WARN, "Warning", {{"key", "value"}});

// Trace spans
{
    TraceSpan span("operation_name");
    // Operations within span get trace correlation
}
```

---

## Benchmark API

```cpp
#include "hsm_benchmark.hpp"
using namespace dfsmshsm::benchmark;

// Run benchmarks
BenchmarkRunner runner;
runner.config().iterations = 100;

auto result = runner.runTimed("test", []() {
    // Code to benchmark
}, bytes_per_iteration);

// Statistics
Statistics stats = result.time_stats_ms;
double mean = stats.mean;
double p99 = stats.p99;

// Benchmark suite
auto suite = createDefaultHSMBenchmarkSuite();
auto results = suite.runAll();
suite.exportJSON(results, "results.json");
```

---

## Tape Pool API

```cpp
#include "hsm_tape_pool.hpp"
using namespace dfsmshsm::tapepool;

TapePoolManager manager;

// Create pool
TapePool pool;
pool.name = "SCRATCH01";
pool.default_media_type = MediaType::LTO8;
manager.createPool(pool);

// Allocate volume
AllocationRequest request;
request.pool_name = "SCRATCH01";
request.space_needed = 5ULL * 1024 * 1024 * 1024;
AllocationResult result = manager.allocateScratch(request);

// Get statistics
PoolStatistics stats = manager.getPoolStatistics("SCRATCH01");

// Health check
auto health = manager.checkPoolHealth("SCRATCH01");
```

---

## Reports API

```cpp
#include "hsm_reports.hpp"
using namespace dfsmshsm::reports;

auto collector = std::make_shared<ReportDataCollector>();

// Add data
MigrationRecord rec;
rec.dataset_name = "USER.DATA";
rec.bytes_migrated = 1024 * 1024;
collector->addMigration(rec);

// Generate reports
ReportGenerator generator(collector);
auto report = generator.createMigrationReport();
report->setTimeRange(ReportTimeRange::lastNDays(7));
report->setFormat(ReportFormat::HTML);
std::string output = report->generate();
```

---

## ABACKUP API

```cpp
#include "hsm_abackup.hpp"
using namespace dfsmshsm::abackup;

ABackupManager manager;

// Define aggregate group
AggregateGroup group;
group.name = "PROD_DATA";
group.volume_patterns = {"VOL*", "USR*"};
group.retention_days = 30;
manager.defineGroup(group);

// Execute ABACKUP
ABackupOptions opts;
opts.group_name = "PROD_DATA";
opts.type = BackupType::FULL;
ABackupResult result = manager.executeABackup(opts);

// Execute ARECOVER
ARecoverOptions recover_opts;
recover_opts.backup_id = result.backup_id;
recover_opts.verify_only = true;
ARecoverResult recover = manager.executeARecover(recover_opts);
```

---

## Disaster Recovery API

```cpp
#include "hsm_disaster_recovery.hpp"
using namespace dfsmshsm::dr;

DisasterRecoveryManager manager;

// Register sites
Site primary;
primary.site_id = "NYC";
primary.role = SiteRole::PRIMARY;
primary.status = SiteStatus::ONLINE;
manager.registerSite(primary);

// Create replication link
ReplicationLink link;
link.link_id = "NYC_CHI";
link.source_site = "NYC";
link.target_site = "CHI";
link.mode = ReplicationMode::SYNCHRONOUS;
manager.createReplicationLink(link);

// Create recovery point
std::string point_id = manager.createRecoveryPoint("CHI", "Hourly");

// Run DR test
auto test_result = manager.runDRTest("CHI");

// Initiate failover
DisasterRecoveryManager::FailoverOptions fo_opts;
fo_opts.target_site = "CHI";
auto fo_result = manager.initiateFailover(fo_opts);
```

---

*Copyright (c) 2025 Bennie Shearer - MIT License*
