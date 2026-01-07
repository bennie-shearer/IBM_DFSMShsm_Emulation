/*
 * DFSMShsm Emulator - ABARS (Aggregate Backup and Recovery Support)
 * @version 4.2.0
 * 
 * Provides comprehensive ABARS functionality including:
 * - Aggregate group management
 * - ABARS control file processing
 * - Activity log generation and analysis
 * - Stack processing and management
 * - Recovery point objectives
 * - Cross-system aggregate support
 * 
 * Author: DFSMShsm Emulator Team
 * License: MIT
 */

#ifndef HSM_ABARS_HPP
#define HSM_ABARS_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <memory>
#include <functional>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <variant>
#include <queue>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <ctime>

namespace dfsmshsm {
namespace abars {

// ============================================================================
// Forward Declarations
// ============================================================================

class AggregateGroup;
class ABARSControlFile;
class ActivityLog;
class StackManager;
class ABARSManager;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief ABARS operation types
 */
enum class ABARSOperation {
    ABACKUP,           // Aggregate backup
    ARECOVER,          // Aggregate recovery
    VERIFY,            // Verify aggregate
    LIST,              // List aggregate contents
    DELETE,            // Delete aggregate
    COPY               // Copy aggregate
};

/**
 * @brief Aggregate backup status
 */
enum class AggregateStatus {
    ACTIVE,            // Valid and usable
    INCOMPLETE,        // Backup did not complete
    EXPIRED,           // Past retention period
    DELETED,           // Marked for deletion
    RECOVERING,        // Recovery in progress
    VERIFIED,          // Verification passed
    CORRUPTED          // Integrity check failed
};

/**
 * @brief Stack status
 */
enum class StackStatus {
    AVAILABLE,         // Ready for use
    IN_USE,            // Currently being written/read
    FULL,              // No more space
    SCRATCH,           // Available for reuse
    EXPIRED,           // Past retention
    ERROR              // Hardware or integrity error
};

/**
 * @brief Activity log entry type
 */
enum class ActivityLogType {
    INFO,
    WARNING,
    ERROR,
    START,
    END,
    PROGRESS,
    DATASET_PROCESSED,
    VOLUME_MOUNTED,
    CHECKPOINT,
    RECOVERY_POINT
};

/**
 * @brief Data mover type
 */
enum class DataMoverType {
    HSM_NATIVE,        // DFSMShsm native data mover
    DFSMS_DSS,         // DFSMS Data Set Services
    FDR,               // Innovation FDR
    CUSTOM             // Custom data mover
};

/**
 * @brief Selection criteria type
 */
enum class SelectionType {
    INCLUDE,           // Include matching datasets
    EXCLUDE,           // Exclude matching datasets
    FILTER             // Filter based on condition
};

// ============================================================================
// Utility Structures
// ============================================================================

/**
 * @brief Dataset selection criteria
 */
struct SelectionCriteria {
    SelectionType type{SelectionType::INCLUDE};
    std::string datasetNameMask;         // Pattern with wildcards
    std::string managementClass;
    std::string storageClass;
    std::string storageGroup;
    std::string volumeMask;
    uint64_t minSize{0};
    uint64_t maxSize{UINT64_MAX};
    uint32_t minDaysUnused{0};
    uint32_t maxDaysUnused{UINT32_MAX};
    bool includeMigrated{true};
    bool includeBackedUp{true};
    bool includeEmpty{false};
    
    bool matches(const std::string& dsn, const std::string& mc = "",
                 [[maybe_unused]] const std::string& sc = "", uint64_t size = 0) const {
        // Simple pattern matching
        if (!datasetNameMask.empty()) {
            std::string pattern = "^";
            for (char c : datasetNameMask) {
                switch (c) {
                    case '*': pattern += ".*"; break;
                    case '%': pattern += "."; break;
                    case '.': pattern += "\\."; break;
                    default: pattern += c; break;
                }
            }
            pattern += "$";
            
            try {
                std::regex re(pattern, std::regex::icase);
                if (!std::regex_match(dsn, re)) return type == SelectionType::EXCLUDE;
            } catch (...) {
                return type == SelectionType::EXCLUDE;
            }
        }
        
        if (size < minSize || size > maxSize) return type == SelectionType::EXCLUDE;
        
        if (!managementClass.empty() && mc != managementClass) {
            return type == SelectionType::EXCLUDE;
        }
        
        return type == SelectionType::INCLUDE;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << (type == SelectionType::INCLUDE ? "INCLUDE" : 
                type == SelectionType::EXCLUDE ? "EXCLUDE" : "FILTER")
            << " DSN(" << datasetNameMask << ")";
        if (!managementClass.empty()) oss << " MC(" << managementClass << ")";
        return oss.str();
    }
};

/**
 * @brief Aggregate backup options
 */
struct ABackupOptions {
    std::string outputClass;             // Output tape class
    std::string unitName;                // Tape unit name
    uint32_t stackSize{255};             // Files per stack
    bool optimizeStacking{true};
    bool compress{true};
    std::string compressionAlgorithm{"ZSTD"};
    bool verify{true};                   // Verify after backup
    bool deleteOnSuccess{false};         // Delete source after successful backup
    bool concurrent{false};              // Allow concurrent access
    uint32_t maxDatasets{0};             // 0 = unlimited
    uint32_t maxTasks{4};                // Parallel tasks
    DataMoverType dataMover{DataMoverType::HSM_NATIVE};
    std::string targetVolumes;           // Specific target volumes
    bool spanning{true};                 // Allow volume spanning
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "StackSize=" << stackSize
            << ", Compress=" << (compress ? "Y" : "N")
            << ", Verify=" << (verify ? "Y" : "N")
            << ", MaxTasks=" << maxTasks;
        return oss.str();
    }
};

/**
 * @brief Aggregate recovery options
 */
struct ARecoverOptions {
    std::string targetVolumes;           // Target volumes for recovery
    bool replaceExisting{false};         // Replace existing datasets
    bool catalogOnly{false};             // Update catalog only
    bool verifyFirst{true};              // Verify aggregate before recovery
    bool restoreCatalog{true};           // Restore catalog entries
    uint32_t maxDatasets{0};             // 0 = unlimited
    uint32_t maxTasks{4};                // Parallel tasks
    std::string newHighLevelQualifier;   // Rename with new HLQ
    bool skipMigrated{false};            // Skip migrated datasets
    DataMoverType dataMover{DataMoverType::HSM_NATIVE};
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Replace=" << (replaceExisting ? "Y" : "N")
            << ", CatalogOnly=" << (catalogOnly ? "Y" : "N")
            << ", Verify=" << (verifyFirst ? "Y" : "N");
        if (!newHighLevelQualifier.empty()) {
            oss << ", NewHLQ=" << newHighLevelQualifier;
        }
        return oss.str();
    }
};

