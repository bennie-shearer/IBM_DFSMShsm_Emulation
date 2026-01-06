/*
 * DFSMShsm Emulator - HSM Control Datasets (MCDS/BCDS/OCDS)
 * @version 4.2.0
 * 
 * Emulates the three critical VSAM control datasets that form the heart of HSM:
 * - MCDS (Migration Control Dataset): Tracks all migrated datasets
 * - BCDS (Backup Control Dataset): Tracks all backup versions
 * - OCDS (Offline Control Dataset): Tracks ML2 tape contents
 * 
 * These datasets use VSAM KSDS structure with specific record formats.
 */

#ifndef HSM_CDS_HPP
#define HSM_CDS_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <optional>
#include <functional>
#include <fstream>
#include <cstring>
#include <atomic>
#include <algorithm>
#include <stdexcept>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace hsm {
namespace cds {

// =============================================================================
// CDS Record Types and Formats
// =============================================================================

// Record type identifiers (first byte of key)
enum class RecordType : uint8_t {
    MCD_DATASET     = 0x01,   // Migrated dataset record
    MCD_VOLUME      = 0x02,   // ML1 volume record
    MCD_TAPEVOL     = 0x03,   // ML2 tape volume record
    BCD_DATASET     = 0x10,   // Backup dataset record
    BCD_VERSION     = 0x11,   // Backup version record
    BCD_VOLUME      = 0x12,   // Backup volume record
    OCD_TAPE        = 0x20,   // Tape volume record
    OCD_DATASET     = 0x21,   // Dataset on tape record
    OCD_FILE        = 0x22,   // File sequence record
    CDS_HEADER      = 0xF0,   // CDS header record
    CDS_FREESPACE   = 0xF1,   // Free space record
    CDS_STATS       = 0xF2    // Statistics record
};

// Migration levels
enum class MigrationLevel : uint8_t {
    ML0 = 0,   // Not migrated (on primary)
    ML1 = 1,   // Migrated to DASD
    ML2 = 2    // Migrated to tape
};

// Backup frequency types
enum class BackupFrequency : uint8_t {
    NONE     = 0,
    DAILY    = 1,
    WEEKLY   = 2,
    MONTHLY  = 3,
    YEARLY   = 4,
    ON_CLOSE = 5   // Backup on dataset close
};

// Dataset organization for tracking
enum class DatasetOrg : uint8_t {
    UNKNOWN = 0,
    PS      = 1,   // Physical sequential
    PO      = 2,   // Partitioned (PDS)
    PDSE    = 3,   // Partitioned extended
    DA      = 4,   // Direct access
    VSAM    = 5,   // VSAM cluster
    HFS     = 6,   // Hierarchical file system
    ZFS     = 7    // zFS aggregate
};

// Timestamp in z/OS format (STCK - Store Clock)
struct STCKTimestamp {
    uint64_t value{0};
    
    STCKTimestamp() = default;
    explicit STCKTimestamp(uint64_t v) : value(v) {}
    
    static STCKTimestamp now() {
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();
        // Convert to STCK format (bit 51 = 1 microsecond)
        return STCKTimestamp(static_cast<uint64_t>(micros) << 12);
    }
    
    std::chrono::system_clock::time_point toTimePoint() const {
        auto micros = static_cast<int64_t>(value >> 12);
        return std::chrono::system_clock::time_point(
            std::chrono::microseconds(micros));
    }
    
    std::string toString() const {
        auto tp = toTimePoint();
        auto time_t_val = std::chrono::system_clock::to_time_t(tp);
        std::tm* tm = std::localtime(&time_t_val);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
        return std::string(buf);
    }
};

// =============================================================================
// MCDS Records - Migration Control Dataset
// =============================================================================

// MCD Dataset Record - tracks a migrated dataset
struct MCDDatasetRecord {
    static constexpr size_t KEY_LENGTH = 44;
    static constexpr size_t MAX_RECORD_LENGTH = 2048;
    
    // Key: Dataset name (44 bytes, padded)
    char datasetName[44]{};
    
    // Record data
    RecordType recordType{RecordType::MCD_DATASET};
    MigrationLevel migrationLevel{MigrationLevel::ML1};
    DatasetOrg dsorg{DatasetOrg::PS};
    uint8_t flags{0};
    
    // Size information
    uint64_t originalSize{0};      // Original size in bytes
    uint64_t compressedSize{0};    // Size after compression
    uint32_t blockSize{0};         // Original block size
    uint32_t recordLength{0};      // Logical record length
    
