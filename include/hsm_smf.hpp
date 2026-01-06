/**
 * @file hsm_smf.hpp
 * @brief SMF (System Management Facilities) Recording for DFSMShsm Emulator
 * @version 4.2.0
 * 
 * Provides comprehensive SMF record generation including:
 * - Type 14 records (Dataset input)
 * - Type 15 records (Dataset output)
 * - Type 42 records (SMS statistics)
 * - Type 62 records (VSAM component statistics)
 * - Custom HSM records (Type 240+)
 * - Record buffering and writing
 * - SMF record formatting and parsing
 * 
 * @copyright Copyright (c) 2024 DFSMShsm Emulator Project
 */

#ifndef HSM_SMF_HPP
#define HSM_SMF_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <cstring>

namespace hsm {
namespace smf {

// ============================================================================
// SMF Record Types
// ============================================================================

/**
 * @brief Standard SMF record types
 */
enum class RecordType : uint8_t {
    TYPE_14 = 14,    ///< Dataset input statistics
    TYPE_15 = 15,    ///< Dataset output statistics
    TYPE_30 = 30,    ///< Common address space work
    TYPE_42 = 42,    ///< SMS statistics
    TYPE_62 = 62,    ///< VSAM component statistics
    TYPE_64 = 64,    ///< VSAM volume dataset entry
    TYPE_80 = 80,    ///< RACF processing
    TYPE_240 = 240,  ///< HSM migration record
    TYPE_241 = 241,  ///< HSM recall record
    TYPE_242 = 242,  ///< HSM backup record
    TYPE_243 = 243,  ///< HSM recovery record
    TYPE_244 = 244   ///< HSM space management record
};

/**
 * @brief SMF record subtypes
 */
enum class SubType : uint8_t {
    SUBTYPE_1 = 1,
    SUBTYPE_2 = 2,
    SUBTYPE_3 = 3,
    SUBTYPE_4 = 4,
    SUBTYPE_5 = 5,
    SUBTYPE_6 = 6,
    SUBTYPE_7 = 7,
    SUBTYPE_8 = 8,
    SUBTYPE_9 = 9,
    SUBTYPE_10 = 10
};

// ============================================================================
// SMF Record Header Structures
// ============================================================================

/**
 * @brief Standard SMF record header (RDW + Header)
 */
#pragma pack(push, 1)
struct SMFRecordHeader {
    uint16_t rdwLength;      ///< Record Descriptor Word - length
    uint16_t rdwSegment;     ///< Record Descriptor Word - segment
    uint8_t flag;            ///< System indicator
    uint8_t recordType;      ///< Record type
    uint32_t time;           ///< Time (hundredths of seconds since midnight)
    uint32_t date;           ///< Date (packed decimal 0cyydddF)
    char systemId[4];        ///< System identifier
    
    SMFRecordHeader() 
        : rdwLength(0), rdwSegment(0), flag(0), recordType(0),
          time(0), date(0) {
        std::memset(systemId, ' ', sizeof(systemId));
    }
};
#pragma pack(pop)

/**
 * @brief Extended SMF header (for subtype records)
 */
#pragma pack(push, 1)
struct SMFExtendedHeader {
    SMFRecordHeader base;
    uint16_t subType;        ///< Subtype
    uint16_t sectionCount;   ///< Number of sections
    
    SMFExtendedHeader() : subType(0), sectionCount(0) {}
};
#pragma pack(pop)

/**
 * @brief Self-defining section triplet
 */
#pragma pack(push, 1)
struct SectionTriplet {
    uint32_t offset;         ///< Offset to section
    uint16_t length;         ///< Length of section
    uint16_t count;          ///< Number of instances
    
    SectionTriplet() : offset(0), length(0), count(0) {}
};
#pragma pack(pop)

// ============================================================================
// Type 14/15 Records - Dataset I/O Statistics
// ============================================================================

/**
 * @brief Device class codes
 */
enum class DeviceClass : uint8_t {
    DASD = 0x20,
    TAPE = 0x80,
    VIRTUAL = 0x40
};

/**
 * @brief Type 14/15 common section
 */
struct Type1415Section {
    std::string jobName;           ///< Job name (8 chars)
    std::string stepName;          ///< Step name (8 chars)
    std::string ddName;            ///< DD name (8 chars)
    std::string datasetName;       ///< Dataset name (44 chars)
    uint8_t deviceClass;           ///< Device class
    std::string volser;            ///< Volume serial (6 chars)
    uint64_t blockCount;           ///< Block count
    uint64_t byteCount;            ///< Byte count
    uint32_t excp;                 ///< EXCP count
    uint16_t bufferSize;           ///< Buffer size
    uint16_t blockSize;            ///< Block size
    uint16_t lrecl;                ///< Logical record length
    std::string recfm;             ///< Record format
    std::chrono::system_clock::time_point openTime;
    std::chrono::system_clock::time_point closeTime;
    