/**
 * @brief Dataset info in aggregate
 */
struct AggregateDatasetInfo {
    std::string datasetName;
    std::string originalVolume;
    uint64_t size{0};
    uint64_t compressedSize{0};
    std::string managementClass;
    std::string storageClass;
    std::string dataClass;
    std::chrono::system_clock::time_point creationDate;
    std::chrono::system_clock::time_point lastReferenceDate;
    std::chrono::system_clock::time_point backupDate;
    uint32_t stackNumber{0};
    uint64_t stackOffset{0};
    bool isMigrated{false};
    bool wasRecovered{false};
    std::string checksum;
    
    double getCompressionRatio() const {
        if (compressedSize == 0) return 1.0;
        return static_cast<double>(size) / compressedSize;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << datasetName << " Size=" << size
            << " Stack=" << stackNumber << ":" << stackOffset;
        return oss.str();
    }
};

/**
 * @brief Activity log entry
 */
struct ActivityLogEntry {
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
    ActivityLogType type{ActivityLogType::INFO};
    std::string message;
    std::string datasetName;
    std::string volumeSerial;
    uint64_t bytesProcessed{0};
    uint32_t recordNumber{0};
    std::string errorCode;
    std::string details;
    
    std::string getTypeString() const {
        switch (type) {
            case ActivityLogType::INFO: return "INFO";
            case ActivityLogType::WARNING: return "WARN";
            case ActivityLogType::ERROR: return "ERROR";
            case ActivityLogType::START: return "START";
            case ActivityLogType::END: return "END";
            case ActivityLogType::PROGRESS: return "PROG";
            case ActivityLogType::DATASET_PROCESSED: return "DSET";
            case ActivityLogType::VOLUME_MOUNTED: return "VMNT";
            case ActivityLogType::CHECKPOINT: return "CHKPT";
            case ActivityLogType::RECOVERY_POINT: return "RCVPT";
            default: return "UNKN";
        }
    }
    
    std::string toString() const {
        std::ostringstream oss;
        auto time = std::chrono::system_clock::to_time_t(timestamp);
        oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
            << " [" << getTypeString() << "] " << message;
        if (!datasetName.empty()) oss << " DSN=" << datasetName;
        if (bytesProcessed > 0) oss << " Bytes=" << bytesProcessed;
        return oss.str();
    }
};

/**
 * @brief Stack information
 */
struct StackInfo {
    uint32_t stackNumber{0};
    std::string volumeSerial;
    StackStatus status{StackStatus::AVAILABLE};
    uint64_t capacity{0};
    uint64_t usedSpace{0};
    uint32_t fileCount{0};
    uint32_t maxFiles{255};
    std::chrono::system_clock::time_point creationTime;
    std::chrono::system_clock::time_point lastWriteTime;
    std::string checksum;
    std::vector<std::string> datasetNames;
    
    double getUsagePercent() const {
        if (capacity == 0) return 0.0;
        return (static_cast<double>(usedSpace) / capacity) * 100.0;
    }
    
    bool hasSpace(uint64_t required) const {
        return (status == StackStatus::AVAILABLE || status == StackStatus::IN_USE) &&
               (usedSpace + required <= capacity) &&
               (fileCount < maxFiles);
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Stack#" << stackNumber << " Vol=" << volumeSerial
            << " Files=" << fileCount << "/" << maxFiles
            << " Usage=" << std::fixed << std::setprecision(1) 
            << getUsagePercent() << "%";
        return oss.str();
    }
};

/**
 * @brief ABARS operation result
 */
struct ABARSResult {
    bool success{false};
    ABARSOperation operation;
    std::string aggregateName;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::chrono::milliseconds duration{0};
    
    // Statistics
    uint32_t datasetsProcessed{0};
    uint32_t datasetsSuccessful{0};
    uint32_t datasetsFailed{0};
    uint32_t datasetsSkipped{0};
    uint64_t bytesProcessed{0};
    uint64_t bytesCompressed{0};
    uint32_t stacksUsed{0};
    uint32_t volumesUsed{0};
    
    // Error handling
    std::string errorCode;
    std::string errorMessage;
    std::vector<std::string> warnings;
    std::vector<std::string> failedDatasets;
    
