/**
 * @file event_system.hpp
 * @brief Observer pattern event system for HSM notifications
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_EVENT_SYSTEM_HPP
#define DFSMSHSM_EVENT_SYSTEM_HPP

#include "hsm_types.hpp"
#include <map>
#include <mutex>
#include <algorithm>
#include <atomic>

namespace dfsmshsm {

class EventSystem {
public:
    using SubscriberId = uint64_t;
    static EventSystem& instance() { static EventSystem s; return s; }

    SubscriberId subscribe(EventType type, EventCallback cb) {
        std::lock_guard<std::mutex> lock(mtx_);
        SubscriberId id = next_++;
        subs_[type].emplace_back(id, std::move(cb));
        return id;
    }

    SubscriberId subscribeAll(EventCallback cb) {
        std::lock_guard<std::mutex> lock(mtx_);
        SubscriberId id = next_++;
        global_.emplace_back(id, std::move(cb));
        return id;
    }

    void unsubscribe(SubscriberId id) {
        std::lock_guard<std::mutex> lock(mtx_);
        for (auto& [t, v] : subs_) v.erase(std::remove_if(v.begin(), v.end(), [id](const Sub& s){ return s.id == id; }), v.end());
        global_.erase(std::remove_if(global_.begin(), global_.end(), [id](const Sub& s){ return s.id == id; }), global_.end());
    }

    void publish(const Event& event) {
        std::vector<EventCallback> cbs;
        { std::lock_guard<std::mutex> lock(mtx_);
          events_.push_back(event); if (events_.size() > 1000) events_.erase(events_.begin());
          auto it = subs_.find(event.type);
          if (it != subs_.end()) for (const auto& s : it->second) cbs.push_back(s.cb);
          for (const auto& s : global_) cbs.push_back(s.cb);
        }
        for (const auto& cb : cbs) { try { cb(event); } catch (...) {} }
    }

    void emit(EventType type, const std::string& src, const std::string& msg, const std::string& ds = "", const std::string& vol = "") {
        Event e;
        e.type = type;
        e.source = src;
        e.message = msg;
        e.dataset_name = ds;
        e.volume_name = vol;
        e.timestamp = std::chrono::system_clock::now();
        publish(e);
    }

    std::vector<Event> getRecentEvents(size_t n = 100) const {
        std::lock_guard<std::mutex> lock(mtx_);
        if (n >= events_.size()) return events_;
        return std::vector<Event>(events_.end() - static_cast<std::ptrdiff_t>(n), events_.end());
    }

    void reset() { std::lock_guard<std::mutex> lock(mtx_); subs_.clear(); global_.clear(); events_.clear(); }

private:
    EventSystem() = default;
    struct Sub { SubscriberId id; EventCallback cb; Sub(SubscriberId i, EventCallback c) : id(i), cb(std::move(c)) {} };
    mutable std::mutex mtx_;
    std::map<EventType, std::vector<Sub>> subs_;
    std::vector<Sub> global_;
    std::vector<Event> events_;
    std::atomic<SubscriberId> next_{1};
};

inline std::string eventTypeToString(EventType t) {
    switch(t) {
        case EventType::DATASET_CREATED: return "DATASET_CREATED";
        case EventType::DATASET_DELETED: return "DATASET_DELETED";
        case EventType::DATASET_MIGRATED: return "DATASET_MIGRATED";
        case EventType::DATASET_RECALLED: return "DATASET_RECALLED";
        case EventType::DATASET_BACKED_UP: return "DATASET_BACKED_UP";
        case EventType::DATASET_ENCRYPTED: return "DATASET_ENCRYPTED";
        case EventType::DATASET_REPLICATED: return "DATASET_REPLICATED";
        case EventType::DATASET_SNAPSHOT: return "DATASET_SNAPSHOT";
        case EventType::DATASET_EXPIRED: return "DATASET_EXPIRED";
        case EventType::VOLUME_CREATED: return "VOLUME_CREATED";
        case EventType::VOLUME_DELETED: return "VOLUME_DELETED";
        case EventType::VOLUME_FULL: return "VOLUME_FULL";
        case EventType::SPACE_THRESHOLD_EXCEEDED: return "SPACE_THRESHOLD_EXCEEDED";
        case EventType::HEALTH_WARNING: return "HEALTH_WARNING";
        case EventType::HEALTH_CRITICAL: return "HEALTH_CRITICAL";
        case EventType::OPERATION_STARTED: return "OPERATION_STARTED";
        case EventType::OPERATION_COMPLETED: return "OPERATION_COMPLETED";
        case EventType::OPERATION_FAILED: return "OPERATION_FAILED";
        case EventType::SYSTEM_STARTUP: return "SYSTEM_STARTUP";
        case EventType::SYSTEM_SHUTDOWN: return "SYSTEM_SHUTDOWN";
        case EventType::QUOTA_WARNING: return "QUOTA_WARNING";
        case EventType::QUOTA_EXCEEDED: return "QUOTA_EXCEEDED";
        case EventType::JOURNAL_CHECKPOINT: return "JOURNAL_CHECKPOINT";
        case EventType::REPLICATION_SYNC: return "REPLICATION_SYNC";
        case EventType::SCHEDULE_TRIGGERED: return "SCHEDULE_TRIGGERED";
    }
    return "UNKNOWN";
}

} // namespace dfsmshsm
#endif
