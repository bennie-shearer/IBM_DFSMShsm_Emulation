/**
 * @file hsm_event_sourcing.hpp
 * @brief Event sourcing implementation for CDS operations
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 *
 * Implements event sourcing pattern for Control Data Set (CDS) operations,
 * providing complete audit trails, point-in-time recovery, and debugging
 * capabilities.
 *
 * Features:
 * - Immutable event log for all CDS operations
 * - Event replay for state reconstruction
 * - Snapshots for efficient recovery
 * - Event compaction for storage management
 * - Projections for derived state views
 */
#ifndef DFSMSHSM_HSM_EVENT_SOURCING_HPP
#define DFSMSHSM_HSM_EVENT_SOURCING_HPP

#include "hsm_types.hpp"
#include "hsm_binary_protocol.hpp"
#include "result.hpp"
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <queue>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <optional>
#include <any>
#include <fstream>

namespace dfsmshsm {

//=============================================================================
// Event Types
//=============================================================================

/**
 * @enum CDSEventType
 * @brief Types of events in the CDS event store
 */
enum class CDSEventType : uint16_t {
    // Dataset events
    DATASET_CREATED = 0x0100,
    DATASET_UPDATED = 0x0101,
    DATASET_DELETED = 0x0102,
    DATASET_MIGRATED = 0x0103,
    DATASET_RECALLED = 0x0104,
    DATASET_BACKED_UP = 0x0105,
    DATASET_RESTORED = 0x0106,
    DATASET_ENCRYPTED = 0x0107,
    DATASET_DECRYPTED = 0x0108,
    DATASET_COMPRESSED = 0x0109,
    DATASET_DECOMPRESSED = 0x010A,
    DATASET_REPLICATED = 0x010B,
    DATASET_EXPIRED = 0x010C,
    DATASET_ACCESSED = 0x010D,
    
    // Volume events
    VOLUME_CREATED = 0x0200,
    VOLUME_UPDATED = 0x0201,
    VOLUME_DELETED = 0x0202,
    VOLUME_ONLINE = 0x0203,
    VOLUME_OFFLINE = 0x0204,
    VOLUME_SPACE_UPDATED = 0x0205,
    
    // Policy events
    POLICY_CREATED = 0x0300,
    POLICY_UPDATED = 0x0301,
    POLICY_DELETED = 0x0302,
    POLICY_ASSIGNED = 0x0303,
    POLICY_UNASSIGNED = 0x0304,
    
    // Quota events
    QUOTA_SET = 0x0400,
    QUOTA_UPDATED = 0x0401,
    QUOTA_REMOVED = 0x0402,
    QUOTA_EXCEEDED = 0x0403,
    
    // System events
    SYSTEM_INITIALIZED = 0x0500,
    SYSTEM_SHUTDOWN = 0x0501,
    CHECKPOINT_CREATED = 0x0502,
    SNAPSHOT_CREATED = 0x0503,
    COMPACTION_COMPLETED = 0x0504,
    
    // Catalog events
    CATALOG_ENTRY_ADDED = 0x0600,
    CATALOG_ENTRY_REMOVED = 0x0601,
    CATALOG_ALIAS_CREATED = 0x0602,
    CATALOG_ALIAS_DELETED = 0x0603,
    