    // Location information
    char targetVolume[6]{};        // ML1 volume or ML2 tape
    uint32_t startTrack{0};        // Starting track (ML1)
    uint16_t fileSequence{0};      // File sequence on tape (ML2)
    
    // Timestamps
    STCKTimestamp migrationTime;   // When migrated
    STCKTimestamp lastAccessTime;  // Last access before migration
    STCKTimestamp lastReferenceTime; // Last reference date
    STCKTimestamp expirationTime;  // Expiration date
    
    // Management info
    char managementClass[8]{};     // SMS management class
    char storageClass[8]{};        // SMS storage class
    uint32_t recallCount{0};       // Number of recalls
    uint16_t daysNonUsage{0};      // Days since last use at migration
    
    // Flags
    bool isCompressed() const { return flags & 0x01; }
    bool isEncrypted() const { return flags & 0x02; }
    bool isRecallInProgress() const { return flags & 0x04; }
    bool isDeletePending() const { return flags & 0x08; }
    bool isMultiVolume() const { return flags & 0x10; }
    
    void setCompressed(bool v) { if (v) flags |= 0x01; else flags &= ~0x01; }
    void setEncrypted(bool v) { if (v) flags |= 0x02; else flags &= ~0x02; }
    void setRecallInProgress(bool v) { if (v) flags |= 0x04; else flags &= ~0x04; }
    void setDeletePending(bool v) { if (v) flags |= 0x08; else flags &= ~0x08; }
    void setMultiVolume(bool v) { if (v) flags |= 0x10; else flags &= ~0x10; }
    
    std::string getDatasetName() const {
        std::string name(datasetName, 44); auto end = name.find_last_not_of(' '); return end != std::string::npos ? name.substr(0, end + 1) : "";
    }
    
    void setDatasetName(const std::string& name) {
        std::memset(datasetName, ' ', 44);
        std::memcpy(datasetName, name.c_str(), std::min(name.size(), size_t(44)));
    }
};

// MCD Volume Record - tracks ML1 DASD usage
struct MCDVolumeRecord {
    char volumeSerial[6]{};        // Volume serial
    RecordType recordType{RecordType::MCD_VOLUME};
    uint8_t status{0};             // Volume status flags
    
    uint64_t totalCapacity{0};     // Total ML1 capacity
    uint64_t usedSpace{0};         // Used space
    uint64_t freeSpace{0};         // Free space
    uint32_t datasetCount{0};      // Number of datasets
    uint32_t fragmentCount{0};     // Number of free space fragments
    
    STCKTimestamp lastUpdateTime;  // Last update
    STCKTimestamp lastMigrateTime; // Last migration to this volume
    
    char deviceType[8]{};          // Device type (e.g., "3390-9")
    uint16_t cylinderCount{0};     // Number of cylinders allocated
    
    bool isOnline() const { return status & 0x01; }
    bool isFull() const { return status & 0x02; }
    bool isThreshold() const { return status & 0x04; }
    
    std::string getVolumeSerial() const {
        return std::string(volumeSerial, 6);
    }
};

// =============================================================================
// BCDS Records - Backup Control Dataset
// =============================================================================

// BCD Dataset Record - tracks backup versions for a dataset
struct BCDDatasetRecord {
    static constexpr size_t KEY_LENGTH = 44;
    
    char datasetName[44]{};        // Dataset name
    RecordType recordType{RecordType::BCD_DATASET};
    BackupFrequency frequency{BackupFrequency::DAILY};
    uint8_t flags{0};
    
    uint32_t versionCount{0};      // Number of backup versions
    uint32_t maxVersions{0};       // Maximum versions to keep
    uint64_t totalBackupSize{0};   // Total size of all backups
    
    STCKTimestamp firstBackupTime; // First backup
    STCKTimestamp lastBackupTime;  // Most recent backup
    STCKTimestamp nextBackupTime;  // Scheduled next backup
    
    char managementClass[8]{};     // SMS management class
    uint16_t retentionDays{0};     // Retention period
    
    bool isAutoBackup() const { return flags & 0x01; }
    bool isIncrementalEligible() const { return flags & 0x02; }
    bool isCommandBackup() const { return flags & 0x04; }
    
    std::string getDatasetName() const {
        std::string name(datasetName, 44); auto end = name.find_last_not_of(' '); return end != std::string::npos ? name.substr(0, end + 1) : "";
    }
    
