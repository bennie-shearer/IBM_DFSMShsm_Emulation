# IBM DFSMShsm (Hierarchy Storage Manager) Emulation
Version 4.2.0

## Overview

A comprehensive C++20 implementation of IBM mainframe DFSMS/hsm functionality for modern systems.

---

## What's New in v4.0.2 - Complete HSM Core

This release completes the DFSMShsm emulation with all core HSM components:

| Feature | Description |
|---------|-------------|
| **HSM Control Datasets** | MCDS/BCDS/OCDS emulation with VSAM-like record management, secondary indexes |
| **Command Processor** | Full TSO/batch command parsing (HMIGRATE, HRECALL, HBACKUP, etc.) with abbreviations |
| **Space Management** | Automatic threshold-based migration, candidate selection, space forecasting |
| **Interval Processing** | Scheduled migration/backup windows with resource limits, factory presets |
| **RACF/SAF Integration** | Security authorization, STGADMIN profiles, FACILITY class, audit logging |
| **Sysplex Coordination** | ENQ/DEQ/GRS emulation, CDS locking, host management, work distribution |

### Previous Features (v3.7.0)
| Feature | Description |
|---------|-------------|
| **Extended DASD Support** | 3390/3380 geometry emulation, CCHH addressing, VTOC management, track I/O |
| **Catalog Integration** | ICF catalog emulation, GDG support with rolloff, alias resolution, LISTCAT |
| **SMF Recording** | Type 14/15/42/62 records, HSM custom types 240-244, binary record format |
| **SDSP Placement** | ML-based dataset placement, access pattern analysis, storage group profiling |
| **Extended Statistics** | Time series analysis, trend detection, capacity planning, anomaly detection |
| **Parallel Processing** | PAV/HyperPAV emulation, work stealing, async I/O, multi-path support |

---

## Features Summary (56 Headers)

### Core HSM
- `hsm_cds.hpp` - HSM Control Datasets (MCDS/BCDS/OCDS)
- `hsm_command.hpp` - Command Processor with TSO/batch parsing
- `hsm_space_management.hpp` - Automatic space management
- `hsm_interval.hpp` - Interval processing and scheduling
- `hsm_security.hpp` - RACF/SAF security integration
- `hsm_sysplex.hpp` - Sysplex coordination and GRS

### Storage Management
- `hsm_migration.hpp` - Dataset migration (ML0->ML1->ML2)
- `hsm_recall.hpp` - Dataset recall processing
- `hsm_backup.hpp` - Incremental/full backup
- `hsm_recover.hpp` - Dataset recovery
- `hsm_delete.hpp` - Migrated/backup deletion
- `hsm_expiration.hpp` - Expiration management

### VSAM Support
- `hsm_vsam_ksds.hpp` - Key-sequenced datasets
- `hsm_vsam_esds.hpp` - Entry-sequenced datasets
- `hsm_vsam_rrds.hpp` - Relative record datasets
- `hsm_vsam_lds.hpp` - Linear datasets
- `hsm_vsam_cluster.hpp` - Cluster management

### Tape Management
- `hsm_tape.hpp` - Tape volume management
- `hsm_tape_pool.hpp` - Tape pool management
- `hsm_tape_stacking.hpp` - Dataset stacking
- `hsm_recycle.hpp` - Tape recycling

### Enterprise Features
- `hsm_sms.hpp` - SMS integration
- `hsm_cloud.hpp` - Cloud tiering (AWS/Azure/GCP)
- `hsm_abars.hpp` - Aggregate backup/recovery
- `hsm_crq.hpp` - Common Recall Queue
- `hsm_replication.hpp` - Fast replication
- `hsm_compliance.hpp` - Compliance and retention
- `hsm_disaster_recovery.hpp` - DR automation

### Infrastructure
- `hsm_dasd.hpp` - Extended DASD support
- `hsm_catalog.hpp` - ICF catalog integration
- `hsm_smf.hpp` - SMF recording
- `hsm_sdsp.hpp` - Smart dataset placement
- `hsm_parallel.hpp` - Parallel processing

### Monitoring & Operations
- `hsm_statistics.hpp` - Extended statistics
- `hsm_reporting.hpp` - Report generation
- `hsm_logging.hpp` - Enterprise logging
- `hsm_metrics.hpp` - Performance metrics
- `hsm_benchmark.hpp` - Benchmarking

