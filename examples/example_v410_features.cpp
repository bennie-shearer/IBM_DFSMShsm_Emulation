/**
 * @file example_v410_features.cpp
 * @brief Demonstration of v4.1.0 features
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 *
 * Demonstrates:
 * - Binary Protocol for efficient CDS serialization
 * - Event Sourcing for complete audit trails
 * - Enhanced LRU Cache with bloom filters
 * - Command History with undo/redo support
 */

#include "hsm_engine.hpp"
#include "hsm_binary_protocol.hpp"
#include "hsm_event_sourcing.hpp"
#include "hsm_lru_cache.hpp"
#include "hsm_command_history.hpp"
#include <iostream>
#include <iomanip>

using namespace dfsmshsm;

//=============================================================================
// Binary Protocol Demo
//=============================================================================

void demo_binary_protocol() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Binary Protocol Demonstration\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    // Create a dataset
    DatasetInfo ds;
    ds.name = "USER.PROD.DATA001";
    ds.tier = StorageTier::PRIMARY;
    ds.state = DatasetState::ACTIVE;
    ds.size = 1024 * 1024 * 50;  // 50 MB
    ds.volume = "PRMVOL";
    ds.owner = "PRODUSER";
    ds.dsorg = "PS";
    ds.record_length = 80;
    ds.block_size = 27920;
    ds.compressed = true;
    ds.encrypted = true;
    ds.deduplicated = true;
    ds.tags = {"production", "critical", "financial"};
    
    std::cout << "Original Dataset:\n";
    std::cout << "  Name: " << ds.name << "\n";
    std::cout << "  Size: " << ds.size << " bytes\n";
    std::cout << "  Owner: " << ds.owner << "\n";
    std::cout << "  Tags: ";
    for (const auto& tag : ds.tags) std::cout << tag << " ";
    std::cout << "\n\n";
    
    // Serialize
    auto serialized = BinarySerializer::serializeDataset(ds);
    std::cout << "Serialized Size: " << serialized.size() << " bytes\n";
    
    // Calculate CRC32
    uint32_t checksum = CRC32::compute(serialized);
    std::cout << "CRC32 Checksum: 0x" << std::hex << checksum << std::dec << "\n\n";
    
    // Deserialize
    auto result = BinarySerializer::deserializeDataset(serialized);
    if (result) {
        std::cout << "Deserialized Successfully:\n";
        std::cout << "  Name: " << result->name << "\n";
        std::cout << "  Size: " << result->size << " bytes\n";
        std::cout << "  Owner: " << result->owner << "\n";
        std::cout << "  Compressed: " << (result->compressed ? "Yes" : "No") << "\n";
        std::cout << "  Encrypted: " << (result->encrypted ? "Yes" : "No") << "\n";
    }
    
    // Demonstrate varint encoding efficiency
    std::cout << "\n--- Varint Encoding Efficiency ---\n";
    BinaryWriter writer;
    
    // Small values: 1 byte
    writer.writeVarint(100);
    std::cout << "Value 100: " << writer.size() << " byte(s)\n";
    
    writer.clear();
    writer.writeVarint(10000);
    std::cout << "Value 10000: " << writer.size() << " byte(s)\n";
    
    writer.clear();
    writer.writeVarint(1000000);
    std::cout << "Value 1000000: " << writer.size() << " byte(s)\n";
    
    writer.clear();
    writer.writeVarint(0xFFFFFFFFFFFFFFFFULL);
    std::cout << "Value MAX_UINT64: " << writer.size() << " byte(s)\n";
}

//=============================================================================
// Event Sourcing Demo
//=============================================================================