    void setDatasetName(const std::string& name) {
        std::memset(datasetName, ' ', 44);
        std::memcpy(datasetName, name.c_str(), std::min(name.size(), size_t(44)));
    }
};

// BCD Version Record - individual backup version
struct BCDVersionRecord {
    char datasetName[44]{};        // Parent dataset
    uint32_t versionNumber{0};     // Version number (1, 2, 3...)
    RecordType recordType{RecordType::BCD_VERSION};
    uint8_t backupType{0};         // 0=full, 1=incremental
    
    uint64_t originalSize{0};      // Original dataset size
    uint64_t backupSize{0};        // Backup size (after compression)
    uint32_t blockCount{0};        // Number of blocks
    
    char volumeSerial[6]{};        // Backup volume
    uint16_t fileSequence{0};      // File sequence on tape
    uint32_t startTrack{0};        // Start track (DASD backup)
    
    STCKTimestamp backupTime;      // When backup was taken
    STCKTimestamp expirationTime;  // When backup expires
    
    uint8_t flags{0};
    
    bool isValid() const { return flags & 0x01; }
    bool isCompressed() const { return flags & 0x02; }
    bool isEncrypted() const { return flags & 0x04; }
    bool isOnTape() const { return flags & 0x08; }
    bool isRecoverable() const { return flags & 0x10; }
};

// =============================================================================
// OCDS Records - Offline Control Dataset
// =============================================================================

// OCD Tape Record - tracks a tape volume
struct OCDTapeRecord {
    char volumeSerial[6]{};        // Tape volume serial
    RecordType recordType{RecordType::OCD_TAPE};
    uint8_t tapeType{0};           // 0=scratch, 1=private, 2=storage group
    uint8_t status{0};
    
    uint64_t capacity{0};          // Tape capacity
    uint64_t usedSpace{0};         // Used space
    uint32_t fileCount{0};         // Number of files
    uint32_t datasetCount{0};      // Number of datasets
    
    STCKTimestamp createTime;      // Tape creation
    STCKTimestamp lastWriteTime;   // Last write
    STCKTimestamp lastReadTime;    // Last read
    STCKTimestamp expirationTime;  // Expiration date
    
    char tapePool[8]{};            // Pool name
    char storageGroup[8]{};        // Storage group
    uint16_t mediaType{0};         // Media type code
    
    bool isOnline() const { return status & 0x01; }
    bool isFull() const { return status & 0x02; }
    bool isExpired() const { return status & 0x04; }
    bool isRecycleCandidate() const { return status & 0x08; }
    bool hasErrors() const { return status & 0x10; }
    
    std::string getVolumeSerial() const {
        return std::string(volumeSerial, 6);
    }
};

// OCD Dataset Record - tracks a dataset on tape
struct OCDDatasetRecord {
    char tapeVolume[6]{};          // Tape volume serial
    uint16_t fileSequence{0};      // File sequence number
    char datasetName[44]{};        // Dataset name
    RecordType recordType{RecordType::OCD_DATASET};
    uint8_t contentType{0};        // 0=migration, 1=backup, 2=dump
    
    uint64_t size{0};              // Size on tape
    uint32_t blockCount{0};        // Block count
    uint32_t blockSize{0};         // Block size
    
    STCKTimestamp writeTime;       // When written to tape
    STCKTimestamp originalRefDate; // Original reference date
    
    MigrationLevel sourceLevel{MigrationLevel::ML1};  // Where it came from
    uint8_t compressionType{0};    // Compression algorithm
    
    std::string getDatasetName() const {
        std::string name(datasetName, 44); auto end = name.find_last_not_of(' '); return end != std::string::npos ? name.substr(0, end + 1) : "";
    }
};

// =============================================================================
// CDS Header and Statistics
// =============================================================================

struct CDSHeader {
    char identifier[9]{"HSMCDS01"};  // CDS identifier
    RecordType recordType{RecordType::CDS_HEADER};
    uint8_t cdsType{0};              // 0=MCDS, 1=BCDS, 2=OCDS
    uint16_t version{0x0380};        // v4.1.0
    
    STCKTimestamp createTime;        // CDS creation time
    STCKTimestamp lastUpdateTime;    // Last update
    STCKTimestamp lastBackupTime;    // Last CDS backup
    
    uint64_t recordCount{0};         // Total records
    uint64_t usedSpace{0};           // Used space
    uint64_t freeSpace{0};           // Free space
    
    char systemId[8]{};              // Owning system
    char hsmVersion[9]{"4.0.2   "};  // HSM version
};

struct CDSStatistics {
    RecordType recordType{RecordType::CDS_STATS};
    uint8_t cdsType{0};
    
    uint64_t totalRecords{0};
    uint64_t addedToday{0};
    uint64_t deletedToday{0};
    uint64_t updatedToday{0};
    