---

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- Header-only library - no external dependencies
- CMake 3.16+ (optional, for building tests)

## Quick Start

```cpp
#include "hsm_cds.hpp"
#include "hsm_command.hpp"
#include "hsm_space_management.hpp"
#include "hsm_security.hpp"
#include "hsm_sysplex.hpp"

using namespace hsm;

int main() {
    // Initialize CDS Manager
    cds::CDSManager cdsManager;
    
    // Initialize Command Processor
    command::CommandProcessor cmdProcessor;
    
    // Initialize Space Manager
    space::SpaceManager spaceManager;
    
    // Initialize Security
    security::SecurityManager securityManager;
    
    // Initialize Sysplex Coordinator
    sysplex::SysplexCoordinator coordinator;
    
    // Process HSM command with security check
    cmdProcessor.registerHandler(command::CommandType::HMIGRATE,
        [&](const command::ParsedCommand& cmd) {
            command::CommandResult result;
            
            // Security check
            auto secResult = securityManager.checkHSMOperation(
                cmd.userId, security::HSMOperation::MIGRATE);
            if (secResult.returnCode != security::SAFReturnCode::SUCCESS) {
                result.returnCode = command::ReturnCode::AUTH_ERROR;
                return result;
            }
            
            // Lock CDS
            coordinator.lockMCDS(cmd.userId);
            
            // Process migration
            auto& opts = std::get<command::MigrateOptions>(cmd.options);
            for (const auto& ds : opts.datasets) {
                cds::MCDDatasetRecord record;
                record.setDatasetName(ds.name);
                record.migrationLevel = cds::MigrationLevel::ML1;
                cdsManager.addMigratedDataset(record);
            }
            
            // Unlock CDS
            coordinator.unlockMCDS(cmd.userId);
            
            result.returnCode = command::ReturnCode::SUCCESS;
            return result;
        });
    
    // Execute command
    auto result = cmdProcessor.process("HMIGRATE DSN(USER.DATA) WAIT", "HSMADMIN");
    
    return result.isSuccess() ? 0 : 1;
}
```

## Building Tests

```bash
mkdir build && cd build
cmake ..
make
./test_v400
```

## Directory Structure

```
dfsmshsm_v4.2.0/
+-- include/           # 56 header files
+-- tests/             # Test suites
+-- examples/          # Usage examples
+-- docs/              # Documentation
+-- CMakeLists.txt     # CMake build
+-- LICENSE            # MIT License
+-- README.md          # This file
```

## License

This project is licensed under the MIT License - see the LICENSE file for details.

------------------------------------------------------------------------------
Copyright (c) 2025 Bennie Shearer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------

DISCLAIMER

This software provides mainframe emulation capabilities. Users are responsible
for proper configuration, security testing, and compliance with applicable
regulations and licensing requirements.

------------------------------------------------------------------------------

## Version History

- **v4.0.2** - Complete HSM core (CDS, Commands, Space Mgmt, Intervals, Security, Sysplex)
- **v3.7.0** - z/OS infrastructure (DASD, Catalog, SMF, SDSP, Statistics, Parallel)
- **v3.6.0** - Enterprise features (SMS, Cloud, ABARS, CRQ, Replication, Compliance)
- **v3.5.0** - Tape pools and reporting
- **v3.4.0** - Logging and benchmarking
- **v3.3.0** - Automatic backup and disaster recovery
- **v3.2.0** - Validation and documentation
- **v3.1.0** - Error handling and Windows support
- **v3.0.0** - Initial release with compression, metrics, REST API

## Author

Bennie Shearer (retired)

## Acknowledgments

Thanks to all my C++ mentors through the years.

Special thanks to:

| Organization | Website |
|--------------|---------|
| DFHSM by IBM | https://www.ibm.com/ |
| CLion by JetBrains s.r.o. | https://www.jetbrains.com/clion/ |
| Claude by Anthropic PBC | https://www.anthropic.com/ |

## Background

See [docs/BACKGROUND.md](docs/BACKGROUND.md) for detailed information about:
- History and purpose of DFSMShsm in mainframe environments
- Why emulation is valuable (modernization, training, development, testing)
- How this project fits into the broader mainframe ecosystem
- Design philosophy (C++20, zero external dependencies, cross-platform)

---

*Copyright (c) 2025 Bennie Shearer - MIT License*