    // Generated resources
    std::string activityLogPath;
    std::string controlFilePath;
    std::string outputVolume;
    
    double getCompressionRatio() const {
        if (bytesCompressed == 0) return 1.0;
        return static_cast<double>(bytesProcessed) / bytesCompressed;
    }
    
    double getSuccessRate() const {
        if (datasetsProcessed == 0) return 100.0;
        return (static_cast<double>(datasetsSuccessful) / datasetsProcessed) * 100.0;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "ABARS " << (operation == ABARSOperation::ABACKUP ? "ABACKUP" : "ARECOVER")
            << " " << aggregateName
            << ": " << (success ? "SUCCESS" : "FAILED")
            << "\n  Datasets: " << datasetsSuccessful << "/" << datasetsProcessed
            << " (" << std::fixed << std::setprecision(1) << getSuccessRate() << "%)"
            << "\n  Bytes: " << bytesProcessed
            << " (Ratio: " << std::setprecision(2) << getCompressionRatio() << "x)"
            << "\n  Stacks: " << stacksUsed << ", Volumes: " << volumesUsed;
        if (!errorMessage.empty()) {
            oss << "\n  Error: " << errorMessage;
        }
        return oss.str();
    }
};

// ============================================================================
// Aggregate Group
// ============================================================================

/**
 * @brief Aggregate Group Definition
 * 
 * Defines a collection of datasets to be backed up together.
 */
class AggregateGroup {
public:
    AggregateGroup() = default;
    
    explicit AggregateGroup(const std::string& name,
                           const std::string& description = "")
        : name_(name), description_(description) {
        createdTime_ = std::chrono::system_clock::now();
        modifiedTime_ = createdTime_;
    }
    
    // Getters
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    const std::vector<SelectionCriteria>& getSelectionCriteria() const { return criteria_; }
    const ABackupOptions& getBackupOptions() const { return backupOptions_; }
    const ARecoverOptions& getRecoverOptions() const { return recoverOptions_; }
    bool isActive() const { return active_; }
    uint32_t getRetentionDays() const { return retentionDays_; }
    uint32_t getRetentionVersions() const { return retentionVersions_; }
    
    // Setters
    AggregateGroup& setDescription(const std::string& desc) {
        description_ = desc;
        markModified();
        return *this;
    }
    
    AggregateGroup& setActive(bool active) {
        active_ = active;
        markModified();
        return *this;
    }
    
    AggregateGroup& setBackupOptions(const ABackupOptions& options) {
        backupOptions_ = options;
        markModified();
        return *this;
    }
    
    AggregateGroup& setRecoverOptions(const ARecoverOptions& options) {
        recoverOptions_ = options;
        markModified();
        return *this;
    }
    
    AggregateGroup& setRetentionDays(uint32_t days) {
        retentionDays_ = days;
        markModified();
        return *this;
    }
    
    AggregateGroup& setRetentionVersions(uint32_t versions) {
        retentionVersions_ = versions;
        markModified();
        return *this;
    }
    
    // Selection criteria management
    AggregateGroup& addInclude(const std::string& mask) {
        SelectionCriteria sc;
        sc.type = SelectionType::INCLUDE;
        sc.datasetNameMask = mask;
        criteria_.push_back(sc);
        markModified();
        return *this;
    }
    
    AggregateGroup& addExclude(const std::string& mask) {
        SelectionCriteria sc;
        sc.type = SelectionType::EXCLUDE;
        sc.datasetNameMask = mask;
        criteria_.push_back(sc);
        markModified();
        return *this;
    }
    
    AggregateGroup& addCriteria(const SelectionCriteria& criterion) {
        criteria_.push_back(criterion);
        markModified();
        return *this;
    }
    
    void clearCriteria() {
        criteria_.clear();
        markModified();
    }
    
    // Check if dataset matches selection
    bool matchesSelection(const std::string& dsn, const std::string& mc = "",
                         const std::string& sc = "", uint64_t size = 0) const {
        bool included = false;
        
        for (const auto& criterion : criteria_) {
            if (criterion.type == SelectionType::INCLUDE) {
                if (criterion.matches(dsn, mc, sc, size)) {
                    included = true;
                }
            } else if (criterion.type == SelectionType::EXCLUDE) {
                if (criterion.matches(dsn, mc, sc, size)) {
                    return false;  // Excluded
                }
            }
        }
        
        // If no include criteria, include all (that weren't excluded)
        if (!hasIncludeCriteria()) {
            return true;
        }
        
        return included;
    }
    
    // Validation
    bool isValid() const {
        if (name_.empty() || name_.length() > 44) return false;
        if (criteria_.empty()) return false;
        return true;
    }
    
    // Serialization
    std::string toControlFile() const {
        std::ostringstream oss;
        oss << "DEFINE AGGGROUP(" << name_ << ")\n"
            << "  DESCRIPTION('" << description_ << "')\n";
        
        for (const auto& sc : criteria_) {
            oss << "  " << sc.toString() << "\n";
        }
        
        oss << "  RETENTION(DAYS(" << retentionDays_ 
            << ") VERSIONS(" << retentionVersions_ << "))\n"
            << "  BACKUP(" << backupOptions_.toString() << ")\n";
        
        return oss.str();
    }

private:
    std::string name_;
    std::string description_;
    std::vector<SelectionCriteria> criteria_;
    ABackupOptions backupOptions_;
    ARecoverOptions recoverOptions_;
    uint32_t retentionDays_{365};
    uint32_t retentionVersions_{5};
    bool active_{true};
    std::chrono::system_clock::time_point createdTime_;
    std::chrono::system_clock::time_point modifiedTime_;
    
