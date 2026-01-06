/**
 * @file scheduler.hpp
 * @brief Task scheduler for automated HSM operations
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 * 
 * Implements task scheduling with:
 * - CRON-style scheduling
 * - One-time and recurring tasks
 * - Task prioritization
 */
#ifndef DFSMSHSM_SCHEDULER_HPP
#define DFSMSHSM_SCHEDULER_HPP

#include "hsm_types.hpp"
#include "event_system.hpp"
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <functional>
#include <condition_variable>

namespace dfsmshsm {

/**
 * @struct SchedulerStatistics
 * @brief Scheduler statistics
 */
struct SchedulerStatistics {
    uint64_t total_tasks = 0;
    uint64_t executed_tasks = 0;
    uint64_t failed_tasks = 0;
    uint64_t skipped_tasks = 0;
    uint64_t pending_tasks = 0;
};

using TaskCallback = std::function<bool(const ScheduledTask&)>;

/**
 * @class Scheduler
 * @brief Manages scheduled HSM operations
 */
class Scheduler {
public:
    Scheduler() : running_(false), next_task_id_(1) {}
    
    ~Scheduler() { stop(); }
    
    /**
     * @brief Start the scheduler
     */
    bool start() {
        if (running_) return false;
        running_ = true;
        
        scheduler_thread_ = std::thread([this] { runScheduler(); });
        return true;
    }
    
    /**
     * @brief Stop the scheduler
     */
    void stop() {
        if (!running_) return;
        running_ = false;
        cv_.notify_all();
        
        if (scheduler_thread_.joinable()) {
            scheduler_thread_.join();
        }
    }
    
    /**
     * @brief Check if scheduler is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Schedule a new task
     * @param name Task name
     * @param operation Operation type
     * @param frequency Frequency of execution
     * @param callback Callback function to execute
     * @param parameters Operation parameters
     * @return Task ID
     */
    std::string scheduleTask(const std::string& name,
                             OperationType operation,
                             ScheduleFrequency frequency,
                             TaskCallback callback,
                             const std::map<std::string, std::string>& parameters = {}) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        ScheduledTask task;
        task.id = "TASK-" + std::to_string(next_task_id_++);
        task.name = name;
        task.operation = operation;
        task.frequency = frequency;
        task.parameters = parameters;
        task.enabled = true;
        task.next_run = calculateNextRun(frequency, "");
        
        tasks_[task.id] = task;
        callbacks_[task.id] = callback;
        
        ++stats_.total_tasks;
        ++stats_.pending_tasks;
        
        cv_.notify_one();
        return task.id;
    }
    
    /**
     * @brief Schedule a task with CRON expression
     * @param name Task name
     * @param operation Operation type
     * @param cron_expr CRON expression (minute hour day month weekday)
     * @param callback Callback function
     * @return Task ID
     */
    std::string scheduleCron(const std::string& name,
                             OperationType operation,
                             const std::string& cron_expr,
                             TaskCallback callback) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        ScheduledTask task;
        task.id = "TASK-" + std::to_string(next_task_id_++);
        task.name = name;
        task.operation = operation;
        task.frequency = ScheduleFrequency::CUSTOM;
        task.cron_expression = cron_expr;
        task.enabled = true;
        task.next_run = calculateNextRun(ScheduleFrequency::CUSTOM, cron_expr);
        
        tasks_[task.id] = task;
        callbacks_[task.id] = callback;
        
        ++stats_.total_tasks;
        ++stats_.pending_tasks;
        
        cv_.notify_one();
        return task.id;
    }
    
    /**
     * @brief Schedule a one-time task
     */
    std::string scheduleOnce(const std::string& name,
                             OperationType operation,
                             std::chrono::system_clock::time_point run_time,
                             TaskCallback callback) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        ScheduledTask task;
        task.id = "TASK-" + std::to_string(next_task_id_++);
        task.name = name;
        task.operation = operation;
        task.frequency = ScheduleFrequency::ONCE;
        task.enabled = true;
        task.next_run = run_time;
        
        tasks_[task.id] = task;
        callbacks_[task.id] = callback;
        
        ++stats_.total_tasks;
        ++stats_.pending_tasks;
        
        cv_.notify_one();
        return task.id;
    }
    
    /**
     * @brief Cancel a scheduled task
     */
    bool cancelTask(const std::string& task_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = tasks_.find(task_id);
        if (it == tasks_.end()) return false;
        
        tasks_.erase(it);
        callbacks_.erase(task_id);
        --stats_.pending_tasks;
        
        return true;
    }
    
    /**
     * @brief Enable or disable a task
     */
    bool setTaskEnabled(const std::string& task_id, bool enabled) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = tasks_.find(task_id);
        if (it == tasks_.end()) return false;
        
        it->second.enabled = enabled;
        return true;
    }
    
    /**
     * @brief Get task information
     */
    std::optional<ScheduledTask> getTask(const std::string& task_id) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = tasks_.find(task_id);
        return it != tasks_.end() ? std::optional<ScheduledTask>(it->second) : std::nullopt;
    }
    
    /**
     * @brief Get all scheduled tasks
     */
    std::vector<ScheduledTask> getAllTasks() const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        std::vector<ScheduledTask> result;
        for (const auto& [id, task] : tasks_) {
            result.push_back(task);
        }
        return result;
    }
    
    /**
     * @brief Get tasks due for execution
     */
    std::vector<ScheduledTask> getDueTasks() const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto now = std::chrono::system_clock::now();
        std::vector<ScheduledTask> result;
        
        for (const auto& [id, task] : tasks_) {
            if (task.enabled && task.next_run <= now) {
                result.push_back(task);
            }
        }
        
        return result;
    }
    
    /**
     * @brief Manually trigger a task
     */
    bool triggerTask(const std::string& task_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto task_it = tasks_.find(task_id);
        auto cb_it = callbacks_.find(task_id);
        
        if (task_it == tasks_.end() || cb_it == callbacks_.end()) {
            return false;
        }
        
        bool success = cb_it->second(task_it->second);
        
        task_it->second.last_run = std::chrono::system_clock::now();
        ++task_it->second.run_count;
        
        if (success) {
            ++stats_.executed_tasks;
        } else {
            ++task_it->second.failure_count;
            ++stats_.failed_tasks;
        }
        
        return success;
    }
    
    /**
     * @brief Get scheduler statistics
     */
    SchedulerStatistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return stats_;
    }
    
    /**
     * @brief Generate scheduler report
     */
    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        
        oss << "=== Scheduler Statistics ===\n"
            << "Total Tasks: " << stats_.total_tasks << "\n"
            << "Executed: " << stats_.executed_tasks << "\n"
            << "Failed: " << stats_.failed_tasks << "\n"
            << "Skipped: " << stats_.skipped_tasks << "\n"
            << "Pending: " << stats_.pending_tasks << "\n\n"
            << "Scheduled Tasks:\n";
        
        for (const auto& [id, task] : tasks_) {
            oss << "  " << id << " (" << task.name << "): "
                << operationToString(task.operation)
                << (task.enabled ? " [ENABLED]" : " [DISABLED]")
                << " runs=" << task.run_count
                << " fails=" << task.failure_count << "\n";
        }
        
        return oss.str();
    }