    // Custom/user events
    CUSTOM_EVENT = 0xFF00
};

/**
 * @brief Convert event type to string
 */
inline std::string eventTypeToString(CDSEventType type) {
    switch (type) {
        case CDSEventType::DATASET_CREATED: return "DATASET_CREATED";
        case CDSEventType::DATASET_UPDATED: return "DATASET_UPDATED";
        case CDSEventType::DATASET_DELETED: return "DATASET_DELETED";
        case CDSEventType::DATASET_MIGRATED: return "DATASET_MIGRATED";
        case CDSEventType::DATASET_RECALLED: return "DATASET_RECALLED";
        case CDSEventType::DATASET_BACKED_UP: return "DATASET_BACKED_UP";
        case CDSEventType::DATASET_RESTORED: return "DATASET_RESTORED";
        case CDSEventType::DATASET_ENCRYPTED: return "DATASET_ENCRYPTED";
        case CDSEventType::DATASET_DECRYPTED: return "DATASET_DECRYPTED";
        case CDSEventType::DATASET_COMPRESSED: return "DATASET_COMPRESSED";
        case CDSEventType::DATASET_DECOMPRESSED: return "DATASET_DECOMPRESSED";
        case CDSEventType::DATASET_REPLICATED: return "DATASET_REPLICATED";
        case CDSEventType::DATASET_EXPIRED: return "DATASET_EXPIRED";
        case CDSEventType::DATASET_ACCESSED: return "DATASET_ACCESSED";
        case CDSEventType::VOLUME_CREATED: return "VOLUME_CREATED";
        case CDSEventType::VOLUME_UPDATED: return "VOLUME_UPDATED";
        case CDSEventType::VOLUME_DELETED: return "VOLUME_DELETED";
        case CDSEventType::VOLUME_ONLINE: return "VOLUME_ONLINE";
        case CDSEventType::VOLUME_OFFLINE: return "VOLUME_OFFLINE";
        case CDSEventType::VOLUME_SPACE_UPDATED: return "VOLUME_SPACE_UPDATED";
        case CDSEventType::POLICY_CREATED: return "POLICY_CREATED";
        case CDSEventType::POLICY_UPDATED: return "POLICY_UPDATED";
        case CDSEventType::POLICY_DELETED: return "POLICY_DELETED";
        case CDSEventType::POLICY_ASSIGNED: return "POLICY_ASSIGNED";
        case CDSEventType::POLICY_UNASSIGNED: return "POLICY_UNASSIGNED";
        case CDSEventType::QUOTA_SET: return "QUOTA_SET";
        case CDSEventType::QUOTA_UPDATED: return "QUOTA_UPDATED";
        case CDSEventType::QUOTA_REMOVED: return "QUOTA_REMOVED";
        case CDSEventType::QUOTA_EXCEEDED: return "QUOTA_EXCEEDED";
        case CDSEventType::SYSTEM_INITIALIZED: return "SYSTEM_INITIALIZED";
        case CDSEventType::SYSTEM_SHUTDOWN: return "SYSTEM_SHUTDOWN";
        case CDSEventType::CHECKPOINT_CREATED: return "CHECKPOINT_CREATED";
        case CDSEventType::SNAPSHOT_CREATED: return "SNAPSHOT_CREATED";
        case CDSEventType::COMPACTION_COMPLETED: return "COMPACTION_COMPLETED";
        case CDSEventType::CATALOG_ENTRY_ADDED: return "CATALOG_ENTRY_ADDED";
        case CDSEventType::CATALOG_ENTRY_REMOVED: return "CATALOG_ENTRY_REMOVED";
        case CDSEventType::CATALOG_ALIAS_CREATED: return "CATALOG_ALIAS_CREATED";
        case CDSEventType::CATALOG_ALIAS_DELETED: return "CATALOG_ALIAS_DELETED";
        case CDSEventType::CUSTOM_EVENT: return "CUSTOM_EVENT";
        default: return "UNKNOWN_EVENT";
    }
}

//=============================================================================
// Event Structure
//=============================================================================

/**
 * @struct CDSEvent
 * @brief Represents a single event in the event store
 */
struct CDSEvent {
    uint64_t sequence_number = 0;           // Global ordering
    CDSEventType type = CDSEventType::CUSTOM_EVENT;
    std::chrono::system_clock::time_point timestamp;
    std::string aggregate_id;               // Entity ID (dataset name, volume name, etc.)
    std::string aggregate_type;             // "dataset", "volume", "policy", etc.
    uint32_t aggregate_version = 0;         // Version of the aggregate after this event
    std::string correlation_id;             // For tracing related events
    std::string causation_id;               // ID of event that caused this one
    std::string user;                       // User who triggered the event
    std::map<std::string, std::string> metadata;  // Additional metadata
    std::vector<uint8_t> payload;           // Event-specific data
    
    CDSEvent() : timestamp(std::chrono::system_clock::now()) {}
    
    CDSEvent(CDSEventType t, const std::string& aggId, const std::string& aggType)
        : type(t), timestamp(std::chrono::system_clock::now()),
          aggregate_id(aggId), aggregate_type(aggType) {}
    
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        oss << "[" << sequence_number << "] "
            << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
            << " " << eventTypeToString(type)
            << " " << aggregate_type << ":" << aggregate_id
            << " v" << aggregate_version;
        if (!user.empty()) oss << " by " << user;
        return oss.str();
    }
};

