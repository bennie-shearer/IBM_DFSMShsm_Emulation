/**
 * @file test_extended.cpp
 * @brief Extended test suite for DFSMShsm v4.1.0
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */

#include "test_framework.hpp"
#include "hsm_types.hpp"
#include "result.hpp"
#include "config_validator.hpp"
#include "hsm_config.hpp"
#include "command_processor.hpp"
#include "lifecycle_manager.hpp"
#include "dataset_catalog.hpp"
#include "migration_manager.hpp"
#include "hsm_engine.hpp"

#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <limits>

using namespace dfsmshsm;
using namespace dfsmshsm::testing;

// Result<T,E> Tests
TEST_CASE(Result_Success) {
    Result<int> r = Ok(42);
    REQUIRE(r.isOk());
    REQUIRE(!r.isError());
    REQUIRE_EQ(r.value(), 42);
}

TEST_CASE(Result_Error) {
    Result<int> r = Err<int>(ErrorCode::NOT_FOUND, "Item missing");
    REQUIRE(!r.isOk());
    REQUIRE(r.isError());
    REQUIRE(r.error().is(ErrorCode::NOT_FOUND));
}

TEST_CASE(Result_VoidSuccess) {
    Result<void> r = Ok();
    REQUIRE(r.isOk());
}

TEST_CASE(Result_VoidError) {
    Result<void> r = Err(ErrorCode::PERMISSION_DENIED, "Access denied");
    REQUIRE(r.isError());
}

TEST_CASE(Result_ValueOr) {
    Result<int> success = Ok(10);
    Result<int> failure = Err<int>(ErrorCode::NOT_FOUND);
    REQUIRE_EQ(success.valueOr(0), 10);
    REQUIRE_EQ(failure.valueOr(99), 99);
}

TEST_CASE(Result_Map) {
    Result<int> r = Ok(5);
    auto doubled = r.map([](int x) { return x * 2; });
    REQUIRE(doubled.isOk());
    REQUIRE_EQ(doubled.value(), 10);
}

TEST_CASE(Result_OnSuccess) {
    int captured = 0;
    Result<int> r = Ok(42);
    r.onSuccess([&captured](int v) { captured = v; });
    REQUIRE_EQ(captured, 42);
}

TEST_CASE(Result_OnError) {
    ErrorCode captured = ErrorCode::SUCCESS;
    Result<int> r = Err<int>(ErrorCode::QUOTA_EXCEEDED);
    r.onError([&captured](const Error& e) { captured = e.code; });
    REQUIRE(captured == ErrorCode::QUOTA_EXCEEDED);
}

TEST_CASE(Error_CategoryChecks) {
    Error resourceErr{ErrorCode::NOT_FOUND};
    Error storageErr{ErrorCode::STORAGE_FULL};
    Error opErr{ErrorCode::MIGRATION_FAILED};
    Error cfgErr{ErrorCode::CONFIG_INVALID};
    Error sysErr{ErrorCode::SYSTEM_ERROR};
    
    REQUIRE(resourceErr.isResourceError());
    REQUIRE(storageErr.isStorageError());
    REQUIRE(opErr.isOperationError());
    REQUIRE(cfgErr.isConfigError());
    REQUIRE(sysErr.isSystemError());
}

// Config Tests
TEST_CASE(Config_SetAndGet) {
    auto& cfg = HSMConfig::instance();
    cfg.clear();
    cfg.set("test.key", "test_value");
    REQUIRE_EQ(cfg.get("test.key"), "test_value");
}

TEST_CASE(Config_TypedGetters) {
    auto& cfg = HSMConfig::instance();
    cfg.clear();
    cfg.set("int.val", "42");
    cfg.set("bool.true", "true");
    REQUIRE_EQ(cfg.getInt("int.val"), 42);
    REQUIRE(cfg.getBool("bool.true"));
}