    Type1415Section()
        : deviceClass(0), blockCount(0), byteCount(0), excp(0),
          bufferSize(0), blockSize(0), lrecl(80) {}
};

/**
 * @brief Type 14 record - Dataset input
 */
struct Type14Record {
    SMFRecordHeader header;
    Type1415Section section;
    uint64_t recordsRead;
    uint64_t blocksRead;
    
    Type14Record() : recordsRead(0), blocksRead(0) {
        header.recordType = static_cast<uint8_t>(RecordType::TYPE_14);
    }
};

/**
 * @brief Type 15 record - Dataset output
 */
struct Type15Record {
    SMFRecordHeader header;
    Type1415Section section;
    uint64_t recordsWritten;
    uint64_t blocksWritten;
    bool newDataset;
    bool extendedDataset;
    
    Type15Record() : recordsWritten(0), blocksWritten(0),
                     newDataset(false), extendedDataset(false) {
        header.recordType = static_cast<uint8_t>(RecordType::TYPE_15);
    }
};

// ============================================================================
// Type 42 Records - SMS Statistics
// ============================================================================

/**
 * @brief Type 42 subtype 6 - Storage group statistics
 */
struct Type42Subtype6 {
    SMFExtendedHeader header;
    std::string storageGroupName;
    uint32_t volumeCount;
    uint64_t totalCapacity;
    uint64_t usedSpace;
    uint64_t freeSpace;
    double utilizationPercent;
    uint32_t datasetCount;
    std::chrono::system_clock::time_point timestamp;
    
    Type42Subtype6() 
        : volumeCount(0), totalCapacity(0), usedSpace(0), freeSpace(0),
          utilizationPercent(0), datasetCount(0) {
        header.base.recordType = static_cast<uint8_t>(RecordType::TYPE_42);
        header.subType = 6;
    }
};

/**
 * @brief Type 42 subtype 9 - ACS routine statistics
 */
struct Type42Subtype9 {
    SMFExtendedHeader header;
    std::string acsRoutineName;
    uint64_t invocationCount;
    uint64_t successCount;
    uint64_t failureCount;
    double averageResponseTime;  ///< Milliseconds
    std::chrono::system_clock::time_point timestamp;
    
    Type42Subtype9()
        : invocationCount(0), successCount(0), failureCount(0),
          averageResponseTime(0) {
        header.base.recordType = static_cast<uint8_t>(RecordType::TYPE_42);
        header.subType = 9;
    }
};

// ============================================================================
// Type 62 Records - VSAM Statistics
// ============================================================================

/**
 * @brief VSAM access type
 */
enum class VSAMAccessType : uint8_t {
    SEQUENTIAL = 0x01,
    DIRECT = 0x02,
    SKIP_SEQUENTIAL = 0x04,
    BACKWARD = 0x08
};

/**
 * @brief Type 62 VSAM component statistics
 */
struct Type62Record {
    SMFExtendedHeader header;
    std::string clusterName;       ///< VSAM cluster name
    std::string componentName;     ///< Component name (data/index)
    uint8_t componentType;         ///< 1=data, 2=index
    std::string volser;
    uint64_t ciSplits;             ///< CI splits
    uint64_t caSplits;             ///< CA splits
    uint64_t recordsInserted;
    uint64_t recordsUpdated;
    uint64_t recordsDeleted;
    uint64_t recordsRetrieved;
    uint64_t excpCount;
    double ciUtilization;
    double caUtilization;
    std::chrono::system_clock::time_point timestamp;
    
    Type62Record()
        : componentType(1), ciSplits(0), caSplits(0),
          recordsInserted(0), recordsUpdated(0), recordsDeleted(0),
          recordsRetrieved(0), excpCount(0),
          ciUtilization(0), caUtilization(0) {
        header.base.recordType = static_cast<uint8_t>(RecordType::TYPE_62);
    }
};

// ============================================================================
// HSM Custom Records (Type 240+)
// ============================================================================

/**
 * @brief Type 240 - HSM Migration record
 */
struct Type240Record {
    SMFExtendedHeader header;
    std::string datasetName;
    std::string sourceVolser;
    std::string targetVolser;
    uint8_t migrationLevel;        ///< ML1 or ML2
    uint64_t dataMigrated;         ///< Bytes migrated
    uint64_t dataCompressed;       ///< Compressed size
    double compressionRatio;
    uint32_t migrationDuration;    ///< Milliseconds
    std::chrono::system_clock::time_point timestamp;
    std::string reasonCode;
    
