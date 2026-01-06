# IBM DFSMShsm (Hierarchy Storage Manager) Emulation - Change Log
Version 4.2.0

All notable changes to this project will be documented in this file.

## [4.2.0] - 2025-01-06

### Added
- New hsm_csv.hpp header with CSV parsing utilities
  - CsvRow class for row-level access
  - CsvDocument for full document handling
  - Type-safe field access (getString, getInt, getDouble, getBool)
  - Column access by name or index
  - CSV parsing with configurable delimiters and quoting
  - CSV serialization with proper escaping
  - CsvBuilder for programmatic creation

- New hsm_args.hpp header with command-line argument parsing
  - ArgParser for defining and parsing arguments
  - Support for flags, options, and multi-value arguments
  - Short (-o) and long (--option) argument forms
  - Required arguments with validation
  - Default values
  - Automatic help generation
  - Positional argument support

- New hsm_uuid.hpp header with UUID utilities
  - UUID v4 (random) generation
  - UUID parsing from string
  - Multiple output formats (standard, compact, uppercase)
  - Nil UUID support
  - Version and variant detection
  - Comparison and hashing support
  - std::hash specialization

### Changed
- Total header count increased to 77 (added 3 new utility headers)
- Minor version bump (4.1.x -> 4.2.0) for new features

## [4.1.9] - 2025-01-06

### Changed
- Consolidated documentation by removing versioned ANALYSIS files
- Single ANALYSIS.md now contains current project state
- Reduced documentation file count from 17 to 11
- CHANGELOG.md remains the authoritative version history

### Removed
- ANALYSIS_v4.1.1.md (consolidated into ANALYSIS.md)
- ANALYSIS_v4.1.4.md (consolidated into ANALYSIS.md)
- ANALYSIS_v4.1.5.md (consolidated into ANALYSIS.md)
- ANALYSIS_v4.1.6.md (consolidated into ANALYSIS.md)
- ANALYSIS_v4.1.7.md (consolidated into ANALYSIS.md)
- ANALYSIS_v4.1.8.md (consolidated into ANALYSIS.md)

## [4.1.8] - 2025-01-06

### Added
- New hsm_json.hpp header with lightweight JSON utilities
  - JsonValue class supporting null, bool, number, string, array, object
  - Type checking (isNull, isBool, isNumber, isString, isArray, isObject)
  - Type-safe accessors (asBool, asNumber, asString, asArray, asObject)
  - Optional accessors (getBool, getNumber, getString)
  - JSON parser with error handling (parse, tryParse)
  - JSON serialization with pretty printing (dump with indent)
  - Fluent builders (JsonObjectBuilder, JsonArrayBuilder)

- New hsm_ini.hpp header with INI configuration file support
  - IniSection class for section management
  - Type-safe getters (getString, getInt, getDouble, getBool, getList)
  - Setters for all supported types
  - IniFile parser (parse, load from file)
  - INI serialization (dump, save to file)
  - Global section support (values without section header)
  - Fluent IniBuilder for programmatic creation

- New hsm_validation.hpp header with validation framework
  - ValidationResult for collecting errors
  - ValidationError with field, message, and code
  - String validators (notEmpty, minLength, maxLength, matches, email)
  - Numeric validators (min, max, between, positive, nonNegative)
  - Collection validators (notEmpty, minSize, maxSize, all)
  - Fluent ValidatorBuilder for chaining validators
  - Optional value validation (required, ifPresent)

### Changed
- Total header count increased to 74 (added 3 new utility headers)
- Enhanced configuration and data handling capabilities
- Improved input validation support

### Fixed
- Corrected example count in VERSION.txt (17 not 18)

## [4.1.7] - 2025-01-06

### Added
- New hsm_format.hpp header with format string utilities
  - Basic formatting (concat, join)
  - Numeric formatting (zeroPad, rightAlign, leftAlign, fixed, scientific)
  - Number separators (withSeparators, percent)
  - Hexadecimal formatting (hex, hexByte, hexBytes)
  - Binary formatting (binary with grouping)
  - Size formatting (bytes, transferRate)
  - Duration formatting (duration, milliseconds)
  - Table formatting (Table class with alignment)
  - Progress bar generation (progressBar)
  - Text wrapping (wrap with indent)
  - Printf-style safe formatting (fmt with {} placeholders)
  - Escape sequences (escape, quote)

