/**
 * @file hsm_command_history.hpp
 * @brief Command history with undo/redo support
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 *
 * Implements the Command pattern for HSM operations with:
 * - Undoable/redoable operations
 * - Command history with configurable depth
 * - Transaction support for compound operations
 * - Audit trail generation
 * - Command replay for recovery
 */
#ifndef DFSMSHSM_HSM_COMMAND_HISTORY_HPP
#define DFSMSHSM_HSM_COMMAND_HISTORY_HPP

#include "hsm_types.hpp"
#include "result.hpp"
#include <vector>
#include <string>
#include <memory>
#include <stack>
#include <deque>
#include <functional>
#include <chrono>
#include <mutex>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <map>
#include <any>

namespace dfsmshsm {

//=============================================================================
// Command Interface
//=============================================================================

/**
 * @class ICommand
 * @brief Abstract interface for executable/undoable commands
 */
class ICommand {
public:
    virtual ~ICommand() = default;
    
    /**
     * @brief Execute the command
     */
    [[nodiscard]] virtual Result<void> execute() = 0;
    
    /**
     * @brief Undo the command (must be called after successful execute)
     */
    [[nodiscard]] virtual Result<void> undo() = 0;
    
    /**
     * @brief Redo the command (must be called after successful undo)
     */
    [[nodiscard]] virtual Result<void> redo() { return execute(); }
    
    /**
     * @brief Check if command can be undone
     */
    [[nodiscard]] virtual bool canUndo() const = 0;
    
    /**
     * @brief Get command name/description
     */
    [[nodiscard]] virtual std::string name() const = 0;
    
    /**
     * @brief Get detailed description
     */
    [[nodiscard]] virtual std::string description() const = 0;
    
    /**
     * @brief Get command ID
     */
    [[nodiscard]] virtual std::string id() const = 0;
    
    /**
     * @brief Merge with another command (for optimization)
     * @return true if merged, false if cannot merge
     */
    [[nodiscard]] virtual bool merge(const ICommand& other) { 
        (void)other; 
        return false; 
    }
    
    /**
     * @brief Get timestamp when command was created
     */
    [[nodiscard]] virtual std::chrono::system_clock::time_point timestamp() const = 0;
};

//=============================================================================
// Base Command Implementation
//=============================================================================

/**
 * @class BaseCommand
 * @brief Base class with common command functionality
 */
class BaseCommand : public ICommand {
public:
    BaseCommand(std::string cmdName, std::string cmdDesc)
        : name_(std::move(cmdName)),
          description_(std::move(cmdDesc)),
          timestamp_(std::chrono::system_clock::now()),
          executed_(false) {
        id_ = generateId();
    }
    
    [[nodiscard]] std::string name() const override { return name_; }
    [[nodiscard]] std::string description() const override { return description_; }
    [[nodiscard]] std::string id() const override { return id_; }
    [[nodiscard]] std::chrono::system_clock::time_point timestamp() const override { 
        return timestamp_; 
    }
    [[nodiscard]] bool wasExecuted() const { return executed_; }

protected:
    void setExecuted(bool val) { executed_ = val; }
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::system_clock::now();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
        std::ostringstream oss;
        oss << "CMD-" << std::hex << micros << "-" << ++counter;
        return oss.str();
    }
    
    std::string name_;
    std::string description_;
    std::string id_;
    std::chrono::system_clock::time_point timestamp_;
    bool executed_;
};

//=============================================================================
// Concrete Command Types
//=============================================================================

/**
 * @class LambdaCommand
 * @brief Command implemented with lambdas
 */
class LambdaCommand : public BaseCommand {
public:
    using ExecuteFunc = std::function<Result<void>()>;
    using UndoFunc = std::function<Result<void>()>;
    
    LambdaCommand(std::string name, std::string desc,
                  ExecuteFunc executeFunc, UndoFunc undoFunc)
        : BaseCommand(std::move(name), std::move(desc)),
          execute_func_(std::move(executeFunc)),
          undo_func_(std::move(undoFunc)) {}
    
    [[nodiscard]] Result<void> execute() override {
        auto result = execute_func_();
        if (result) setExecuted(true);
        return result;
    }
    
    [[nodiscard]] Result<void> undo() override {
        if (!wasExecuted()) {
            return Err<void>(ErrorCode::INVALID_STATE, "Command not executed");
        }
        auto result = undo_func_();
        if (result) setExecuted(false);
        return result;
    }
    