    Type240Record()
        : migrationLevel(1), dataMigrated(0), dataCompressed(0),
          compressionRatio(1.0), migrationDuration(0) {
        header.base.recordType = static_cast<uint8_t>(RecordType::TYPE_240);
    }
};

/**
 * @brief Type 241 - HSM Recall record
 */
struct Type241Record {
    SMFExtendedHeader header;
    std::string datasetName;
    std::string sourceVolser;      ///< ML1/ML2 source
    std::string targetVolser;      ///< Primary DASD
    uint8_t recallLevel;           ///< Level recalled from
    uint64_t dataRecalled;         ///< Bytes recalled
    uint32_t recallDuration;       ///< Milliseconds
    std::string requestingJob;
    std::string requestingUser;
    bool waitRecall;               ///< Wait or nowait
    std::chrono::system_clock::time_point timestamp;
    
    Type241Record()
        : recallLevel(1), dataRecalled(0), recallDuration(0), waitRecall(true) {
        header.base.recordType = static_cast<uint8_t>(RecordType::TYPE_241);
    }
};

/**
 * @brief Type 242 - HSM Backup record
 */
struct Type242Record {
    SMFExtendedHeader header;
    std::string datasetName;
    std::string sourceVolser;
    std::string targetVolser;
    uint16_t backupVersion;
    uint64_t dataBackedUp;
    bool fullBackup;               ///< Full or incremental
    bool compressed;
    double compressionRatio;
    uint32_t backupDuration;
    std::chrono::system_clock::time_point timestamp;
    std::string backupProfile;
    
    Type242Record()
        : backupVersion(1), dataBackedUp(0), fullBackup(true),
          compressed(false), compressionRatio(1.0), backupDuration(0) {
        header.base.recordType = static_cast<uint8_t>(RecordType::TYPE_242);
    }
};

/**
 * @brief Type 243 - HSM Recovery record
 */
struct Type243Record {
    SMFExtendedHeader header;
    std::string datasetName;
    std::string sourceVolser;      ///< Backup source
    std::string targetVolser;      ///< Recovery target
    uint16_t recoveredVersion;
    uint64_t dataRecovered;
    uint32_t recoveryDuration;
    bool replaceExisting;
    std::string requestingUser;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::system_clock::time_point originalBackupTime;
    
    Type243Record()
        : recoveredVersion(1), dataRecovered(0), recoveryDuration(0),
          replaceExisting(false) {
        header.base.recordType = static_cast<uint8_t>(RecordType::TYPE_243);
    }
};

/**
 * @brief Type 244 - HSM Space Management record
 */
struct Type244Record {
    SMFExtendedHeader header;
    std::string volumeSerial;
    std::string storageGroup;
    uint64_t spaceBefore;          ///< Free space before
    uint64_t spaceAfter;           ///< Free space after
    uint32_t datasetsMigrated;
    uint32_t datasetsRecalled;
    uint32_t datasetsDeleted;
    double thresholdHigh;
    double thresholdLow;
    uint32_t processingDuration;
    std::chrono::system_clock::time_point timestamp;
    std::string processType;       ///< INTERVAL, PRIMARY, SECONDARY
    
    Type244Record()
        : spaceBefore(0), spaceAfter(0), datasetsMigrated(0),
          datasetsRecalled(0), datasetsDeleted(0),
          thresholdHigh(90.0), thresholdLow(80.0), processingDuration(0) {
        header.base.recordType = static_cast<uint8_t>(RecordType::TYPE_244);
    }
};

// ============================================================================
// SMF Record Builder
// ============================================================================

/**
 * @brief Builds SMF records in binary format
 */
class SMFRecordBuilder {
public:
    SMFRecordBuilder() {
        std::memcpy(systemId_, "HSM1", 4);
    }
    
    /**
     * @brief Set system ID
     */
    void setSystemId(const std::string& sysid) {
        std::memset(systemId_, ' ', 4);
        std::memcpy(systemId_, sysid.c_str(), std::min(sysid.size(), size_t(4)));
    }
    
    /**
     * @brief Build Type 14 record
     */
    std::vector<uint8_t> buildType14(const Type14Record& record) {
        std::vector<uint8_t> data;
        
        // Build header
        appendHeader(data, RecordType::TYPE_14);
        
        // Build section
        appendType1415Section(data, record.section);
        
        // Add input-specific fields
        appendUint64(data, record.recordsRead);
        appendUint64(data, record.blocksRead);
        
        // Update RDW
        updateRDW(data);
        
        return data;
    }
    
    /**
     * @brief Build Type 15 record
     */
    std::vector<uint8_t> buildType15(const Type15Record& record) {
        std::vector<uint8_t> data;
        
        appendHeader(data, RecordType::TYPE_15);
        appendType1415Section(data, record.section);
        
        appendUint64(data, record.recordsWritten);
        appendUint64(data, record.blocksWritten);
        appendUint8(data, record.newDataset ? 1 : 0);
        appendUint8(data, record.extendedDataset ? 1 : 0);
        
        updateRDW(data);
        return data;
    }
    