- New hsm_filesystem.hpp header with cross-platform filesystem utilities
  - Path utilities (getExtension, getStem, joinPath, normalize, makeRelative)
  - File queries (exists, isFile, isDirectory, fileSize, availableSpace)
  - File operations (createDirectory, remove, copyFile, move)
  - Content operations (readFile, writeFile, readLines, writeLines)
  - Directory traversal (listDirectory, listFiles, findByExtension, walkDirectory)
  - Temporary files (TempFile, TempDirectory RAII classes)
  - Current directory utilities (ScopedDirectory)

- New hsm_source_location.hpp header with debug utilities
  - Source location capture (C++20 compatible with fallback)
  - Location formatting (formatLocation, formatLocationFull, formatLocationShort)
  - Call frame tracking (CallFrame, CallStack classes)
  - RAII scope guards for call tracking (ScopeGuard)
  - Debug context (DebugContext with category support)
  - Function signature utilities (simplifyFunctionName, extractClassName)
  - Debug assertions (HSM_ASSERT, HSM_ASSERT_MSG macros)
  - Trace logging (TraceEntry with timing)

### Changed
- Total header count increased to 71 (added 3 new utility headers)
- Enhanced debugging and diagnostic capabilities
- Improved cross-platform file handling

## [4.1.6] - 2025-01-06

### Added
- New hsm_span.hpp header with C++20 std::span utilities
  - Span creation utilities (makeSpan, asBytes, fromVector, fromString)
  - Subspan operations (takeFirst, takeLast, dropFirst, dropLast, splitAt)
  - Safe span access (safeAt, safeGet, safeFront, safeBack)
  - Span comparison (equal, startsWith, endsWith, contains)
  - Span search (find, findIf, count)
  - Span modification (fill, copy, reverse, sort)
  - Byte span utilities (toHexString, checksum, xorBytes)
  - Iteration utilities (forEachIndexed, chunk, slidingWindow)

- New hsm_ranges.hpp header with C++20 ranges utilities
  - Range concepts (Range, SizedRange, RandomAccessRange)
  - Range-to-container conversions (toVector, toString)
  - Range algorithms (allOf, anyOf, noneOf, count, find, contains)
  - Aggregation functions (sum, product, min, max, average)
  - Transformation functions (map, filter, reduce, take, skip)
  - Partitioning functions (partition, group, unique)
  - Combination functions (zip, enumerate, flatten)
  - Sorting functions (sorted, sortedBy, reversed)
  - Generation functions (iota, repeat)

- New hsm_algorithm.hpp header with algorithm utilities
  - Numeric utilities (clamp, lerp, mapRange, inRange, approxEqual)
  - Math functions (gcd, lcm, isPowerOfTwo, nextPowerOfTwo)
  - Statistical functions (mean, median, mode, variance, standardDeviation, percentile)
  - Search algorithms (binarySearch, lowerBoundIndex, upperBoundIndex, kSmallest, kLargest)
  - Set operations (intersection, setUnion, difference, symmetricDifference, isSubset)
  - Container transformations (removeDuplicates, rotate, shuffle, argsort, reorder)
  - Batch processing utilities (processBatches, split)

### Changed
- Total header count increased to 68 (added 3 new utility headers)
- Enhanced C++20 feature utilization with span and ranges support
- Improved algorithm library coverage for common operations

## [4.1.5] - 2025-01-06

### Added
- New hsm_concepts.hpp header with C++20 concepts for type safety
  - Basic type concepts (Numeric, Integral, FloatingPoint)
  - String concepts (StringLike, StringViewConvertible)
  - Container concepts (Container, ResizableContainer, SequenceContainer)
  - Iterator concepts (InputIterator, ForwardIterator, RandomAccessIterator)
  - Callable concepts (Callable, Predicate, Comparator)
  - HSM-specific concepts (DatasetName, VolumeSerial, Serializable)
  - Concept-based utility functions (clamp, safe_cast, sum, filter)