    void markModified() {
        modifiedTime_ = std::chrono::system_clock::now();
    }
    
    bool hasIncludeCriteria() const {
        for (const auto& sc : criteria_) {
            if (sc.type == SelectionType::INCLUDE) return true;
        }
        return false;
    }
};

// ============================================================================
// ABARS Control File
// ============================================================================

/**
 * @brief ABARS Control File Handler
 * 
 * Processes ABARS control statements for backup/recovery operations.
 */
class ABARSControlFile {
public:
    /**
     * @brief Control statement
     */
    struct Statement {
        std::string keyword;
        std::map<std::string, std::string> parameters;
        int lineNumber{0};
        bool valid{true};
        std::string errorMessage;
        
        std::string getParameter(const std::string& name, 
                                 const std::string& defaultValue = "") const {
            auto it = parameters.find(name);
            if (it != parameters.end()) return it->second;
            return defaultValue;
        }
    };
    
    ABARSControlFile() = default;
    
    // Parse control file content
    bool parse(const std::string& content) {
        statements_.clear();
        errors_.clear();
        
        std::istringstream stream(content);
        std::string line;
        int lineNum = 0;
        std::string continuedLine;
        
        while (std::getline(stream, line)) {
            lineNum++;
            
            // Skip comments and empty lines
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            if (line[start] == '*' || line.substr(start, 2) == "/*") continue;
            
            // Handle continuation
            if (!line.empty() && line.back() == '-') {
                continuedLine += line.substr(0, line.length() - 1);
                continue;
            }
            
            line = continuedLine + line;
            continuedLine.clear();
            
            // Parse statement
            Statement stmt = parseStatement(line, lineNum);
            statements_.push_back(stmt);
            
            if (!stmt.valid) {
                errors_.push_back("Line " + std::to_string(lineNum) + 
                                 ": " + stmt.errorMessage);
            }
        }
        
        return errors_.empty();
    }
    
    // Parse from file
    bool parseFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            errors_.push_back("Cannot open file: " + filename);
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return parse(buffer.str());
    }
    
    // Generate control file content
    std::string generate(const AggregateGroup& group) const {
        std::ostringstream oss;
        
        oss << "/*---------------------------------------------------------*/\n"
            << "/* ABARS Control File for " << group.getName() << "          */\n"
            << "/* Generated: " << getCurrentTimestamp() << "                */\n"
            << "/*---------------------------------------------------------*/\n\n";
        
        oss << group.toControlFile();
        
        return oss.str();
    }
    
    // Getters
    const std::vector<Statement>& getStatements() const { return statements_; }
    const std::vector<std::string>& getErrors() const { return errors_; }
    bool hasErrors() const { return !errors_.empty(); }
    
    // Find statements by keyword
    std::vector<const Statement*> findStatements(const std::string& keyword) const {
        std::vector<const Statement*> result;
        for (const auto& stmt : statements_) {
            if (stmt.keyword == keyword) {
                result.push_back(&stmt);
            }
        }
        return result;
    }

private:
    std::vector<Statement> statements_;
    std::vector<std::string> errors_;
    
    Statement parseStatement(const std::string& line, int lineNum) {
        Statement stmt;
        stmt.lineNumber = lineNum;
        
        // Find keyword
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) {
            stmt.valid = false;
            stmt.errorMessage = "Empty statement";
            return stmt;
        }
        
        size_t end = line.find_first_of(" \t(", start);
        if (end == std::string::npos) {
            stmt.keyword = line.substr(start);
        } else {
            stmt.keyword = line.substr(start, end - start);
        }
        
        // Convert keyword to uppercase
        std::transform(stmt.keyword.begin(), stmt.keyword.end(), 
                      stmt.keyword.begin(), ::toupper);
        
        // Parse parameters
        size_t parenStart = line.find('(', end);
        while (parenStart != std::string::npos) {
            // Find parameter name (before parenthesis)
            size_t nameStart = line.find_last_of(" \t(", parenStart - 1);
            if (nameStart == std::string::npos) nameStart = 0;
            else nameStart++;
            
            std::string paramName = line.substr(nameStart, parenStart - nameStart);
            std::transform(paramName.begin(), paramName.end(), 
                          paramName.begin(), ::toupper);
            
            // Find parameter value (inside parentheses)
            size_t parenEnd = findMatchingParen(line, parenStart);
            if (parenEnd == std::string::npos) {
                stmt.valid = false;
                stmt.errorMessage = "Unmatched parenthesis";
                return stmt;
            }
            
            std::string paramValue = line.substr(parenStart + 1, parenEnd - parenStart - 1);
            
            // Remove quotes if present
            if (!paramValue.empty() && paramValue.front() == '\'' && 
                paramValue.back() == '\'') {
                paramValue = paramValue.substr(1, paramValue.length() - 2);
            }
            
            if (!paramName.empty()) {
                stmt.parameters[paramName] = paramValue;
            }
            
            parenStart = line.find('(', parenEnd + 1);
        }
        
        return stmt;
    }
    
    size_t findMatchingParen(const std::string& str, size_t start) const {
        if (start >= str.length() || str[start] != '(') {
            return std::string::npos;
        }
        
        int depth = 1;
        for (size_t i = start + 1; i < str.length(); ++i) {
            if (str[i] == '(') depth++;
            else if (str[i] == ')') {
                depth--;
                if (depth == 0) return i;
            }
        }
        return std::string::npos;
    }
    
    std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time), "%Y/%m/%d %H:%M:%S");
        return oss.str();
    }
};

