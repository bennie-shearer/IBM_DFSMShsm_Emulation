/*
 * DFSMShsm Emulator - Interval Processing
 * @version 4.2.0
 * 
 * Emulates HSM interval-based processing:
 * - Primary space management windows
 * - Secondary space management intervals
 * - Backup windows
 * - Migration windows
 * - Automatic space management scheduling
 * - Activity throttling
 * - Resource management
 */

#ifndef HSM_INTERVAL_HPP
#define HSM_INTERVAL_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <functional>
#include <optional>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>

namespace hsm {
namespace interval {

// =============================================================================
// Time and Schedule Types
// =============================================================================

enum class DayOfWeek : uint8_t {
    SUNDAY = 0,
    MONDAY = 1,
    TUESDAY = 2,
    WEDNESDAY = 3,
    THURSDAY = 4,
    FRIDAY = 5,
    SATURDAY = 6,
    ALL_DAYS = 7,      // Every day
    WEEKDAYS = 8,      // Mon-Fri
    WEEKENDS = 9       // Sat-Sun
};

enum class IntervalType {
    PRIMARY_SPACE_MANAGEMENT,      // Primary SM interval
    SECONDARY_SPACE_MANAGEMENT,    // Secondary SM interval
    AUTOMATIC_BACKUP,              // Automatic backup window
    AUTOMATIC_MIGRATION,           // Automatic migration window
    AUTOMATIC_RECALL,              // Automatic recall window
    AUTOMATIC_DELETE,              // Automatic deletion window
    AUDIT,                         // Audit interval
    RECYCLE,                       // Tape recycle interval
    MAINTENANCE,                   // System maintenance window
    CUSTOM                         // User-defined interval
};

enum class IntervalState {
    INACTIVE,          // Not in window
    STARTING,          // Window opening
    ACTIVE,            // Window open, processing
    STOPPING,          // Window closing
    SUSPENDED,         // Temporarily suspended
    HELD               // Held by operator
};

struct TimeOfDay {
    int hour{0};       // 0-23
    int minute{0};     // 0-59
    int second{0};     // 0-59
    
    TimeOfDay() = default;
    TimeOfDay(int h, int m, int s = 0) : hour(h), minute(m), second(s) {}
    
    int toSeconds() const {
        return hour * 3600 + minute * 60 + second;
    }
    
    static TimeOfDay fromSeconds(int secs) {
        TimeOfDay t;
        t.hour = secs / 3600;
        t.minute = (secs % 3600) / 60;
        t.second = secs % 60;
        return t;
    }
    
    bool operator<(const TimeOfDay& other) const {
        return toSeconds() < other.toSeconds();
    }
    
    bool operator<=(const TimeOfDay& other) const {
        return toSeconds() <= other.toSeconds();
    }
    
    bool operator==(const TimeOfDay& other) const {
        return toSeconds() == other.toSeconds();
    }
    
    std::string toString() const {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, minute, second);
        return std::string(buf);
    }
    
    static TimeOfDay now() {
        auto now = std::chrono::system_clock::now();
        auto time_t_val = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&time_t_val);
        return TimeOfDay(tm->tm_hour, tm->tm_min, tm->tm_sec);
    }
};

struct TimeWindow {
    TimeOfDay startTime;
    TimeOfDay endTime;
    
    TimeWindow() = default;
    TimeWindow(TimeOfDay start, TimeOfDay end) : startTime(start), endTime(end) {}
    TimeWindow(int startHour, int startMin, int endHour, int endMin)
        : startTime(startHour, startMin), endTime(endHour, endMin) {}
    
    bool isActive(const TimeOfDay& time) const {
        if (startTime < endTime) {
            // Normal case: start before end (e.g., 08:00 - 17:00)
            return time.toSeconds() >= startTime.toSeconds() && 
                   time.toSeconds() < endTime.toSeconds();
        } else {
            // Overnight case: end before start (e.g., 22:00 - 06:00)
            return time.toSeconds() >= startTime.toSeconds() || 
                   time.toSeconds() < endTime.toSeconds();
        }
    }
    
    bool isActiveNow() const {
        return isActive(TimeOfDay::now());
    }
    
