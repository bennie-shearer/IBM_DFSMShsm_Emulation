/**
 * @file journal_manager.hpp
 * @brief Write-ahead logging for crash recovery
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 * 
 * Implements write-ahead logging (WAL) for:
 * - Crash recovery
 * - Transaction support
 * - Point-in-time recovery
 */
#ifndef DFSMSHSM_JOURNAL_MANAGER_HPP
#define DFSMSHSM_JOURNAL_MANAGER_HPP

#include "hsm_types.hpp"
#include "event_system.hpp"
#include <deque>
#include <mutex>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace dfsmshsm {

/**
 * @struct Transaction
 * @brief Active transaction information
 */
struct Transaction {
    uint64_t id = 0;
    std::vector<JournalEntry> entries;
    std::chrono::system_clock::time_point started;
    bool committed = false;
    bool rolled_back = false;
};

/**
 * @struct JournalStatistics
 * @brief Journal statistics
 */
struct JournalStatistics {
    uint64_t total_entries = 0;
    uint64_t total_transactions = 0;
    uint64_t committed_transactions = 0;
    uint64_t rolled_back_transactions = 0;
    uint64_t checkpoints = 0;
    uint64_t bytes_written = 0;
    uint64_t recovered_entries = 0;
};

/**
 * @class JournalManager
 * @brief Manages write-ahead logging for crash recovery
 */
class JournalManager {
public:
    /**
     * @brief Constructor
     * @param journal_dir Directory for journal files
     * @param max_entries Maximum entries before checkpoint
     */
    explicit JournalManager(const std::filesystem::path& journal_dir,
                            size_t max_entries = 10000)
        : journal_dir_(journal_dir), max_entries_(max_entries),
          running_(false), next_seq_(1), next_txn_(1) {}
    
    ~JournalManager() { stop(); }
    
    /**
     * @brief Initialize and start the journal manager
     */
    bool start() {
        if (running_) return false;
        
        try {
            if (!std::filesystem::exists(journal_dir_)) {
                std::filesystem::create_directories(journal_dir_);
            }
            
            // Recover from existing journal if present
            recover();
            
            // Open new journal file
            openNewJournal();
            
            running_ = true;
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /**
     * @brief Stop the journal manager
     */
    void stop() {
        if (!running_) return;
        running_ = false;
        
        // Force checkpoint before shutdown
        checkpoint();
        
        std::lock_guard<std::mutex> lock(mtx_);
        if (journal_file_.is_open()) {
            journal_file_.close();
        }
    }
    
    /**
     * @brief Check if manager is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Begin a new transaction
     * @return Transaction ID
     */
    uint64_t beginTransaction() {
        std::lock_guard<std::mutex> lock(mtx_);
        
        uint64_t txn_id = next_txn_++;
        Transaction txn;
        txn.id = txn_id;
        txn.started = std::chrono::system_clock::now();
        active_transactions_[txn_id] = txn;
        
        // Write begin marker
        JournalEntry entry;
        entry.sequence_number = next_seq_++;
        entry.transaction_id = txn_id;
        entry.type = JournalEntryType::BEGIN_TXN;
        entry.timestamp = std::chrono::system_clock::now();
        writeEntry(entry);
        
        ++stats_.total_transactions;
        return txn_id;
    }
    
    /**
     * @brief Write a journal entry
     * @param txn_id Transaction ID
     * @param type Entry type
     * @param target Target dataset/volume
     * @param data Optional data payload
     * @return Sequence number
     */
    uint64_t write(uint64_t txn_id, JournalEntryType type,
                   const std::string& target,
                   const std::vector<uint8_t>& data = {}) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = active_transactions_.find(txn_id);
        if (it == active_transactions_.end()) {
            return 0;  // Invalid transaction
        }
        
        JournalEntry entry;
        entry.sequence_number = next_seq_++;
        entry.transaction_id = txn_id;
        entry.type = type;
        entry.target = target;
        entry.data = data;
        entry.checksum = computeChecksum(entry);
        entry.timestamp = std::chrono::system_clock::now();
        
        it->second.entries.push_back(entry);
        writeEntry(entry);
        
        ++stats_.total_entries;
        
        // Check if checkpoint needed
        if (entries_.size() >= max_entries_) {
            checkpointInternal();
        }
        
        return entry.sequence_number;
    }
    
    /**
     * @brief Write metadata update
     */
    uint64_t writeMetadata(uint64_t txn_id, const std::string& target,
                           const std::string& metadata) {
        std::vector<uint8_t> data(metadata.begin(), metadata.end());
        return write(txn_id, JournalEntryType::METADATA_UPDATE, target, data);
    }
    
    /**
     * @brief Commit a transaction
     * @param txn_id Transaction ID
     * @return True if successful
     */
    bool commit(uint64_t txn_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = active_transactions_.find(txn_id);
        if (it == active_transactions_.end()) {
            return false;
        }
        
        // Write commit marker
        JournalEntry entry;
        entry.sequence_number = next_seq_++;
        entry.transaction_id = txn_id;
        entry.type = JournalEntryType::COMMIT_TXN;
        entry.timestamp = std::chrono::system_clock::now();
        writeEntry(entry);
        
        // Sync to disk
        if (journal_file_.is_open()) {
            journal_file_.flush();
        }
        
        it->second.committed = true;
        committed_transactions_.push_back(it->second);
        active_transactions_.erase(it);
        
        ++stats_.committed_transactions;
        return true;
    }
    
    /**
     * @brief Rollback a transaction
     * @param txn_id Transaction ID
     * @return True if successful
     */
    bool rollback(uint64_t txn_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = active_transactions_.find(txn_id);
        if (it == active_transactions_.end()) {
            return false;
        }
        
        // Write rollback marker
        JournalEntry entry;
        entry.sequence_number = next_seq_++;
        entry.transaction_id = txn_id;
        entry.type = JournalEntryType::ROLLBACK_TXN;
        entry.timestamp = std::chrono::system_clock::now();
        writeEntry(entry);
        
        it->second.rolled_back = true;
        active_transactions_.erase(it);
        
        ++stats_.rolled_back_transactions;
        return true;
    }
    
    /**
     * @brief Force a checkpoint
     */
    void checkpoint() {
        std::lock_guard<std::mutex> lock(mtx_);
        checkpointInternal();
    }
    
    /**
     * @brief Recover from journal after crash
     * @return Number of entries recovered
     */
    size_t recover() {
        std::lock_guard<std::mutex> lock(mtx_);
        
        size_t recovered = 0;
        
        // Find all journal files
        std::vector<std::filesystem::path> journal_files;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(journal_dir_)) {
                if (entry.path().extension() == ".wal") {
                    journal_files.push_back(entry.path());
                }
            }
        } catch (...) {
            return 0;
        }
        