// ============================================================================
// Activity Log
// ============================================================================

/**
 * @brief ABARS Activity Log
 * 
 * Records all ABARS operation activity for audit and troubleshooting.
 */
class ActivityLog {
public:
    ActivityLog() {
        startTime_ = std::chrono::system_clock::now();
    }
    
    explicit ActivityLog(const std::string& name)
        : name_(name) {
        startTime_ = std::chrono::system_clock::now();
    }
    
    // Logging methods
    void logInfo(const std::string& message) {
        addEntry(ActivityLogType::INFO, message);
    }
    
    void logWarning(const std::string& message) {
        addEntry(ActivityLogType::WARNING, message);
        warningCount_++;
    }
    
    void logError(const std::string& message, const std::string& errorCode = "") {
        ActivityLogEntry entry;
        entry.type = ActivityLogType::ERROR;
        entry.message = message;
        entry.errorCode = errorCode;
        addEntry(entry);
        errorCount_++;
    }
    
    void logStart(ABARSOperation operation, const std::string& aggregateName) {
        ActivityLogEntry entry;
        entry.type = ActivityLogType::START;
        entry.message = "Starting " + getOperationName(operation) + 
                       " for aggregate " + aggregateName;
        addEntry(entry);
    }
    
    void logEnd(ABARSOperation operation, bool success) {
        ActivityLogEntry entry;
        entry.type = ActivityLogType::END;
        entry.message = getOperationName(operation) + 
                       (success ? " completed successfully" : " failed");
        addEntry(entry);
    }
    
    void logDatasetProcessed(const std::string& dsn, uint64_t bytes, bool success) {
        ActivityLogEntry entry;
        entry.type = ActivityLogType::DATASET_PROCESSED;
        entry.datasetName = dsn;
        entry.bytesProcessed = bytes;
        entry.message = success ? "Processed" : "Failed to process";
        addEntry(entry);
        
        if (success) datasetsProcessed_++;
        bytesProcessed_ += bytes;
    }
    
    void logVolumeMounted(const std::string& volser) {
        ActivityLogEntry entry;
        entry.type = ActivityLogType::VOLUME_MOUNTED;
        entry.volumeSerial = volser;
        entry.message = "Volume mounted: " + volser;
        addEntry(entry);
    }
    
    void logCheckpoint(uint32_t checkpointNumber, const std::string& details = "") {
        ActivityLogEntry entry;
        entry.type = ActivityLogType::CHECKPOINT;
        entry.recordNumber = checkpointNumber;
        entry.message = "Checkpoint " + std::to_string(checkpointNumber);
        entry.details = details;
        addEntry(entry);
    }
    
    void logProgress(double percentComplete, uint64_t bytesProcessed) {
        ActivityLogEntry entry;
        entry.type = ActivityLogType::PROGRESS;
        entry.bytesProcessed = bytesProcessed;
        entry.message = "Progress: " + std::to_string(static_cast<int>(percentComplete)) + "%";
        addEntry(entry);
    }
    
    // Getters
    const std::string& getName() const { return name_; }
    const std::vector<ActivityLogEntry>& getEntries() const { return entries_; }
    uint32_t getErrorCount() const { return errorCount_; }
    uint32_t getWarningCount() const { return warningCount_; }
    uint32_t getDatasetsProcessed() const { return datasetsProcessed_; }
    uint64_t getBytesProcessed() const { return bytesProcessed_; }
    
    // Get entries by type
    std::vector<const ActivityLogEntry*> getEntriesByType(ActivityLogType type) const {
        std::vector<const ActivityLogEntry*> result;
        for (const auto& entry : entries_) {
            if (entry.type == type) {
                result.push_back(&entry);
            }
        }
        return result;
    }
    
    // Export to string
    std::string toString() const {
        std::ostringstream oss;
        oss << "ABARS Activity Log: " << name_ << "\n"
            << "Started: " << formatTime(startTime_) << "\n"
            << "Entries: " << entries_.size() 
            << " (Errors: " << errorCount_ 
            << ", Warnings: " << warningCount_ << ")\n"
            << "Datasets: " << datasetsProcessed_ 
            << ", Bytes: " << bytesProcessed_ << "\n"
            << std::string(60, '-') << "\n";
        
        for (const auto& entry : entries_) {
            oss << entry.toString() << "\n";
        }
        
        return oss.str();
    }
    
    // Export to file
    bool writeToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) return false;
        file << toString();
        return true;
    }

