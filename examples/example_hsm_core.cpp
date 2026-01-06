/*
 * DFSMShsm v4.1.4 - HSM Core Example
 * @version 4.2.0
 * 
 * Demonstrates the complete HSM core functionality:
 * - CDS Management
 * - Command Processing
 * - Space Management
 * - Interval Processing
 * - Security (RACF/SAF)
 * - Sysplex Coordination
 */

#include <iostream>
#include <iomanip>

#include "hsm_cds.hpp"
#include "hsm_command.hpp"
#include "hsm_space_management.hpp"
#include "hsm_interval.hpp"
#include "hsm_security.hpp"
#include "hsm_sysplex.hpp"

using namespace hsm;

void demoCDS() {
    std::cout << "=== CDS Management Demo ===" << std::endl;
    
    cds::CDSManager manager;
    
    // Add migrated dataset
    cds::MCDDatasetRecord mcd;
    mcd.setDatasetName("USER.PROD.DATA");
    mcd.migrationLevel = cds::MigrationLevel::ML1;
    mcd.originalSize = 1000000;
    mcd.compressedSize = 400000;
    mcd.setCompressed(true);
    manager.addMigratedDataset(mcd);
    
    // Add backup
    cds::BCDDatasetRecord bcd;
    bcd.setDatasetName("USER.PROD.DATA");
    bcd.frequency = cds::BackupFrequency::DAILY;
    bcd.versionCount = 3;
    manager.addBackupDataset(bcd);
    
    // Query status
    auto status = manager.getDatasetStatus("USER.PROD.DATA");
    std::cout << "Dataset: USER.PROD.DATA" << std::endl;
    std::cout << "  Migrated: " << (status.isMigrated ? "Yes" : "No") << std::endl;
    std::cout << "  Has Backup: " << (status.hasBackup ? "Yes" : "No") << std::endl;
    std::cout << "  Backup Versions: " << status.backupVersions << std::endl;
    
    // Summary
    auto summary = manager.getMCDSSummary();
    std::cout << "MCDS Records: " << summary.recordCount << std::endl;
    std::cout << std::endl;
}

void demoCommand() {
    std::cout << "=== Command Processor Demo ===" << std::endl;
    
    command::CommandProcessor processor;
    
    // Register handler
    processor.registerHandler(command::CommandType::HMIGRATE,
        [](const command::ParsedCommand& cmd) {
            command::CommandResult result;
            auto& opts = std::get<command::MigrateOptions>(cmd.options);
            
            std::cout << "Processing HMIGRATE for " << opts.datasets.size() 
                      << " dataset(s)" << std::endl;
            for (const auto& ds : opts.datasets) {
                std::cout << "  - " << ds.name << std::endl;
            }
            
            result.returnCode = command::ReturnCode::SUCCESS;
            result.message = "Migration queued";
            return result;
        });
    
    // Process commands
    auto r1 = processor.process("HMIGRATE DSN(USER.DATA.TEST) WAIT");
    std::cout << "Result: " << r1.message << std::endl;
    
    auto r2 = processor.process("HMIG DSN(USER.DATA.FILE2) PRIORITY(7)");
    std::cout << "Result: " << r2.message << std::endl;
    std::cout << std::endl;
}

void demoSpaceManagement() {
    std::cout << "=== Space Management Demo ===" << std::endl;
    
    space::SpaceManager manager;
    
    // Register volume
    space::VolumeSpaceInfo vol;
    vol.volser = "VOL001";
    vol.totalCapacity = 1000000000;   // 1 GB
    vol.allocatedSpace = 850000000;   // 850 MB (85%)
    vol.freeSpace = 150000000;
    vol.recalculate();
    manager.registerVolume(vol);
    
    // Add datasets
    for (int i = 0; i < 5; i++) {
        space::DatasetSpaceInfo ds;
        ds.datasetName = "USER.TEST.DS" + std::to_string(i);
        ds.volser = "VOL001";
        ds.allocatedSpace = 10000000;  // 10 MB
        ds.lastReferenceDate = std::chrono::system_clock::now() - 
                               std::chrono::hours(24 * (i * 10));
        ds.updateDaysNonUsage();
        manager.registerDataset(ds);
    }
    
    // Check thresholds
    auto overThreshold = manager.getVolumesAboveThreshold(80.0);
    std::cout << "Volumes above 80%: " << overThreshold.size() << std::endl;
    
    // Select migration candidates
    auto candidates = manager.selectMigrationCandidates("VOL001", 3);
    std::cout << "Migration candidates:" << std::endl;
    for (const auto& cand : candidates) {
        std::cout << "  - " << cand.dataset.datasetName 
                  << " (score " << std::fixed << std::setprecision(2) 
                  << cand.score << ")" << std::endl;
    }
    
    // Space analysis
    auto analysis = manager.analyzeSpace();
    std::cout << "Total utilization: " 
              << std::fixed << std::setprecision(1) 
              << analysis.overallUtilization << "%" << std::endl;
    std::cout << std::endl;
}