private:
    void runScheduler() {
        while (running_) {
            std::unique_lock<std::mutex> lock(mtx_);
            
            // Find next task to run
            auto now = std::chrono::system_clock::now();
            std::chrono::system_clock::time_point next_wake = 
                now + std::chrono::minutes(1);
            
            for (auto& [id, task] : tasks_) {
                if (task.enabled && task.next_run <= now) {
                    // Execute task
                    auto cb_it = callbacks_.find(id);
                    if (cb_it != callbacks_.end()) {
                        lock.unlock();
                        
                        bool success = false;
                        try {
                            success = cb_it->second(task);
                        } catch (...) {
                            success = false;
                        }
                        
                        lock.lock();
                        
                        task.last_run = std::chrono::system_clock::now();
                        ++task.run_count;
                        
                        if (success) {
                            ++stats_.executed_tasks;
                        } else {
                            ++task.failure_count;
                            ++stats_.failed_tasks;
                        }
                        
                        EventSystem::instance().emit(
                            EventType::SCHEDULE_TRIGGERED,
                            "Scheduler",
                            "Executed task " + task.name +
                            (success ? " successfully" : " with failure"));
                        
                        // Calculate next run time
                        if (task.frequency == ScheduleFrequency::ONCE) {
                            task.enabled = false;
                            --stats_.pending_tasks;
                        } else {
                            task.next_run = calculateNextRun(task.frequency, 
                                                              task.cron_expression);
                        }
                    }
                }
                
                if (task.enabled && task.next_run < next_wake) {
                    next_wake = task.next_run;
                }
            }
            
            // Wait until next task or timeout
            cv_.wait_until(lock, next_wake, [this] { return !running_; });
        }
    }
    
    std::chrono::system_clock::time_point calculateNextRun(
            ScheduleFrequency freq, const std::string& cron_expr) {
        auto now = std::chrono::system_clock::now();
        
        switch (freq) {
            case ScheduleFrequency::HOURLY:
                return now + std::chrono::hours(1);
            case ScheduleFrequency::DAILY:
                return now + std::chrono::hours(24);
            case ScheduleFrequency::WEEKLY:
                return now + std::chrono::hours(24 * 7);
            case ScheduleFrequency::MONTHLY:
                return now + std::chrono::hours(24 * 30);
            case ScheduleFrequency::CUSTOM: {
                // Basic CRON expression parsing (minute field only for simplicity)
                // Format: "*/N" means every N minutes, "N" means at minute N each hour
                if (!cron_expr.empty()) {
                    if (cron_expr.substr(0, 2) == "*/") {
                        try {
                            int minutes = std::stoi(cron_expr.substr(2));
                            if (minutes > 0 && minutes <= 60) {
                                return now + std::chrono::minutes(minutes);
                            }
                        } catch (...) {
                            // Fall through to default
                        }
                    } else {
                        try {
                            int minutes = std::stoi(cron_expr);
                            if (minutes >= 0 && minutes < 60) {
                                return now + std::chrono::minutes(60 - minutes);
                            }
                        } catch (...) {
                            // Fall through to default
                        }
                    }
                }
                // Default to hourly if cron parsing fails
                return now + std::chrono::hours(1);
            }
            case ScheduleFrequency::ONCE:
            default:
                return now;
        }
    }
    
    std::atomic<bool> running_;
    std::atomic<uint64_t> next_task_id_;
    std::thread scheduler_thread_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    
    std::map<std::string, ScheduledTask> tasks_;
    std::map<std::string, TaskCallback> callbacks_;
    SchedulerStatistics stats_;
};

} // namespace dfsmshsm
#endif // DFSMSHSM_SCHEDULER_HPP
