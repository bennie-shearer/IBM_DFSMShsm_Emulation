/*
 * DFSMShsm Emulator - Command Processor
 * @version 4.2.0
 * 
 * Emulates HSM command processing for TSO and batch environments:
 * - HSEND: Submit HSM commands
 * - HMIGRATE: Migrate datasets
 * - HRECALL: Recall migrated datasets
 * - HBACKUP: Backup datasets
 * - HRECOVER: Recover from backup
 * - HDELETE: Delete migrated/backup data
 * - HQUERY: Query HSM status
 * - HLIST: List datasets/volumes
 * - HALTER: Alter dataset attributes
 * - HCANCEL: Cancel pending requests
 */

#ifndef HSM_COMMAND_PROCESSOR_HPP
#define HSM_COMMAND_PROCESSOR_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <functional>
#include <optional>
#include <variant>
#include <queue>
#include <chrono>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <atomic>
#include <stdexcept>

namespace hsm {
namespace command {

// =============================================================================
// Command Types and Enumerations
// =============================================================================

enum class CommandType {
    UNKNOWN,
    HMIGRATE,
    HRECALL,
    HBACKUP,
    HRECOVER,
    HDELETE,
    HQUERY,
    HLIST,
    HALTER,
    HCANCEL,
    HSEND,
    SETSYS,
    ADDVOL,
    DELVOL,
    HOLD,
    RELEASE,
    AUDIT,
    RECYCLE,
    EXPIREBV,
    FREEVOL
};

enum class CommandSource {
    TSO,           // TSO command
    BATCH,         // JCL EXEC PGM=IKJEFT01
    OPERATOR,      // Operator console
    API,           // API call
    AUTOMATIC,     // Automatic processing
    SCHEDULED      // Scheduled task
};

enum class CommandPriority {
    LOW = 1,
    NORMAL = 5,
    HIGH = 7,
    URGENT = 9,
    CRITICAL = 10
};

enum class CommandStatus {
    PENDING,       // Waiting in queue
    VALIDATING,    // Being validated
    QUEUED,        // Queued for execution
    EXECUTING,     // Currently executing
    COMPLETED,     // Successfully completed
    FAILED,        // Failed
    CANCELLED,     // Cancelled by user
    HELD           // Held for later
};

// Return codes
enum class ReturnCode : int {
    SUCCESS = 0,
    WARNING = 4,
    ERROR = 8,
    SEVERE = 12,
    CRITICAL = 16,
    SYNTAX_ERROR = 20,
    AUTH_ERROR = 24,
    NOT_FOUND = 28,
    ALREADY_EXISTS = 32,
    BUSY = 36,
    CANCELLED = 40
};

// =============================================================================
// Command Parameter Types
// =============================================================================

struct DatasetSpec {
    std::string name;
    bool isPattern{false};       // Contains wildcards
    bool includeMembers{false};  // MEMBER(*) for PDS
    std::string member;          // Specific member
};

struct VolumeSpec {
    std::string volser;
    std::string deviceType;
    bool isPattern{false};
};

struct DateSpec {
    int year{0};
    int dayOfYear{0};  // Julian day
    bool isRelative{false};
    int relativeDays{0};  // For DAYS(n)
    
    static DateSpec today() {
        DateSpec d;
        auto now = std::chrono::system_clock::now();
        auto time_t_val = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&time_t_val);
        d.year = tm->tm_year + 1900;
        d.dayOfYear = tm->tm_yday + 1;
        return d;
    }
    
