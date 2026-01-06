/**
 * @file command_processor.hpp
 * @brief Interactive command processor for CLI
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_COMMAND_PROCESSOR_HPP
#define DFSMSHSM_COMMAND_PROCESSOR_HPP

#include "hsm_engine.hpp"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace dfsmshsm {

class CommandProcessor {
public:
    explicit CommandProcessor(HSMEngine* engine) : engine_(engine) {
        registerCommands();
    }
    
    std::string process(const std::string& input) {
        auto tokens = tokenize(input);
        if (tokens.empty()) return "";
        
        std::string cmd = toUpper(tokens[0]);
        tokens.erase(tokens.begin());
        
        auto it = handlers_.find(cmd);
        if (it != handlers_.end()) {
            return it->second(tokens);
        }
        return "Unknown command: " + cmd + ". Type HELP for available commands.";
    }
    
    std::string getHelp() const {
        return R"(=== DFSMShsm v4.1.0 Commands ===

DATASET OPERATIONS:
  LIST                      - List all datasets
  STATUS                    - Show system status
  STATS                     - Show statistics
  
MIGRATION:
  MIGRATE <dataset> [tier]  - Migrate dataset (ML1, ML2, TAPE)
  RECALL <dataset>          - Recall to primary storage
  
BACKUP:
  BACKUP <dataset>          - Backup a dataset
  RECOVER <dataset>         - Recover from backup
  
VOLUME:
  VOLUME LIST               - List all volumes
  VOLUME CREATE <n> <tier> <size> - Create volume
  
DEDUPLICATION (v4.1.0):
  DEDUPE STATUS             - Show dedupe statistics
  DEDUPE GC                 - Run garbage collection
  
QUOTA (v4.1.0):
  QUOTA SET <user> <limit>  - Set user quota (bytes)
  QUOTA SHOW <user>         - Show user quota usage
  
ENCRYPTION (v4.1.0):
  ENCRYPT <dataset>         - Encrypt a dataset
  DECRYPT <dataset>         - Decrypt a dataset
  KEY GENERATE <name>       - Generate encryption key
  KEY ROTATE <key_id>       - Rotate encryption key
  
SNAPSHOTS (v4.1.0):
  SNAPSHOT CREATE <dataset> [desc] - Create snapshot
  SNAPSHOT LIST <dataset>   - List snapshots
  SNAPSHOT DELETE <snap_id> - Delete snapshot
  SNAPSHOT RESTORE <snap_id>- Restore from snapshot
  
REPLICATION (v4.1.0):
  REPLICATE <dataset> <tier>- Create replica
  REPLICA LIST <dataset>    - List replicas
  REPLICA SYNC <replica_id> - Sync replica
  
SCHEDULING (v4.1.0):
  SCHEDULE LIST             - List scheduled tasks
  SCHEDULE ADD <name> <op> <freq> - Add task
  SCHEDULE CANCEL <task_id> - Cancel task
  
JOURNALING (v4.1.0):
  JOURNAL STATUS            - Show journal status
  JOURNAL CHECKPOINT        - Force checkpoint
  
MONITORING:
  HEALTH                    - Show health report
  AUDIT [n]                 - Show recent audit records
  PERF                      - Show performance metrics
  REPORT                    - Generate full report
  
SYSTEM:
  CONFIG GET <key>          - Get config value
  CONFIG SET <key> <value>  - Set config value
  DELETE <dataset>          - Delete dataset
  HELP                      - Show this help
  QUIT                      - Exit
)";
    }

private:
    void registerCommands() {
        handlers_["HELP"] = [this](const std::vector<std::string>&) { return getHelp(); };
        handlers_["QUIT"] = [](const std::vector<std::string>&) { return std::string("QUIT"); };
        handlers_["EXIT"] = [](const std::vector<std::string>&) { return std::string("QUIT"); };
        
        handlers_["STATUS"] = [this](const std::vector<std::string>&) { 
            return engine_->getStatusReport(); 
        };
        handlers_["STATS"] = [this](const std::vector<std::string>&) { 
            return engine_->getStatusReport(); 
        };
        handlers_["PERF"] = [this](const std::vector<std::string>&) { 
            return engine_->getStatusReport(); 
        };
        handlers_["REPORT"] = [this](const std::vector<std::string>&) { 
            return engine_->generateFullReport(); 
        };
        
        handlers_["HEALTH"] = [this](const std::vector<std::string>&) {
            auto r = engine_->getHealthReport();
            std::ostringstream oss;
            oss << "Health: " << healthToString(r.overall_status) << "\n"
                << "Storage: " << std::fixed << std::setprecision(1) 
                << r.primary_space_percent << "%\n"
                << "Memory: " << r.memory_usage_percent << "%\n"
                << "Pending: " << r.pending_operations << "\n";
            return oss.str();
        };
        
        handlers_["AUDIT"] = [](const std::vector<std::string>& args) {
            size_t n = args.empty() ? 10 : std::min(size_t(std::stoi(args[0])), size_t(100));
            auto recs = AuditLogger::instance().getRecentRecords(n);
            std::ostringstream oss;
            oss << "=== Recent Audit Records ===\n";
            for (const auto& r : recs) {
                oss << r.operation_id << " | " << operationToString(r.operation) 
                    << " | " << r.target << " | " << statusToString(r.status) << "\n";
            }
            return oss.str();
        };
        
        handlers_["LIST"] = [this](const std::vector<std::string>&) {
            auto ds = engine_->getAllDatasets();
            std::ostringstream oss;
            oss << "=== Datasets (" << ds.size() << ") ===\n";
            for (const auto& d : ds) {
                oss << d.name << " | " << tierToString(d.tier) << " | " 
                    << stateToString(d.state) << " | " << formatBytes(d.size);
                if (d.is_encrypted) oss << " [E]";
                if (d.is_deduplicated) oss << " [D]";
                if (d.has_backup) oss << " [B]";
                oss << "\n";
            }
            return oss.str();
        };
        
        handlers_["MIGRATE"] = [this](const std::vector<std::string>& args) {
            if (args.empty()) return std::string("Usage: MIGRATE <dataset> [tier]");
            StorageTier tier = StorageTier::MIGRATION_1;
            if (args.size() > 1) {
                auto t = stringToTier(toUpper(args[1]));
                if (t) tier = *t;
            }
            auto r = engine_->migrateDataset(args[0], tier);
            return statusToString(r.status) + ": " + r.message;
        };
        
        handlers_["RECALL"] = [this](const std::vector<std::string>& args) {
            if (args.empty()) return std::string("Usage: RECALL <dataset>");
            auto r = engine_->recallDataset(args[0]);
            return statusToString(r.status) + ": " + r.message;
        };
        
        handlers_["BACKUP"] = [this](const std::vector<std::string>& args) {
            if (args.empty()) return std::string("Usage: BACKUP <dataset>");
            auto r = engine_->backupDataset(args[0]);
            return statusToString(r.status) + ": " + r.message;
        };
        
        handlers_["RECOVER"] = [this](const std::vector<std::string>& args) {
            if (args.empty()) return std::string("Usage: RECOVER <dataset>");
            auto r = engine_->recoverDataset(args[0]);
            return statusToString(r.status) + ": " + r.message;
        };
        
        handlers_["DELETE"] = [this](const std::vector<std::string>& args) {
            if (args.empty()) return std::string("Usage: DELETE <dataset>");
            auto r = engine_->deleteDataset(args[0]);
            return statusToString(r.status) + ": " + r.message;
        };
        
        handlers_["VOLUME"] = [this](const std::vector<std::string>& args) {
            if (args.empty()) return std::string("Usage: VOLUME CREATE|LIST ...");
            std::string sub = toUpper(args[0]);
            if (sub == "LIST") {
                auto vols = engine_->getVolumes();
                std::ostringstream oss;
                oss << "=== Volumes (" << vols.size() << ") ===\n";
                for (const auto& v : vols) {
                    oss << v.name << " | " << tierToString(v.tier) << " | "
                        << formatBytes(v.used_space) << "/" << formatBytes(v.total_space) << "\n";
                }
                return oss.str();
            } else if (sub == "CREATE" && args.size() >= 4) {
                auto tier = stringToTier(toUpper(args[2]));
                if (!tier) return std::string("Invalid tier");
                uint64_t size = std::stoull(args[3]);
                return engine_->createVolume(args[1], *tier, size) ? 
                       std::string("Volume created") : std::string("Failed");
            }
            return std::string("Invalid VOLUME command");
        };
        
        // Deduplication commands
        handlers_["DEDUPE"] = [this](const std::vector<std::string>& args) {
            if (args.empty()) return std::string("Usage: DEDUPE STATUS|GC");
            std::string sub = toUpper(args[0]);
            if (sub == "STATUS") {
                return engine_->getDedupeManager().generateReport();
            } else if (sub == "GC") {
                size_t cleaned = engine_->runDedupeGarbageCollection();
                return "Cleaned " + std::to_string(cleaned) + " orphaned blocks";
            }
            return std::string("Invalid DEDUPE command");
        };
        
        // Quota commands
        handlers_["QUOTA"] = [this](const std::vector<std::string>& args) {
            if (args.size() < 2) return std::string("Usage: QUOTA SET|SHOW <user> [limit]");
            std::string sub = toUpper(args[0]);
            if (sub == "SET" && args.size() >= 3) {
                uint64_t limit = std::stoull(args[2]);
                return engine_->setUserQuota(args[1], limit) ? 
                       std::string("Quota set") : std::string("Failed");
            } else if (sub == "SHOW") {
                auto usage = engine_->getUserQuotaUsage(args[1]);
                if (!usage) return std::string("No quota for " + args[1]);
                std::ostringstream oss;
                oss << "User: " << usage->id << "\n"
                    << "Used: " << formatBytes(usage->space_used) << " / " 
                    << formatBytes(usage->space_limit) << " ("
                    << std::fixed << std::setprecision(1) << usage->space_percent << "%)\n"
                    << "Status: " << quotaResultToString(usage->status) << "\n";
                return oss.str();
            }
            return std::string("Invalid QUOTA command");
        };
        
        // Encryption commands
        handlers_["ENCRYPT"] = [this](const std::vector<std::string>& args) {
            if (args.empty()) return std::string("Usage: ENCRYPT <dataset>");
            auto r = engine_->encryptDataset(args[0]);
            return statusToString(r.status) + ": " + r.message;
        };
        
        handlers_["DECRYPT"] = [this](const std::vector<std::string>& args) {
            if (args.empty()) return std::string("Usage: DECRYPT <dataset>");
            auto r = engine_->decryptDataset(args[0]);
            return statusToString(r.status) + ": " + r.message;
        };
        
        handlers_["KEY"] = [this](const std::vector<std::string>& args) {
            if (args.size() < 2) return std::string("Usage: KEY GENERATE|ROTATE <name|id>");
            std::string sub = toUpper(args[0]);
            if (sub == "GENERATE") {
                auto key_id = engine_->generateEncryptionKey(args[1]);
                return "Generated key: " + key_id;
            } else if (sub == "ROTATE") {
                auto new_id = engine_->rotateEncryptionKey(args[1]);
                return new_id ? "Rotated to: " + *new_id : std::string("Key not found");
            }
            return std::string("Invalid KEY command");
        };
        
        // Snapshot commands
        handlers_["SNAPSHOT"] = [this](const std::vector<std::string>& args) {
            if (args.size() < 2) return std::string("Usage: SNAPSHOT CREATE|LIST|DELETE|RESTORE ...");
            std::string sub = toUpper(args[0]);
            if (sub == "CREATE") {
                std::string desc = args.size() > 2 ? args[2] : "";
                auto id = engine_->createSnapshot(args[1], desc);
                return id ? "Created snapshot: " + *id : std::string("Failed");
            } else if (sub == "LIST") {
                auto snaps = engine_->getDatasetSnapshots(args[1]);
                std::ostringstream oss;
                oss << "=== Snapshots for " << args[1] << " ===\n";
                for (const auto& s : snaps) {
                    oss << s.id << " | v" << s.version << " | " << s.description << "\n";
                }
                return oss.str();
            } else if (sub == "DELETE") {
                return engine_->deleteSnapshot(args[1]) ? 
                       std::string("Deleted") : std::string("Failed");
            } else if (sub == "RESTORE") {
                auto r = engine_->restoreFromSnapshot(args[1]);
                return statusToString(r.status) + ": " + r.message;
            }
            return std::string("Invalid SNAPSHOT command");
        };
        
        // Replication commands
        handlers_["REPLICATE"] = [this](const std::vector<std::string>& args) {
            if (args.size() < 2) return std::string("Usage: REPLICATE <dataset> <tier>");
            auto tier = stringToTier(toUpper(args[1]));
            if (!tier) return std::string("Invalid tier");
            auto id = engine_->replicateDataset(args[0], *tier, true);
            return id ? "Created replica: " + *id : std::string("Failed");
        };
        
        handlers_["REPLICA"] = [this](const std::vector<std::string>& args) {
            if (args.size() < 2) return std::string("Usage: REPLICA LIST|SYNC ...");
            std::string sub = toUpper(args[0]);
            if (sub == "LIST") {
                auto reps = engine_->getDatasetReplicas(args[1]);
                std::ostringstream oss;
                oss << "=== Replicas for " << args[1] << " ===\n";
                for (const auto& r : reps) {
                    oss << r.id << " | " << tierToString(r.target_tier) 
                        << " | " << r.target_volume << "\n";
                }
                return oss.str();
            } else if (sub == "SYNC") {
                return engine_->syncReplica(args[1]) ? 
                       std::string("Synced") : std::string("Failed");
            }
            return std::string("Invalid REPLICA command");
        };
        
        // Schedule commands
        handlers_["SCHEDULE"] = [this](const std::vector<std::string>& args) {
            if (args.empty()) return std::string("Usage: SCHEDULE LIST|ADD|CANCEL ...");
            std::string sub = toUpper(args[0]);
            if (sub == "LIST") {
                auto tasks = engine_->getScheduledTasks();
                std::ostringstream oss;
                oss << "=== Scheduled Tasks ===\n";
                for (const auto& t : tasks) {
                    oss << t.id << " | " << t.name << " | " 
                        << operationToString(t.operation) << " | "
                        << frequencyToString(t.frequency) 
                        << (t.enabled ? " [ON]" : " [OFF]") << "\n";
                }
                return oss.str();
            } else if (sub == "CANCEL" && args.size() >= 2) {
                return engine_->cancelTask(args[1]) ? 
                       std::string("Cancelled") : std::string("Failed");
            }
            return std::string("Invalid SCHEDULE command");
        };
        
        // Journal commands
        handlers_["JOURNAL"] = [this](const std::vector<std::string>& args) {
            if (args.empty()) return std::string("Usage: JOURNAL STATUS|CHECKPOINT");
            std::string sub = toUpper(args[0]);
            if (sub == "STATUS") {
                return engine_->getJournalManager().generateReport();
            } else if (sub == "CHECKPOINT") {
                engine_->journalCheckpoint();
                return std::string("Checkpoint completed");
            }
            return std::string("Invalid JOURNAL command");
        };
        
        handlers_["CONFIG"] = [](const std::vector<std::string>& args) {
            if (args.size() < 2) return std::string("Usage: CONFIG GET|SET <key> [value]");
            std::string sub = toUpper(args[0]);
            if (sub == "GET") {
                return args[1] + " = (config not implemented)";
            } else if (sub == "SET" && args.size() >= 3) {
                return "Set " + args[1] + " = " + args[2];
            }
            return std::string("Invalid CONFIG command");
        };
    }
    
    std::vector<std::string> tokenize(const std::string& input) {
        std::vector<std::string> tokens;
        std::istringstream iss(input);
        std::string token;
        while (iss >> token) tokens.push_back(token);
        return tokens;
    }
    
    static std::string toUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), 
                       [](unsigned char c) { return std::toupper(c); });
        return s;
    }
    
    HSMEngine* engine_;
    std::map<std::string, std::function<std::string(const std::vector<std::string>&)>> handlers_;
};

} // namespace dfsmshsm
#endif // DFSMSHSM_COMMAND_PROCESSOR_HPP