    [[nodiscard]] bool canUndo() const override { return wasExecuted(); }

private:
    ExecuteFunc execute_func_;
    UndoFunc undo_func_;
};

/**
 * @class DatasetCreateCommand
 * @brief Command to create a dataset
 */
class DatasetCreateCommand : public BaseCommand {
public:
    using CreateFunc = std::function<Result<void>(const DatasetInfo&)>;
    using DeleteFunc = std::function<Result<void>(const std::string&)>;
    
    DatasetCreateCommand(const DatasetInfo& dataset,
                        CreateFunc createFunc, DeleteFunc deleteFunc)
        : BaseCommand("CREATE_DATASET", "Create dataset: " + dataset.name),
          dataset_(dataset),
          create_func_(std::move(createFunc)),
          delete_func_(std::move(deleteFunc)) {}
    
    [[nodiscard]] Result<void> execute() override {
        auto result = create_func_(dataset_);
        if (result) setExecuted(true);
        return result;
    }
    
    [[nodiscard]] Result<void> undo() override {
        if (!wasExecuted()) {
            return Err<void>(ErrorCode::INVALID_STATE, "Command not executed");
        }
        auto result = delete_func_(dataset_.name);
        if (result) setExecuted(false);
        return result;
    }
    
    [[nodiscard]] bool canUndo() const override { return wasExecuted(); }
    [[nodiscard]] const DatasetInfo& dataset() const { return dataset_; }

private:
    DatasetInfo dataset_;
    CreateFunc create_func_;
    DeleteFunc delete_func_;
};

/**
 * @class DatasetDeleteCommand
 * @brief Command to delete a dataset
 */
class DatasetDeleteCommand : public BaseCommand {
public:
    using DeleteFunc = std::function<Result<void>(const std::string&)>;
    using CreateFunc = std::function<Result<void>(const DatasetInfo&)>;
    using GetFunc = std::function<std::optional<DatasetInfo>(const std::string&)>;
    
    DatasetDeleteCommand(const std::string& name,
                        DeleteFunc deleteFunc, CreateFunc createFunc, GetFunc getFunc)
        : BaseCommand("DELETE_DATASET", "Delete dataset: " + name),
          dataset_name_(name),
          delete_func_(std::move(deleteFunc)),
          create_func_(std::move(createFunc)),
          get_func_(std::move(getFunc)) {}
    
    [[nodiscard]] Result<void> execute() override {
        // Save dataset info for undo
        auto info = get_func_(dataset_name_);
        if (!info) {
            return Err<void>(ErrorCode::NOT_FOUND, "Dataset not found: " + dataset_name_);
        }
        saved_dataset_ = *info;
        
        auto result = delete_func_(dataset_name_);
        if (result) setExecuted(true);
        return result;
    }
    
    [[nodiscard]] Result<void> undo() override {
        if (!wasExecuted()) {
            return Err<void>(ErrorCode::INVALID_STATE, "Command not executed");
        }
        auto result = create_func_(saved_dataset_);
        if (result) setExecuted(false);
        return result;
    }
    
    [[nodiscard]] bool canUndo() const override { return wasExecuted(); }

private:
    std::string dataset_name_;
    DatasetInfo saved_dataset_;
    DeleteFunc delete_func_;
    CreateFunc create_func_;
    GetFunc get_func_;
};

/**
 * @class DatasetMigrateCommand
 * @brief Command to migrate a dataset
 */
class DatasetMigrateCommand : public BaseCommand {
public:
    using MigrateFunc = std::function<Result<void>(const std::string&, StorageTier)>;
    using GetTierFunc = std::function<std::optional<StorageTier>(const std::string&)>;
    
    DatasetMigrateCommand(const std::string& name, StorageTier targetTier,
                         MigrateFunc migrateFunc, GetTierFunc getTierFunc)
        : BaseCommand("MIGRATE_DATASET", 
                     "Migrate " + name + " to " + tierToString(targetTier)),
          dataset_name_(name),
          target_tier_(targetTier),
          migrate_func_(std::move(migrateFunc)),
          get_tier_func_(std::move(getTierFunc)) {}
    
    [[nodiscard]] Result<void> execute() override {
        // Save original tier for undo
        auto tier = get_tier_func_(dataset_name_);
        if (!tier) {
            return Err<void>(ErrorCode::NOT_FOUND, "Dataset not found: " + dataset_name_);
        }
        original_tier_ = *tier;
        
        auto result = migrate_func_(dataset_name_, target_tier_);
        if (result) setExecuted(true);
        return result;
    }
    