    int durationMinutes() const {
        int startSecs = startTime.toSeconds();
        int endSecs = endTime.toSeconds();
        if (endSecs > startSecs) {
            return (endSecs - startSecs) / 60;
        } else {
            return (86400 - startSecs + endSecs) / 60;
        }
    }
    
    std::string toString() const {
        return startTime.toString() + " - " + endTime.toString();
    }
};

// =============================================================================
// Interval Definition
// =============================================================================

struct IntervalDefinition {
    std::string name;
    IntervalType type{IntervalType::CUSTOM};
    
    // Schedule
    std::vector<TimeWindow> windows;           // Time windows
    std::set<DayOfWeek> activeDays;            // Active days
    std::set<int> activeDaysOfMonth;           // Specific days (1-31)
    
    // Processing parameters
    int maxTasks{10};                          // Max concurrent tasks
    int taskLimitPerMinute{100};               // Rate limiting
    int priorityBoost{0};                      // Priority adjustment
    
    // Resource limits
    uint64_t maxBytesPerInterval{0};           // 0 = unlimited
    uint64_t maxDatasetsPerInterval{0};        // 0 = unlimited
    int maxCPUPercent{80};                     // CPU limit
    int maxIOPercent{90};                      // I/O limit
    
    // Behavior
    bool enabled{true};
    bool allowOverlap{false};                  // Allow overlap with other intervals
    bool resumeOnRestart{true};                // Resume after restart
    bool requiresQuiesce{false};               // Require system quiesce
    
    // Targets
    std::vector<std::string> volumePatterns;   // Target volumes
    std::vector<std::string> datasetPatterns;  // Target datasets
    std::vector<std::string> storageGroups;    // Target storage groups
    
    // Exclusions
    std::vector<std::string> excludeVolumes;
    std::vector<std::string> excludeDatasets;
    
    bool isDayActive() const {
        if (activeDays.empty() && activeDaysOfMonth.empty()) {
            return true;  // All days
        }
        
        auto now = std::chrono::system_clock::now();
        auto time_t_val = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&time_t_val);
        
        DayOfWeek today = static_cast<DayOfWeek>(tm->tm_wday);
        int dayOfMonth = tm->tm_mday;
        
        // Check day of week
        if (!activeDays.empty()) {
            if (activeDays.count(DayOfWeek::ALL_DAYS)) {
                // OK
            } else if (activeDays.count(DayOfWeek::WEEKDAYS) && 
                       today >= DayOfWeek::MONDAY && today <= DayOfWeek::FRIDAY) {
                // OK
            } else if (activeDays.count(DayOfWeek::WEEKENDS) &&
                       (today == DayOfWeek::SATURDAY || today == DayOfWeek::SUNDAY)) {
                // OK
            } else if (!activeDays.count(today)) {
                return false;
            }
        }
        
        // Check day of month
        if (!activeDaysOfMonth.empty() && !activeDaysOfMonth.count(dayOfMonth)) {
            return false;
        }
        
        return true;
    }
    
    bool isWindowActive() const {
        if (!isDayActive()) {
            return false;
        }
        
        if (windows.empty()) {
            return true;  // No windows = always active
        }
        
        TimeOfDay now = TimeOfDay::now();
        for (const auto& window : windows) {
            if (window.isActive(now)) {
                return true;
            }
        }
        
        return false;
    }
};

// =============================================================================
// Interval Statistics
// =============================================================================

struct IntervalStatistics {
    std::string intervalName;
    uint64_t totalActivations{0};
    uint64_t currentActivation{0};
    
    uint64_t datasetsProcessed{0};
    uint64_t bytesProcessed{0};
    uint64_t tasksCompleted{0};
    uint64_t tasksFailed{0};
    
    std::chrono::system_clock::time_point lastStartTime;
    std::chrono::system_clock::time_point lastEndTime;
    std::chrono::milliseconds totalActiveTime{0};
    std::chrono::milliseconds averageActiveDuration{0};
    
    double averageTasksPerMinute{0.0};
    double peakTasksPerMinute{0.0};
};

// =============================================================================
// Interval Task
// =============================================================================