    /**
     * @brief Build Type 42 subtype 6 record
     */
    std::vector<uint8_t> buildType42S6(const Type42Subtype6& record) {
        std::vector<uint8_t> data;
        
        appendExtendedHeader(data, RecordType::TYPE_42, 6);
        
        appendString(data, record.storageGroupName, 8);
        appendUint32(data, record.volumeCount);
        appendUint64(data, record.totalCapacity);
        appendUint64(data, record.usedSpace);
        appendUint64(data, record.freeSpace);
        appendDouble(data, record.utilizationPercent);
        appendUint32(data, record.datasetCount);
        appendTimestamp(data, record.timestamp);
        
        updateRDW(data);
        return data;
    }
    
    /**
     * @brief Build Type 62 record
     */
    std::vector<uint8_t> buildType62(const Type62Record& record) {
        std::vector<uint8_t> data;
        
        appendExtendedHeader(data, RecordType::TYPE_62, 1);
        
        appendString(data, record.clusterName, 44);
        appendString(data, record.componentName, 44);
        appendUint8(data, record.componentType);
        appendString(data, record.volser, 6);
        appendUint64(data, record.ciSplits);
        appendUint64(data, record.caSplits);
        appendUint64(data, record.recordsInserted);
        appendUint64(data, record.recordsUpdated);
        appendUint64(data, record.recordsDeleted);
        appendUint64(data, record.recordsRetrieved);
        appendUint64(data, record.excpCount);
        appendDouble(data, record.ciUtilization);
        appendDouble(data, record.caUtilization);
        appendTimestamp(data, record.timestamp);
        
        updateRDW(data);
        return data;
    }
    
    /**
     * @brief Build Type 240 HSM Migration record
     */
    std::vector<uint8_t> buildType240(const Type240Record& record) {
        std::vector<uint8_t> data;
        
        appendExtendedHeader(data, RecordType::TYPE_240, 1);
        
        appendString(data, record.datasetName, 44);
        appendString(data, record.sourceVolser, 6);
        appendString(data, record.targetVolser, 6);
        appendUint8(data, record.migrationLevel);
        appendUint64(data, record.dataMigrated);
        appendUint64(data, record.dataCompressed);
        appendDouble(data, record.compressionRatio);
        appendUint32(data, record.migrationDuration);
        appendTimestamp(data, record.timestamp);
        appendString(data, record.reasonCode, 8);
        
        updateRDW(data);
        return data;
    }
    
    /**
     * @brief Build Type 241 HSM Recall record
     */
    std::vector<uint8_t> buildType241(const Type241Record& record) {
        std::vector<uint8_t> data;
        
        appendExtendedHeader(data, RecordType::TYPE_241, 1);
        
        appendString(data, record.datasetName, 44);
        appendString(data, record.sourceVolser, 6);
        appendString(data, record.targetVolser, 6);
        appendUint8(data, record.recallLevel);
        appendUint64(data, record.dataRecalled);
        appendUint32(data, record.recallDuration);
        appendString(data, record.requestingJob, 8);
        appendString(data, record.requestingUser, 8);
        appendUint8(data, record.waitRecall ? 1 : 0);
        appendTimestamp(data, record.timestamp);
        
        updateRDW(data);
        return data;
    }
    
    /**
     * @brief Build Type 242 HSM Backup record
     */
    std::vector<uint8_t> buildType242(const Type242Record& record) {
        std::vector<uint8_t> data;
        
        appendExtendedHeader(data, RecordType::TYPE_242, 1);
        
        appendString(data, record.datasetName, 44);
        appendString(data, record.sourceVolser, 6);
        appendString(data, record.targetVolser, 6);
        appendUint16(data, record.backupVersion);
        appendUint64(data, record.dataBackedUp);
        appendUint8(data, record.fullBackup ? 1 : 0);
        appendUint8(data, record.compressed ? 1 : 0);
        appendDouble(data, record.compressionRatio);
        appendUint32(data, record.backupDuration);
        appendTimestamp(data, record.timestamp);
        appendString(data, record.backupProfile, 8);
        
        updateRDW(data);
        return data;
    }

private:
    char systemId_[4];
    
    void appendHeader(std::vector<uint8_t>& data, RecordType type) {
        // RDW placeholder
        appendUint16(data, 0);  // Length (updated later)
        appendUint16(data, 0);  // Segment
        
        // SMF header
        appendUint8(data, 0x80);  // System indicator
        appendUint8(data, static_cast<uint8_t>(type));
        appendUint32(data, getCurrentTime());
        appendUint32(data, getCurrentDate());
        data.insert(data.end(), systemId_, systemId_ + 4);
    }
    
    void appendExtendedHeader(std::vector<uint8_t>& data, RecordType type, uint16_t subtype) {
        appendHeader(data, type);
        appendUint16(data, subtype);
        appendUint16(data, 1);  // Section count
    }
    