//=============================================================================
// Event Payload Helpers
//=============================================================================

/**
 * @class EventPayloadBuilder
 * @brief Helper for building event payloads
 */
class EventPayloadBuilder {
public:
    EventPayloadBuilder& set(const std::string& key, const std::string& value) {
        data_[key] = value;
        return *this;
    }
    
    EventPayloadBuilder& set(const std::string& key, int value) {
        data_[key] = std::to_string(value);
        return *this;
    }
    
    EventPayloadBuilder& set(const std::string& key, long value) {
        data_[key] = std::to_string(value);
        return *this;
    }
    
    EventPayloadBuilder& set(const std::string& key, long long value) {
        data_[key] = std::to_string(value);
        return *this;
    }
    
    EventPayloadBuilder& set(const std::string& key, unsigned long value) {
        data_[key] = std::to_string(value);
        return *this;
    }
    
    EventPayloadBuilder& set(const std::string& key, unsigned long long value) {
        data_[key] = std::to_string(value);
        return *this;
    }
    
    EventPayloadBuilder& set(const std::string& key, double value) {
        data_[key] = std::to_string(value);
        return *this;
    }
    
    EventPayloadBuilder& set(const std::string& key, bool value) {
        data_[key] = value ? "true" : "false";
        return *this;
    }
    
    [[nodiscard]] std::vector<uint8_t> build() const {
        BinaryWriter writer;
        writer.writeVarint(data_.size());
        for (const auto& [k, v] : data_) {
            writer.writeString(k);
            writer.writeString(v);
        }
        return writer.release();
    }
    
    [[nodiscard]] static std::map<std::string, std::string> parse(
            const std::vector<uint8_t>& payload) {
        std::map<std::string, std::string> result;
        if (payload.empty()) return result;
        
        BinaryReader reader(payload);
        auto count = reader.readVarint();
        if (!count) return result;
        
        for (size_t i = 0; i < *count; ++i) {
            auto key = reader.readString();
            auto value = reader.readString();
            if (key && value) {
                result[*key] = *value;
            }
        }
        return result;
    }

private:
    std::map<std::string, std::string> data_;
};

//=============================================================================
// Event Store
//=============================================================================

/**
 * @class EventStore
 * @brief Persistent event store for CDS operations
 */
class EventStore {
public:
    using EventHandler = std::function<void(const CDSEvent&)>;
    
    EventStore() : next_sequence_(1), snapshot_interval_(1000) {}
    
    /**
     * @brief Append an event to the store
     */
    [[nodiscard]] Result<uint64_t> append(CDSEvent event) {
        std::unique_lock lock(mutex_);
        
        event.sequence_number = next_sequence_++;
        
        // Update aggregate version
        auto& version = aggregate_versions_[event.aggregate_type + ":" + event.aggregate_id];
        event.aggregate_version = ++version;
        
        // Store event
        events_.push_back(event);
        
        // Index by aggregate
        aggregate_events_[event.aggregate_type + ":" + event.aggregate_id]
            .push_back(event.sequence_number);
        
        // Index by type
        type_events_[event.type].push_back(event.sequence_number);
        
        // Update statistics
        ++stats_.events_appended;
        stats_.total_events = events_.size();
        
        // Notify subscribers
        lock.unlock();
        notifySubscribers(event);
        
        // Check if snapshot needed
        if (events_.size() % snapshot_interval_ == 0) {
            createSnapshot();
        }
        
        return event.sequence_number;
    }
    
    /**
     * @brief Get event by sequence number
     */
    [[nodiscard]] std::optional<CDSEvent> getEvent(uint64_t sequence) const {
        std::shared_lock lock(mutex_);
        
        for (const auto& event : events_) {
            if (event.sequence_number == sequence) {
                return event;
            }
        }
        return std::nullopt;
    }
    