struct IntervalTask {
    uint64_t taskId{0};
    std::string intervalName;
    std::string description;
    
    std::function<bool()> execute;     // Task function
    int priority{5};                    // Task priority
    int retryCount{0};
    int maxRetries{3};
    
    std::chrono::system_clock::time_point submitTime;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    
    bool completed{false};
    bool success{false};
    std::string errorMessage;
};

// =============================================================================
// Resource Monitor
// =============================================================================

class ResourceMonitor {
public:
    struct ResourceUsage {
        double cpuPercent{0.0};
        double ioPercent{0.0};
        double memoryPercent{0.0};
        uint64_t activeThreads{0};
        uint64_t pendingIO{0};
    };
    
private:
    mutable std::mutex mutex_;
    ResourceUsage current_;
    std::vector<ResourceUsage> history_;
    size_t historySize_{60};  // 1 minute at 1-second samples
    
public:
    void update(const ResourceUsage& usage) {
        std::lock_guard lock(mutex_);
        current_ = usage;
        history_.push_back(usage);
        if (history_.size() > historySize_) {
            history_.erase(history_.begin());
        }
    }
    
    ResourceUsage getCurrent() const {
        std::lock_guard lock(mutex_);
        return current_;
    }
    
    double getAverageCPU() const {
        std::lock_guard lock(mutex_);
        if (history_.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& r : history_) {
            sum += r.cpuPercent;
        }
        return sum / history_.size();
    }
    
    double getAverageIO() const {
        std::lock_guard lock(mutex_);
        if (history_.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& r : history_) {
            sum += r.ioPercent;
        }
        return sum / history_.size();
    }
    
    bool isUnderThreshold(int maxCPU, int maxIO) const {
        std::lock_guard lock(mutex_);
        return current_.cpuPercent < maxCPU && current_.ioPercent < maxIO;
    }
};

// =============================================================================
// Interval Scheduler
// =============================================================================

class IntervalScheduler {
public:
    struct Config {
        std::chrono::seconds checkInterval{60};     // Check every minute
        size_t maxConcurrentIntervals{5};
        bool enableThrottling{true};
        bool enableResourceMonitoring{true};
        
        Config() = default;
    };
    
    using IntervalCallback = std::function<void(const std::string& intervalName, 
                                                 IntervalState state)>;
    using TaskCallback = std::function<void(const IntervalTask& task)>;
    
private:
    Config config_;
    mutable std::shared_mutex mutex_;
    
    std::map<std::string, IntervalDefinition> intervals_;
    std::map<std::string, IntervalState> states_;
    std::map<std::string, IntervalStatistics> statistics_;
    
    std::priority_queue<IntervalTask, std::vector<IntervalTask>,
        std::function<bool(const IntervalTask&, const IntervalTask&)>> taskQueue_;
    
    std::vector<IntervalCallback> stateCallbacks_;
    std::vector<TaskCallback> taskCallbacks_;
    
    ResourceMonitor resourceMonitor_;
    
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> nextTaskId_{1};
    std::thread schedulerThread_;
    std::condition_variable_any cv_;
    
    // Active interval tracking
    std::map<std::string, std::chrono::system_clock::time_point> activeStartTimes_;
    std::map<std::string, uint64_t> currentDatasetsProcessed_;
    std::map<std::string, uint64_t> currentBytesProcessed_;
    
public:
    IntervalScheduler() 
        : config_(),
          taskQueue_([](const IntervalTask& a, const IntervalTask& b) {
              return a.priority < b.priority;
          }) {}
    
    explicit IntervalScheduler(const Config& config) 
        : config_(config),
          taskQueue_([](const IntervalTask& a, const IntervalTask& b) {
              return a.priority < b.priority;
          }) {}
    
    ~IntervalScheduler() {
        stop();
    }
    
    // =========================================================================
    // Interval Management
    // =========================================================================
    
    void addInterval(const IntervalDefinition& interval) {
        std::unique_lock lock(mutex_);
        intervals_[interval.name] = interval;
        states_[interval.name] = IntervalState::INACTIVE;
        statistics_[interval.name] = IntervalStatistics{};
        statistics_[interval.name].intervalName = interval.name;
    }
    