        // Sort by name (timestamp-based)
        std::sort(journal_files.begin(), journal_files.end());
        
        // Read and replay entries
        for (const auto& file : journal_files) {
            auto entries = readJournalFile(file);
            recovered += entries.size();
            
            for (const auto& entry : entries) {
                entries_.push_back(entry);
                
                if (entry.sequence_number >= next_seq_) {
                    next_seq_ = entry.sequence_number + 1;
                }
                if (entry.transaction_id >= next_txn_) {
                    next_txn_ = entry.transaction_id + 1;
                }
            }
        }
        
        stats_.recovered_entries = recovered;
        
        EventSystem::instance().emit(
            EventType::JOURNAL_CHECKPOINT,
            "JournalManager",
            "Recovered " + std::to_string(recovered) + " journal entries");
        
        return recovered;
    }
    
    /**
     * @brief Get entries for a specific transaction
     */
    std::vector<JournalEntry> getTransactionEntries(uint64_t txn_id) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        std::vector<JournalEntry> result;
        for (const auto& entry : entries_) {
            if (entry.transaction_id == txn_id) {
                result.push_back(entry);
            }
        }
        return result;
    }
    
    /**
     * @brief Get recent journal entries
     */
    std::vector<JournalEntry> getRecentEntries(size_t count) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        std::vector<JournalEntry> result;
        size_t start = entries_.size() > count ? entries_.size() - count : 0;
        for (size_t i = start; i < entries_.size(); ++i) {
            result.push_back(entries_[i]);
        }
        return result;
    }
    
    /**
     * @brief Get journal statistics
     */
    JournalStatistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return stats_;
    }
    
    /**
     * @brief Get active transaction count
     */
    size_t getActiveTransactionCount() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return active_transactions_.size();
    }
    
    /**
     * @brief Get journal backlog size
     */
    size_t getBacklogSize() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return entries_.size();
    }
    
    /**
     * @brief Generate journal report
     */
    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        
        oss << "=== Journal Statistics ===\n"
            << "Total Entries: " << stats_.total_entries << "\n"
            << "Total Transactions: " << stats_.total_transactions << "\n"
            << "Committed: " << stats_.committed_transactions << "\n"
            << "Rolled Back: " << stats_.rolled_back_transactions << "\n"
            << "Checkpoints: " << stats_.checkpoints << "\n"
            << "Bytes Written: " << formatBytes(stats_.bytes_written) << "\n"
            << "Active Transactions: " << active_transactions_.size() << "\n"
            << "Pending Entries: " << entries_.size() << "\n"
            << "Next Sequence: " << next_seq_ << "\n";
        
        return oss.str();
    }