    [[nodiscard]] Result<void> undo() override {
        if (!wasExecuted()) {
            return Err<void>(ErrorCode::INVALID_STATE, "Command not executed");
        }
        auto result = migrate_func_(dataset_name_, original_tier_);
        if (result) setExecuted(false);
        return result;
    }
    
    [[nodiscard]] bool canUndo() const override { return wasExecuted(); }

private:
    std::string dataset_name_;
    StorageTier target_tier_;
    StorageTier original_tier_;
    MigrateFunc migrate_func_;
    GetTierFunc get_tier_func_;
};

/**
 * @class PropertyChangeCommand
 * @brief Generic command for changing a property value
 */
template<typename T>
class PropertyChangeCommand : public BaseCommand {
public:
    using GetFunc = std::function<T()>;
    using SetFunc = std::function<void(const T&)>;
    
    PropertyChangeCommand(std::string propertyName, T newValue,
                         GetFunc getFunc, SetFunc setFunc)
        : BaseCommand("CHANGE_" + propertyName, 
                     "Change " + propertyName + " property"),
          property_name_(std::move(propertyName)),
          new_value_(std::move(newValue)),
          get_func_(std::move(getFunc)),
          set_func_(std::move(setFunc)) {}
    
    [[nodiscard]] Result<void> execute() override {
        old_value_ = get_func_();
        set_func_(new_value_);
        setExecuted(true);
        return Ok();
    }
    
    [[nodiscard]] Result<void> undo() override {
        if (!wasExecuted()) {
            return Err<void>(ErrorCode::INVALID_STATE, "Command not executed");
        }
        set_func_(old_value_);
        setExecuted(false);
        return Ok();
    }
    
    [[nodiscard]] bool canUndo() const override { return wasExecuted(); }
    
    [[nodiscard]] bool merge(const ICommand& other) override {
        auto* otherProp = dynamic_cast<const PropertyChangeCommand<T>*>(&other);
        if (otherProp && otherProp->property_name_ == property_name_) {
            new_value_ = otherProp->new_value_;
            return true;
        }
        return false;
    }

private:
    std::string property_name_;
    T old_value_;
    T new_value_;
    GetFunc get_func_;
    SetFunc set_func_;
};

//=============================================================================
// Composite Command (Transaction)
//=============================================================================

/**
 * @class CompositeCommand
 * @brief Groups multiple commands into a single undoable unit
 */
class CompositeCommand : public BaseCommand {
public:
    explicit CompositeCommand(std::string name)
        : BaseCommand(std::move(name), "Composite command") {}
    
    /**
     * @brief Add a command to the composite
     */
    void addCommand(std::shared_ptr<ICommand> cmd) {
        commands_.push_back(std::move(cmd));
        updateDescription();
    }
    
    [[nodiscard]] Result<void> execute() override {
        for (size_t i = 0; i < commands_.size(); ++i) {
            auto result = commands_[i]->execute();
            if (!result) {
                // Undo already executed commands
                for (size_t j = i; j > 0; --j) {
                    commands_[j-1]->undo();  // Ignore undo errors
                }
                return result;
            }
        }
        setExecuted(true);
        return Ok();
    }
    
    [[nodiscard]] Result<void> undo() override {
        if (!wasExecuted()) {
            return Err<void>(ErrorCode::INVALID_STATE, "Command not executed");
        }
        
        // Undo in reverse order
        for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
            auto result = (*it)->undo();
            if (!result) {
                return result;
            }
        }
        setExecuted(false);
        return Ok();
    }
    
    [[nodiscard]] bool canUndo() const override {
        if (!wasExecuted()) return false;
        for (const auto& cmd : commands_) {
            if (!cmd->canUndo()) return false;
        }
        return true;
    }
    
    [[nodiscard]] size_t size() const { return commands_.size(); }
    [[nodiscard]] const std::vector<std::shared_ptr<ICommand>>& commands() const {
        return commands_;
    }

private:
    void updateDescription() {
        std::ostringstream oss;
        oss << "Composite (" << commands_.size() << " commands)";
        description_ = oss.str();
    }
    
    std::vector<std::shared_ptr<ICommand>> commands_;
};

//=============================================================================
// Command History
//=============================================================================

/**
 * @struct CommandHistoryEntry
 * @brief Entry in the command history
 */
struct CommandHistoryEntry {
    std::shared_ptr<ICommand> command;
    std::chrono::system_clock::time_point executed_at;
    bool undone = false;
    std::string user;
};