    void removeInterval(const std::string& name) {
        std::unique_lock lock(mutex_);
        intervals_.erase(name);
        states_.erase(name);
        statistics_.erase(name);
    }
    
    void updateInterval(const IntervalDefinition& interval) {
        std::unique_lock lock(mutex_);
        intervals_[interval.name] = interval;
    }
    
    std::optional<IntervalDefinition> getInterval(const std::string& name) const {
        std::shared_lock lock(mutex_);
        auto it = intervals_.find(name);
        if (it != intervals_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<IntervalDefinition> getAllIntervals() const {
        std::shared_lock lock(mutex_);
        std::vector<IntervalDefinition> result;
        for (const auto& [name, interval] : intervals_) {
            result.push_back(interval);
        }
        return result;
    }
    
    // =========================================================================
    // State Management
    // =========================================================================
    
    IntervalState getState(const std::string& name) const {
        std::shared_lock lock(mutex_);
        auto it = states_.find(name);
        return it != states_.end() ? it->second : IntervalState::INACTIVE;
    }
    
    void holdInterval(const std::string& name) {
        std::unique_lock lock(mutex_);
        if (states_.count(name)) {
            states_[name] = IntervalState::HELD;
            notifyStateChange(name, IntervalState::HELD);
        }
    }
    
    void releaseInterval(const std::string& name) {
        std::unique_lock lock(mutex_);
        if (states_.count(name) && states_[name] == IntervalState::HELD) {
            states_[name] = IntervalState::INACTIVE;
            notifyStateChange(name, IntervalState::INACTIVE);
        }
    }
    
    void suspendInterval(const std::string& name) {
        std::unique_lock lock(mutex_);
        if (states_.count(name)) {
            states_[name] = IntervalState::SUSPENDED;
            notifyStateChange(name, IntervalState::SUSPENDED);
        }
    }
    
    void resumeInterval(const std::string& name) {
        std::unique_lock lock(mutex_);
        if (states_.count(name) && states_[name] == IntervalState::SUSPENDED) {
            states_[name] = IntervalState::INACTIVE;
            notifyStateChange(name, IntervalState::INACTIVE);
        }
    }
    
    std::vector<std::string> getActiveIntervals() const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> result;
        for (const auto& [name, state] : states_) {
            if (state == IntervalState::ACTIVE) {
                result.push_back(name);
            }
        }
        return result;
    }
    
    // =========================================================================
    // Task Management
    // =========================================================================
    
    uint64_t submitTask(const std::string& intervalName,
                       std::function<bool()> execute,
                       const std::string& description = "",
                       int priority = 5) {
        std::unique_lock lock(mutex_);
        
        IntervalTask task;
        task.taskId = nextTaskId_++;
        task.intervalName = intervalName;
        task.description = description;
        task.execute = std::move(execute);
        task.priority = priority;
        task.submitTime = std::chrono::system_clock::now();
        
        taskQueue_.push(task);
        cv_.notify_one();
        
        return task.taskId;
    }
    
    size_t getPendingTaskCount() const {
        std::shared_lock lock(mutex_);
        return taskQueue_.size();
    }
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    void addStateCallback(IntervalCallback callback) {
        std::unique_lock lock(mutex_);
        stateCallbacks_.push_back(std::move(callback));
    }
    
    void addTaskCallback(TaskCallback callback) {
        std::unique_lock lock(mutex_);
        taskCallbacks_.push_back(std::move(callback));
    }
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    IntervalStatistics getStatistics(const std::string& name) const {
        std::shared_lock lock(mutex_);
        auto it = statistics_.find(name);
        if (it != statistics_.end()) {
            return it->second;
        }
        return IntervalStatistics{};
    }
    
    std::map<std::string, IntervalStatistics> getAllStatistics() const {
        std::shared_lock lock(mutex_);
        return statistics_;
    }
    
    void resetStatistics(const std::string& name) {
        std::unique_lock lock(mutex_);
        if (statistics_.count(name)) {
            statistics_[name] = IntervalStatistics{};
            statistics_[name].intervalName = name;
        }
    }
    
    // =========================================================================
    // Resource Monitoring
    // =========================================================================
    
    void updateResourceUsage(const ResourceMonitor::ResourceUsage& usage) {
        resourceMonitor_.update(usage);
    }
    
    ResourceMonitor::ResourceUsage getResourceUsage() const {
        return resourceMonitor_.getCurrent();
    }
    
    // =========================================================================
    // Scheduler Control
    // =========================================================================
    
    void start() {
        if (running_.exchange(true)) {
            return;  // Already running
        }
        
        schedulerThread_ = std::thread([this]() {
            schedulerLoop();
        });
    }
    
    void stop() {
        if (!running_.exchange(false)) {
            return;  // Not running
        }
        
        cv_.notify_all();
        
        if (schedulerThread_.joinable()) {
            schedulerThread_.join();
        }
    }
    
    bool isRunning() const {
        return running_;
    }
    
    // Manual check (for testing)
    void checkIntervals() {
        std::unique_lock lock(mutex_);
        checkIntervalsImpl();
    }
    
    // Process pending tasks (for testing)
    void processTasks(size_t maxTasks = 10) {
        std::unique_lock lock(mutex_);
        processTasksImpl(maxTasks);
    }
    
private:
    void schedulerLoop() {
        while (running_) {
            {
                std::unique_lock lock(mutex_);
                
                // Check interval windows
                checkIntervalsImpl();
                
                // Process tasks
                processTasksImpl(10);
                
                // Wait for next check or notification
                cv_.wait_for(lock, config_.checkInterval, [this]() {
                    return !running_ || !taskQueue_.empty();
                });
            }
        }
    }
    
    void checkIntervalsImpl() {
        auto now = std::chrono::system_clock::now();
        
        for (auto& [name, interval] : intervals_) {
            if (!interval.enabled) continue;
            
            IntervalState& state = states_[name];
            
            // Skip held or suspended intervals
            if (state == IntervalState::HELD || state == IntervalState::SUSPENDED) {
                continue;
            }
            
            bool shouldBeActive = interval.isWindowActive();
            
            if (shouldBeActive && state == IntervalState::INACTIVE) {
                // Check resource limits
                if (config_.enableResourceMonitoring) {
                    if (!resourceMonitor_.isUnderThreshold(
                            interval.maxCPUPercent, interval.maxIOPercent)) {
                        continue;  // Skip, resources exhausted
                    }
                }
                
                // Check overlap
                if (!interval.allowOverlap && countActiveIntervals() >= config_.maxConcurrentIntervals) {
                    continue;  // Too many active intervals
                }
                
                // Start interval
                state = IntervalState::STARTING;
                notifyStateChange(name, IntervalState::STARTING);
                
                activeStartTimes_[name] = now;
                currentDatasetsProcessed_[name] = 0;
                currentBytesProcessed_[name] = 0;
                
                state = IntervalState::ACTIVE;
                statistics_[name].totalActivations++;
                statistics_[name].currentActivation++;
                statistics_[name].lastStartTime = now;
                
                notifyStateChange(name, IntervalState::ACTIVE);
                
            } else if (!shouldBeActive && state == IntervalState::ACTIVE) {
                // Stop interval
                state = IntervalState::STOPPING;
                notifyStateChange(name, IntervalState::STOPPING);
                
                auto startTime = activeStartTimes_[name];
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
                
                statistics_[name].lastEndTime = now;
                statistics_[name].totalActiveTime += duration;
                
                if (statistics_[name].totalActivations > 0) {
                    statistics_[name].averageActiveDuration = 
                        statistics_[name].totalActiveTime / 
                        static_cast<int>(statistics_[name].totalActivations);
                }
                
                activeStartTimes_.erase(name);
                
                state = IntervalState::INACTIVE;
                notifyStateChange(name, IntervalState::INACTIVE);
            }
        }
    }
    
    void processTasksImpl(size_t maxTasks) {
        size_t processed = 0;
        
        while (!taskQueue_.empty() && processed < maxTasks) {
            IntervalTask task = taskQueue_.top();
            taskQueue_.pop();
            
            // Check if interval is active
            auto stateIt = states_.find(task.intervalName);
            if (stateIt == states_.end() || stateIt->second != IntervalState::ACTIVE) {
                // Re-queue if interval not active
                if (stateIt != states_.end() && 
                    stateIt->second != IntervalState::HELD) {
                    taskQueue_.push(task);
                }
                continue;
            }
            
            // Check resource limits
            auto intervalIt = intervals_.find(task.intervalName);
            if (intervalIt != intervals_.end()) {
                const auto& interval = intervalIt->second;
                
                // Check dataset limit
                if (interval.maxDatasetsPerInterval > 0 &&
                    currentDatasetsProcessed_[task.intervalName] >= interval.maxDatasetsPerInterval) {
                    continue;  // Limit reached
                }
                
                // Check byte limit
                if (interval.maxBytesPerInterval > 0 &&
                    currentBytesProcessed_[task.intervalName] >= interval.maxBytesPerInterval) {
                    continue;  // Limit reached
                }
            }
            
            // Execute task
            task.startTime = std::chrono::system_clock::now();
            
            try {
                task.success = task.execute();
            } catch (const std::exception& e) {
                task.success = false;
                task.errorMessage = e.what();
            } catch (...) {
                task.success = false;
                task.errorMessage = "Unknown error";
            }
            
            task.endTime = std::chrono::system_clock::now();
            task.completed = true;
            
            // Update statistics
            auto& stats = statistics_[task.intervalName];
            if (task.success) {
                stats.tasksCompleted++;
            } else {
                stats.tasksFailed++;
                
                // Retry if needed
                if (task.retryCount < task.maxRetries) {
                    task.retryCount++;
                    task.completed = false;
                    taskQueue_.push(task);
                }
            }
            
            // Notify callbacks
            for (const auto& callback : taskCallbacks_) {
                callback(task);
            }
            
            processed++;
        }
    }
    
    size_t countActiveIntervals() const {
        size_t count = 0;
        for (const auto& [name, state] : states_) {
            if (state == IntervalState::ACTIVE) {
                count++;
            }
        }
        return count;
    }
    
    void notifyStateChange(const std::string& name, IntervalState state) {
        for (const auto& callback : stateCallbacks_) {
            callback(name, state);
        }
    }
};

// =============================================================================
// Predefined Interval Types
// =============================================================================

class IntervalFactory {
public:
    static IntervalDefinition createPrimarySpaceManagement(
            const std::string& name = "PRIMARY_SM",
            int startHour = 0, int endHour = 6) {
        IntervalDefinition interval;
        interval.name = name;
        interval.type = IntervalType::PRIMARY_SPACE_MANAGEMENT;
        interval.windows.push_back(TimeWindow(startHour, 0, endHour, 0));
        interval.activeDays.insert(DayOfWeek::ALL_DAYS);
        interval.maxTasks = 20;
        interval.taskLimitPerMinute = 200;
        interval.maxCPUPercent = 60;
        interval.maxIOPercent = 80;
        return interval;
    }
    