    /**
     * @brief Get all events for an aggregate
     */
    [[nodiscard]] std::vector<CDSEvent> getAggregateEvents(
            const std::string& aggregateType,
            const std::string& aggregateId) const {
        std::shared_lock lock(mutex_);
        
        std::vector<CDSEvent> result;
        auto key = aggregateType + ":" + aggregateId;
        auto it = aggregate_events_.find(key);
        
        if (it != aggregate_events_.end()) {
            for (uint64_t seq : it->second) {
                for (const auto& event : events_) {
                    if (event.sequence_number == seq) {
                        result.push_back(event);
                        break;
                    }
                }
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get events by type
     */
    [[nodiscard]] std::vector<CDSEvent> getEventsByType(CDSEventType type) const {
        std::shared_lock lock(mutex_);
        
        std::vector<CDSEvent> result;
        auto it = type_events_.find(type);
        
        if (it != type_events_.end()) {
            for (uint64_t seq : it->second) {
                for (const auto& event : events_) {
                    if (event.sequence_number == seq) {
                        result.push_back(event);
                        break;
                    }
                }
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get events in a time range
     */
    [[nodiscard]] std::vector<CDSEvent> getEventsInRange(
            const std::chrono::system_clock::time_point& start,
            const std::chrono::system_clock::time_point& end) const {
        std::shared_lock lock(mutex_);
        
        std::vector<CDSEvent> result;
        for (const auto& event : events_) {
            if (event.timestamp >= start && event.timestamp <= end) {
                result.push_back(event);
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get events after a sequence number
     */
    [[nodiscard]] std::vector<CDSEvent> getEventsAfter(uint64_t afterSequence) const {
        std::shared_lock lock(mutex_);
        
        std::vector<CDSEvent> result;
        for (const auto& event : events_) {
            if (event.sequence_number > afterSequence) {
                result.push_back(event);
            }
        }
        
        return result;
    }
    
    /**
     * @brief Subscribe to new events
     */
    uint64_t subscribe(EventHandler handler) {
        std::unique_lock lock(mutex_);
        uint64_t id = next_subscriber_id_++;
        subscribers_[id] = std::move(handler);
        return id;
    }
    
    /**
     * @brief Unsubscribe from events
     */
    void unsubscribe(uint64_t subscriberId) {
        std::unique_lock lock(mutex_);
        subscribers_.erase(subscriberId);
    }
    
    /**
     * @brief Get current version of an aggregate
     */
    [[nodiscard]] uint32_t getAggregateVersion(
            const std::string& aggregateType,
            const std::string& aggregateId) const {
        std::shared_lock lock(mutex_);
        
        auto key = aggregateType + ":" + aggregateId;
        auto it = aggregate_versions_.find(key);
        return it != aggregate_versions_.end() ? it->second : 0;
    }
    
    /**
     * @brief Get the last sequence number
     */
    [[nodiscard]] uint64_t getLastSequence() const {
        std::shared_lock lock(mutex_);
        return next_sequence_ - 1;
    }
    
    /**
     * @brief Get total event count
     */
    [[nodiscard]] size_t getEventCount() const {
        std::shared_lock lock(mutex_);
        return events_.size();
    }
    
    /**
     * @brief Create a snapshot of current state
     */
    void createSnapshot() {
        std::shared_lock lock(mutex_);
        
        Snapshot snapshot;
        snapshot.sequence_number = next_sequence_ - 1;
        snapshot.timestamp = std::chrono::system_clock::now();
        snapshot.aggregate_versions = aggregate_versions_;
        snapshot.event_count = events_.size();
        
        snapshots_.push_back(snapshot);
        ++stats_.snapshots_created;
    }
    
    /**
     * @brief Compact events up to a sequence number
     */
    [[nodiscard]] Result<size_t> compact(uint64_t upToSequence) {
        std::unique_lock lock(mutex_);
        
        // Find the latest snapshot before the sequence
        const Snapshot* latestSnapshot = nullptr;
        for (const auto& snap : snapshots_) {
            if (snap.sequence_number <= upToSequence) {
                latestSnapshot = &snap;
            }
        }
        
        if (!latestSnapshot) {
            return Err<size_t>(ErrorCode::NOT_FOUND, "No snapshot available for compaction");
        }
        
        // Remove events before the snapshot
        size_t removed = 0;
        auto it = events_.begin();
        while (it != events_.end() && it->sequence_number <= latestSnapshot->sequence_number) {
            it = events_.erase(it);
            ++removed;
        }
        
        // Update indices
        rebuildIndices();
        
        stats_.events_compacted += removed;
        ++stats_.compactions_performed;
        
        return removed;
    }
    
    /**
     * @brief Clear all events (for testing)
     */
    void clear() {
        std::unique_lock lock(mutex_);
        events_.clear();
        aggregate_events_.clear();
        type_events_.clear();
        aggregate_versions_.clear();
        snapshots_.clear();
        next_sequence_ = 1;
    }
    
    /**
     * @brief Get statistics
     */
    struct Statistics {
        std::atomic<uint64_t> events_appended{0};
        std::atomic<uint64_t> events_compacted{0};
        std::atomic<uint64_t> snapshots_created{0};
        std::atomic<uint64_t> compactions_performed{0};
        std::atomic<uint64_t> replays_performed{0};
        size_t total_events{0};
    };
    
    [[nodiscard]] const Statistics& getStatistics() const { return stats_; }
    
    /**
     * @brief Generate report
     */
    [[nodiscard]] std::string generateReport() const {
        std::shared_lock lock(mutex_);
        std::ostringstream oss;
        
        oss << "=== Event Store Statistics ===\n"
            << "Total Events: " << events_.size() << "\n"
            << "Last Sequence: " << (next_sequence_ - 1) << "\n"
            << "Aggregates: " << aggregate_versions_.size() << "\n"
            << "Snapshots: " << snapshots_.size() << "\n"
            << "Events Appended: " << stats_.events_appended.load() << "\n"
            << "Events Compacted: " << stats_.events_compacted.load() << "\n"
            << "Snapshots Created: " << stats_.snapshots_created.load() << "\n"
            << "Compactions: " << stats_.compactions_performed.load() << "\n"
            << "Subscribers: " << subscribers_.size() << "\n\n";
        
        // Event type distribution
        oss << "Event Type Distribution:\n";
        for (const auto& [type, seqs] : type_events_) {
            oss << "  " << eventTypeToString(type) << ": " << seqs.size() << "\n";
        }
        
        return oss.str();
    }

private:
    struct Snapshot {
        uint64_t sequence_number;
        std::chrono::system_clock::time_point timestamp;
        std::map<std::string, uint32_t> aggregate_versions;
        size_t event_count;
    };
    
    void notifySubscribers(const CDSEvent& event) {
        std::shared_lock lock(mutex_);
        for (const auto& [id, handler] : subscribers_) {
            try {
                handler(event);
            } catch (...) {
                // Ignore subscriber errors
            }
        }
    }
    
    void rebuildIndices() {
        aggregate_events_.clear();
        type_events_.clear();
        
        for (const auto& event : events_) {
            aggregate_events_[event.aggregate_type + ":" + event.aggregate_id]
                .push_back(event.sequence_number);
            type_events_[event.type].push_back(event.sequence_number);
        }
    }
    
    mutable std::shared_mutex mutex_;
    std::vector<CDSEvent> events_;
    std::map<std::string, std::vector<uint64_t>> aggregate_events_;  // type:id -> sequences
    std::map<CDSEventType, std::vector<uint64_t>> type_events_;
    std::map<std::string, uint32_t> aggregate_versions_;  // type:id -> version
    std::vector<Snapshot> snapshots_;
    std::map<uint64_t, EventHandler> subscribers_;
    uint64_t next_sequence_;
    uint64_t next_subscriber_id_ = 1;
    size_t snapshot_interval_;
    Statistics stats_;
};

//=============================================================================
// Event Replay
//=============================================================================

/**
 * @class EventReplayer
 * @brief Replays events to reconstruct state
 */
template<typename State>
class EventReplayer {
public:
    using ApplyFunction = std::function<void(State&, const CDSEvent&)>;
    
    EventReplayer(const EventStore& store) : store_(store) {}
    
    /**
     * @brief Register event handler for a type
     */
    void registerHandler(CDSEventType type, ApplyFunction handler) {
        handlers_[type] = std::move(handler);
    }
    
    /**
     * @brief Replay all events for an aggregate
     */
    [[nodiscard]] State replayAggregate(
            const std::string& aggregateType,
            const std::string& aggregateId,
            const State& initialState = State{}) {
        State state = initialState;
        
        auto events = store_.getAggregateEvents(aggregateType, aggregateId);
        for (const auto& event : events) {
            applyEvent(state, event);
        }
        
        return state;
    }
    
    /**
     * @brief Replay events up to a specific sequence
     */
    [[nodiscard]] State replayUpTo(
            const std::string& aggregateType,
            const std::string& aggregateId,
            uint64_t maxSequence,
            const State& initialState = State{}) {
        State state = initialState;
        
        auto events = store_.getAggregateEvents(aggregateType, aggregateId);
        for (const auto& event : events) {
            if (event.sequence_number > maxSequence) break;
            applyEvent(state, event);
        }
        
        return state;
    }
    
    /**
     * @brief Replay events up to a specific time
     */
    [[nodiscard]] State replayUpToTime(
            const std::string& aggregateType,
            const std::string& aggregateId,
            const std::chrono::system_clock::time_point& time,
            const State& initialState = State{}) {
        State state = initialState;
        
        auto events = store_.getAggregateEvents(aggregateType, aggregateId);
        for (const auto& event : events) {
            if (event.timestamp > time) break;
            applyEvent(state, event);
        }
        
        return state;
    }

private:
    void applyEvent(State& state, const CDSEvent& event) {
        auto it = handlers_.find(event.type);
        if (it != handlers_.end()) {
            it->second(state, event);
        }
    }
    
    const EventStore& store_;
    std::map<CDSEventType, ApplyFunction> handlers_;
};

//=============================================================================
// Projections
//=============================================================================

/**
 * @class Projection
 * @brief Base class for event projections
 */
class Projection {
public:
    virtual ~Projection() = default;
    
    virtual void apply(const CDSEvent& event) = 0;
    virtual void reset() = 0;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual uint64_t lastProcessedSequence() const = 0;
};

/**
 * @class DatasetCountProjection
 * @brief Tracks dataset counts per tier
 */
class DatasetCountProjection : public Projection {
public:
    void apply(const CDSEvent& event) override {
        if (event.type == CDSEventType::DATASET_CREATED) {
            ++total_datasets_;
            auto payload = EventPayloadBuilder::parse(event.payload);
            auto it = payload.find("tier");
            if (it != payload.end()) {
                ++datasets_per_tier_[it->second];
            }
        } else if (event.type == CDSEventType::DATASET_DELETED) {
            if (total_datasets_ > 0) --total_datasets_;
        } else if (event.type == CDSEventType::DATASET_MIGRATED) {
            auto payload = EventPayloadBuilder::parse(event.payload);
            auto from = payload.find("from_tier");
            auto to = payload.find("to_tier");
            if (from != payload.end() && datasets_per_tier_[from->second] > 0) {
                --datasets_per_tier_[from->second];
            }
            if (to != payload.end()) {
                ++datasets_per_tier_[to->second];
            }
        }
        last_sequence_ = event.sequence_number;
    }
    
    void reset() override {
        total_datasets_ = 0;
        datasets_per_tier_.clear();
        last_sequence_ = 0;
    }
    
    [[nodiscard]] std::string name() const override { return "DatasetCountProjection"; }
    [[nodiscard]] uint64_t lastProcessedSequence() const override { return last_sequence_; }
    
    [[nodiscard]] uint64_t totalDatasets() const { return total_datasets_; }
    [[nodiscard]] const std::map<std::string, uint64_t>& datasetsPerTier() const {
        return datasets_per_tier_;
    }

private:
    uint64_t total_datasets_ = 0;
    std::map<std::string, uint64_t> datasets_per_tier_;
    uint64_t last_sequence_ = 0;
};

/**
 * @class StorageUsageProjection
 * @brief Tracks storage usage over time
 */
class StorageUsageProjection : public Projection {
public:
    void apply(const CDSEvent& event) override {
        auto payload = EventPayloadBuilder::parse(event.payload);
        
        if (event.type == CDSEventType::DATASET_CREATED ||
            event.type == CDSEventType::VOLUME_SPACE_UPDATED) {
            auto sizeIt = payload.find("size");
            if (sizeIt != payload.end()) {
                total_bytes_ += std::stoull(sizeIt->second);
            }
        } else if (event.type == CDSEventType::DATASET_DELETED) {
            auto sizeIt = payload.find("size");
            if (sizeIt != payload.end()) {
                uint64_t size = std::stoull(sizeIt->second);
                if (total_bytes_ >= size) total_bytes_ -= size;
            }
        }
        
        last_sequence_ = event.sequence_number;
    }
    
    void reset() override {
        total_bytes_ = 0;
        last_sequence_ = 0;
    }
    
    [[nodiscard]] std::string name() const override { return "StorageUsageProjection"; }
    [[nodiscard]] uint64_t lastProcessedSequence() const override { return last_sequence_; }
    
    [[nodiscard]] uint64_t totalBytes() const { return total_bytes_; }

private:
    uint64_t total_bytes_ = 0;
    uint64_t last_sequence_ = 0;
};

/**
 * @class OperationCountProjection
 * @brief Tracks operation counts by type
 */
class OperationCountProjection : public Projection {
public:
    void apply(const CDSEvent& event) override {
        ++operations_by_type_[event.type];
        ++total_operations_;
        last_sequence_ = event.sequence_number;
    }
    
    void reset() override {
        operations_by_type_.clear();
        total_operations_ = 0;
        last_sequence_ = 0;
    }
    
    [[nodiscard]] std::string name() const override { return "OperationCountProjection"; }
    [[nodiscard]] uint64_t lastProcessedSequence() const override { return last_sequence_; }
    
    [[nodiscard]] uint64_t totalOperations() const { return total_operations_; }
    [[nodiscard]] uint64_t getCount(CDSEventType type) const {
        auto it = operations_by_type_.find(type);
        return it != operations_by_type_.end() ? it->second : 0;
    }
    
    [[nodiscard]] std::string generateReport() const {
        std::ostringstream oss;
        oss << "=== Operation Counts ===\n"
            << "Total: " << total_operations_ << "\n";
        for (const auto& [type, count] : operations_by_type_) {
            oss << "  " << eventTypeToString(type) << ": " << count << "\n";
        }
        return oss.str();
    }

private:
    std::map<CDSEventType, uint64_t> operations_by_type_;
    uint64_t total_operations_ = 0;
    uint64_t last_sequence_ = 0;
};

//=============================================================================
// Projection Manager
//=============================================================================

/**
 * @class ProjectionManager
 * @brief Manages multiple projections
 */
class ProjectionManager {
public:
    explicit ProjectionManager(EventStore& store) : store_(store) {
        // Subscribe to new events
        subscription_id_ = store_.subscribe([this](const CDSEvent& event) {
            applyToAll(event);
        });
    }
    
    ~ProjectionManager() {
        store_.unsubscribe(subscription_id_);
    }
    
    /**
     * @brief Register a projection
     */
    void registerProjection(std::shared_ptr<Projection> projection) {
        std::unique_lock lock(mutex_);
        projections_[projection->name()] = std::move(projection);
    }
    
    /**
     * @brief Get a projection by name
     */
    template<typename T>
    [[nodiscard]] std::shared_ptr<T> getProjection(const std::string& name) {
        std::shared_lock lock(mutex_);
        auto it = projections_.find(name);
        if (it != projections_.end()) {
            return std::dynamic_pointer_cast<T>(it->second);
        }
        return nullptr;
    }
    
    /**
     * @brief Rebuild all projections from events
     */
    void rebuildAll() {
        std::unique_lock lock(mutex_);
        
        // Reset all projections
        for (auto& [name, projection] : projections_) {
            projection->reset();
        }
        
        // Replay all events
        auto events = store_.getEventsAfter(0);
        for (const auto& event : events) {
            for (auto& [name, projection] : projections_) {
                projection->apply(event);
            }
        }
    }
    
    /**
     * @brief Catch up projections to current state
     */
    void catchUp() {
        std::unique_lock lock(mutex_);
        
        for (auto& [name, projection] : projections_) {
            auto events = store_.getEventsAfter(projection->lastProcessedSequence());
            for (const auto& event : events) {
                projection->apply(event);
            }
        }
    }

private:
    void applyToAll(const CDSEvent& event) {
        std::shared_lock lock(mutex_);
        for (auto& [name, projection] : projections_) {
            projection->apply(event);
        }
    }
    
    EventStore& store_;
    uint64_t subscription_id_;
    mutable std::shared_mutex mutex_;
    std::map<std::string, std::shared_ptr<Projection>> projections_;
};

//=============================================================================
// Event Sourced Repository
//=============================================================================

/**
 * @class EventSourcedDatasetRepository
 * @brief Repository for datasets using event sourcing
 */
class EventSourcedDatasetRepository {
public:
    explicit EventSourcedDatasetRepository(EventStore& store) : store_(store) {}
    
    /**
     * @brief Create a new dataset
     */
    [[nodiscard]] Result<void> create(const DatasetInfo& dataset) {
        CDSEvent event(CDSEventType::DATASET_CREATED, dataset.name, "dataset");
        event.payload = EventPayloadBuilder()
            .set("tier", static_cast<int>(dataset.tier))
            .set("size", dataset.size)
            .set("owner", dataset.owner)
            .set("volume", dataset.volume)
            .build();
        
        auto result = store_.append(std::move(event));
        if (!result) return Err<void>(result.error());
        
        // Cache the dataset
        datasets_[dataset.name] = dataset;
        return Ok();
    }
    
    /**
     * @brief Update a dataset
     */
    [[nodiscard]] Result<void> update(const std::string& name,
                                       const std::map<std::string, std::string>& changes) {
        auto it = datasets_.find(name);
        if (it == datasets_.end()) {
            return Err<void>(ErrorCode::NOT_FOUND, "Dataset not found: " + name);
        }
        
        CDSEvent event(CDSEventType::DATASET_UPDATED, name, "dataset");
        EventPayloadBuilder builder;
        for (const auto& [k, v] : changes) {
            builder.set(k, v);
        }
        event.payload = builder.build();
        
        auto result = store_.append(std::move(event));
        if (!result) return Err<void>(result.error());
        
        return Ok();
    }
    
    /**
     * @brief Delete a dataset
     */
    [[nodiscard]] Result<void> remove(const std::string& name) {
        auto it = datasets_.find(name);
        if (it == datasets_.end()) {
            return Err<void>(ErrorCode::NOT_FOUND, "Dataset not found: " + name);
        }
        
        CDSEvent event(CDSEventType::DATASET_DELETED, name, "dataset");
        event.payload = EventPayloadBuilder()
            .set("size", it->second.size)
            .build();
        
        auto result = store_.append(std::move(event));
        if (!result) return Err<void>(result.error());
        
        datasets_.erase(it);
        return Ok();
    }
    
    /**
     * @brief Record dataset migration
     */
    [[nodiscard]] Result<void> recordMigration(const std::string& name,
                                                StorageTier fromTier,
                                                StorageTier toTier) {
        CDSEvent event(CDSEventType::DATASET_MIGRATED, name, "dataset");
        event.payload = EventPayloadBuilder()
            .set("from_tier", static_cast<int>(fromTier))
            .set("to_tier", static_cast<int>(toTier))
            .build();
        
        auto result = store_.append(std::move(event));
        if (!result) return Err<void>(result.error());
        
        return Ok();
    }
    
    /**
     * @brief Get dataset (from cache or replay)
     */
    [[nodiscard]] std::optional<DatasetInfo> get(const std::string& name) const {
        auto it = datasets_.find(name);
        if (it != datasets_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Get dataset history
     */
    [[nodiscard]] std::vector<CDSEvent> getHistory(const std::string& name) const {
        return store_.getAggregateEvents("dataset", name);
    }
    
    /**
     * @brief Rebuild cache from events
     */
    void rebuildFromEvents() {
        datasets_.clear();
        
        // Get all dataset events
        auto createdEvents = store_.getEventsByType(CDSEventType::DATASET_CREATED);
        auto deletedEvents = store_.getEventsByType(CDSEventType::DATASET_DELETED);
        
        // Build set of deleted datasets
        std::set<std::string> deleted;
        for (const auto& event : deletedEvents) {
            deleted.insert(event.aggregate_id);
        }
        
        // Reconstruct current datasets
        for (const auto& event : createdEvents) {
            if (deleted.find(event.aggregate_id) == deleted.end()) {
                DatasetInfo ds;
                ds.name = event.aggregate_id;
                
                auto payload = EventPayloadBuilder::parse(event.payload);
                if (auto it = payload.find("tier"); it != payload.end()) {
                    ds.tier = static_cast<StorageTier>(std::stoi(it->second));
                }
                if (auto it = payload.find("size"); it != payload.end()) {
                    ds.size = std::stoull(it->second);
                }
                if (auto it = payload.find("owner"); it != payload.end()) {
                    ds.owner = it->second;
                }
                if (auto it = payload.find("volume"); it != payload.end()) {
                    ds.volume = it->second;
                }
                
                datasets_[ds.name] = ds;
            }
        }
    }

private:
    EventStore& store_;
    std::map<std::string, DatasetInfo> datasets_;
};

} // namespace dfsmshsm
#endif // DFSMSHSM_HSM_EVENT_SOURCING_HPP