- New hsm_string_utils.hpp header with string manipulation utilities
  - Trimming functions (trim, ltrim, rtrim)
  - Case conversion (toUpper, toLower, toTitleCase)
  - Splitting and joining (split, join)
  - Padding functions (padLeft, padRight, center)
  - Search and replace (replaceAll, contains, startsWith, endsWith)
  - Numeric conversions (toInt, formatBytes)
  - Dataset name utilities (isValidDatasetName, getHLQ, getLLQ)

- New hsm_time_utils.hpp header with time/date utilities
  - Current time functions (now, unixTimestamp)
  - Time point conversions (fromUnixTimestamp, toTimeT)
  - Formatting functions (toISO8601, toDateString, toDateTimeString)
  - z/OS time formats (toZOSTimestamp, toJulianDate, toSMFTimestamp)
  - Duration functions (durationSeconds, formatDuration)
  - Timer utility classes (Timer, ScopedTimer)
  - Date components extraction

- New hsm_memory.hpp header with memory management utilities
  - MemoryStats class for tracking allocations
  - Buffer class for fixed-size memory buffers
  - ObjectPool template for object pooling
  - RingBuffer template for circular buffers
  - Smart pointer utilities
  - Memory alignment functions
  - Secure memory zeroing

### Changed
- Total header count increased to 65 (added 4 new utility headers)
- Enhanced type safety with C++20 concepts
- Improved code reusability with common utility functions

## [4.1.4] - 2025-01-06

### Added
- New hsm_platform.hpp header for comprehensive cross-platform compatibility
  - Platform detection macros (HSM_PLATFORM_WINDOWS, HSM_PLATFORM_LINUX, HSM_PLATFORM_MACOS)
  - Compiler detection macros (HSM_COMPILER_GCC, HSM_COMPILER_CLANG, HSM_COMPILER_MSVC)
  - C++ standard version detection (HSM_CPP17, HSM_CPP20, HSM_CPP23)
  - Attribute macros (HSM_NODISCARD, HSM_MAYBE_UNUSED, HSM_LIKELY, HSM_UNLIKELY)
  - Force inline/no inline macros for all compilers
  - Diagnostic control macros for warning suppression
  - Static assertion helpers (HSM_STATIC_ASSERT_SIZE, HSM_STATIC_ASSERT_POD)
  - Byte swap utilities (byteSwap16, byteSwap32, byteSwap64)
  - Platform query functions (isWindows(), isLinux(), isMacOS(), isPosix())
  - Endianness detection (isLittleEndian(), isBigEndian())
- VERSION.txt file in docs directory with comprehensive version information

### Fixed
- Fixed all remaining compiler warnings in example_v410_features.cpp
  - Added (void) casts for all nodiscard return values
  - All examples now compile warning-free

### Changed
- Total header count increased to 61 (added hsm_platform.hpp)
- Improved cross-platform compatibility documentation

## [4.1.3] - 2025-01-06

### Fixed
- Replaced Unicode box-drawing characters with ASCII alternatives in documentation
- All source files now contain only ASCII-safe characters (0x00-0x7F range)

### Changed
- Updated directory tree diagrams to use ASCII characters (+, |, -) instead of Unicode

## [4.1.2] - 2025-01-05

### Added
- Code coverage tooling support via CMake option `ENABLE_COVERAGE`
- Coverage report generation with `make coverage` target (requires lcov/genhtml)
- Coverage cleanup with `make coverage-clean` target
- Support for lcov/genhtml based HTML coverage reports
- Pragma-based warning suppression in test files for cleaner builds

### Fixed
- Fixed [[nodiscard]] return value warnings in test code (added explicit (void) casts)
- Fixed trivially true assertions (unsigned >= 0 comparisons)
- All test code now compiles cleanly with appropriate warning suppressions

### Changed
- Test file now uses #pragma GCC diagnostic to selectively disable sign comparison warnings
- Improved test code quality with explicit handling of unused return values

### Notes
- Virtual destructor issue in hsm_dasd.hpp was determined to be a false positive
  (no actual virtual methods - "virtual" only appears in comments about virtual DASD)