void demo_event_sourcing() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Event Sourcing Demonstration\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    EventStore store;
    
    // Set up projections
    auto datasetCountProj = std::make_shared<DatasetCountProjection>();
    auto operationCountProj = std::make_shared<OperationCountProjection>();
    
    ProjectionManager manager(store);
    manager.registerProjection(datasetCountProj);
    manager.registerProjection(operationCountProj);
    
    std::cout << "--- Recording Events ---\n\n";
    
    // Simulate dataset lifecycle
    auto logEvent = [&](CDSEventType type, const std::string& name) {
        CDSEvent event(type, name, "dataset");
        event.user = "ADMIN";
        event.payload = EventPayloadBuilder()
            .set("tier", static_cast<int>(StorageTier::PRIMARY))
            .set("size", 1024 * 1024)
            .build();
        
        auto seq = store.append(event);
        std::cout << "Event #" << *seq << ": " << eventTypeToString(type) 
                  << " - " << name << "\n";
    };
    
    // Create some datasets
    logEvent(CDSEventType::DATASET_CREATED, "USER.DATA.FILE1");
    logEvent(CDSEventType::DATASET_CREATED, "USER.DATA.FILE2");
    logEvent(CDSEventType::DATASET_CREATED, "USER.DATA.FILE3");
    
    // Migrate one
    logEvent(CDSEventType::DATASET_MIGRATED, "USER.DATA.FILE1");
    
    // Back up some
    logEvent(CDSEventType::DATASET_BACKED_UP, "USER.DATA.FILE1");
    logEvent(CDSEventType::DATASET_BACKED_UP, "USER.DATA.FILE2");
    
    // Delete one
    logEvent(CDSEventType::DATASET_DELETED, "USER.DATA.FILE3");
    
    std::cout << "\n--- Event Store Statistics ---\n";
    std::cout << store.generateReport() << "\n";
    
    std::cout << "--- Projection Results ---\n";
    std::cout << "Total Datasets: " << datasetCountProj->totalDatasets() << "\n";
    std::cout << operationCountProj->generateReport() << "\n";
    
    // Query aggregate history
    std::cout << "--- History for USER.DATA.FILE1 ---\n";
    auto history = store.getAggregateEvents("dataset", "USER.DATA.FILE1");
    for (const auto& event : history) {
        std::cout << "  " << event.toString() << "\n";
    }
}

//=============================================================================
// LRU Cache Demo
//=============================================================================

void demo_lru_cache() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Enhanced LRU Cache Demonstration\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    // Configure cache
    EnhancedLRUCache<std::string, DatasetInfo>::Config config;
    config.max_entries = 5;
    config.use_bloom_filter = true;
    config.default_ttl = std::chrono::seconds(60);
    
    EnhancedLRUCache<std::string, DatasetInfo> cache(config);
    
    // Set size function
    cache.setSizeFunc([](const DatasetInfo& ds) {
        return sizeof(DatasetInfo) + ds.name.size() + ds.volume.size();
    });
    
    std::cout << "--- Adding Datasets to Cache ---\n";
    
    // Add some datasets
    for (int i = 1; i <= 7; ++i) {
        DatasetInfo ds;
        ds.name = "CACHE.TEST.DS" + std::to_string(i);
        ds.tier = StorageTier::PRIMARY;
        ds.size = 1024 * i;
        
        cache.put(ds.name, ds);
        std::cout << "Added: " << ds.name << " (cache size: " << cache.size() << ")\n";
    }
    
    std::cout << "\n--- Cache Statistics ---\n";
    std::cout << cache.generateReport();
    
    std::cout << "\n--- Testing Lookups ---\n";
    
    // Test hits and misses
    std::vector<std::string> lookups = {
        "CACHE.TEST.DS5", "CACHE.TEST.DS6", "CACHE.TEST.DS1",
        "CACHE.TEST.DS7", "NONEXISTENT.DS"
    };
    
    for (const auto& name : lookups) {
        auto result = cache.get(name);
        std::cout << name << ": " << (result ? "HIT" : "MISS") << "\n";
    }
    
    std::cout << "\n--- Final Statistics ---\n";
    const auto& stats = cache.statistics();
    std::cout << "Hit Rate: " << std::fixed << std::setprecision(1) 
              << (stats.hitRate() * 100) << "%\n";
    std::cout << "Bloom Filter FPR: " << (cache.bloomFilterFPR() * 100) << "%\n";
    std::cout << "Evictions: " << stats.evictions.load() << "\n";
}

//=============================================================================
// Command History Demo
//=============================================================================