    void appendType1415Section(std::vector<uint8_t>& data, const Type1415Section& section) {
        appendString(data, section.jobName, 8);
        appendString(data, section.stepName, 8);
        appendString(data, section.ddName, 8);
        appendString(data, section.datasetName, 44);
        appendUint8(data, section.deviceClass);
        appendString(data, section.volser, 6);
        appendUint64(data, section.blockCount);
        appendUint64(data, section.byteCount);
        appendUint32(data, section.excp);
        appendUint16(data, section.bufferSize);
        appendUint16(data, section.blockSize);
        appendUint16(data, section.lrecl);
        appendString(data, section.recfm, 4);
        appendTimestamp(data, section.openTime);
        appendTimestamp(data, section.closeTime);
    }
    
    void updateRDW(std::vector<uint8_t>& data) {
        uint16_t length = static_cast<uint16_t>(data.size());
        data[0] = static_cast<uint8_t>(length >> 8);
        data[1] = static_cast<uint8_t>(length & 0xFF);
    }
    
    void appendUint8(std::vector<uint8_t>& data, uint8_t value) {
        data.push_back(value);
    }
    
    void appendUint16(std::vector<uint8_t>& data, uint16_t value) {
        data.push_back(static_cast<uint8_t>(value >> 8));
        data.push_back(static_cast<uint8_t>(value & 0xFF));
    }
    
    void appendUint32(std::vector<uint8_t>& data, uint32_t value) {
        data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(value & 0xFF));
    }
    
    void appendUint64(std::vector<uint8_t>& data, uint64_t value) {
        for (int i = 7; i >= 0; i--) {
            data.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }
    
    void appendDouble(std::vector<uint8_t>& data, double value) {
        // Store as fixed-point with 4 decimal places
        int64_t fixed = static_cast<int64_t>(value * 10000);
        appendUint64(data, static_cast<uint64_t>(fixed));
    }
    
    void appendString(std::vector<uint8_t>& data, const std::string& str, size_t length) {
        for (size_t i = 0; i < length; i++) {
            if (i < str.size()) {
                data.push_back(static_cast<uint8_t>(str[i]));
            } else {
                data.push_back(' ');  // Pad with spaces
            }
        }
    }
    
    void appendTimestamp(std::vector<uint8_t>& data, 
                         const std::chrono::system_clock::time_point& tp) {
        auto time = std::chrono::system_clock::to_time_t(tp);
        appendUint64(data, static_cast<uint64_t>(time));
    }
    
    uint32_t getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto midnight = std::chrono::floor<std::chrono::days>(now);
        auto duration = now - midnight;
        auto hundredths = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 10;
        return static_cast<uint32_t>(hundredths);
    }
    
    uint32_t getCurrentDate() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&time);
        
        // Format: 0cyydddF (packed decimal)
        int year = tm->tm_year;  // Years since 1900
        int day = tm->tm_yday + 1;  // Day of year (1-366)
        
        uint32_t century = (year >= 100) ? 1 : 0;
        year = year % 100;
        
        return (century << 24) | (year << 16) | (day << 4) | 0x0F;
    }
};

// ============================================================================
// SMF Record Writer
// ============================================================================

/**
 * @brief Configuration for SMF writer
 */
struct SMFWriterConfig {
    std::string outputPath;          ///< Output file path
    size_t bufferSize;               ///< Buffer size in bytes
    bool asyncWrite;                 ///< Use async writing
    uint32_t flushIntervalMs;        ///< Flush interval in ms
    std::vector<RecordType> enabledTypes;  ///< Types to write
    
    SMFWriterConfig()
        : outputPath("smf_records.bin"), bufferSize(65536),
          asyncWrite(true), flushIntervalMs(1000) {
        // Enable all HSM types by default
        enabledTypes = {
            RecordType::TYPE_14, RecordType::TYPE_15,
            RecordType::TYPE_42, RecordType::TYPE_62,
            RecordType::TYPE_240, RecordType::TYPE_241,
            RecordType::TYPE_242, RecordType::TYPE_243,
            RecordType::TYPE_244
        };
    }
};

/**
 * @brief SMF record writer with buffering
 */
class SMFWriter {
public:
    explicit SMFWriter(const SMFWriterConfig& config = SMFWriterConfig())
        : config_(config), running_(false), recordsWritten_(0), bytesWritten_(0) {}
    
    ~SMFWriter() {
        stop();
    }
    
    /**
     * @brief Start the writer
     */
    bool start() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (running_) return true;
        
        outputFile_.open(config_.outputPath, std::ios::binary | std::ios::app);
        if (!outputFile_.is_open()) {
            return false;
        }
        
        running_ = true;
        
        if (config_.asyncWrite) {
            writerThread_ = std::thread(&SMFWriter::writerLoop, this);
        }
        