## [4.1.1] - 2025-01-05

### Changed
- Simplified product name expansion from "Data Facility Storage Management Hierarchy Storage Manager" to "Hierarchy Storage Manager"
- Updated all documentation headers to use shorter product name format

## [4.1.0] - 2025-01-05

### Added - Major New Features

#### Binary Protocol Support (`hsm_binary_protocol.hpp`)
- CRC32 class with IEEE polynomial checksum calculator
- BinaryWriter/BinaryReader with primitive, varint, and string support
- ZigZag encoding for efficient signed integer storage
- Variable-length integer (varint) encoding for space efficiency
- BinaryRecordHeader with magic number, version, type, flags, checksum
- BinarySerializer for DatasetInfo, VolumeInfo, OperationRecord
- BinaryFileWriter/Reader for file-based record I/O
- BinaryProtocolStats for serialization metrics tracking

#### Event Sourcing (`hsm_event_sourcing.hpp`)
- CDSEventType enum with 40+ event types
- CDSEvent struct with sequence_number, type, timestamps, correlation/causation IDs
- EventPayloadBuilder for structured event payload construction
- EventStore with append-only log, indexing by aggregate/type/time
- Snapshot support for efficient recovery (configurable intervals)
- Event compaction for storage optimization
- Subscription system for real-time event notifications
- EventReplayer for state reconstruction from events
- Projections: DatasetCountProjection, StorageUsageProjection, OperationCountProjection
- ProjectionManager for managing multiple projections
- EventSourcedDatasetRepository for event-sourced CRUD operations

#### Enhanced LRU Cache (`hsm_lru_cache.hpp`)
- BloomFilter with 7 hash functions and configurable bit sizes
- CacheEntry with TTL, access count, size tracking, dirty flag
- CacheStatistics with hits, misses, bloom filter rejections, evictions
- EnhancedLRUCache with bloom filter integration
- Write-through and write-back policies
- getOrCompute() for lazy loading with callbacks
- DatasetCache and VolumeCache specialized caches
- MultiLevelCache for hot/cold data separation
- Optimal bloom filter sizing calculations

#### Command History (`hsm_command_history.hpp`)
- ICommand interface with execute/undo/redo/canUndo/merge
- LambdaCommand for commands implemented with lambdas
- DatasetCreateCommand, DatasetDeleteCommand, DatasetMigrateCommand
- PropertyChangeCommand for generic property changes with merge support
- CompositeCommand for transaction grouping
- CommandHistory with configurable depth and undo/redo stacks
- CommandFactory for standardized command creation
- TransactionBuilder for fluent transaction building
- CommandSerializer for persistence and audit trails

### Changed
- Updated project declaration format in CMakeLists.txt
- Added test_v410_features test suite with 46 comprehensive tests
- Added example_v410_features demonstration program
- Enhanced ErrorCode enum with INVALID_STATE and NOT_SUPPORTED
- Added missing fields to DatasetInfo (dsorg, record_length, block_size, tags, etc.)
- Added missing fields to VolumeInfo (device_type, storage_group, free_space)
- Added OperationRecord struct for complete operation tracking
- Added LifecyclePolicy fields (target_tier, pattern, days_before_migration)
- Enhanced HSMConfig with volume accessors and toString()
- Added has_value() method to Result class for std::optional compatibility

### Fixed
- Fixed Result class template parameter issues
- Fixed EventPayloadBuilder ambiguous overload for int type
- Fixed CommandHistory Config default member initializer issue

## [4.0.2] - 2025-01-01

### Changed
- Standardized all documentation headers with full product name expansion
- Updated header format: "IBM DFSMShsm (Hierarchy Storage Manager) Emulation - [Document Type]"
- Added "IBM" prefix to product name in all document titles
- Removed markdown bold formatting (__) from parenthetical product name expansion
- Consistent "Version X.X.X" format on second line across all documentation
- Updated version references from 4.0.1 to 4.0.2 across 265 occurrences in all source files

### Verified
- All source files confirmed ASCII-safe (no Unicode encoding issues)