    uint64_t totalSize{0};
    uint64_t averageRecordSize{0};
    
    uint32_t readCount{0};
    uint32_t writeCount{0};
    uint32_t deleteCount{0};
    
    STCKTimestamp statsDate;
};

// =============================================================================
// CDS Manager - Unified Control Dataset Manager
// =============================================================================

class CDSManager {
public:
    struct Config {
        std::string mcdsPath{"./mcds.cds"};
        std::string bcdsPath{"./bcds.cds"};
        std::string ocdsPath{"./ocds.cds"};
        
        size_t cacheSize{10000};        // Records to cache in memory
        bool autoFlush{true};            // Auto-flush on updates
        size_t flushInterval{1000};      // Flush every N updates
        bool enableJournal{true};        // Write-ahead journal
        std::string journalPath{"./cds_journal.log"};
        
        Config() = default;
    };
    
private:
    Config config_;
    mutable std::shared_mutex mutex_;
    
    // In-memory caches
    std::unordered_map<std::string, MCDDatasetRecord> mcdCache_;
    std::unordered_map<std::string, BCDDatasetRecord> bcdCache_;
    std::unordered_map<std::string, OCDTapeRecord> ocdCache_;
    
    // Secondary indexes
    std::multimap<std::string, std::string> volumeToDatasets_;  // Volume -> datasets
    std::multimap<std::string, std::string> datasetToBackups_;  // Dataset -> backup versions
    std::multimap<std::string, std::string> tapeToDatasets_;    // Tape -> datasets
    
    // Statistics
    CDSHeader mcdsHeader_;
    CDSHeader bcdsHeader_;
    CDSHeader ocdsHeader_;
    
    CDSStatistics mcdsStats_;
    CDSStatistics bcdsStats_;
    CDSStatistics ocdsStats_;
    
    std::atomic<uint64_t> pendingUpdates_{0};
    std::atomic<bool> initialized_{false};
    
    // Journal
    std::ofstream journalFile_;
    
    void writeJournal(const std::string& operation, const std::string& key,
                      const std::string& data) {
        if (!config_.enableJournal || !journalFile_.is_open()) return;
        
        auto now = std::chrono::system_clock::now();
        auto time_t_val = std::chrono::system_clock::to_time_t(now);
        
        journalFile_ << time_t_val << "|" << operation << "|" << key << "|"
                     << data.size() << "|" << data << "\n";
        journalFile_.flush();
    }
    
public:
    CDSManager() : config_() {
        initializeHeaders();
    }
    
    explicit CDSManager(const Config& config) : config_(config) {
        initializeHeaders();
        
        if (config_.enableJournal) {
            journalFile_.open(config_.journalPath, std::ios::app);
        }
    }
    
    ~CDSManager() {
        if (journalFile_.is_open()) {
            journalFile_.close();
        }
    }
    
    // =========================================================================
    // MCDS Operations - Migration Control
    // =========================================================================
    
    bool addMigratedDataset(const MCDDatasetRecord& record) {
        std::unique_lock lock(mutex_);
        
        std::string key = record.getDatasetName();
        if (mcdCache_.count(key) > 0) {
            return false;  // Already exists
        }
        
        mcdCache_[key] = record;
        
        // Update volume index
        std::string volser(record.targetVolume, 6);
        volumeToDatasets_.emplace(volser, key);
        
        // Update statistics
        mcdsStats_.totalRecords++;
        mcdsStats_.addedToday++;
        mcdsHeader_.recordCount++;
        mcdsHeader_.lastUpdateTime = STCKTimestamp::now();
        
        writeJournal("MCD_ADD", key, std::to_string(record.originalSize));
        
        checkFlush();
        return true;
    }
    