/**
 * @class CommandHistory
 * @brief Manages command history with undo/redo support
 */
class CommandHistory {
public:
    struct Config {
        size_t max_history_size = 1000;
        bool enable_merging = true;
        std::chrono::seconds merge_window = std::chrono::seconds(5);
    };
    
    CommandHistory() : config_() {}
    
    explicit CommandHistory(const Config& config)
        : config_(config) {}
    
    /**
     * @brief Execute a command and add to history
     */
    [[nodiscard]] Result<void> execute(std::shared_ptr<ICommand> cmd,
                                       const std::string& user = "") {
        auto result = cmd->execute();
        if (!result) return result;
        
        std::unique_lock lock(mutex_);
        
        // Clear redo stack
        redo_stack_.clear();
        
        // Try to merge with previous command
        if (config_.enable_merging && !undo_stack_.empty()) {
            auto& last = undo_stack_.back();
            auto timeDiff = std::chrono::system_clock::now() - last.executed_at;
            if (timeDiff < config_.merge_window && last.command->merge(*cmd)) {
                // Merged successfully, don't add new entry
                return Ok();
            }
        }
        
        // Add to history
        CommandHistoryEntry entry;
        entry.command = std::move(cmd);
        entry.executed_at = std::chrono::system_clock::now();
        entry.user = user;
        undo_stack_.push_back(std::move(entry));
        
        // Limit history size
        while (undo_stack_.size() > config_.max_history_size) {
            undo_stack_.pop_front();
        }
        
        ++stats_.commands_executed;
        return Ok();
    }
    
    /**
     * @brief Undo the last command
     */
    [[nodiscard]] Result<void> undo() {
        std::unique_lock lock(mutex_);
        
        if (undo_stack_.empty()) {
            return Err<void>(ErrorCode::NOT_FOUND, "Nothing to undo");
        }
        
        auto& entry = undo_stack_.back();
        if (!entry.command->canUndo()) {
            return Err<void>(ErrorCode::INVALID_STATE, "Command cannot be undone");
        }
        
        lock.unlock();
        auto result = entry.command->undo();
        lock.lock();
        
        if (result) {
            entry.undone = true;
            redo_stack_.push_back(std::move(entry));
            undo_stack_.pop_back();
            ++stats_.undos_performed;
        }
        
        return result;
    }
    
    /**
     * @brief Redo the last undone command
     */
    [[nodiscard]] Result<void> redo() {
        std::unique_lock lock(mutex_);
        
        if (redo_stack_.empty()) {
            return Err<void>(ErrorCode::NOT_FOUND, "Nothing to redo");
        }
        
        auto entry = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        
        lock.unlock();
        auto result = entry.command->redo();
        lock.lock();
        
        if (result) {
            entry.undone = false;
            entry.executed_at = std::chrono::system_clock::now();
            undo_stack_.push_back(std::move(entry));
            ++stats_.redos_performed;
        } else {
            // Put it back if redo failed
            redo_stack_.push_back(std::move(entry));
        }
        
        return result;
    }
    
    /**
     * @brief Check if undo is possible
     */
    [[nodiscard]] bool canUndo() const {
        std::shared_lock lock(mutex_);
        return !undo_stack_.empty() && undo_stack_.back().command->canUndo();
    }
    
    /**
     * @brief Check if redo is possible
     */
    [[nodiscard]] bool canRedo() const {
        std::shared_lock lock(mutex_);
        return !redo_stack_.empty();
    }
    
    /**
     * @brief Get the description of the next undo command
     */
    [[nodiscard]] std::optional<std::string> nextUndoDescription() const {
        std::shared_lock lock(mutex_);
        if (undo_stack_.empty()) return std::nullopt;
        return undo_stack_.back().command->description();
    }
    
    /**
     * @brief Get the description of the next redo command
     */
    [[nodiscard]] std::optional<std::string> nextRedoDescription() const {
        std::shared_lock lock(mutex_);
        if (redo_stack_.empty()) return std::nullopt;
        return redo_stack_.back().command->description();
    }
    
    /**
     * @brief Get recent history entries
     */
    [[nodiscard]] std::vector<CommandHistoryEntry> getHistory(size_t count = 10) const {
        std::shared_lock lock(mutex_);
        
        std::vector<CommandHistoryEntry> result;
        size_t n = std::min(count, undo_stack_.size());
        
        for (size_t i = 0; i < n; ++i) {
            result.push_back(undo_stack_[undo_stack_.size() - 1 - i]);
        }
        
        return result;
    }
    