## [4.0.0] - 2025-12-27

### Added
- Major version release consolidating all v3.x features
- Comprehensive test suite for v4.0.0 features (`test_v400_features.cpp`)
- Full integration of all subsystems

## [3.7.0] - 2025-12-21

### Added - z/OS Infrastructure Emulation

#### Extended DASD Support (`hsm_dasd.hpp`)
- 3390/3380 device geometry emulation with accurate track/cylinder layouts
- CCHH (Cylinder-Cylinder-Head-Head) addressing for direct track access
- VTOC (Volume Table of Contents) management with Format 1/4/5 DSCBs
- Track I/O operations with LRU caching for performance
- Extent allocation with track/cylinder granularity
- Multi-volume support with device registry
- Full DSCB parsing and creation
- Space management with allocation tracking

#### Catalog Integration (`hsm_catalog.hpp`)
- ICF (Integrated Catalog Facility) catalog emulation
- GDG (Generation Data Group) support with automatic rolloff
- Alias resolution for HLQ routing
- VVDS/BCS association management
- LISTCAT export functionality
- Non-VSAM and VSAM cluster entries
- Catalog search with pattern matching
- Generation versioning with limit enforcement

#### SMF Recording (`hsm_smf.hpp`)
- System Management Facilities record generation
- Type 14 (input dataset) and Type 15 (output dataset) records
- Type 42 (SMS statistics) and Type 62 (VSAM) records
- HSM custom types 240-244 (migration, recall, backup, delete, expire)
- Binary record construction with proper RDW (Record Descriptor Word)
- SMF header with standard fields (timestamp, system ID, job info)
- Product section with HSM-specific counters
- Record writer with buffering and auto-flush

#### SDSP - System Data Set Placement (`hsm_sdsp.hpp`)
- Dataset placement optimization with ML-based allocation
- DatasetProfile with access patterns and growth analysis
- StorageGroupProfile with capacity/performance metrics
- PlacementScore calculation using weighted factors
- Smart allocation with profile learning
- Rebalancing recommendations with migration cost
- Pattern recognition (random, sequential, mixed, burst)
- Tier affinity suggestions

#### Extended Statistics (`hsm_extended_statistics.hpp`)
- TimeSeries data storage with automatic aggregation (minute/hour/day)
- ThroughputMetrics per operation type (migration, recall, backup, recovery)
- TrendAnalyzer with linear regression and R^2 calculation
- CapacityPlanner with growth rate forecasting and threshold alerts
- AnomalyDetector using z-score with configurable thresholds (2sigma/3sigma/4sigma)
- SLATracker with compliance percentage and violation tracking
- ReportGenerator with multiple formats (text, JSON)
- Seasonality detection (hourly, daily, weekly patterns)
- Percentile calculations (P50, P90, P95, P99)

#### Parallel Processing (`hsm_parallel.hpp`)
- PAV (Parallel Access Volume) emulation with up to 8 static aliases
- HyperPAV with dynamic alias pools (up to 255 aliases)
- UCB (Unit Control Block) emulation for z/OS devices
- Allocation strategies: round-robin, least-busy, affinity, weighted
- IOPriorityQueue with 5 priority levels (CRITICAL to BACKGROUND)
- WorkStealingPool for load-balanced task execution
- AsyncIOManager for non-blocking I/O operations
- MultiPathManager with path health monitoring and failover
- I/O statistics tracking per device and volume
- Background pool rebalancing

### Added - Tests
- `tests/test_v370_features.cpp` - Comprehensive test suite for all v3.7.0 features
- DASD tests (geometry, CCHH addressing, VTOC, extent allocation)
- Catalog tests (ICF, GDG rolloff, alias resolution, LISTCAT)
- SMF tests (record types, binary format, buffering)
- SDSP tests (profiling, placement scoring, rebalancing)
- Statistics tests (time series, trends, anomalies, SLA)
- Parallel tests (PAV/HyperPAV, work stealing, async I/O, multipath)