        return true;
    }
    
    /**
     * @brief Stop the writer
     */
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) return;
            running_ = false;
        }
        
        cv_.notify_one();
        
        if (writerThread_.joinable()) {
            writerThread_.join();
        }
        
        flush();
        
        if (outputFile_.is_open()) {
            outputFile_.close();
        }
    }
    
    /**
     * @brief Write a record
     */
    void writeRecord(const std::vector<uint8_t>& record) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!running_) return;
        
        if (config_.asyncWrite) {
            recordQueue_.push(record);
            cv_.notify_one();
        } else {
            writeRecordInternal(record);
        }
    }
    
    /**
     * @brief Write Type 14 record
     */
    void writeType14(const Type14Record& record) {
        if (!isTypeEnabled(RecordType::TYPE_14)) return;
        writeRecord(builder_.buildType14(record));
    }
    
    /**
     * @brief Write Type 15 record
     */
    void writeType15(const Type15Record& record) {
        if (!isTypeEnabled(RecordType::TYPE_15)) return;
        writeRecord(builder_.buildType15(record));
    }
    
    /**
     * @brief Write Type 42 subtype 6 record
     */
    void writeType42S6(const Type42Subtype6& record) {
        if (!isTypeEnabled(RecordType::TYPE_42)) return;
        writeRecord(builder_.buildType42S6(record));
    }
    
    /**
     * @brief Write Type 62 record
     */
    void writeType62(const Type62Record& record) {
        if (!isTypeEnabled(RecordType::TYPE_62)) return;
        writeRecord(builder_.buildType62(record));
    }
    
    /**
     * @brief Write Type 240 migration record
     */
    void writeType240(const Type240Record& record) {
        if (!isTypeEnabled(RecordType::TYPE_240)) return;
        writeRecord(builder_.buildType240(record));
    }
    
    /**
     * @brief Write Type 241 recall record
     */
    void writeType241(const Type241Record& record) {
        if (!isTypeEnabled(RecordType::TYPE_241)) return;
        writeRecord(builder_.buildType241(record));
    }
    
    /**
     * @brief Write Type 242 backup record
     */
    void writeType242(const Type242Record& record) {
        if (!isTypeEnabled(RecordType::TYPE_242)) return;
        writeRecord(builder_.buildType242(record));
    }
    
    /**
     * @brief Flush buffered records
     */
    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        while (!recordQueue_.empty()) {
            writeRecordInternal(recordQueue_.front());
            recordQueue_.pop();
        }
        
        if (outputFile_.is_open()) {
            outputFile_.flush();
        }
    }
    
    /**
     * @brief Get statistics
     */
    std::string getStatistics() const {
        std::ostringstream oss;
        oss << "SMF Writer Statistics:\n"
            << "  Output: " << config_.outputPath << "\n"
            << "  Status: " << (running_ ? "Running" : "Stopped") << "\n"
            << "  Records Written: " << recordsWritten_ << "\n"
            << "  Bytes Written: " << bytesWritten_ << "\n"
            << "  Queue Size: " << recordQueue_.size() << "\n";
        return oss.str();
    }

private:
    SMFWriterConfig config_;
    SMFRecordBuilder builder_;
    std::ofstream outputFile_;
    std::queue<std::vector<uint8_t>> recordQueue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread writerThread_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> recordsWritten_;
    std::atomic<uint64_t> bytesWritten_;
    
    bool isTypeEnabled(RecordType type) const {
        return std::find(config_.enabledTypes.begin(), 
                        config_.enabledTypes.end(), type) != config_.enabledTypes.end();
    }
    
    void writeRecordInternal(const std::vector<uint8_t>& record) {
        if (outputFile_.is_open()) {
            outputFile_.write(reinterpret_cast<const char*>(record.data()), 
                            record.size());
            recordsWritten_++;
            bytesWritten_ += record.size();
        }
    }
    
    void writerLoop() {
        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);
            
            cv_.wait_for(lock, std::chrono::milliseconds(config_.flushIntervalMs),
                        [this] { return !running_ || !recordQueue_.empty(); });
            
            while (!recordQueue_.empty()) {
                writeRecordInternal(recordQueue_.front());
                recordQueue_.pop();
            }
            
            if (outputFile_.is_open()) {
                outputFile_.flush();
            }
        }
    }
};

// ============================================================================
// SMF Record Reader
// ============================================================================

/**
 * @brief Parsed SMF record
 */
struct ParsedSMFRecord {
    RecordType type;
    uint16_t subType;
    std::chrono::system_clock::time_point timestamp;
    std::vector<uint8_t> rawData;
    std::map<std::string, std::string> fields;
    
    ParsedSMFRecord() : type(RecordType::TYPE_14), subType(0) {}
};

/**
 * @brief SMF record reader
 */