    static IntervalDefinition createSecondarySpaceManagement(
            const std::string& name = "SECONDARY_SM",
            int startHour = 2, int endHour = 5) {
        IntervalDefinition interval;
        interval.name = name;
        interval.type = IntervalType::SECONDARY_SPACE_MANAGEMENT;
        interval.windows.push_back(TimeWindow(startHour, 0, endHour, 0));
        interval.activeDays.insert(DayOfWeek::ALL_DAYS);
        interval.maxTasks = 10;
        interval.taskLimitPerMinute = 100;
        interval.maxCPUPercent = 40;
        interval.maxIOPercent = 60;
        return interval;
    }
    
    static IntervalDefinition createAutomaticBackup(
            const std::string& name = "AUTO_BACKUP",
            int startHour = 23, int endHour = 5) {
        IntervalDefinition interval;
        interval.name = name;
        interval.type = IntervalType::AUTOMATIC_BACKUP;
        interval.windows.push_back(TimeWindow(startHour, 0, endHour, 0));
        interval.activeDays.insert(DayOfWeek::ALL_DAYS);
        interval.maxTasks = 15;
        interval.taskLimitPerMinute = 150;
        interval.maxCPUPercent = 50;
        interval.maxIOPercent = 70;
        return interval;
    }
    
    static IntervalDefinition createWeekendMaintenance(
            const std::string& name = "WEEKEND_MAINT") {
        IntervalDefinition interval;
        interval.name = name;
        interval.type = IntervalType::MAINTENANCE;
        interval.windows.push_back(TimeWindow(1, 0, 7, 0));  // 1 AM to 7 AM
        interval.activeDays.insert(DayOfWeek::WEEKENDS);
        interval.maxTasks = 5;
        interval.maxCPUPercent = 30;
        interval.maxIOPercent = 40;
        interval.requiresQuiesce = true;
        return interval;
    }
    