    static DateSpec daysAgo(int days) {
        DateSpec d;
        d.isRelative = true;
        d.relativeDays = days;
        return d;
    }
};

// =============================================================================
// Command Options
// =============================================================================

struct MigrateOptions {
    std::vector<DatasetSpec> datasets;
    std::vector<VolumeSpec> volumes;
    bool ml2{false};             // Force ML2 (tape)
    bool deleteOriginal{true};   // Delete after migration
    bool wait{false};            // Wait for completion
    bool nowait{true};           // Don't wait (default)
    int priority{5};             // Priority 1-10
    DateSpec afterDate;          // Migrate if not accessed after
    int daysNonUsage{0};         // Days since last use
    std::string managementClass;
    std::string storageClass;
    bool compress{true};
    bool immediate{false};       // Skip queue
    std::string targetVolume;    // Specific target
};

struct RecallOptions {
    std::vector<DatasetSpec> datasets;
    bool wait{true};             // Wait for completion
    bool nowait{false};
    int priority{5};
    bool deleteBackup{false};    // Delete from ML
    std::string targetVolume;    // Recall to specific volume
    std::string targetPool;      // Recall to pool
    bool synchronous{false};     // Synchronous recall
};

struct BackupOptions {
    std::vector<DatasetSpec> datasets;
    std::vector<VolumeSpec> volumes;
    bool wait{false};
    bool nowait{true};
    int priority{5};
    bool newName{false};         // Backup with new name
    std::string targetName;      // New dataset name
    std::string targetVolume;
    bool compress{true};
    bool incremental{false};     // Incremental backup
    int versions{1};             // Number of versions to keep
    DateSpec retentionDate;
    int retentionDays{0};
};

struct RecoverOptions {
    std::vector<DatasetSpec> datasets;
    bool wait{true};
    bool nowait{false};
    int priority{5};
    std::string targetName;      // Recover to new name
    std::string targetVolume;
    int generation{0};           // Specific GDG generation
    int version{0};              // Specific backup version
    bool replace{false};         // Replace existing
    bool fromVolume{false};      // Recover from specific volume
    std::string sourceVolume;
    DateSpec asOfDate;           // Point-in-time recovery
};

struct DeleteOptions {
    std::vector<DatasetSpec> datasets;
    bool migrated{true};         // Delete migrated copies
    bool backup{false};          // Delete backups
    bool expired{false};         // Only if expired
    bool purge{false};           // Purge (delete even if unexpired)
    int versions{0};             // Delete specific number of versions
    bool wait{false};
};

struct QueryOptions {
    std::string queryType;       // ACTIVE, WAITING, HELD, etc.
    std::string datasetName;
    std::string volumeSerial;
    std::string user;
    bool detail{false};
    int maxResults{100};
};

struct ListOptions {
    std::vector<DatasetSpec> datasets;
    std::vector<VolumeSpec> volumes;
    bool level{false};           // Show migration level
    bool term{true};             // Output to terminal
    bool data{false};            // Full data output
    bool history{false};         // Show history
    bool backup{false};          // List backups
    bool migrated{false};        // List migrated only
    std::string outputDataset;   // Output to dataset
};

struct AlterOptions {
    std::vector<DatasetSpec> datasets;
    std::optional<int> priority;
    std::optional<std::string> managementClass;
    std::optional<std::string> storageClass;
    std::optional<int> daysNonUsage;
    std::optional<DateSpec> expirationDate;
    std::optional<int> backupVersions;
};

struct CancelOptions {
    uint64_t requestId{0};
    std::string datasetName;
    std::string user;
    bool all{false};             // Cancel all for user
};

struct SetsysOptions {
    std::map<std::string, std::string> parameters;
};

// =============================================================================
// Parsed Command
// =============================================================================

using CommandOptions = std::variant<
    MigrateOptions,
    RecallOptions,
    BackupOptions,
    RecoverOptions,
    DeleteOptions,
    QueryOptions,
    ListOptions,
    AlterOptions,
    CancelOptions,
    SetsysOptions
>;

struct ParsedCommand {
    CommandType type{CommandType::UNKNOWN};
    CommandSource source{CommandSource::TSO};
    CommandPriority priority{CommandPriority::NORMAL};
    std::string rawCommand;
    std::string userId;
    std::chrono::system_clock::time_point submitTime;
    CommandOptions options;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    bool isValid() const { return errors.empty() && type != CommandType::UNKNOWN; }
};

// =============================================================================
// Command Result
// =============================================================================

struct CommandResult {
    ReturnCode returnCode{ReturnCode::SUCCESS};
    std::string message;
    std::vector<std::string> output;
    std::chrono::milliseconds executionTime{0};
    uint64_t requestId{0};
    std::string datasetName;
    
    bool isSuccess() const { return returnCode == ReturnCode::SUCCESS; }
    bool isWarning() const { return returnCode == ReturnCode::WARNING; }
    bool isError() const { return static_cast<int>(returnCode) >= 8; }
};

// =============================================================================
// Command Queue Entry
// =============================================================================

struct QueuedCommand {
    uint64_t requestId{0};
    ParsedCommand command;
    CommandStatus status{CommandStatus::PENDING};
    std::chrono::system_clock::time_point queueTime;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    CommandResult result;
    int retryCount{0};
    int maxRetries{3};
};

// =============================================================================
// Command Parser
// =============================================================================

class CommandParser {
public:
    struct Config {
        bool strictSyntax{true};
        bool allowAbbreviations{true};
        size_t minAbbrevLength{3};
        
        Config() = default;
    };
    
private:
    Config config_;
    
    // Command name mappings
    std::map<std::string, CommandType> commandMap_{
        {"HMIGRATE", CommandType::HMIGRATE},
        {"HMIG", CommandType::HMIGRATE},
        {"HRECALL", CommandType::HRECALL},
        {"HREC", CommandType::HRECALL},
        {"HBACKUP", CommandType::HBACKUP},
        {"HBACK", CommandType::HBACKUP},
        {"HRECOVER", CommandType::HRECOVER},
        {"HRECO", CommandType::HRECOVER},
        {"HDELETE", CommandType::HDELETE},
        {"HDEL", CommandType::HDELETE},
        {"HQUERY", CommandType::HQUERY},
        {"HQ", CommandType::HQUERY},
        {"HLIST", CommandType::HLIST},
        {"HL", CommandType::HLIST},
        {"HALTER", CommandType::HALTER},
        {"HALT", CommandType::HALTER},
        {"HCANCEL", CommandType::HCANCEL},
        {"HCAN", CommandType::HCANCEL},
        {"HSEND", CommandType::HSEND},
        {"SETSYS", CommandType::SETSYS},
        {"ADDVOL", CommandType::ADDVOL},
        {"DELVOL", CommandType::DELVOL},
        {"HOLD", CommandType::HOLD},
        {"RELEASE", CommandType::RELEASE},
        {"AUDIT", CommandType::AUDIT},
        {"RECYCLE", CommandType::RECYCLE},
        {"EXPIREBV", CommandType::EXPIREBV},
        {"FREEVOL", CommandType::FREEVOL}
    };
    
public:
    CommandParser() : config_() {}
    explicit CommandParser(const Config& config) : config_(config) {}
    