TEST_CASE(ConfigValidator_Valid) {
    auto& cfg = HSMConfig::instance();
    cfg.clear();
    cfg.set("migration.age_days", "30");
    auto result = ConfigValidator::validate(cfg);
    REQUIRE(result.valid);
}

TEST_CASE(ConfigValidator_Invalid) {
    auto& cfg = HSMConfig::instance();
    cfg.clear();
    cfg.set("migration.age_days", "5000");
    auto result = ConfigValidator::validate(cfg);
    REQUIRE(!result.valid);
}

// Lifecycle Tests
TEST_CASE(Lifecycle_CreatePolicy) {
    LifecycleManager mgr;
    mgr.start();
    LifecyclePolicy policy;
    policy.name = "DEFAULT";
    policy.tier_down_days = 30;
    REQUIRE(mgr.createPolicy(policy));
    REQUIRE(!mgr.createPolicy(policy));
    mgr.stop();
}

TEST_CASE(Lifecycle_AssignPolicy) {
    LifecycleManager mgr;
    mgr.start();
    LifecyclePolicy policy;
    policy.name = "ASSIGNED";
    mgr.createPolicy(policy);
    mgr.assignPolicy("PROD.DATA", "ASSIGNED");
    auto assigned = mgr.getAssignedPolicy("PROD.DATA");
    REQUIRE(assigned.has_value());
    mgr.stop();
}

TEST_CASE(Lifecycle_Statistics) {
    LifecycleManager mgr;
    mgr.start();
    mgr.recordAction(OperationType::OP_MIGRATE, 1024);
    mgr.recordAction(OperationType::OP_MIGRATE, 2048);
    auto stats = mgr.getStatistics();
    REQUIRE_EQ(stats.tier_transitions, 2ULL);
    mgr.stop();
}

// Command Processor Tests
TEST_CASE(Command_Help) {
    std::filesystem::path test_dir = "/tmp/hsm_cmd_test";
    HSMEngine engine;
    engine.initialize(test_dir);
    engine.start();
    CommandProcessor proc(&engine);
    std::string help = proc.getHelp();
    REQUIRE(help.find("MIGRATE") != std::string::npos);
    engine.stop();
    std::filesystem::remove_all(test_dir);
}

TEST_CASE(Command_Unknown) {
    std::filesystem::path test_dir = "/tmp/hsm_cmd_test2";
    HSMEngine engine;
    engine.initialize(test_dir);
    engine.start();
    CommandProcessor proc(&engine);
    std::string result = proc.process("FOOBAR");
    REQUIRE(result.find("Unknown") != std::string::npos);
    engine.stop();
    std::filesystem::remove_all(test_dir);
}

// Edge Cases
TEST_CASE(Edge_ZeroSizeDataset) {
    DatasetCatalog catalog;
    DatasetInfo ds;
    ds.name = "ZERO.SIZE";
    ds.size = 0;
    REQUIRE(catalog.addDataset(ds));
}

TEST_CASE(Edge_MaxSizeDataset) {
    DatasetCatalog catalog;
    DatasetInfo ds;
    ds.name = "MAX.SIZE";
    ds.size = std::numeric_limits<uint64_t>::max();
    REQUIRE(catalog.addDataset(ds));
}

TEST_CASE(Stress_ConcurrentCatalog) {
    DatasetCatalog catalog;
    std::atomic<int> successes{0};
    std::vector<std::thread> threads;
    
    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([&catalog, &successes, t]() {
            for (int i = 0; i < 100; ++i) {
                DatasetInfo ds;
                ds.name = "DS." + std::to_string(t) + "." + std::to_string(i);
                ds.size = 1024;
                if (catalog.addDataset(ds)) successes++;
            }
        });
    }
    
    for (auto& th : threads) th.join();
    REQUIRE_EQ(successes.load(), 1000);
}

int main(int argc, char* argv[]) {
    std::string filter;
    if (argc > 1) filter = argv[1];
    return TestRunner::instance().run(filter);
}