    static IntervalDefinition createTapeRecycle(
            const std::string& name = "TAPE_RECYCLE",
            int dayOfMonth = 1) {
        IntervalDefinition interval;
        interval.name = name;
        interval.type = IntervalType::RECYCLE;
        interval.windows.push_back(TimeWindow(3, 0, 6, 0));  // 3 AM to 6 AM
        interval.activeDaysOfMonth.insert(dayOfMonth);
        interval.maxTasks = 5;
        interval.taskLimitPerMinute = 50;
        return interval;
    }
    
    static IntervalDefinition createBusinessHoursExclusion(
            const std::string& name = "NO_MIGRATION_BUSINESS") {
        IntervalDefinition interval;
        interval.name = name;
        interval.type = IntervalType::CUSTOM;
        // Active outside business hours (inverted logic - NOT 9-5 weekdays)
        interval.windows.push_back(TimeWindow(17, 0, 9, 0));  // 5 PM to 9 AM
        interval.activeDays.insert(DayOfWeek::WEEKDAYS);
        // Also active all day on weekends
        interval.activeDays.insert(DayOfWeek::WEEKENDS);
        interval.maxTasks = 30;
        return interval;
    }
};

// =============================================================================
// Utility Functions
// =============================================================================

inline std::string intervalTypeToString(IntervalType type) {
    switch (type) {
        case IntervalType::PRIMARY_SPACE_MANAGEMENT: return "PRIMARY_SM";
        case IntervalType::SECONDARY_SPACE_MANAGEMENT: return "SECONDARY_SM";
        case IntervalType::AUTOMATIC_BACKUP: return "AUTO_BACKUP";
        case IntervalType::AUTOMATIC_MIGRATION: return "AUTO_MIGRATE";
        case IntervalType::AUTOMATIC_RECALL: return "AUTO_RECALL";
        case IntervalType::AUTOMATIC_DELETE: return "AUTO_DELETE";
        case IntervalType::AUDIT: return "AUDIT";
        case IntervalType::RECYCLE: return "RECYCLE";
        case IntervalType::MAINTENANCE: return "MAINTENANCE";
        case IntervalType::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

inline std::string intervalStateToString(IntervalState state) {
    switch (state) {
        case IntervalState::INACTIVE: return "INACTIVE";
        case IntervalState::STARTING: return "STARTING";
        case IntervalState::ACTIVE: return "ACTIVE";
        case IntervalState::STOPPING: return "STOPPING";
        case IntervalState::SUSPENDED: return "SUSPENDED";
        case IntervalState::HELD: return "HELD";
        default: return "UNKNOWN";
    }
}

inline std::string dayOfWeekToString(DayOfWeek day) {
    switch (day) {
        case DayOfWeek::SUNDAY: return "SUNDAY";
        case DayOfWeek::MONDAY: return "MONDAY";
        case DayOfWeek::TUESDAY: return "TUESDAY";
        case DayOfWeek::WEDNESDAY: return "WEDNESDAY";
        case DayOfWeek::THURSDAY: return "THURSDAY";
        case DayOfWeek::FRIDAY: return "FRIDAY";
        case DayOfWeek::SATURDAY: return "SATURDAY";
        case DayOfWeek::ALL_DAYS: return "ALL_DAYS";
        case DayOfWeek::WEEKDAYS: return "WEEKDAYS";
        case DayOfWeek::WEEKENDS: return "WEEKENDS";
        default: return "UNKNOWN";
    }
}

}  // namespace interval
}  // namespace hsm

#endif // HSM_INTERVAL_HPP