    ParsedCommand parse(const std::string& commandString, 
                       const std::string& userId = "SYSTEM",
                       CommandSource source = CommandSource::TSO) {
        ParsedCommand cmd;
        cmd.rawCommand = commandString;
        cmd.userId = userId;
        cmd.source = source;
        cmd.submitTime = std::chrono::system_clock::now();
        
        // Tokenize
        auto tokens = tokenize(commandString);
        if (tokens.empty()) {
            cmd.errors.push_back("Empty command");
            return cmd;
        }
        
        // Handle HSEND wrapper
        size_t startIdx = 0;
        if (toUpper(tokens[0]) == "HSEND") {
            startIdx = 1;
            if (tokens.size() < 2) {
                cmd.errors.push_back("HSEND requires a command");
                return cmd;
            }
        }
        
        // Identify command type
        std::string cmdName = toUpper(tokens[startIdx]);
        auto it = commandMap_.find(cmdName);
        if (it == commandMap_.end()) {
            // Try abbreviation matching
            it = findAbbreviation(cmdName);
        }
        
        if (it == commandMap_.end()) {
            cmd.errors.push_back("Unknown command: " + cmdName);
            return cmd;
        }
        
        cmd.type = it->second;
        
        // Parse options based on command type
        std::vector<std::string> args(tokens.begin() + startIdx + 1, tokens.end());
        
        switch (cmd.type) {
            case CommandType::HMIGRATE:
                cmd.options = parseMigrateOptions(args, cmd.errors, cmd.warnings);
                break;
            case CommandType::HRECALL:
                cmd.options = parseRecallOptions(args, cmd.errors, cmd.warnings);
                break;
            case CommandType::HBACKUP:
                cmd.options = parseBackupOptions(args, cmd.errors, cmd.warnings);
                break;
            case CommandType::HRECOVER:
                cmd.options = parseRecoverOptions(args, cmd.errors, cmd.warnings);
                break;
            case CommandType::HDELETE:
                cmd.options = parseDeleteOptions(args, cmd.errors, cmd.warnings);
                break;
            case CommandType::HQUERY:
                cmd.options = parseQueryOptions(args, cmd.errors, cmd.warnings);
                break;
            case CommandType::HLIST:
                cmd.options = parseListOptions(args, cmd.errors, cmd.warnings);
                break;
            case CommandType::HALTER:
                cmd.options = parseAlterOptions(args, cmd.errors, cmd.warnings);
                break;
            case CommandType::HCANCEL:
                cmd.options = parseCancelOptions(args, cmd.errors, cmd.warnings);
                break;
            case CommandType::SETSYS:
                cmd.options = parseSetsysOptions(args, cmd.errors, cmd.warnings);
                break;
            default:
                cmd.errors.push_back("Command not yet implemented: " + cmdName);
        }
        
        return cmd;
    }
    
private:
    std::vector<std::string> tokenize(const std::string& input) {
        std::vector<std::string> tokens;
        std::string current;
        bool inQuotes = false;
        bool inParens = false;
        int parenDepth = 0;
        
        for (size_t i = 0; i < input.size(); ++i) {
            char c = input[i];
            
            if (c == '\'' && !inParens) {
                inQuotes = !inQuotes;
                current += c;
            } else if (c == '(' && !inQuotes) {
                inParens = true;
                parenDepth++;
                current += c;
            } else if (c == ')' && !inQuotes) {
                parenDepth--;
                if (parenDepth <= 0) {
                    inParens = false;
                    parenDepth = 0;
                }
                current += c;
            } else if ((c == ' ' || c == '\t' || c == ',') && !inQuotes && !inParens) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        
        if (!current.empty()) {
            tokens.push_back(current);
        }
        
        return tokens;
    }
    
    std::string toUpper(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }
    
    std::map<std::string, CommandType>::iterator findAbbreviation(const std::string& abbrev) {
        if (!config_.allowAbbreviations || abbrev.size() < config_.minAbbrevLength) {
            return commandMap_.end();
        }
        
        for (auto it = commandMap_.begin(); it != commandMap_.end(); ++it) {
            if (it->first.size() >= abbrev.size() &&
                it->first.substr(0, abbrev.size()) == abbrev) {
                return it;
            }
        }
        return commandMap_.end();
    }
    
    DatasetSpec parseDatasetSpec(const std::string& spec) {
        DatasetSpec ds;
        std::string s = spec;
        
        // Remove quotes
        if (s.front() == '\'' && s.back() == '\'') {
            s = s.substr(1, s.size() - 2);
        }
        
        // Check for member
        auto parenPos = s.find('(');
        if (parenPos != std::string::npos) {
            ds.name = s.substr(0, parenPos);
            auto endParen = s.find(')', parenPos);
            if (endParen != std::string::npos) {
                ds.member = s.substr(parenPos + 1, endParen - parenPos - 1);
                if (ds.member == "*") {
                    ds.includeMembers = true;
                    ds.member.clear();
                }
            }
        } else {
            ds.name = s;
        }
        
        // Check for wildcards
        ds.isPattern = (ds.name.find('*') != std::string::npos ||
                       ds.name.find('%') != std::string::npos);
        
        return ds;
    }
    
    std::pair<std::string, std::string> parseKeywordValue(const std::string& token) {
        auto parenPos = token.find('(');
        if (parenPos == std::string::npos) {
            return {toUpper(token), ""};
        }
        
        std::string keyword = toUpper(token.substr(0, parenPos));
        auto endParen = token.rfind(')');
        std::string value;
        if (endParen != std::string::npos && endParen > parenPos) {
            value = token.substr(parenPos + 1, endParen - parenPos - 1);
        }
        
        return {keyword, value};
    }
    
    MigrateOptions parseMigrateOptions(const std::vector<std::string>& args,
                                       std::vector<std::string>& errors,
                                       std::vector<std::string>& warnings) {
        MigrateOptions opts;
        
        for (const auto& arg : args) {
            auto [keyword, value] = parseKeywordValue(arg);
            
            if (keyword == "DSNAME" || keyword == "DSN" || keyword == "DS") {
                // Parse dataset list
                auto dsNames = splitList(value);
                for (const auto& ds : dsNames) {
                    opts.datasets.push_back(parseDatasetSpec(ds));
                }
            } else if (keyword == "VOLUME" || keyword == "VOL") {
                VolumeSpec vs;
                vs.volser = value;
                opts.volumes.push_back(vs);
            } else if (keyword == "ML2") {
                opts.ml2 = true;
            } else if (keyword == "WAIT") {
                opts.wait = true;
                opts.nowait = false;
            } else if (keyword == "NOWAIT") {
                opts.nowait = true;
                opts.wait = false;
            } else if (keyword == "PRIORITY" || keyword == "PRI") {
                opts.priority = std::stoi(value);
            } else if (keyword == "DAYS") {
                opts.daysNonUsage = std::stoi(value);
            } else if (keyword == "MC" || keyword == "MGMTCLAS") {
                opts.managementClass = value;
            } else if (keyword == "SC" || keyword == "STORCLAS") {
                opts.storageClass = value;
            } else if (keyword == "COMPRESS") {
                opts.compress = true;
            } else if (keyword == "NOCOMPRESS") {
                opts.compress = false;
            } else if (keyword == "IMMEDIATE" || keyword == "IMM") {
                opts.immediate = true;
            } else if (keyword == "TARGET") {
                opts.targetVolume = value;
            } else if (!arg.empty() && arg[0] != '-') {
                // Positional dataset name
                opts.datasets.push_back(parseDatasetSpec(arg));
            } else {
                warnings.push_back("Unknown option: " + arg);
            }
        }
        
        if (opts.datasets.empty() && opts.volumes.empty()) {
            errors.push_back("No datasets or volumes specified");
        }
        
        return opts;
    }
    
    RecallOptions parseRecallOptions(const std::vector<std::string>& args,
                                     std::vector<std::string>& errors,
                                     std::vector<std::string>& warnings) {
        RecallOptions opts;
        
        for (const auto& arg : args) {
            auto [keyword, value] = parseKeywordValue(arg);
            
            if (keyword == "DSNAME" || keyword == "DSN" || keyword == "DS") {
                auto dsNames = splitList(value);
                for (const auto& ds : dsNames) {
                    opts.datasets.push_back(parseDatasetSpec(ds));
                }
            } else if (keyword == "WAIT") {
                opts.wait = true;
                opts.nowait = false;
            } else if (keyword == "NOWAIT") {
                opts.nowait = true;
                opts.wait = false;
            } else if (keyword == "PRIORITY" || keyword == "PRI") {
                opts.priority = std::stoi(value);
            } else if (keyword == "DELVOL") {
                opts.deleteBackup = true;
            } else if (keyword == "TARGET") {
                opts.targetVolume = value;
            } else if (keyword == "POOL") {
                opts.targetPool = value;
            } else if (keyword == "SYNCHRONOUS" || keyword == "SYNC") {
                opts.synchronous = true;
            } else if (!arg.empty() && arg[0] != '-') {
                opts.datasets.push_back(parseDatasetSpec(arg));
            } else {
                warnings.push_back("Unknown option: " + arg);
            }
        }
        
        if (opts.datasets.empty()) {
            errors.push_back("No datasets specified");
        }
        
        return opts;
    }
    
    BackupOptions parseBackupOptions(const std::vector<std::string>& args,
                                     std::vector<std::string>& errors,
                                     std::vector<std::string>& warnings) {
        BackupOptions opts;
        
        for (const auto& arg : args) {
            auto [keyword, value] = parseKeywordValue(arg);
            
            if (keyword == "DSNAME" || keyword == "DSN" || keyword == "DS") {
                auto dsNames = splitList(value);
                for (const auto& ds : dsNames) {
                    opts.datasets.push_back(parseDatasetSpec(ds));
                }
            } else if (keyword == "VOLUME" || keyword == "VOL") {
                VolumeSpec vs;
                vs.volser = value;
                opts.volumes.push_back(vs);
            } else if (keyword == "WAIT") {
                opts.wait = true;
                opts.nowait = false;
            } else if (keyword == "NOWAIT") {
                opts.nowait = true;
                opts.wait = false;
            } else if (keyword == "PRIORITY" || keyword == "PRI") {
                opts.priority = std::stoi(value);
            } else if (keyword == "NEWNAME") {
                opts.newName = true;
                opts.targetName = value;
            } else if (keyword == "TARGET") {
                opts.targetVolume = value;
            } else if (keyword == "INCREMENTAL" || keyword == "INCR") {
                opts.incremental = true;
            } else if (keyword == "VERSIONS" || keyword == "VERS") {
                opts.versions = std::stoi(value);
            } else if (keyword == "RETAINDAYS" || keyword == "RETAIN") {
                opts.retentionDays = std::stoi(value);
            } else if (!arg.empty() && arg[0] != '-') {
                opts.datasets.push_back(parseDatasetSpec(arg));
            } else {
                warnings.push_back("Unknown option: " + arg);
            }
        }
        
        if (opts.datasets.empty() && opts.volumes.empty()) {
            errors.push_back("No datasets or volumes specified");
        }
        
        return opts;
    }
    
    RecoverOptions parseRecoverOptions(const std::vector<std::string>& args,
                                       std::vector<std::string>& errors,
                                       std::vector<std::string>& warnings) {
        RecoverOptions opts;
        
        for (const auto& arg : args) {
            auto [keyword, value] = parseKeywordValue(arg);
            
            if (keyword == "DSNAME" || keyword == "DSN" || keyword == "DS") {
                auto dsNames = splitList(value);
                for (const auto& ds : dsNames) {
                    opts.datasets.push_back(parseDatasetSpec(ds));
                }
            } else if (keyword == "WAIT") {
                opts.wait = true;
                opts.nowait = false;
            } else if (keyword == "NOWAIT") {
                opts.nowait = true;
                opts.wait = false;
            } else if (keyword == "PRIORITY" || keyword == "PRI") {
                opts.priority = std::stoi(value);
            } else if (keyword == "NEWNAME") {
                opts.targetName = value;
            } else if (keyword == "TARGET") {
                opts.targetVolume = value;
            } else if (keyword == "GENERATION" || keyword == "GEN") {
                opts.generation = std::stoi(value);
            } else if (keyword == "VERSION" || keyword == "VERS") {
                opts.version = std::stoi(value);
            } else if (keyword == "REPLACE" || keyword == "REP") {
                opts.replace = true;
            } else if (keyword == "FROMVOLUME" || keyword == "FROMVOL") {
                opts.fromVolume = true;
                opts.sourceVolume = value;
            } else if (!arg.empty() && arg[0] != '-') {
                opts.datasets.push_back(parseDatasetSpec(arg));
            } else {
                warnings.push_back("Unknown option: " + arg);
            }
        }
        
        if (opts.datasets.empty()) {
            errors.push_back("No datasets specified");
        }
        
        return opts;
    }
    
    DeleteOptions parseDeleteOptions(const std::vector<std::string>& args,
                                     std::vector<std::string>& errors,
                                     std::vector<std::string>& warnings) {
        DeleteOptions opts;
        
        for (const auto& arg : args) {
            auto [keyword, value] = parseKeywordValue(arg);
            
            if (keyword == "DSNAME" || keyword == "DSN" || keyword == "DS") {
                auto dsNames = splitList(value);
                for (const auto& ds : dsNames) {
                    opts.datasets.push_back(parseDatasetSpec(ds));
                }
            } else if (keyword == "MIGRATED" || keyword == "MIG") {
                opts.migrated = true;
            } else if (keyword == "BACKUP" || keyword == "BACK") {
                opts.backup = true;
            } else if (keyword == "BOTH") {
                opts.migrated = true;
                opts.backup = true;
            } else if (keyword == "EXPIRED") {
                opts.expired = true;
            } else if (keyword == "PURGE") {
                opts.purge = true;
            } else if (keyword == "VERSIONS" || keyword == "VERS") {
                opts.versions = std::stoi(value);
            } else if (keyword == "WAIT") {
                opts.wait = true;
            } else if (!arg.empty() && arg[0] != '-') {
                opts.datasets.push_back(parseDatasetSpec(arg));
            } else {
                warnings.push_back("Unknown option: " + arg);
            }
        }
        
        if (opts.datasets.empty()) {
            errors.push_back("No datasets specified");
        }
        
        return opts;
    }
    
    QueryOptions parseQueryOptions(const std::vector<std::string>& args,
                                   std::vector<std::string>& errors,
                                   std::vector<std::string>& warnings) {
        QueryOptions opts;
        opts.queryType = "ACTIVE";  // Default
        
        for (const auto& arg : args) {
            auto [keyword, value] = parseKeywordValue(arg);
            
            if (keyword == "ACTIVE" || keyword == "ACT") {
                opts.queryType = "ACTIVE";
            } else if (keyword == "WAITING" || keyword == "WAIT") {
                opts.queryType = "WAITING";
            } else if (keyword == "HELD") {
                opts.queryType = "HELD";
            } else if (keyword == "SETSYS") {
                opts.queryType = "SETSYS";
            } else if (keyword == "CDSVOLS") {
                opts.queryType = "CDSVOLS";
            } else if (keyword == "DATASETNAME" || keyword == "DSNAME" || keyword == "DSN") {
                if (value.empty()) {
                    errors.push_back("DSNAME requires a value");
                } else {
                    opts.datasetName = value;
                }
            } else if (keyword == "VOLUME" || keyword == "VOL") {
                if (value.empty()) {
                    errors.push_back("VOLUME requires a value");
                } else if (value.length() > 6) {
                    errors.push_back("VOLUME serial must be 1-6 characters");
                } else {
                    opts.volumeSerial = value;
                }
            } else if (keyword == "USER") {
                if (value.empty()) {
                    errors.push_back("USER requires a value");
                } else {
                    opts.user = value;
                }
            } else if (keyword == "DETAIL") {
                opts.detail = true;
            } else if (keyword == "MAX") {
                try {
                    int maxVal = std::stoi(value);
                    if (maxVal <= 0) {
                        errors.push_back("MAX must be a positive integer");
                    } else if (maxVal > 10000) {
                        warnings.push_back("MAX value exceeds recommended limit of 10000");
                        opts.maxResults = maxVal;
                    } else {
                        opts.maxResults = maxVal;
                    }
                } catch (...) {
                    errors.push_back("MAX requires a numeric value");
                }
            } else {
                warnings.push_back("Unknown option: " + arg);
            }
        }
        
        return opts;
    }
    
    ListOptions parseListOptions(const std::vector<std::string>& args,
                                 std::vector<std::string>& errors,
                                 std::vector<std::string>& warnings) {
        ListOptions opts;
        
        for (const auto& arg : args) {
            auto [keyword, value] = parseKeywordValue(arg);
            
            if (keyword == "DSNAME" || keyword == "DSN" || keyword == "DS") {
                auto dsNames = splitList(value);
                for (const auto& ds : dsNames) {
                    opts.datasets.push_back(parseDatasetSpec(ds));
                }
            } else if (keyword == "VOLUME" || keyword == "VOL") {
                VolumeSpec vs;
                vs.volser = value;
                opts.volumes.push_back(vs);
            } else if (keyword == "LEVEL" || keyword == "LEV") {
                opts.level = true;
            } else if (keyword == "TERM" || keyword == "TERMINAL") {
                opts.term = true;
            } else if (keyword == "DATA") {
                opts.data = true;
            } else if (keyword == "HISTORY" || keyword == "HIST") {
                opts.history = true;
            } else if (keyword == "BACKUP" || keyword == "BACK") {
                opts.backup = true;
            } else if (keyword == "MIGRATED" || keyword == "MIG") {
                opts.migrated = true;
            } else if (keyword == "OUTDATASET" || keyword == "ODS") {
                opts.outputDataset = value;
            } else if (!arg.empty() && arg[0] != '-') {
                opts.datasets.push_back(parseDatasetSpec(arg));
            } else {
                warnings.push_back("Unknown option: " + arg);
            }
        }
        
        if (opts.datasets.empty() && opts.volumes.empty()) {
            errors.push_back("No datasets or volumes specified");
        }
        
        return opts;
    }
    
    AlterOptions parseAlterOptions(const std::vector<std::string>& args,
                                   std::vector<std::string>& errors,
                                   std::vector<std::string>& warnings) {
        AlterOptions opts;
        
        for (const auto& arg : args) {
            auto [keyword, value] = parseKeywordValue(arg);
            
            if (keyword == "DSNAME" || keyword == "DSN" || keyword == "DS") {
                auto dsNames = splitList(value);
                for (const auto& ds : dsNames) {
                    opts.datasets.push_back(parseDatasetSpec(ds));
                }
            } else if (keyword == "PRIORITY" || keyword == "PRI") {
                opts.priority = std::stoi(value);
            } else if (keyword == "MC" || keyword == "MGMTCLAS") {
                opts.managementClass = value;
            } else if (keyword == "SC" || keyword == "STORCLAS") {
                opts.storageClass = value;
            } else if (keyword == "DAYS") {
                opts.daysNonUsage = std::stoi(value);
            } else if (keyword == "EXPDT" || keyword == "EXPIRATION") {
                // Parse date
            } else if (keyword == "VERSIONS" || keyword == "VERS") {
                opts.backupVersions = std::stoi(value);
            } else if (!arg.empty() && arg[0] != '-') {
                opts.datasets.push_back(parseDatasetSpec(arg));
            } else {
                warnings.push_back("Unknown option: " + arg);
            }
        }
        
        if (opts.datasets.empty()) {
            errors.push_back("No datasets specified");
        }
        
        return opts;
    }
    
    CancelOptions parseCancelOptions(const std::vector<std::string>& args,
                                     std::vector<std::string>& errors,
                                     std::vector<std::string>& warnings) {
        CancelOptions opts;
        
        for (const auto& arg : args) {
            auto [keyword, value] = parseKeywordValue(arg);
            
            if (keyword == "REQUEST" || keyword == "REQ") {
                opts.requestId = std::stoull(value);
            } else if (keyword == "DSNAME" || keyword == "DSN" || keyword == "DS") {
                opts.datasetName = value;
            } else if (keyword == "USER") {
                opts.user = value;
            } else if (keyword == "ALL") {
                opts.all = true;
            } else {
                warnings.push_back("Unknown option: " + arg);
            }
        }
        
        if (opts.requestId == 0 && opts.datasetName.empty() && 
            opts.user.empty() && !opts.all) {
            errors.push_back("Must specify REQUEST, DSNAME, USER, or ALL");
        }
        
        return opts;
    }
    
    SetsysOptions parseSetsysOptions(const std::vector<std::string>& args,
                                     std::vector<std::string>& errors,
                                     std::vector<std::string>& warnings) {
        SetsysOptions opts;
        
        // Parameters that warrant warnings when changed
        static const std::set<std::string> sensitiveParams = {
            "MAXMIGRATIONTASKS", "MAXRECALLTASKS", "MAXBACKUPTASKS",
            "PRIMARYSPMGMTSTART", "SECONDARYSPMGMTSTART",
            "ML1DAYS", "ML2DAYS", "MIGRATIONLEVEL", "RECYCLEPERCENT"
        };
        
        static const std::set<std::string> deprecatedParams = {
            "TAPERECALLLIMITS", "SMALLDATASETPACKING"
        };
        
        for (const auto& arg : args) {
            auto [keyword, value] = parseKeywordValue(arg);
            opts.parameters[keyword] = value;
            
            // Warn about sensitive parameter changes
            if (sensitiveParams.count(keyword) > 0) {
                warnings.push_back("Changing " + keyword + " may affect system performance");
            }
            
            // Warn about deprecated parameters
            if (deprecatedParams.count(keyword) > 0) {
                warnings.push_back(keyword + " is deprecated and may be removed in future releases");
            }
        }
        
        if (opts.parameters.empty()) {
            errors.push_back("No parameters specified");
        }
        
        return opts;
    }
    
    std::vector<std::string> splitList(const std::string& value) {
        std::vector<std::string> result;
        std::string current;
        
        for (char c : value) {
            if (c == ',' || c == ' ') {
                if (!current.empty()) {
                    result.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        
        if (!current.empty()) {
            result.push_back(current);
        }
        
        return result;
    }
};

// =============================================================================
// Command Executor
// =============================================================================

class CommandExecutor {
public:
    using CommandHandler = std::function<CommandResult(const ParsedCommand&)>;
    
    struct Config {
        size_t maxQueueSize{10000};
        size_t workerThreads{4};
        bool enableAudit{true};
        int defaultPriority{5};
        
        Config() = default;
    };
    
private:
    Config config_;
    mutable std::mutex mutex_;
    
    std::map<CommandType, CommandHandler> handlers_;
    std::priority_queue<QueuedCommand, std::vector<QueuedCommand>,
        std::function<bool(const QueuedCommand&, const QueuedCommand&)>> commandQueue_;
    
    std::atomic<uint64_t> nextRequestId_{1};
    std::map<uint64_t, QueuedCommand> activeCommands_;
    std::map<uint64_t, QueuedCommand> completedCommands_;
    
    std::vector<std::function<void(const QueuedCommand&)>> auditCallbacks_;
    
public:
    CommandExecutor() 
        : config_(), 
          commandQueue_([](const QueuedCommand& a, const QueuedCommand& b) {
              // Higher priority first, then earlier submit time
              if (static_cast<int>(a.command.priority) != 
                  static_cast<int>(b.command.priority)) {
                  return static_cast<int>(a.command.priority) < 
                         static_cast<int>(b.command.priority);
              }
              return a.queueTime > b.queueTime;
          }) {}
    
    explicit CommandExecutor(const Config& config) 
        : config_(config),
          commandQueue_([](const QueuedCommand& a, const QueuedCommand& b) {
              if (static_cast<int>(a.command.priority) != 
                  static_cast<int>(b.command.priority)) {
                  return static_cast<int>(a.command.priority) < 
                         static_cast<int>(b.command.priority);
              }
              return a.queueTime > b.queueTime;
          }) {}
    
    // Register command handlers
    void registerHandler(CommandType type, CommandHandler handler) {
        std::lock_guard lock(mutex_);
        handlers_[type] = std::move(handler);
    }
    
    void registerAuditCallback(std::function<void(const QueuedCommand&)> callback) {
        std::lock_guard lock(mutex_);
        auditCallbacks_.push_back(std::move(callback));
    }
    
    // Submit command for execution
    uint64_t submit(const ParsedCommand& command) {
        std::lock_guard lock(mutex_);
        
        if (!command.isValid()) {
            return 0;  // Invalid command
        }
        
        QueuedCommand qc;
        qc.requestId = nextRequestId_++;
        qc.command = command;
        qc.status = CommandStatus::PENDING;
        qc.queueTime = std::chrono::system_clock::now();
        
        if (commandQueue_.size() >= config_.maxQueueSize) {
            qc.status = CommandStatus::FAILED;
            qc.result.returnCode = ReturnCode::BUSY;
            qc.result.message = "Queue full";
            return 0;
        }
        
        commandQueue_.push(qc);
        
        if (config_.enableAudit) {
            for (const auto& callback : auditCallbacks_) {
                callback(qc);
            }
        }
        
        return qc.requestId;
    }
    
    // Execute immediately (synchronous)
    CommandResult execute(const ParsedCommand& command) {
        if (!command.isValid()) {
            CommandResult result;
            result.returnCode = ReturnCode::SYNTAX_ERROR;
            result.message = "Invalid command";
            for (const auto& err : command.errors) {
                result.output.push_back("ERROR: " + err);
            }
            return result;
        }
        
        auto it = handlers_.find(command.type);
        if (it == handlers_.end()) {
            CommandResult result;
            result.returnCode = ReturnCode::ERROR;
            result.message = "No handler for command type";
            return result;
        }
        
        auto start = std::chrono::steady_clock::now();
        CommandResult result = it->second(command);
        auto end = std::chrono::steady_clock::now();
        
        result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        return result;
    }
    
    // Process next command from queue
    std::optional<CommandResult> processNext() {
        std::unique_lock lock(mutex_);
        
        if (commandQueue_.empty()) {
            return std::nullopt;
        }
        
        QueuedCommand qc = commandQueue_.top();
        commandQueue_.pop();
        
        qc.status = CommandStatus::EXECUTING;
        qc.startTime = std::chrono::system_clock::now();
        
        activeCommands_[qc.requestId] = qc;
        
        lock.unlock();  // Release lock during execution
        
        CommandResult result = execute(qc.command);
        result.requestId = qc.requestId;
        
        lock.lock();
        
        qc.result = result;
        qc.status = result.isError() ? CommandStatus::FAILED : CommandStatus::COMPLETED;
        qc.endTime = std::chrono::system_clock::now();
        
        activeCommands_.erase(qc.requestId);
        completedCommands_[qc.requestId] = qc;
        
        // Limit completed commands history
        while (completedCommands_.size() > 10000) {
            completedCommands_.erase(completedCommands_.begin());
        }
        
        return result;
    }
    
    // Cancel command
    bool cancel(uint64_t requestId) {
        std::lock_guard lock(mutex_);
        
        auto it = activeCommands_.find(requestId);
        if (it != activeCommands_.end()) {
            it->second.status = CommandStatus::CANCELLED;
            it->second.result.returnCode = ReturnCode::CANCELLED;
            completedCommands_[requestId] = it->second;
            activeCommands_.erase(it);
            return true;
        }
        
        return false;
    }
    
    // Query status
    std::optional<QueuedCommand> getCommandStatus(uint64_t requestId) const {
        std::lock_guard lock(mutex_);
        
        auto it = activeCommands_.find(requestId);
        if (it != activeCommands_.end()) {
            return it->second;
        }
        
        auto cit = completedCommands_.find(requestId);
        if (cit != completedCommands_.end()) {
            return cit->second;
        }
        
        return std::nullopt;
    }
    
    std::vector<QueuedCommand> getActiveCommands() const {
        std::lock_guard lock(mutex_);
        
        std::vector<QueuedCommand> result;
        for (const auto& [id, cmd] : activeCommands_) {
            result.push_back(cmd);
        }
        return result;
    }
    
    size_t getQueueSize() const {
        std::lock_guard lock(mutex_);
        return commandQueue_.size();
    }
};

// =============================================================================
// Command Formatter - Output formatting
// =============================================================================

class CommandFormatter {
public:
    static std::string formatResult(const CommandResult& result) {
        std::ostringstream oss;
        
        oss << "RC=" << static_cast<int>(result.returnCode);
        if (!result.message.empty()) {
            oss << " - " << result.message;
        }
        oss << "\n";
        
        for (const auto& line : result.output) {
            oss << line << "\n";
        }
        
        return oss.str();
    }
    
    static std::string formatCommand(const ParsedCommand& cmd) {
        std::ostringstream oss;
        
        oss << "Command: " << commandTypeToString(cmd.type) << "\n";
        oss << "User: " << cmd.userId << "\n";
        oss << "Source: " << sourceToString(cmd.source) << "\n";
        
        if (!cmd.errors.empty()) {
            oss << "Errors:\n";
            for (const auto& err : cmd.errors) {
                oss << "  " << err << "\n";
            }
        }
        
        if (!cmd.warnings.empty()) {
            oss << "Warnings:\n";
            for (const auto& warn : cmd.warnings) {
                oss << "  " << warn << "\n";
            }
        }
        
        return oss.str();
    }
    
    static std::string commandTypeToString(CommandType type) {
        switch (type) {
            case CommandType::HMIGRATE: return "HMIGRATE";
            case CommandType::HRECALL: return "HRECALL";
            case CommandType::HBACKUP: return "HBACKUP";
            case CommandType::HRECOVER: return "HRECOVER";
            case CommandType::HDELETE: return "HDELETE";
            case CommandType::HQUERY: return "HQUERY";
            case CommandType::HLIST: return "HLIST";
            case CommandType::HALTER: return "HALTER";
            case CommandType::HCANCEL: return "HCANCEL";
            case CommandType::HSEND: return "HSEND";
            case CommandType::SETSYS: return "SETSYS";
            case CommandType::ADDVOL: return "ADDVOL";
            case CommandType::DELVOL: return "DELVOL";
            case CommandType::HOLD: return "HOLD";
            case CommandType::RELEASE: return "RELEASE";
            case CommandType::AUDIT: return "AUDIT";
            case CommandType::RECYCLE: return "RECYCLE";
            case CommandType::EXPIREBV: return "EXPIREBV";
            case CommandType::FREEVOL: return "FREEVOL";
            default: return "UNKNOWN";
        }
    }
    
    static std::string sourceToString(CommandSource source) {
        switch (source) {
            case CommandSource::TSO: return "TSO";
            case CommandSource::BATCH: return "BATCH";
            case CommandSource::OPERATOR: return "OPERATOR";
            case CommandSource::API: return "API";
            case CommandSource::AUTOMATIC: return "AUTOMATIC";
            case CommandSource::SCHEDULED: return "SCHEDULED";
            default: return "UNKNOWN";
        }
    }
    
    static std::string statusToString(CommandStatus status) {
        switch (status) {
            case CommandStatus::PENDING: return "PENDING";
            case CommandStatus::VALIDATING: return "VALIDATING";
            case CommandStatus::QUEUED: return "QUEUED";
            case CommandStatus::EXECUTING: return "EXECUTING";
            case CommandStatus::COMPLETED: return "COMPLETED";
            case CommandStatus::FAILED: return "FAILED";
            case CommandStatus::CANCELLED: return "CANCELLED";
            case CommandStatus::HELD: return "HELD";
            default: return "UNKNOWN";
        }
    }
};

// =============================================================================
// Integrated Command Processor
// =============================================================================

class CommandProcessor {
public:
    struct Config {
        CommandParser::Config parserConfig;
        CommandExecutor::Config executorConfig;
        bool echoCommands{true};
        
        Config() = default;
    };
    
private:
    Config config_;
    CommandParser parser_;
    CommandExecutor executor_;
    
public:
    CommandProcessor() 
        : config_(), 
          parser_(config_.parserConfig), 
          executor_(config_.executorConfig) {}
    
    explicit CommandProcessor(const Config& config) 
        : config_(config),
          parser_(config.parserConfig),
          executor_(config.executorConfig) {}
    
    // Register handler
    void registerHandler(CommandType type, CommandExecutor::CommandHandler handler) {
        executor_.registerHandler(type, std::move(handler));
    }
    
    // Parse and execute command
    CommandResult process(const std::string& commandString,
                         const std::string& userId = "SYSTEM",
                         CommandSource source = CommandSource::TSO) {
        // Parse command
        ParsedCommand cmd = parser_.parse(commandString, userId, source);
        
        if (!cmd.isValid()) {
            CommandResult result;
            result.returnCode = ReturnCode::SYNTAX_ERROR;
            result.message = "Syntax error in command";
            for (const auto& err : cmd.errors) {
                result.output.push_back("ERROR: " + err);
            }
            return result;
        }
        
        // Execute
        return executor_.execute(cmd);
    }
    
    // Submit for async execution
    uint64_t submit(const std::string& commandString,
                   const std::string& userId = "SYSTEM",
                   CommandSource source = CommandSource::TSO) {
        ParsedCommand cmd = parser_.parse(commandString, userId, source);
        return executor_.submit(cmd);
    }
    
    // Get status
    std::optional<QueuedCommand> getStatus(uint64_t requestId) const {
        return executor_.getCommandStatus(requestId);
    }
    
    // Cancel
    bool cancel(uint64_t requestId) {
        return executor_.cancel(requestId);
    }
    
    // Process queue
    std::optional<CommandResult> processNext() {
        return executor_.processNext();
    }
    
    // Accessors
    CommandParser& getParser() { return parser_; }
    CommandExecutor& getExecutor() { return executor_; }
};

}  // namespace command
}  // namespace hsm

#endif // HSM_COMMAND_PROCESSOR_HPP