private:
    void openNewJournal() {
        if (journal_file_.is_open()) {
            journal_file_.close();
        }
        
        auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        
        std::ostringstream fn;
        fn << "journal_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".wal";
        
        current_journal_ = journal_dir_ / fn.str();
        journal_file_.open(current_journal_, std::ios::binary | std::ios::app);
        
        // Write header
        if (journal_file_) {
            std::string header = "DFSMSHSM_WAL_V3\n";
            journal_file_.write(header.c_str(), header.size());
            stats_.bytes_written += header.size();
        }
    }
    
    void writeEntry(const JournalEntry& entry) {
        if (!journal_file_) return;
        
        // Serialize entry
        std::ostringstream oss;
        oss << entry.sequence_number << "|"
            << entry.transaction_id << "|"
            << static_cast<int>(entry.type) << "|"
            << entry.target << "|"
            << entry.data.size() << "|"
            << entry.checksum << "\n";
        
        std::string line = oss.str();
        journal_file_.write(line.c_str(), line.size());
        
        if (!entry.data.empty()) {
            journal_file_.write(reinterpret_cast<const char*>(entry.data.data()),
                              entry.data.size());
            journal_file_.write("\n", 1);
        }
        
        stats_.bytes_written += line.size() + entry.data.size();
        entries_.push_back(entry);
    }
    
    std::vector<JournalEntry> readJournalFile(const std::filesystem::path& path) {
        std::vector<JournalEntry> result;
        std::ifstream file(path, std::ios::binary);
        if (!file) return result;
        
        std::string line;
        std::getline(file, line);  // Skip header
        
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            JournalEntry entry;
            std::istringstream iss(line);
            std::string field;
            
            try {
                std::getline(iss, field, '|');
                entry.sequence_number = std::stoull(field);
                
                std::getline(iss, field, '|');
                entry.transaction_id = std::stoull(field);
                
                std::getline(iss, field, '|');
                entry.type = static_cast<JournalEntryType>(std::stoi(field));
                
                std::getline(iss, field, '|');
                entry.target = field;
                
                std::getline(iss, field, '|');
                size_t data_size = std::stoull(field);
                
                std::getline(iss, field, '|');
                entry.checksum = std::stoul(field);
                
                if (data_size > 0) {
                    entry.data.resize(data_size);
                    file.read(reinterpret_cast<char*>(entry.data.data()), data_size);
                    std::getline(file, line);  // Skip newline
                }
                
                result.push_back(entry);
            } catch (...) {
                // Skip malformed entries
                continue;
            }
        }
        
        return result;
    }
    
    void checkpointInternal() {
        // Write checkpoint marker
        JournalEntry entry;
        entry.sequence_number = next_seq_++;
        entry.transaction_id = 0;
        entry.type = JournalEntryType::CHECKPOINT;
        entry.timestamp = std::chrono::system_clock::now();
        writeEntry(entry);
        
        // Flush to disk
        if (journal_file_.is_open()) {
            journal_file_.flush();
        }
        
        // Clear committed entries older than checkpoint
        entries_.clear();
        
        // Start new journal file
        openNewJournal();
        
        ++stats_.checkpoints;
        
        EventSystem::instance().emit(
            EventType::JOURNAL_CHECKPOINT,
            "JournalManager",
            "Checkpoint completed");
    }
    
    uint32_t computeChecksum(const JournalEntry& entry) {
        uint32_t checksum = 0;
        checksum ^= static_cast<uint32_t>(entry.sequence_number);
        checksum ^= static_cast<uint32_t>(entry.transaction_id);
        checksum ^= static_cast<uint32_t>(entry.type);
        for (char c : entry.target) checksum = checksum * 31 + c;
        for (uint8_t b : entry.data) checksum = checksum * 31 + b;
        return checksum;
    }
    
    std::filesystem::path journal_dir_;
    std::filesystem::path current_journal_;
    std::ofstream journal_file_;
    size_t max_entries_;
    
    std::atomic<bool> running_;
    std::atomic<uint64_t> next_seq_;
    std::atomic<uint64_t> next_txn_;
    
    mutable std::mutex mtx_;
    std::deque<JournalEntry> entries_;
    std::map<uint64_t, Transaction> active_transactions_;
    std::vector<Transaction> committed_transactions_;
    JournalStatistics stats_;
};

} // namespace dfsmshsm
#endif // DFSMSHSM_JOURNAL_MANAGER_HPP