class SMFReader {
public:
    explicit SMFReader(const std::string& inputPath)
        : inputPath_(inputPath), recordsRead_(0) {}
    
    /**
     * @brief Open the SMF file
     */
    bool open() {
        inputFile_.open(inputPath_, std::ios::binary);
        return inputFile_.is_open();
    }
    
    /**
     * @brief Close the file
     */
    void close() {
        if (inputFile_.is_open()) {
            inputFile_.close();
        }
    }
    
    /**
     * @brief Read next record
     */
    std::optional<ParsedSMFRecord> readNext() {
        if (!inputFile_.is_open() || inputFile_.eof()) {
            return std::nullopt;
        }
        
        // Read RDW
        uint8_t rdw[4];
        inputFile_.read(reinterpret_cast<char*>(rdw), 4);
        if (inputFile_.gcount() < 4) {
            return std::nullopt;
        }
        
        uint16_t length = (static_cast<uint16_t>(rdw[0]) << 8) | rdw[1];
        if (length < 4) {
            return std::nullopt;
        }
        
        ParsedSMFRecord record;
        record.rawData.resize(length);
        std::memcpy(record.rawData.data(), rdw, 4);
        
        inputFile_.read(reinterpret_cast<char*>(record.rawData.data() + 4), 
                       length - 4);
        if (inputFile_.gcount() < static_cast<std::streamsize>(length - 4)) {
            return std::nullopt;
        }
        
        // Parse header
        if (length >= 18) {
            record.type = static_cast<RecordType>(record.rawData[5]);
            
            // Parse timestamp
            uint32_t time = (static_cast<uint32_t>(record.rawData[6]) << 24) |
                           (static_cast<uint32_t>(record.rawData[7]) << 16) |
                           (static_cast<uint32_t>(record.rawData[8]) << 8) |
                           record.rawData[9];
            uint32_t date = (static_cast<uint32_t>(record.rawData[10]) << 24) |
                           (static_cast<uint32_t>(record.rawData[11]) << 16) |
                           (static_cast<uint32_t>(record.rawData[12]) << 8) |
                           record.rawData[13];
            
            record.timestamp = parseDateTime(time, date);
        }
        
        // Parse subtype for extended header
        if (length >= 22) {
            record.subType = (static_cast<uint16_t>(record.rawData[18]) << 8) |
                            record.rawData[19];
        }
        
        recordsRead_++;
        return record;
    }
    
    /**
     * @brief Read all records
     */
    std::vector<ParsedSMFRecord> readAll() {
        std::vector<ParsedSMFRecord> records;
        
        while (auto record = readNext()) {
            records.push_back(*record);
        }
        
        return records;
    }
    
    /**
     * @brief Filter records by type
     */
    std::vector<ParsedSMFRecord> readByType(RecordType type) {
        std::vector<ParsedSMFRecord> records;
        
        while (auto record = readNext()) {
            if (record->type == type) {
                records.push_back(*record);
            }
        }
        
        return records;
    }
    
    /**
     * @brief Get read statistics
     */
    uint64_t getRecordsRead() const { return recordsRead_; }

private:
    std::string inputPath_;
    std::ifstream inputFile_;
    uint64_t recordsRead_;
    
    std::chrono::system_clock::time_point parseDateTime(uint32_t time, uint32_t date) {
        // Extract date components
        int century = (date >> 24) & 0x01;
        int year = ((date >> 16) & 0xFF);
        int dayOfYear = ((date >> 4) & 0xFFF);
        
        year += (century ? 2000 : 1900);
        
        // Convert hundredths of seconds to time
        int hours = static_cast<int>(time / 360000);
        int minutes = static_cast<int>((time % 360000) / 6000);
        int seconds = static_cast<int>((time % 6000) / 100);
        
        // Build time_point (simplified)
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_yday = dayOfYear - 1;
        tm.tm_hour = hours;
        tm.tm_min = minutes;
        tm.tm_sec = seconds;
        
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }
};

// ============================================================================
// SMF Manager - Integrated SMF Operations
// ============================================================================

/**
 * @brief Event types for SMF recording
 */
enum class SMFEventType {
    DATASET_OPEN,
    DATASET_CLOSE,
    DATASET_MIGRATION,
    DATASET_RECALL,
    DATASET_BACKUP,
    DATASET_RECOVERY,
    SPACE_MANAGEMENT,
    VSAM_ACCESS,
    SMS_ACTIVITY
};

/**
 * @brief SMF event data
 */
struct SMFEvent {
    SMFEventType type;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> attributes;
    
    SMFEvent() : type(SMFEventType::DATASET_OPEN),
                 timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Comprehensive SMF manager
 */
class SMFManager {
public:
    explicit SMFManager(const SMFWriterConfig& config = SMFWriterConfig())
        : writer_(config), config_(config) {}
    