    /**
     * @brief Clear all history
     */
    void clear() {
        std::unique_lock lock(mutex_);
        undo_stack_.clear();
        redo_stack_.clear();
    }
    
    /**
     * @brief Get history sizes
     */
    [[nodiscard]] size_t undoStackSize() const {
        std::shared_lock lock(mutex_);
        return undo_stack_.size();
    }
    
    [[nodiscard]] size_t redoStackSize() const {
        std::shared_lock lock(mutex_);
        return redo_stack_.size();
    }
    
    /**
     * @brief Statistics
     */
    struct Statistics {
        std::atomic<uint64_t> commands_executed{0};
        std::atomic<uint64_t> undos_performed{0};
        std::atomic<uint64_t> redos_performed{0};
    };
    
    [[nodiscard]] const Statistics& statistics() const { return stats_; }
    
    /**
     * @brief Generate report
     */
    [[nodiscard]] std::string generateReport() const {
        std::shared_lock lock(mutex_);
        std::ostringstream oss;
        
        oss << "=== Command History Report ===\n"
            << "Undo Stack Size: " << undo_stack_.size() << "\n"
            << "Redo Stack Size: " << redo_stack_.size() << "\n"
            << "Commands Executed: " << stats_.commands_executed.load() << "\n"
            << "Undos Performed: " << stats_.undos_performed.load() << "\n"
            << "Redos Performed: " << stats_.redos_performed.load() << "\n\n"
            << "Recent Commands:\n";
        
        size_t n = std::min(size_t(10), undo_stack_.size());
        for (size_t i = 0; i < n; ++i) {
            const auto& entry = undo_stack_[undo_stack_.size() - 1 - i];
            auto time_t = std::chrono::system_clock::to_time_t(entry.executed_at);
            oss << "  " << (i + 1) << ". " 
                << std::put_time(std::localtime(&time_t), "%H:%M:%S") << " "
                << entry.command->name() << ": " << entry.command->description();
            if (!entry.user.empty()) {
                oss << " (by " << entry.user << ")";
            }
            oss << "\n";
        }
        
        return oss.str();
    }

private:
    Config config_;
    mutable std::shared_mutex mutex_;
    std::deque<CommandHistoryEntry> undo_stack_;
    std::vector<CommandHistoryEntry> redo_stack_;
    Statistics stats_;
};

//=============================================================================
// Command Factory
//=============================================================================

/**
 * @class CommandFactory
 * @brief Factory for creating HSM commands
 */
class CommandFactory {
public:
    /**
     * @brief Create a lambda-based command
     */
    [[nodiscard]] static std::shared_ptr<LambdaCommand> createLambda(
            std::string name, std::string desc,
            LambdaCommand::ExecuteFunc execute,
            LambdaCommand::UndoFunc undo) {
        return std::make_shared<LambdaCommand>(
            std::move(name), std::move(desc),
            std::move(execute), std::move(undo));
    }
    
    /**
     * @brief Create a composite command
     */
    [[nodiscard]] static std::shared_ptr<CompositeCommand> createComposite(
            std::string name) {
        return std::make_shared<CompositeCommand>(std::move(name));
    }
    
    /**
     * @brief Create a dataset create command
     */
    [[nodiscard]] static std::shared_ptr<DatasetCreateCommand> createDatasetCreate(
            const DatasetInfo& dataset,
            DatasetCreateCommand::CreateFunc createFunc,
            DatasetCreateCommand::DeleteFunc deleteFunc) {
        return std::make_shared<DatasetCreateCommand>(
            dataset, std::move(createFunc), std::move(deleteFunc));
    }
    
    /**
     * @brief Create a dataset delete command
     */
    [[nodiscard]] static std::shared_ptr<DatasetDeleteCommand> createDatasetDelete(
            const std::string& name,
            DatasetDeleteCommand::DeleteFunc deleteFunc,
            DatasetDeleteCommand::CreateFunc createFunc,
            DatasetDeleteCommand::GetFunc getFunc) {
        return std::make_shared<DatasetDeleteCommand>(
            name, std::move(deleteFunc), std::move(createFunc), std::move(getFunc));
    }
    