private:
    std::string name_{"ACTLOG"};
    std::vector<ActivityLogEntry> entries_;
    std::chrono::system_clock::time_point startTime_;
    uint32_t errorCount_{0};
    uint32_t warningCount_{0};
    uint32_t datasetsProcessed_{0};
    uint64_t bytesProcessed_{0};
    mutable std::mutex mutex_;
    
    void addEntry(ActivityLogType type, const std::string& message) {
        ActivityLogEntry entry;
        entry.type = type;
        entry.message = message;
        addEntry(entry);
    }
    
    void addEntry(const ActivityLogEntry& entry) {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.push_back(entry);
    }
    
    std::string getOperationName(ABARSOperation op) const {
        switch (op) {
            case ABARSOperation::ABACKUP: return "ABACKUP";
            case ABARSOperation::ARECOVER: return "ARECOVER";
            case ABARSOperation::VERIFY: return "VERIFY";
            case ABARSOperation::LIST: return "LIST";
            case ABARSOperation::DELETE: return "DELETE";
            case ABARSOperation::COPY: return "COPY";
            default: return "UNKNOWN";
        }
    }
    
    std::string formatTime(std::chrono::system_clock::time_point tp) const {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

// ============================================================================
// Stack Manager
// ============================================================================

/**
 * @brief ABARS Stack Manager
 * 
 * Manages tape stacks for aggregate backup operations.
 */
class StackManager {
public:
    StackManager() = default;
    
    // Stack allocation
    uint32_t allocateStack(const std::string& volser, uint64_t capacity) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        StackInfo stack;
        stack.stackNumber = nextStackNumber_++;
        stack.volumeSerial = volser;
        stack.capacity = capacity;
        stack.status = StackStatus::AVAILABLE;
        stack.creationTime = std::chrono::system_clock::now();
        stack.maxFiles = maxFilesPerStack_;
        
        stacks_[stack.stackNumber] = stack;
        volumeStacks_[volser].push_back(stack.stackNumber);
        
        return stack.stackNumber;
    }
    
    // Get stack for writing
    std::optional<uint32_t> getAvailableStack(uint64_t requiredSpace) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [num, stack] : stacks_) {
            if (stack.hasSpace(requiredSpace)) {
                return num;
            }
        }
        return std::nullopt;
    }
    
    // Write to stack
    bool writeToStack(uint32_t stackNumber, const std::string& dsn, uint64_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = stacks_.find(stackNumber);
        if (it == stacks_.end()) return false;
        
        auto& stack = it->second;
        if (!stack.hasSpace(size)) return false;
        
        stack.status = StackStatus::IN_USE;
        stack.usedSpace += size;
        stack.fileCount++;
        stack.lastWriteTime = std::chrono::system_clock::now();
        stack.datasetNames.push_back(dsn);
        
        if (stack.fileCount >= stack.maxFiles || 
            stack.usedSpace >= stack.capacity * 0.95) {
            stack.status = StackStatus::FULL;
        }
        
        return true;
    }
    
    // Close stack
    bool closeStack(uint32_t stackNumber) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = stacks_.find(stackNumber);
        if (it == stacks_.end()) return false;
        
        it->second.status = StackStatus::FULL;
        return true;
    }
    
    // Get stack info
    std::optional<StackInfo> getStackInfo(uint32_t stackNumber) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = stacks_.find(stackNumber);
        if (it != stacks_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    // Get stacks for volume
    std::vector<uint32_t> getStacksForVolume(const std::string& volser) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = volumeStacks_.find(volser);
        if (it != volumeStacks_.end()) {
            return it->second;
        }
        return {};
    }
    
    // Get all stacks
    std::vector<StackInfo> getAllStacks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<StackInfo> result;
        for (const auto& [num, stack] : stacks_) {
            result.push_back(stack);
        }
        return result;
    }
    
    // Configuration
    void setMaxFilesPerStack(uint32_t max) { maxFilesPerStack_ = max; }
    uint32_t getMaxFilesPerStack() const { return maxFilesPerStack_; }
    
    // Statistics
    struct Statistics {
        uint32_t totalStacks{0};
        uint32_t activeStacks{0};
        uint32_t fullStacks{0};
        uint64_t totalCapacity{0};
        uint64_t usedCapacity{0};
        uint32_t totalFiles{0};
        
        double getUsagePercent() const {
            if (totalCapacity == 0) return 0.0;
            return (static_cast<double>(usedCapacity) / totalCapacity) * 100.0;
        }
    };
    
    Statistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Statistics stats;
        stats.totalStacks = static_cast<uint32_t>(stacks_.size());
        
        for (const auto& [num, stack] : stacks_) {
            stats.totalCapacity += stack.capacity;
            stats.usedCapacity += stack.usedSpace;
            stats.totalFiles += stack.fileCount;
            
            if (stack.status == StackStatus::IN_USE || 
                stack.status == StackStatus::AVAILABLE) {
                stats.activeStacks++;
            } else if (stack.status == StackStatus::FULL) {
                stats.fullStacks++;
            }
        }
        
        return stats;
    }

private:
    std::map<uint32_t, StackInfo> stacks_;
    std::map<std::string, std::vector<uint32_t>> volumeStacks_;
    uint32_t nextStackNumber_{1};
    uint32_t maxFilesPerStack_{255};
    mutable std::mutex mutex_;
};

// ============================================================================
// ABARS Manager
// ============================================================================

/**
 * @brief ABARS Manager - Main entry point
 * 
 * Manages all ABARS operations including:
 * - Aggregate group definitions
 * - Backup operations
 * - Recovery operations
 * - Activity logging
 * - Stack management
 */
class ABARSManager {
public:
    /**
     * @brief Manager statistics
     */
    struct Statistics {
        uint64_t backupsCompleted{0};
        uint64_t backupsFailed{0};
        uint64_t recoveriesCompleted{0};
        uint64_t recoveriesFailed{0};
        uint64_t datasetsBackedUp{0};
        uint64_t datasetsRecovered{0};
        uint64_t bytesBackedUp{0};
        uint64_t bytesRecovered{0};
        std::chrono::system_clock::time_point startTime{std::chrono::system_clock::now()};
        