void demo_command_history() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Command History with Undo/Redo Demonstration\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    CommandHistory history;
    
    // Simulated system state
    std::map<std::string, int> values;
    
    auto setValue = [&](const std::string& key, int newValue) {
        int oldValue = values[key];
        auto cmd = CommandFactory::createLambda(
            "SET_VALUE",
            "Set " + key + " to " + std::to_string(newValue),
            [&values, key, newValue]() -> Result<void> {
                values[key] = newValue;
                return Ok();
            },
            [&values, key, oldValue]() -> Result<void> {
                values[key] = oldValue;
                return Ok();
            }
        );
        (void)history.execute(std::move(cmd), "ADMIN");
    };
    
    std::cout << "--- Executing Commands ---\n";
    setValue("threshold", 50);
    std::cout << "Set threshold = 50 (current: " << values["threshold"] << ")\n";
    
    setValue("threshold", 75);
    std::cout << "Set threshold = 75 (current: " << values["threshold"] << ")\n";
    
    setValue("max_size", 1000);
    std::cout << "Set max_size = 1000 (current: " << values["max_size"] << ")\n";
    
    setValue("threshold", 90);
    std::cout << "Set threshold = 90 (current: " << values["threshold"] << ")\n";
    
    std::cout << "\n--- Current State ---\n";
    for (const auto& [k, v] : values) {
        std::cout << "  " << k << " = " << v << "\n";
    }
    
    std::cout << "\n--- Undo Operations ---\n";
    
    while (history.canUndo()) {
        auto desc = history.nextUndoDescription();
        (void)history.undo();
        std::cout << "Undid: " << (desc ? *desc : "unknown") << "\n";
        std::cout << "  threshold = " << values["threshold"] 
                  << ", max_size = " << values["max_size"] << "\n";
    }
    
    std::cout << "\n--- Redo Operations ---\n";
    
    // Redo two operations
    for (int i = 0; i < 2 && history.canRedo(); ++i) {
        auto desc = history.nextRedoDescription();
        (void)history.redo();
        std::cout << "Redid: " << (desc ? *desc : "unknown") << "\n";
        std::cout << "  threshold = " << values["threshold"] 
                  << ", max_size = " << values["max_size"] << "\n";
    }
    
    std::cout << "\n--- Command History Report ---\n";
    std::cout << history.generateReport();
}

//=============================================================================
// Transaction Demo
//=============================================================================

void demo_transactions() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Composite Command (Transaction) Demonstration\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    CommandHistory history;
    
    // Simulated dataset state
    struct DatasetState {
        std::string name;
        StorageTier tier = StorageTier::PRIMARY;
        bool encrypted = false;
        bool compressed = false;
    };
    
    DatasetState ds;
    ds.name = "TRANS.TEST.DATA";
    
    std::cout << "Initial State:\n";
    std::cout << "  Name: " << ds.name << "\n";
    std::cout << "  Tier: " << tierToString(ds.tier) << "\n";
    std::cout << "  Encrypted: " << (ds.encrypted ? "Yes" : "No") << "\n";
    std::cout << "  Compressed: " << (ds.compressed ? "Yes" : "No") << "\n";
    
    // Build a transaction
    std::cout << "\n--- Executing Transaction (Migrate + Encrypt + Compress) ---\n";
    
    StorageTier oldTier = ds.tier;
    bool wasEncrypted = ds.encrypted;
    bool wasCompressed = ds.compressed;
    
    auto transaction = TransactionBuilder("ARCHIVE_DATASET")
        .add("MIGRATE", "Migrate to ML1",
            [&ds]() -> Result<void> {
                ds.tier = StorageTier::MIGRATION_1;
                return Ok();
            },
            [&ds, oldTier]() -> Result<void> {
                ds.tier = oldTier;
                return Ok();
            })
        .add("ENCRYPT", "Enable encryption",
            [&ds]() -> Result<void> {
                ds.encrypted = true;
                return Ok();
            },
            [&ds, wasEncrypted]() -> Result<void> {
                ds.encrypted = wasEncrypted;
                return Ok();
            })
        .add("COMPRESS", "Enable compression",
            [&ds]() -> Result<void> {
                ds.compressed = true;
                return Ok();
            },
            [&ds, wasCompressed]() -> Result<void> {
                ds.compressed = wasCompressed;
                return Ok();
            })
        .build();
    
    (void)history.execute(transaction, "ADMIN");
    
    std::cout << "\nAfter Transaction:\n";
    std::cout << "  Name: " << ds.name << "\n";
    std::cout << "  Tier: " << tierToString(ds.tier) << "\n";
    std::cout << "  Encrypted: " << (ds.encrypted ? "Yes" : "No") << "\n";
    std::cout << "  Compressed: " << (ds.compressed ? "Yes" : "No") << "\n";
    
    // Undo entire transaction
    std::cout << "\n--- Undoing Entire Transaction ---\n";
    (void)history.undo();
    
    std::cout << "\nAfter Undo:\n";
    std::cout << "  Name: " << ds.name << "\n";
    std::cout << "  Tier: " << tierToString(ds.tier) << "\n";
    std::cout << "  Encrypted: " << (ds.encrypted ? "Yes" : "No") << "\n";
    std::cout << "  Compressed: " << (ds.compressed ? "Yes" : "No") << "\n";
}