    /**
     * @brief Start SMF recording
     */
    bool start() {
        return writer_.start();
    }
    
    /**
     * @brief Stop SMF recording
     */
    void stop() {
        writer_.stop();
    }
    
    /**
     * @brief Record dataset open
     */
    void recordDatasetOpen(const std::string& dsname, const std::string& volser,
                           const std::string& jobName, bool forInput) {
        if (forInput) {
            Type14Record record;
            record.section.datasetName = dsname;
            record.section.volser = volser;
            record.section.jobName = jobName;
            record.section.openTime = std::chrono::system_clock::now();
            writer_.writeType14(record);
        } else {
            Type15Record record;
            record.section.datasetName = dsname;
            record.section.volser = volser;
            record.section.jobName = jobName;
            record.section.openTime = std::chrono::system_clock::now();
            writer_.writeType15(record);
        }
    }
    
    /**
     * @brief Record dataset migration
     */
    void recordMigration(const std::string& dsname, const std::string& sourceVol,
                         const std::string& targetVol, uint8_t level,
                         uint64_t bytes, uint64_t compressedBytes,
                         uint32_t durationMs) {
        Type240Record record;
        record.datasetName = dsname;
        record.sourceVolser = sourceVol;
        record.targetVolser = targetVol;
        record.migrationLevel = level;
        record.dataMigrated = bytes;
        record.dataCompressed = compressedBytes;
        record.compressionRatio = bytes > 0 ? 
            static_cast<double>(bytes) / compressedBytes : 1.0;
        record.migrationDuration = durationMs;
        record.timestamp = std::chrono::system_clock::now();
        writer_.writeType240(record);
    }
    
    /**
     * @brief Record dataset recall
     */
    void recordRecall(const std::string& dsname, const std::string& sourceVol,
                      const std::string& targetVol, uint8_t level,
                      uint64_t bytes, uint32_t durationMs,
                      const std::string& requestingJob) {
        Type241Record record;
        record.datasetName = dsname;
        record.sourceVolser = sourceVol;
        record.targetVolser = targetVol;
        record.recallLevel = level;
        record.dataRecalled = bytes;
        record.recallDuration = durationMs;
        record.requestingJob = requestingJob;
        record.timestamp = std::chrono::system_clock::now();
        writer_.writeType241(record);
    }
    
    /**
     * @brief Record dataset backup
     */
    void recordBackup(const std::string& dsname, const std::string& sourceVol,
                      const std::string& targetVol, uint16_t version,
                      uint64_t bytes, bool full, bool compressed,
                      uint32_t durationMs) {
        Type242Record record;
        record.datasetName = dsname;
        record.sourceVolser = sourceVol;
        record.targetVolser = targetVol;
        record.backupVersion = version;
        record.dataBackedUp = bytes;
        record.fullBackup = full;
        record.compressed = compressed;
        record.backupDuration = durationMs;
        record.timestamp = std::chrono::system_clock::now();
        writer_.writeType242(record);
    }
    
    /**
     * @brief Record storage group statistics
     */
    void recordStorageGroupStats(const std::string& sgName, uint32_t volumes,
                                  uint64_t total, uint64_t used, uint64_t free,
                                  uint32_t datasets) {
        Type42Subtype6 record;
        record.storageGroupName = sgName;
        record.volumeCount = volumes;
        record.totalCapacity = total;
        record.usedSpace = used;
        record.freeSpace = free;
        record.utilizationPercent = total > 0 ? 100.0 * used / total : 0;
        record.datasetCount = datasets;
        record.timestamp = std::chrono::system_clock::now();
        writer_.writeType42S6(record);
    }
    
    /**
     * @brief Record VSAM statistics
     */
    void recordVSAMStats(const std::string& cluster, const std::string& component,
                          uint8_t compType, uint64_t ciSplits, uint64_t caSplits,
                          uint64_t inserts, uint64_t updates, uint64_t deletes,
                          uint64_t retrieves) {
        Type62Record record;
        record.clusterName = cluster;
        record.componentName = component;
        record.componentType = compType;
        record.ciSplits = ciSplits;
        record.caSplits = caSplits;
        record.recordsInserted = inserts;
        record.recordsUpdated = updates;
        record.recordsDeleted = deletes;
        record.recordsRetrieved = retrieves;
        record.timestamp = std::chrono::system_clock::now();
        writer_.writeType62(record);
    }
    
    /**
     * @brief Flush all records
     */
    void flush() {
        writer_.flush();
    }
    
    /**
     * @brief Get statistics
     */
    std::string getStatistics() const {
        return writer_.getStatistics();
    }
    
    /**
     * @brief Get the writer
     */
    SMFWriter& getWriter() { return writer_; }

private:
    SMFWriter writer_;
    SMFWriterConfig config_;
};

} // namespace smf
} // namespace hsm

#endif // HSM_SMF_HPP