        std::string toString() const {
            std::ostringstream oss;
            oss << "Backups: " << backupsCompleted << " completed, " 
                << backupsFailed << " failed\n"
                << "Recoveries: " << recoveriesCompleted << " completed, "
                << recoveriesFailed << " failed\n"
                << "Datasets: " << datasetsBackedUp << " backed up, "
                << datasetsRecovered << " recovered\n"
                << "Bytes: " << bytesBackedUp << " backed up, "
                << bytesRecovered << " recovered";
            return oss.str();
        }
    };
    
    ABARSManager() {
        stackManager_ = std::make_unique<StackManager>();
    }
    
    // Aggregate group management
    bool defineGroup(const AggregateGroup& group) {
        if (!group.isValid()) return false;
        
        std::lock_guard<std::mutex> lock(mutex_);
        groups_[group.getName()] = group;
        return true;
    }
    
    bool deleteGroup(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return groups_.erase(name) > 0;
    }
    
    AggregateGroup* getGroup(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = groups_.find(name);
        if (it != groups_.end()) return &it->second;
        return nullptr;
    }
    
    std::vector<std::string> listGroups() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        for (const auto& [name, group] : groups_) {
            result.push_back(name);
        }
        return result;
    }
    
    // Execute ABACKUP
    ABARSResult executeABackup(const std::string& groupName,
                               const std::vector<std::string>& datasets,
                               [[maybe_unused]] const ABackupOptions& options = ABackupOptions()) {
        ABARSResult result;
        result.operation = ABARSOperation::ABACKUP;
        result.aggregateName = groupName;
        result.startTime = std::chrono::system_clock::now();
        
        // Create activity log
        auto activityLog = std::make_unique<ActivityLog>(groupName + "_BACKUP");
        activityLog->logStart(ABARSOperation::ABACKUP, groupName);
        
        // Get or create aggregate group
        AggregateGroup* group = getGroup(groupName);
        if (!group) {
            // Create temporary group
            AggregateGroup tempGroup(groupName);
            for (const auto& dsn : datasets) {
                tempGroup.addInclude(dsn);
            }
            defineGroup(tempGroup);
            group = getGroup(groupName);
        }
        
        if (!group) {
            result.success = false;
            result.errorMessage = "Failed to create aggregate group";
            activityLog->logError(result.errorMessage);
            return result;
        }
        
        // Allocate stack
        uint32_t stackNum = stackManager_->allocateStack("TAPE01", 
            10ULL * 1024 * 1024 * 1024);  // 10GB stack
        
        activityLog->logInfo("Allocated stack " + std::to_string(stackNum));
        
        // Process datasets
        for (const auto& dsn : datasets) {
            // Simulate backup
            uint64_t datasetSize = 1024 * 1024 * (10 + (rand() % 100));  // 10-110 MB
            uint64_t compressedSize = static_cast<uint64_t>(datasetSize * 0.4);
            
            if (stackManager_->writeToStack(stackNum, dsn, compressedSize)) {
                AggregateDatasetInfo dsi;
                dsi.datasetName = dsn;
                dsi.size = datasetSize;
                dsi.compressedSize = compressedSize;
                dsi.stackNumber = stackNum;
                dsi.backupDate = std::chrono::system_clock::now();
                
                datasetInfo_[dsn] = dsi;
                
                result.datasetsSuccessful++;
                result.bytesProcessed += datasetSize;
                result.bytesCompressed += compressedSize;
                
                activityLog->logDatasetProcessed(dsn, datasetSize, true);
            } else {
                result.datasetsFailed++;
                result.failedDatasets.push_back(dsn);
                activityLog->logError("Failed to backup dataset: " + dsn);
            }
            
            result.datasetsProcessed++;
            
            // Log progress
            double progress = (static_cast<double>(result.datasetsProcessed) / 
                              datasets.size()) * 100.0;
            activityLog->logProgress(progress, result.bytesProcessed);
        }
        
        // Close stack
        stackManager_->closeStack(stackNum);
        result.stacksUsed = 1;
        result.volumesUsed = 1;
        
        result.endTime = std::chrono::system_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.endTime - result.startTime);
        
        result.success = (result.datasetsFailed == 0);
        
        activityLog->logEnd(ABARSOperation::ABACKUP, result.success);
        
        // Store activity log
        result.activityLogPath = "/tmp/" + groupName + "_backup.log";
        activityLog->writeToFile(result.activityLogPath);
        activityLogs_[groupName] = std::move(activityLog);
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            if (result.success) {
                stats_.backupsCompleted++;
            } else {
                stats_.backupsFailed++;
            }
            stats_.datasetsBackedUp += result.datasetsSuccessful;
            stats_.bytesBackedUp += result.bytesProcessed;
        }
        
        return result;
    }
    
    // Execute ARECOVER
    ABARSResult executeARecover(const std::string& groupName,
                                const std::vector<std::string>& datasets = {},
                                [[maybe_unused]] const ARecoverOptions& options = ARecoverOptions()) {
        ABARSResult result;
        result.operation = ABARSOperation::ARECOVER;
        result.aggregateName = groupName;
        result.startTime = std::chrono::system_clock::now();
        
        // Create activity log
        auto activityLog = std::make_unique<ActivityLog>(groupName + "_RECOVER");
        activityLog->logStart(ABARSOperation::ARECOVER, groupName);
        
        // Determine datasets to recover
        std::vector<std::string> toRecover;
        if (datasets.empty()) {
            // Recover all datasets in aggregate
            for (const auto& [dsn, info] : datasetInfo_) {
                toRecover.push_back(dsn);
            }
        } else {
            toRecover = datasets;
        }
        
        // Process datasets
        for (const auto& dsn : toRecover) {
            auto it = datasetInfo_.find(dsn);
            if (it == datasetInfo_.end()) {
                result.datasetsSkipped++;
                activityLog->logWarning("Dataset not found in aggregate: " + dsn);
                continue;
            }
            
            const auto& info = it->second;
            
            // Simulate recovery
            result.datasetsSuccessful++;
            result.bytesProcessed += info.size;
            
            activityLog->logDatasetProcessed(dsn, info.size, true);
            
            result.datasetsProcessed++;
            
            // Log progress
            double progress = (static_cast<double>(result.datasetsProcessed) / 
                              toRecover.size()) * 100.0;
            activityLog->logProgress(progress, result.bytesProcessed);
        }
        
        result.endTime = std::chrono::system_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.endTime - result.startTime);
        
        result.success = (result.datasetsFailed == 0);
        
        activityLog->logEnd(ABARSOperation::ARECOVER, result.success);
        
        // Store activity log
        result.activityLogPath = "/tmp/" + groupName + "_recover.log";
        activityLog->writeToFile(result.activityLogPath);
        activityLogs_[groupName + "_recover"] = std::move(activityLog);
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            if (result.success) {
                stats_.recoveriesCompleted++;
            } else {
                stats_.recoveriesFailed++;
            }
            stats_.datasetsRecovered += result.datasetsSuccessful;
            stats_.bytesRecovered += result.bytesProcessed;
        }
        
        return result;
    }
    
    // Verify aggregate
    ABARSResult verifyAggregate(const std::string& groupName) {
        ABARSResult result;
        result.operation = ABARSOperation::VERIFY;
        result.aggregateName = groupName;
        result.startTime = std::chrono::system_clock::now();
        
        // Count datasets
        for (const auto& [dsn, info] : datasetInfo_) {
            result.datasetsProcessed++;
            result.datasetsSuccessful++;
            result.bytesProcessed += info.size;
        }
        
        result.success = true;
        result.endTime = std::chrono::system_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.endTime - result.startTime);
        
        return result;
    }
    
    // List aggregate contents
    std::vector<AggregateDatasetInfo> listAggregateContents(
            [[maybe_unused]] const std::string& groupName = "") const {
        std::vector<AggregateDatasetInfo> result;
        
        for (const auto& [dsn, info] : datasetInfo_) {
            result.push_back(info);
        }
        
        return result;
    }
    
    // Stack manager access
    StackManager& getStackManager() { return *stackManager_; }
    const StackManager& getStackManager() const { return *stackManager_; }
    
    // Activity log access
    const ActivityLog* getActivityLog(const std::string& name) const {
        auto it = activityLogs_.find(name);
        if (it != activityLogs_.end()) {
            return it->second.get();
        }
        return nullptr;
    }
    
    // Statistics
    const Statistics& getStatistics() const { return stats_; }
    
    void resetStatistics() {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_ = Statistics{};
    }
    
    // Generate summary report
    std::string generateSummaryReport() const {
        std::ostringstream oss;
        oss << "ABARS Manager Summary\n"
            << "====================\n"
            << "Aggregate Groups: " << groups_.size() << "\n"
            << "Datasets in Aggregates: " << datasetInfo_.size() << "\n\n"
            << "Stack Statistics:\n"
            << "  " << stackManager_->getStatistics().totalStacks << " stacks\n"
            << "  " << stackManager_->getStatistics().totalFiles << " files\n\n"
            << stats_.toString();
        return oss.str();
    }

