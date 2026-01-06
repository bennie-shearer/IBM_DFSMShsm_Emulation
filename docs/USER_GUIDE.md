# IBM DFSMShsm (Hierarchy Storage Manager) Emulation - User Guide
Version 4.2.0

## Introduction

DFSMShsm v4.2.0 is a comprehensive Hierarchical Storage Management emulator that replicates IBM mainframe DFSMS/hsm functionality. This guide covers installation, configuration, and usage.

## Installation

### Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.16+
- POSIX threads support

### Building from Source

```bash
# Clone/extract package
cd dfsmshsm_v4_package

# Configure
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --parallel

# Run tests
ctest --output-on-failure

# Install (optional)
cmake --install . --prefix /usr/local
```

### Windows (MinGW)

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
```

## Quick Start

```cpp
#include "hsm_engine.hpp"
using namespace dfsmshsm;

int main() {
    HSMEngine engine;
    
    // Initialize
    engine.initialize("./hsm_data");
    engine.start();
    
    // Create volumes
    engine.createVolume("PRIMARY01", StorageTier::PRIMARY, 10737418240);
    engine.createVolume("ML1VOL01", StorageTier::MIGRATION_1, 107374182400);
    
    // Create dataset
    auto result = engine.createDataset("USER.DATA", 1048576);
    
    // Migrate to lower tier
    engine.migrateDataset("USER.DATA", StorageTier::MIGRATION_1);
    
    // Recall when needed
    engine.recallDataset("USER.DATA");
    
    engine.stop();
    return 0;
}
```

## Storage Tiers

| Tier | Purpose | Typical Media |
|------|---------|---------------|
| PRIMARY | Active data, fast access | SSD, NVMe |
| MIGRATION_1 | Inactive data | HDD |
| MIGRATION_2 | Rarely accessed | Slower HDD |
| TAPE | Archive | Virtual tape |
| BACKUP | Backup copies | Dedicated storage |
| CLOUD | Cloud storage | S3-compatible |

## Current Features

### Data Deduplication

Reduces storage by eliminating duplicate data blocks:

```cpp
// Enable deduplication
std::vector<uint8_t> data = loadData();
engine.deduplicateDataset("LARGE.DATA", data);

// Check savings
auto stats = engine.getDedupeStatistics();
std::cout << "Dedupe ratio: " << stats.dedupe_ratio << "\n";
std::cout << "Bytes saved: " << stats.bytes_saved << "\n";
```

### Quota Management

Enforce storage limits per user or group:

```cpp
// Set 1GB quota for user
engine.setUserQuota("JOHN", 1073741824);

// Check before allocation
auto result = engine.checkQuota("JOHN", "", 104857600);
if (result == QuotaCheckResult::OK) {
    engine.createDataset("JOHN.NEW.DATA", 104857600, "", "JOHN");
}
```

### Write-Ahead Journaling

Ensures data integrity with crash recovery:

```cpp
// Transaction-based operations
uint64_t txn = engine.beginTransaction();
// ... operations ...
engine.commitTransaction(txn);

// Or rollback on error
engine.rollbackTransaction(txn);

// Force checkpoint
engine.journalCheckpoint();
```

### Encryption at Rest

Protect sensitive data:

```cpp
// Encrypt dataset (auto-generates key)
auto result = engine.encryptDataset("SENSITIVE.DATA");
std::string key_id = result.details["key_id"];

// Decrypt when needed
engine.decryptDataset("SENSITIVE.DATA");

// Key rotation
auto new_key = engine.rotateEncryptionKey(key_id);
```

### Point-in-Time Snapshots

Create and restore snapshots:

```cpp
// Create snapshot
auto snap_id = engine.createSnapshot("PROD.DATA", "Before update", 30);

// Restore if needed
engine.restoreFromSnapshot(*snap_id);

// List snapshots
auto snaps = engine.getDatasetSnapshots("PROD.DATA");
```

### Data Replication

Cross-tier replication for reliability:

```cpp
// Create replica on backup tier
auto rep_id = engine.replicateDataset("CRITICAL.DATA", StorageTier::BACKUP, true);

// Sync replica
engine.syncReplica(*rep_id);

// Set replication policy
ReplicationPolicy policy;
policy.replica_count = 2;
policy.sync_interval_seconds = 3600;
engine.setReplicationPolicy("CRITICAL.DATA", policy);
```

### Task Scheduling

Automate operations:

```cpp
// Schedule daily backup
engine.scheduleTask("DailyBackup", OperationType::OP_BACKUP, ScheduleFrequency::DAILY);

// View scheduled tasks
auto tasks = engine.getScheduledTasks();
```

## CLI Commands

Start the CLI:
```bash
./dfsmshsm_cli /path/to/data
```

### Basic Commands

```
LIST                      - List all datasets
STATUS                    - System status
STATS                     - Statistics
HEALTH                    - Health report
HELP                      - Show help
QUIT                      - Exit
```

### Dataset Commands

```
MIGRATE <dataset> [tier]  - Migrate dataset
RECALL <dataset>          - Recall to primary
BACKUP <dataset>          - Backup dataset
RECOVER <dataset>         - Recover from backup
DELETE <dataset>          - Delete dataset
```

### HSM Commands

```
DEDUPE STATUS             - Deduplication stats
DEDUPE GC                 - Garbage collection

QUOTA SET <user> <limit>  - Set quota
QUOTA SHOW <user>         - Show usage

ENCRYPT <dataset>         - Encrypt dataset
DECRYPT <dataset>         - Decrypt dataset

SNAPSHOT CREATE <ds> [desc] - Create snapshot
SNAPSHOT LIST <dataset>   - List snapshots
SNAPSHOT RESTORE <id>     - Restore snapshot

REPLICATE <ds> <tier>     - Create replica
REPLICA LIST <dataset>    - List replicas
REPLICA SYNC <id>         - Sync replica

SCHEDULE LIST             - List tasks
JOURNAL STATUS            - Journal status
JOURNAL CHECKPOINT        - Force checkpoint
```

## Configuration

### Migration Policy

```cpp
MigrationPolicy policy;
policy.primary_age_days = 30;        // Migrate after 30 days
policy.ml1_age_days = 90;            // ML1 to ML2 after 90 days
policy.enable_auto_migrate = true;
policy.low_space_threshold = 0.8;    // 80% triggers migration
engine.setMigrationPolicy(policy);
```

### Backup Policy

```cpp
BackupPolicy policy;
policy.backup_frequency_hours = 24;
policy.keep_versions = 7;
policy.compress_backups = true;
engine.setBackupPolicy(policy);
```

## Best Practices

1. **Regular Backups**: Schedule automatic backups for critical data
2. **Quota Management**: Set quotas to prevent storage exhaustion
3. **Deduplication**: Enable for datasets with repetitive content
4. **Encryption**: Encrypt sensitive data at rest
5. **Replication**: Replicate critical data to multiple tiers
6. **Monitoring**: Check health reports regularly
7. **Journaling**: Let journal checkpoint periodically

## Troubleshooting

### No Space Available

```
ERROR_NO_SPACE: Create additional volumes or run migration
```

### Quota Exceeded

```
ERROR_QUOTA_EXCEEDED: Increase quota or delete unused data
```

### Dataset Not Found

```
ERROR_NOT_FOUND: Verify dataset name and check catalog
```

## Version History

- **v4.0.2**: Deduplication, quota, journaling, encryption, snapshots, replication, scheduling, event system, thread pool, monitoring, Windows compatibility

---

*Copyright (c) 2025 Bennie Shearer - MIT License*