void demoInterval() {
    std::cout << "=== Interval Processing Demo ===" << std::endl;
    
    interval::IntervalScheduler scheduler;
    
    // Create intervals using factory
    auto primary = interval::IntervalFactory::createPrimarySpaceManagement();
    auto backup = interval::IntervalFactory::createAutomaticBackup();
    
    scheduler.addInterval(primary);
    scheduler.addInterval(backup);
    
    std::cout << "Registered intervals:" << std::endl;
    
    auto primaryInt = scheduler.getInterval("PRIMARY_SM");
    if (primaryInt) {
        std::cout << "  - " << primaryInt->name << std::endl;
        std::cout << "    Windows: " << primaryInt->windows.size() << std::endl;
        std::cout << "    Max tasks: " << primaryInt->maxTasks << std::endl;
    }
    
    auto backupInt = scheduler.getInterval("AUTO_BACKUP");
    if (backupInt) {
        std::cout << "  - " << backupInt->name << std::endl;
        std::cout << "    Windows: " << backupInt->windows.size() << std::endl;
        std::cout << "    Max tasks: " << backupInt->maxTasks << std::endl;
    }
    
    // Submit task
    scheduler.submitTask("PRIMARY_SM", []() {
        std::cout << "    Executing space management task..." << std::endl;
        return true;
    });
    
    std::cout << "Pending tasks: " << scheduler.getPendingTaskCount() << std::endl;
    std::cout << std::endl;
}

void demoSecurity() {
    std::cout << "=== Security (RACF/SAF) Demo ===" << std::endl;
    
    security::SecurityManager manager;
    
    // Add user
    security::UserProfile user;
    user.userId = "HSMADMIN";
    user.defaultGroup = "HSMSUPVR";
    user.connectGroups.push_back("HSMSUPVR");
    user.isSpecial = true;
    manager.addUser(user);
    
    // Add profile
    security::ResourceProfile profile;
    profile.profileName = "USER.**";
    profile.resourceClass = security::ResourceClass::DATASET;
    profile.universalAccess = security::AccessLevel::NONE;
    profile.isGeneric = true;
    
    security::ResourceProfile::AccessEntry entry;
    entry.identity = "HSMADMIN";
    entry.access = security::AccessLevel::ALTER;
    profile.accessList.push_back(entry);
    manager.addProfile(profile);
    
    // Check access
    auto dsResult = manager.checkDatasetAccess("HSMADMIN", "USER.PROD.DATA",
                                                security::AccessLevel::UPDATE);
    std::cout << "Dataset access check: " 
              << security::safReturnCodeToString(dsResult.returnCode) << std::endl;
    
    auto hsmResult = manager.checkHSMOperation("HSMADMIN",
                                                security::HSMOperation::MIGRATE,
                                                "USER.PROD.DATA");
    std::cout << "HSM operation check: "
              << security::safReturnCodeToString(hsmResult.returnCode) << std::endl;
    std::cout << std::endl;
}

void demoSysplex() {
    std::cout << "=== Sysplex Coordination Demo ===" << std::endl;
    
    sysplex::SysplexCoordinator::Config config;
    config.hostId = "HSM1";
    config.systemId = "SYS1";
    config.sysplexName = "PLEX1";
    
    sysplex::SysplexCoordinator coordinator(config);
    
    // Register another host
    sysplex::HostInfo remoteHost;
    remoteHost.hostId = "HSM2";
    remoteHost.systemId = "SYS2";
    remoteHost.status = sysplex::HostStatus::ACTIVE;
    coordinator.registerHost(remoteHost);
    
    std::cout << "Hosts in sysplex:" << std::endl;
    for (const auto& host : coordinator.getAllHosts()) {
        std::cout << "  - " << host.hostId << " (" 
                  << sysplex::hostStatusToString(host.status) << ")" << std::endl;
    }
    
    // Lock CDS
    std::cout << "Locking MCDS..." << std::endl;
    auto lockResult = coordinator.lockMCDS("JOB001");
    std::cout << "  Result: " << sysplex::enqResultToString(lockResult) << std::endl;
    
    // Lock dataset
    std::cout << "Locking dataset..." << std::endl;
    auto dsLock = coordinator.lockDataset("JOB001", "USER.PROD.DATA");
    std::cout << "  Result: " << sysplex::enqResultToString(dsLock) << std::endl;
    
    // Release locks
    coordinator.unlockDataset("JOB001", "USER.PROD.DATA");
    coordinator.unlockMCDS("JOB001");
    std::cout << "Locks released." << std::endl;
    
    // Statistics
    auto stats = coordinator.getStatistics();
    std::cout << "Sysplex statistics:" << std::endl;
    std::cout << "  Total hosts: " << stats.totalHosts << std::endl;
    std::cout << "  Active hosts: " << stats.activeHosts << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "DFSMShsm v4.1.0 - HSM Core Demo" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    demoCDS();
    demoCommand();
    demoSpaceManagement();
    demoInterval();
    demoSecurity();
    demoSysplex();
    
    std::cout << "========================================" << std::endl;
    std::cout << "Demo complete!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