    std::optional<MCDDatasetRecord> getMigratedDataset(const std::string& datasetName) const {
        std::shared_lock lock(mutex_);
        
        auto it = mcdCache_.find(datasetName);
        if (it != mcdCache_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    bool updateMigratedDataset(const MCDDatasetRecord& record) {
        std::unique_lock lock(mutex_);
        
        std::string key = record.getDatasetName();
        auto it = mcdCache_.find(key);
        if (it == mcdCache_.end()) {
            return false;  // Not found
        }
        
        it->second = record;
        mcdsStats_.updatedToday++;
        mcdsHeader_.lastUpdateTime = STCKTimestamp::now();
        
        writeJournal("MCD_UPD", key, std::to_string(record.originalSize));
        
        checkFlush();
        return true;
    }
    
    bool deleteMigratedDataset(const std::string& datasetName) {
        std::unique_lock lock(mutex_);
        
        auto it = mcdCache_.find(datasetName);
        if (it == mcdCache_.end()) {
            return false;
        }
        
        // Remove from volume index
        std::string volser(it->second.targetVolume, 6);
        auto range = volumeToDatasets_.equal_range(volser);
        for (auto vit = range.first; vit != range.second; ) {
            if (vit->second == datasetName) {
                vit = volumeToDatasets_.erase(vit);
            } else {
                ++vit;
            }
        }
        
        mcdCache_.erase(it);
        
        mcdsStats_.totalRecords--;
        mcdsStats_.deletedToday++;
        mcdsHeader_.recordCount--;
        mcdsHeader_.lastUpdateTime = STCKTimestamp::now();
        
        writeJournal("MCD_DEL", datasetName, "");
        
        checkFlush();
        return true;
    }
    
    std::vector<MCDDatasetRecord> getMigratedDatasetsByVolume(const std::string& volser) const {
        std::shared_lock lock(mutex_);
        
        std::vector<MCDDatasetRecord> result;
        auto range = volumeToDatasets_.equal_range(volser);
        
        for (auto it = range.first; it != range.second; ++it) {
            auto dit = mcdCache_.find(it->second);
            if (dit != mcdCache_.end()) {
                result.push_back(dit->second);
            }
        }
        
        return result;
    }
    
    std::vector<MCDDatasetRecord> getMigratedDatasetsByLevel(MigrationLevel level) const {
        std::shared_lock lock(mutex_);
        
        std::vector<MCDDatasetRecord> result;
        for (const auto& [key, record] : mcdCache_) {
            if (record.migrationLevel == level) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    std::vector<MCDDatasetRecord> searchMigratedDatasets(const std::string& pattern) const {
        std::shared_lock lock(mutex_);
        
        std::vector<MCDDatasetRecord> result;
        for (const auto& [key, record] : mcdCache_) {
            if (matchPattern(key, pattern)) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    // =========================================================================
    // BCDS Operations - Backup Control
    // =========================================================================
    
    bool addBackupDataset(const BCDDatasetRecord& record) {
        std::unique_lock lock(mutex_);
        
        std::string key = record.getDatasetName();
        if (bcdCache_.count(key) > 0) {
            return false;
        }
        
        bcdCache_[key] = record;
        
        bcdsStats_.totalRecords++;
        bcdsStats_.addedToday++;
        bcdsHeader_.recordCount++;
        bcdsHeader_.lastUpdateTime = STCKTimestamp::now();
        
        writeJournal("BCD_ADD", key, std::to_string(record.versionCount));
        
        checkFlush();
        return true;
    }
    
    std::optional<BCDDatasetRecord> getBackupDataset(const std::string& datasetName) const {
        std::shared_lock lock(mutex_);
        
        auto it = bcdCache_.find(datasetName);
        if (it != bcdCache_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    bool updateBackupDataset(const BCDDatasetRecord& record) {
        std::unique_lock lock(mutex_);
        
        std::string key = record.getDatasetName();
        auto it = bcdCache_.find(key);
        if (it == bcdCache_.end()) {
            return false;
        }
        
        it->second = record;
        bcdsStats_.updatedToday++;
        bcdsHeader_.lastUpdateTime = STCKTimestamp::now();
        
        writeJournal("BCD_UPD", key, std::to_string(record.versionCount));
        
        checkFlush();
        return true;
    }
    
    bool deleteBackupDataset(const std::string& datasetName) {
        std::unique_lock lock(mutex_);
        
        auto it = bcdCache_.find(datasetName);
        if (it == bcdCache_.end()) {
            return false;
        }
        
        bcdCache_.erase(it);
        
        bcdsStats_.totalRecords--;
        bcdsStats_.deletedToday++;
        bcdsHeader_.recordCount--;
        bcdsHeader_.lastUpdateTime = STCKTimestamp::now();
        
        writeJournal("BCD_DEL", datasetName, "");
        
        checkFlush();
        return true;
    }
    
    std::vector<BCDDatasetRecord> getBackupDatasetsByFrequency(BackupFrequency freq) const {
        std::shared_lock lock(mutex_);
        
        std::vector<BCDDatasetRecord> result;
        for (const auto& [key, record] : bcdCache_) {
            if (record.frequency == freq) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    std::vector<BCDDatasetRecord> getBackupsPendingProcessing(
            const STCKTimestamp& cutoffTime) const {
        std::shared_lock lock(mutex_);
        
        std::vector<BCDDatasetRecord> result;
        for (const auto& [key, record] : bcdCache_) {
            if (record.nextBackupTime.value <= cutoffTime.value) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    // =========================================================================
    // OCDS Operations - Offline Control
    // =========================================================================
    
    bool addTapeVolume(const OCDTapeRecord& record) {
        std::unique_lock lock(mutex_);
        
        std::string key = record.getVolumeSerial();
        if (ocdCache_.count(key) > 0) {
            return false;
        }
        
        ocdCache_[key] = record;
        
        ocdsStats_.totalRecords++;
        ocdsStats_.addedToday++;
        ocdsHeader_.recordCount++;
        ocdsHeader_.lastUpdateTime = STCKTimestamp::now();
        
        writeJournal("OCD_ADD", key, std::to_string(record.capacity));
        
        checkFlush();
        return true;
    }
    
    std::optional<OCDTapeRecord> getTapeVolume(const std::string& volser) const {
        std::shared_lock lock(mutex_);
        
        auto it = ocdCache_.find(volser);
        if (it != ocdCache_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    bool updateTapeVolume(const OCDTapeRecord& record) {
        std::unique_lock lock(mutex_);
        
        std::string key = record.getVolumeSerial();
        auto it = ocdCache_.find(key);
        if (it == ocdCache_.end()) {
            return false;
        }
        
        it->second = record;
        ocdsStats_.updatedToday++;
        ocdsHeader_.lastUpdateTime = STCKTimestamp::now();
        
        writeJournal("OCD_UPD", key, std::to_string(record.usedSpace));
        
        checkFlush();
        return true;
    }
    
    bool deleteTapeVolume(const std::string& volser) {
        std::unique_lock lock(mutex_);
        
        auto it = ocdCache_.find(volser);
        if (it == ocdCache_.end()) {
            return false;
        }
        
        ocdCache_.erase(it);
        
        ocdsStats_.totalRecords--;
        ocdsStats_.deletedToday++;
        ocdsHeader_.recordCount--;
        ocdsHeader_.lastUpdateTime = STCKTimestamp::now();
        
        writeJournal("OCD_DEL", volser, "");
        
        checkFlush();
        return true;
    }
    
    std::vector<OCDTapeRecord> getTapesByPool(const std::string& poolName) const {
        std::shared_lock lock(mutex_);
        
        std::vector<OCDTapeRecord> result;
        for (const auto& [key, record] : ocdCache_) {
            if (std::string(record.tapePool, 8).find(poolName) != std::string::npos) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    std::vector<OCDTapeRecord> getRecycleCandidates() const {
        std::shared_lock lock(mutex_);
        
        std::vector<OCDTapeRecord> result;
        auto now = STCKTimestamp::now();
        
        for (const auto& [key, record] : ocdCache_) {
            if (record.isRecycleCandidate() || 
                (record.expirationTime.value > 0 && 
                 record.expirationTime.value < now.value)) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    // =========================================================================
    // Cross-CDS Queries
    // =========================================================================
    
    struct DatasetStatus {
        std::string datasetName;
        bool isMigrated{false};
        MigrationLevel migrationLevel{MigrationLevel::ML0};
        bool hasBackup{false};
        uint32_t backupVersions{0};
        STCKTimestamp lastBackupTime;
        std::string currentLocation;
    };
    
    DatasetStatus getDatasetStatus(const std::string& datasetName) const {
        std::shared_lock lock(mutex_);
        
        DatasetStatus status;
        status.datasetName = datasetName;
        
        // Check MCDS
        auto mcdIt = mcdCache_.find(datasetName);
        if (mcdIt != mcdCache_.end()) {
            status.isMigrated = true;
            status.migrationLevel = mcdIt->second.migrationLevel;
            status.currentLocation = std::string(mcdIt->second.targetVolume, 6);
        }
        
        // Check BCDS
        auto bcdIt = bcdCache_.find(datasetName);
        if (bcdIt != bcdCache_.end()) {
            status.hasBackup = true;
            status.backupVersions = bcdIt->second.versionCount;
            status.lastBackupTime = bcdIt->second.lastBackupTime;
        }
        
        return status;
    }
    
    // =========================================================================
    // Statistics and Reporting
    // =========================================================================
    
    struct CDSSummary {
        std::string cdsName;
        uint64_t recordCount{0};
        uint64_t addedToday{0};
        uint64_t deletedToday{0};
        uint64_t updatedToday{0};
        STCKTimestamp lastUpdate;
    };
    
    CDSSummary getMCDSSummary() const {
        std::shared_lock lock(mutex_);
        
        CDSSummary summary;
        summary.cdsName = "MCDS";
        summary.recordCount = mcdsHeader_.recordCount;
        summary.addedToday = mcdsStats_.addedToday;
        summary.deletedToday = mcdsStats_.deletedToday;
        summary.updatedToday = mcdsStats_.updatedToday;
        summary.lastUpdate = mcdsHeader_.lastUpdateTime;
        return summary;
    }
    
    CDSSummary getBCDSSummary() const {
        std::shared_lock lock(mutex_);
        
        CDSSummary summary;
        summary.cdsName = "BCDS";
        summary.recordCount = bcdsHeader_.recordCount;
        summary.addedToday = bcdsStats_.addedToday;
        summary.deletedToday = bcdsStats_.deletedToday;
        summary.updatedToday = bcdsStats_.updatedToday;
        summary.lastUpdate = bcdsHeader_.lastUpdateTime;
        return summary;
    }
    
    CDSSummary getOCDSSummary() const {
        std::shared_lock lock(mutex_);
        
        CDSSummary summary;
        summary.cdsName = "OCDS";
        summary.recordCount = ocdsHeader_.recordCount;
        summary.addedToday = ocdsStats_.addedToday;
        summary.deletedToday = ocdsStats_.deletedToday;
        summary.updatedToday = ocdsStats_.updatedToday;
        summary.lastUpdate = ocdsHeader_.lastUpdateTime;
        return summary;
    }
    
    uint64_t getTotalMigratedDatasets() const {
        std::shared_lock lock(mutex_);
        return mcdCache_.size();
    }
    
    uint64_t getTotalBackupDatasets() const {
        std::shared_lock lock(mutex_);
        return bcdCache_.size();
    }
    
    uint64_t getTotalTapeVolumes() const {
        std::shared_lock lock(mutex_);
        return ocdCache_.size();
    }
    
    // =========================================================================
    // Persistence
    // =========================================================================
    
    bool saveToDisk() {
        std::shared_lock lock(mutex_);
        return saveToDiskInternal();
    }
    
private:
    // Internal version called while lock is already held
    bool saveToDiskInternal() {
        try {
            // Save MCDS
            saveCDS(config_.mcdsPath, "MCDS", mcdCache_, mcdsHeader_);
            
            // Save BCDS
            saveCDS(config_.bcdsPath, "BCDS", bcdCache_, bcdsHeader_);
            
            // Save OCDS
            saveCDS(config_.ocdsPath, "OCDS", ocdCache_, ocdsHeader_);
            
            return true;
        } catch (...) {
            return false;
        }
    }
    
public:
    
    bool loadFromDisk() {
        std::unique_lock lock(mutex_);
        
        try {
            loadCDS(config_.mcdsPath, "MCDS", mcdCache_, mcdsHeader_);
            loadCDS(config_.bcdsPath, "BCDS", bcdCache_, bcdsHeader_);
            loadCDS(config_.ocdsPath, "OCDS", ocdCache_, ocdsHeader_);
            
            rebuildIndexes();
            initialized_ = true;
            return true;
        } catch (...) {
            return false;
        }
    }
    
    void flush() {
        if (pendingUpdates_ > 0) {
            saveToDiskInternal();  // Use internal version since we're called while holding lock
            pendingUpdates_ = 0;
        }
    }
    
    void resetDailyStatistics() {
        std::unique_lock lock(mutex_);
        
        mcdsStats_.addedToday = 0;
        mcdsStats_.deletedToday = 0;
        mcdsStats_.updatedToday = 0;
        
        bcdsStats_.addedToday = 0;
        bcdsStats_.deletedToday = 0;
        bcdsStats_.updatedToday = 0;
        
        ocdsStats_.addedToday = 0;
        ocdsStats_.deletedToday = 0;
        ocdsStats_.updatedToday = 0;
        
        auto now = STCKTimestamp::now();
        mcdsStats_.statsDate = now;
        bcdsStats_.statsDate = now;
        ocdsStats_.statsDate = now;
    }
    
private:
    void initializeHeaders() {
        mcdsHeader_.cdsType = 0;  // MCDS
        std::memcpy(mcdsHeader_.identifier, "HSMCDS01", 8);
        mcdsHeader_.createTime = STCKTimestamp::now();
        mcdsHeader_.lastUpdateTime = mcdsHeader_.createTime;
        
        bcdsHeader_.cdsType = 1;  // BCDS
        std::memcpy(bcdsHeader_.identifier, "HSMCDS01", 8);
        bcdsHeader_.createTime = STCKTimestamp::now();
        bcdsHeader_.lastUpdateTime = bcdsHeader_.createTime;
        
        ocdsHeader_.cdsType = 2;  // OCDS
        std::memcpy(ocdsHeader_.identifier, "HSMCDS01", 8);
        ocdsHeader_.createTime = STCKTimestamp::now();
        ocdsHeader_.lastUpdateTime = ocdsHeader_.createTime;
        
        mcdsStats_.cdsType = 0;
        bcdsStats_.cdsType = 1;
        ocdsStats_.cdsType = 2;
    }
    
    void checkFlush() {
        pendingUpdates_++;
        if (config_.autoFlush && pendingUpdates_ >= config_.flushInterval) {
            flush();
        }
    }
    
    void rebuildIndexes() {
        volumeToDatasets_.clear();
        for (const auto& [key, record] : mcdCache_) {
            std::string volser(record.targetVolume, 6);
            volumeToDatasets_.emplace(volser, key);
        }
        
        tapeToDatasets_.clear();
        // Tape to datasets would be populated from OCD dataset records
    }
    
    bool matchPattern(const std::string& str, const std::string& pattern) const {
        // Simple wildcard matching (* and ?)
        size_t s = 0, p = 0;
        size_t starIdx = std::string::npos;
        size_t matchIdx = 0;
        
        while (s < str.size()) {
            if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == str[s])) {
                s++;
                p++;
            } else if (p < pattern.size() && pattern[p] == '*') {
                starIdx = p++;
                matchIdx = s;
            } else if (starIdx != std::string::npos) {
                p = starIdx + 1;
                s = ++matchIdx;
            } else {
                return false;
            }
        }
        
        while (p < pattern.size() && pattern[p] == '*') {
            p++;
        }
        
        return p == pattern.size();
    }
    
    template<typename T>
    void saveCDS(const std::string& path, const std::string& /*name*/,
                 const T& cache, const CDSHeader& header) {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot open CDS file for writing");
        }
        
        // Write header
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        
        // Write record count
        uint64_t count = cache.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));
        
        // Write records
        for (const auto& [key, record] : cache) {
            size_t keyLen = key.size();
            file.write(reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
            file.write(key.c_str(), keyLen);
            file.write(reinterpret_cast<const char*>(&record), sizeof(record));
        }
    }
    
    template<typename T>
    void loadCDS(const std::string& path, const std::string& /*name*/,
                 T& cache, CDSHeader& header) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return;  // File doesn't exist yet
        }
        
        // Read header
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        
        // Read record count
        uint64_t count = 0;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        
        // Read records
        cache.clear();
        for (uint64_t i = 0; i < count; ++i) {
            size_t keyLen = 0;
            file.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
            
            std::string key(keyLen, '\0');
            file.read(&key[0], keyLen);
            
            typename T::mapped_type record;
            file.read(reinterpret_cast<char*>(&record), sizeof(record));
            
            cache[key] = record;
        }
    }
};

// =============================================================================
// CDS Utility Functions
// =============================================================================

inline std::string migrationLevelToString(MigrationLevel level) {
    switch (level) {
        case MigrationLevel::ML0: return "ML0";
        case MigrationLevel::ML1: return "ML1";
        case MigrationLevel::ML2: return "ML2";
        default: return "UNKNOWN";
    }
}

inline std::string backupFrequencyToString(BackupFrequency freq) {
    switch (freq) {
        case BackupFrequency::NONE: return "NONE";
        case BackupFrequency::DAILY: return "DAILY";
        case BackupFrequency::WEEKLY: return "WEEKLY";
        case BackupFrequency::MONTHLY: return "MONTHLY";
        case BackupFrequency::YEARLY: return "YEARLY";
        case BackupFrequency::ON_CLOSE: return "ON_CLOSE";
        default: return "UNKNOWN";
    }
}

inline std::string datasetOrgToString(DatasetOrg org) {
    switch (org) {
        case DatasetOrg::UNKNOWN: return "UNKNOWN";
        case DatasetOrg::PS: return "PS";
        case DatasetOrg::PO: return "PO";
        case DatasetOrg::PDSE: return "PDSE";
        case DatasetOrg::DA: return "DA";
        case DatasetOrg::VSAM: return "VSAM";
        case DatasetOrg::HFS: return "HFS";
        case DatasetOrg::ZFS: return "ZFS";
        default: return "UNKNOWN";
    }
}

}  // namespace cds
}  // namespace hsm

#endif // HSM_CDS_HPP