    /**
     * @brief Create a migration command
     */
    [[nodiscard]] static std::shared_ptr<DatasetMigrateCommand> createMigration(
            const std::string& name, StorageTier targetTier,
            DatasetMigrateCommand::MigrateFunc migrateFunc,
            DatasetMigrateCommand::GetTierFunc getTierFunc) {
        return std::make_shared<DatasetMigrateCommand>(
            name, targetTier, std::move(migrateFunc), std::move(getTierFunc));
    }
    
    /**
     * @brief Create a property change command
     */
    template<typename T>
    [[nodiscard]] static std::shared_ptr<PropertyChangeCommand<T>> createPropertyChange(
            std::string propertyName, T newValue,
            typename PropertyChangeCommand<T>::GetFunc getFunc,
            typename PropertyChangeCommand<T>::SetFunc setFunc) {
        return std::make_shared<PropertyChangeCommand<T>>(
            std::move(propertyName), std::move(newValue),
            std::move(getFunc), std::move(setFunc));
    }
};

//=============================================================================
// Transaction Builder
//=============================================================================

/**
 * @class TransactionBuilder
 * @brief Fluent interface for building composite commands
 */
class TransactionBuilder {
public:
    explicit TransactionBuilder(std::string name)
        : composite_(CommandFactory::createComposite(std::move(name))) {}
    
    /**
     * @brief Add a command to the transaction
     */
    TransactionBuilder& add(std::shared_ptr<ICommand> cmd) {
        composite_->addCommand(std::move(cmd));
        return *this;
    }
    
    /**
     * @brief Add a lambda command
     */
    TransactionBuilder& add(std::string name, std::string desc,
                           LambdaCommand::ExecuteFunc execute,
                           LambdaCommand::UndoFunc undo) {
        composite_->addCommand(CommandFactory::createLambda(
            std::move(name), std::move(desc),
            std::move(execute), std::move(undo)));
        return *this;
    }
    
    /**
     * @brief Build the composite command
     */
    [[nodiscard]] std::shared_ptr<CompositeCommand> build() {
        return std::move(composite_);
    }

private:
    std::shared_ptr<CompositeCommand> composite_;
};

//=============================================================================
// Command Serialization (for persistence/replay)
//=============================================================================

/**
 * @struct SerializedCommand
 * @brief Serialized representation of a command
 */
struct SerializedCommand {
    std::string id;
    std::string type;
    std::string name;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> parameters;
    std::string user;
    
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        oss << "Command[" << id << "] " << type << ": " << name << "\n"
            << "  Time: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\n"
            << "  Description: " << description << "\n";
        if (!user.empty()) {
            oss << "  User: " << user << "\n";
        }
        if (!parameters.empty()) {
            oss << "  Parameters:\n";
            for (const auto& [k, v] : parameters) {
                oss << "    " << k << " = " << v << "\n";
            }
        }
        return oss.str();
    }
};

/**
 * @class CommandSerializer
 * @brief Serializes commands for persistence
 */
class CommandSerializer {
public:
    /**
     * @brief Serialize a command
     */
    [[nodiscard]] static SerializedCommand serialize(const ICommand& cmd) {
        SerializedCommand sc;
        sc.id = cmd.id();
        sc.name = cmd.name();
        sc.description = cmd.description();
        sc.timestamp = cmd.timestamp();
        
        // Determine type based on name
        if (cmd.name().find("CREATE_DATASET") != std::string::npos) {
            sc.type = "DATASET_CREATE";
        } else if (cmd.name().find("DELETE_DATASET") != std::string::npos) {
            sc.type = "DATASET_DELETE";
        } else if (cmd.name().find("MIGRATE_DATASET") != std::string::npos) {
            sc.type = "DATASET_MIGRATE";
        } else if (cmd.name().find("CHANGE_") != std::string::npos) {
            sc.type = "PROPERTY_CHANGE";
        } else {
            sc.type = "GENERIC";
        }
        
        return sc;
    }
    
    /**
     * @brief Export history to text format
     */
    [[nodiscard]] static std::string exportHistory(
            const std::vector<CommandHistoryEntry>& entries) {
        std::ostringstream oss;
        oss << "# Command History Export\n"
            << "# Generated: " << formatTimestamp(std::chrono::system_clock::now()) << "\n"
            << "# Entries: " << entries.size() << "\n\n";
        
        for (const auto& entry : entries) {
            auto sc = serialize(*entry.command);
            sc.user = entry.user;
            oss << sc.toString() << "\n";
        }
        
        return oss.str();
    }

private:
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace dfsmshsm
#endif // DFSMSHSM_HSM_COMMAND_HISTORY_HPP
