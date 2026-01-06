# IBM DFSMShsm (Hierarchy Storage Manager) Emulation - Architecture Guide
 Version 4.2.0

## System Overview

```
+-------------------------------------------------------------+
|                      CLIENT LAYER                            |
|  REST API | CLI/Command | Event Subscribers | Metrics       |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|                      ENGINE LAYER                            |
|          HSMEngine (Facade) - Orchestrates All              |
|  Migration | Recall | Backup | Snapshot | Replication       |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|                      SERVICE LAYER                           |
| Encryption | Compression | Dedupe | Cache | Journal         |
| Lifecycle | Quota | Scheduler | Audit | Performance         |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|                      STORAGE LAYER                           |
|  Volume Manager | Dataset Catalog | Storage Tier Manager    |
|  PRIMARY -> ML1 -> ML2 -> TAPE -> ARCHIVE                       |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|                  INFRASTRUCTURE LAYER                        |
| HSMConfig | HSMLogger | EventSystem | Result<T,E> | Types   |
+-------------------------------------------------------------+
```

## Component Count

- **Headers:** 77
- **Source Files:** 2
- **Test Files:** 9
- **Examples:** 11
- **Documentation:** 8 files

## Key Design Patterns

1. **Facade Pattern** - HSMEngine provides unified API
2. **Singleton** - HSMConfig, MetricsRegistry
3. **Observer** - EventBus for pub/sub
4. **RAII** - Resource management
5. **Result Monad** - Error handling (v4.0.2)

## Thread Safety

- All public APIs thread-safe
- `std::mutex` for data structures
- `std::atomic` for counters/flags
- `std::shared_mutex` for read-heavy data

## Storage Hierarchy

```
PRIMARY (Hot) -> ML1 (Warm) -> ML2 (Cool) -> TAPE/ARCHIVE (Cold)
     ^                                           |
     +-------------- RECALL ---------------------+
```