//=============================================================================
// Integration Demo
//=============================================================================

void demo_integration() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Integrated Workflow Demonstration\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    // Create all components
    EventStore events;
    DatasetCache cache;
    CommandHistory history;
    
    // Set up projections
    auto opCountProj = std::make_shared<OperationCountProjection>();
    ProjectionManager projManager(events);
    projManager.registerProjection(opCountProj);
    
    std::cout << "--- Simulating Dataset Lifecycle ---\n\n";
    
    // Define dataset operations with full tracking
    auto createDataset = [&](const std::string& name, uint64_t size) {
        DatasetInfo ds;
        ds.name = name;
        ds.tier = StorageTier::PRIMARY;
        ds.size = size;
        
        auto cmd = CommandFactory::createLambda(
            "CREATE_DATASET", "Create " + name,
            [&cache, &events, ds]() -> Result<void> {
                cache.put(ds.name, ds);
                
                CDSEvent event(CDSEventType::DATASET_CREATED, ds.name, "dataset");
                event.payload = EventPayloadBuilder()
                    .set("size", ds.size)
                    .set("tier", static_cast<int>(ds.tier))
                    .build();
                (void)events.append(event);
                
                return Ok();
            },
            [&cache, &events, name=ds.name]() -> Result<void> {
                cache.remove(name);
                
                CDSEvent event(CDSEventType::DATASET_DELETED, name, "dataset");
                (void)events.append(event);
                
                return Ok();
            }
        );
        
        (void)history.execute(std::move(cmd), "ADMIN");
        std::cout << "Created: " << name << " (" << size << " bytes)\n";
    };
    
    // Create several datasets
    createDataset("PROD.DATA.FILE1", 1024 * 1024);
    createDataset("PROD.DATA.FILE2", 2048 * 1024);
    createDataset("PROD.DATA.FILE3", 512 * 1024);
    
    std::cout << "\n--- Current State ---\n";
    std::cout << "Cache Entries: " << cache.size() << "\n";
    std::cout << "Events Recorded: " << events.getEventCount() << "\n";
    std::cout << opCountProj->generateReport();
    
    std::cout << "\n--- Undo Last Creation ---\n";
    (void)history.undo();
    
    std::cout << "Cache Entries: " << cache.size() << "\n";
    std::cout << "Events Recorded: " << events.getEventCount() << "\n";
    
    std::cout << "\n--- Verify Cache Contents ---\n";
    for (const auto& key : cache.keys()) {
        auto ds = cache.get(key);
        if (ds) {
            std::cout << "  " << ds->name << " (size: " << ds->size << ")\n";
        }
    }
    
    std::cout << "\n--- Final Cache Statistics ---\n";
    std::cout << cache.generateReport();
}

//=============================================================================
// Main
//=============================================================================

int main() {
    std::cout << "========================================================\n";
    std::cout << "DFSMShsm v4.1.0 Features Demonstration\n";
    std::cout << "========================================================\n";
    
    try {
        demo_binary_protocol();
        demo_event_sourcing();
        demo_lru_cache();
        demo_command_history();
        demo_transactions();
        demo_integration();
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "All demonstrations completed successfully!\n";
        std::cout << std::string(60, '=') << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