### Added - Examples
- `examples/example_v370_features.cpp` - Demonstration of all v3.7.0 capabilities
- DASD geometry and VTOC manipulation
- Catalog operations with GDG management
- SMF record generation and writing
- SDSP placement optimization
- Statistics collection and analysis
- Parallel I/O with PAV/HyperPAV

### Changed
- Updated version to 3.7.0 across all header files
- Enhanced README with v3.7.0 z/OS infrastructure features
- Updated API_REFERENCE.md with v3.7.0 API documentation
- Total header count: 50 (6 new z/OS infrastructure headers)
- Total code size: ~1.3 MB in headers

## [3.6.0] - 2025-12-21

### Added - Enterprise Features

#### SMS Integration (`hsm_sms.hpp`)
- Storage Management Subsystem (SMS) emulation
- Storage Class definitions with performance/availability objectives
- Data Class definitions with DSORG, record format, space allocation
- Management Class with backup/migration/retention policies
- Storage Group management with volume pools
- ACS (Automatic Class Selection) routines with rule-based processing
- Factory functions for common configurations
- Volume selection based on capacity and availability

#### Cloud Tiering (`hsm_cloud_tier.hpp`)
- Multi-cloud provider support (AWS S3, Azure Blob, GCP, IBM COS)
- CloudProvider abstract interface with S3/Azure implementations
- Cloud migration policies with pattern matching
- Transparent recall from cloud storage
- Cost analyzer with storage class comparison
- Support for Glacier/Archive storage tiers
- Compression before upload option
- Cloud catalog tracking

#### ABARS Enhancement (`hsm_abars.hpp`)
- Enhanced Aggregate Backup and Recovery Support
- Aggregate group definition with include/exclude patterns
- ABARS control file parser with continuation support
- Activity log with comprehensive entry types
- Stack manager for tape allocation and tracking
- ABackup/ARecover operations with options
- Progress tracking and checkpoint logging
- Retention management for aggregates

#### Common Recall Queue (`hsm_crq.hpp`)
- Multi-host recall coordination
- Priority-based recall queue (CRITICAL to DEFERRED)
- Host manager with load balancing
- Recall request lifecycle management
- Queue statistics and monitoring
- Event callback system
- Automatic host failover detection

#### Fast Replication (`hsm_fast_replication.hpp`)
- FlashCopy pair management
- Consistency group coordination
- Copy set orchestration for DR
- Full, incremental, and persistent copy types
- Recovery operations (restore, failover, failback, refresh)
- Progress tracking with track-level granularity
- Background copy simulation

#### Audit & Compliance (`hsm_compliance.hpp`)
- WORM (Write Once Read Many) volume support
- Retention policy management (time-based, event-based, regulatory)
- Legal hold management with matter tracking
- Comprehensive audit logging
- Compliance checking (SEC 17a-4, HIPAA, SOX, GDPR, PCI-DSS)
- Compliance report generation
- Data governance controls

### Added - Tests
- 42 new tests in `tests/test_v360_features.cpp` covering all enterprise features
- SMS integration tests (storage classes, allocation, ACS)
- Cloud tiering tests (providers, migration, cost analysis)
- ABARS tests (aggregate groups, backup/recover operations)
- CRQ tests (queue operations, host management, completion)
- Fast replication tests (FlashCopy pairs, groups, recovery)
- Compliance tests (retention, legal holds, WORM, audit)
- Integration tests (SMS+Cloud, ABARS+Compliance, FastRep+CRQ)

### Added - Examples
- `examples/example_v360_features.cpp` - Comprehensive demonstration of all v3.6.0 features
- SMS allocation workflow examples
- Cloud migration scenarios
- ABARS backup/recover demonstrations
- CRQ multi-host recall simulation
- FlashCopy DR setup examples
- Compliance policy configuration

### Changed
- Updated version to 3.6.0 across all header files
- Enhanced README with v3.6.0 enterprise features
- Updated API_REFERENCE.md with v3.6.0 API documentation
- Total header count: 44 (6 new enterprise headers)

## [3.5.0] - 2025-12-21

### Added - Major Features

