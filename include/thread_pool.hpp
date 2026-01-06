/**
 * @file thread_pool.hpp
 * @brief C++20 thread pool for concurrent HSM operations
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_THREAD_POOL_HPP
#define DFSMSHSM_THREAD_POOL_HPP

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>

namespace dfsmshsm {

class ThreadPool {
public:
    explicit ThreadPool(size_t n = 0) : stop_(false), active_(0) {
        if (n == 0) { n = std::thread::hardware_concurrency(); if (n == 0) n = 4; }
        for (size_t i = 0; i < n; ++i) workers_.emplace_back([this]{ workerLoop(); });
    }
    ~ThreadPool() { shutdown(); }
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<R> result = task->get_future();
        { std::lock_guard<std::mutex> lock(mtx_); if (stop_) throw std::runtime_error("Pool stopped"); tasks_.emplace([task]{ (*task)(); }); }
        cv_.notify_one();
        return result;
    }

    void waitAll() { std::unique_lock<std::mutex> lock(mtx_); done_.wait(lock, [this]{ return tasks_.empty() && active_ == 0; }); }
    void shutdown(bool wait = true) {
        if (wait) waitAll();
        { std::lock_guard<std::mutex> lock(mtx_); stop_ = true; }
        cv_.notify_all();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }
    size_t threadCount() const { return workers_.size(); }
    size_t pendingTasks() const { std::lock_guard<std::mutex> lock(mtx_); return tasks_.size(); }
    size_t activeTasks() const { return active_; }

private:
    void workerLoop() {
        while (true) {
            std::function<void()> task;
            { std::unique_lock<std::mutex> lock(mtx_); cv_.wait(lock, [this]{ return stop_ || !tasks_.empty(); });
              if (stop_ && tasks_.empty()) return;
              task = std::move(tasks_.front()); tasks_.pop(); ++active_; }
            task();
            { std::lock_guard<std::mutex> lock(mtx_); --active_; }
            done_.notify_all();
        }
    }
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mtx_;
    std::condition_variable cv_, done_;
    std::atomic<bool> stop_;
    std::atomic<size_t> active_;
};

} // namespace dfsmshsm
#endif