private:
    std::map<std::string, AggregateGroup> groups_;
    std::map<std::string, AggregateDatasetInfo> datasetInfo_;
    std::map<std::string, std::unique_ptr<ActivityLog>> activityLogs_;
    std::unique_ptr<StackManager> stackManager_;
    Statistics stats_;
    mutable std::mutex mutex_;
    mutable std::mutex statsMutex_;
};

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a standard aggregate group
 */
inline AggregateGroup createStandardAggregateGroup(
        const std::string& name,
        const std::vector<std::string>& includeMasks) {
    
    AggregateGroup group(name, "Standard aggregate group");
    
    for (const auto& mask : includeMasks) {
        group.addInclude(mask);
    }
    
    ABackupOptions opts;
    opts.compress = true;
    opts.verify = true;
    opts.stackSize = 255;
    group.setBackupOptions(opts);
    
    group.setRetentionDays(365);
    group.setRetentionVersions(5);
    
    return group;
}

/**
 * @brief Create aggregate group for system datasets
 */
inline AggregateGroup createSystemAggregateGroup(const std::string& name) {
    AggregateGroup group(name, "System datasets aggregate");
    
    group.addInclude("SYS1.*");
    group.addInclude("SYS2.*");
    group.addExclude("SYS1.DUMP*");
    group.addExclude("SYS1.LOGREC");
    
    ABackupOptions opts;
    opts.compress = true;
    opts.verify = true;
    opts.maxTasks = 8;
    group.setBackupOptions(opts);
    
    return group;
}

/**
 * @brief Create aggregate group for user datasets
 */
inline AggregateGroup createUserAggregateGroup(
        const std::string& name,
        const std::string& userPrefix) {
    
    AggregateGroup group(name, "User datasets aggregate for " + userPrefix);
    
    group.addInclude(userPrefix + ".*");
    group.addExclude(userPrefix + ".TEMP.*");
    
    return group;
}

} // namespace abars
} // namespace dfsmshsm

#endif // HSM_ABARS_HPP