#### Enhanced Logging/Tracing (`hsm_logging.hpp`)
- Structured logging with configurable levels (TRACE through FATAL)
- JSON and plain text output formats
- Automatic log rotation by size and time
- Trace correlation with request IDs and span IDs
- Thread-safe asynchronous logging with queue
- Multiple sink support (console, file, callback)
- Trace analysis with performance statistics

#### Performance Benchmarking (`hsm_benchmark.hpp`)
- High-resolution timing measurements
- Statistical analysis (mean, median, std dev, percentiles)
- Built-in migration, recall, and compression benchmarks
- Throughput and IOPS calculations
- Benchmark suites with JSON/CSV export
- Performance comparison and regression detection

#### Tape Pool Management (`hsm_tape_pool.hpp`)
- Multiple tape pool definitions (scratch, private, storage groups)
- Automatic scratch volume allocation
- Volume expiration and retention management
- Pool capacity tracking and thresholds
- Health monitoring with warning/critical alerts
- Storage group management with priority allocation

#### Report Generation (`hsm_reports.hpp`)
- Multiple output formats (TEXT, HTML, JSON, CSV)
- Migration activity reports with statistics
- Recall activity reports with wait time analysis
- Storage utilization reports
- Dashboard summary reports
- Configurable time ranges (hourly, daily, weekly, monthly)

#### ABACKUP/ARECOVER Emulation (`hsm_abackup.hpp`)
- Aggregate group definition and management
- Full, incremental, and differential backup types
- Backup catalog with version management
- Point-in-time recovery support
- Backup chain validation
- Retention policy enforcement

#### Disaster Recovery (`hsm_disaster_recovery.hpp`)
- Multi-site configuration (primary, secondary, tertiary)
- Synchronous and asynchronous replication modes
- Recovery point management (RPO/RTO tracking)
- Automated failover and failback operations
- Site health monitoring
- DR testing and validation

### Added - Tests
- 33 new tests in `tests/test_v350_features.cpp` covering all new features
- Additional tests in `tests/test_v3_5_0.cpp`
- Logging system tests (levels, formatting, sinks, tracing)
- Benchmark system tests (timing, statistics, suites)
- Tape pool tests (allocation, health checks, expiration)
- Report generation tests (all formats, statistics)
- ABACKUP/ARECOVER tests (backup chain, recovery validation)
- Disaster recovery tests (failover, replication, DR testing)

### Added - Examples
- `examples/example_v350_features.cpp` - Comprehensive demonstration of all v3.5.0 features
- `examples/v3_5_0_demo.cpp` - Feature showcase

### Changed
- Updated version to 3.5.0 across all 38 header files
- Enhanced README with v3.5.0 feature examples
- Updated API_REFERENCE.md with v3.5.0 API documentation

## [3.4.0] - 2025-12-21

### Added
- `Result<T,E>` monadic error handling type system (`result.hpp`)
- 40+ error codes with descriptive messages
- `ConfigValidator` for comprehensive configuration validation
- Range validation for all configuration parameters
- Cross-parameter consistency checks
- 25 additional tests in `test_extended.cpp`
- API_REFERENCE.md documentation
- ARCHITECTURE.md documentation
- MIGRATION_GUIDE.md documentation

## [3.3.0] - 2025-12-21

### Added
- LZ4 and ZSTD compression algorithms
- Prometheus/Grafana metrics export (`metrics_exporter.hpp`)
- REST API interface (`rest_api.hpp`)
- Enhanced Windows/MinGW compatibility

## [3.2.0] - 2025-12-21

### Added
- Data deduplication with block-level hashing
- User/group quota management
- AES-256 encryption at rest
- Point-in-time snapshot manager
- Cross-tier replication
- CRON-based task scheduling
- Write-ahead journal for crash recovery

## [3.1.0] - 2025-12-21

### Added
- Enhanced compression framework
- Lifecycle policy management
- Health monitoring dashboard

## [3.0.0] - 2025-12-21

### Added
- Complete modular architecture rewrite
- Modern C++20 implementation
- Comprehensive test framework
- Event-driven processing
- Thread pool for async operations

## [2.x] - Previous Versions
- Initial implementation of HSM functionality
- Basic migration and recall operations
- Dataset catalog management
- Volume management
