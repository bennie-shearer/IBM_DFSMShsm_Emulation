/**
 * @file test_framework.hpp
 * @brief Enhanced test framework with comprehensive assertions
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 * 
 * Provides a lightweight test framework for unit and integration testing.
 * Supports test suites, fixtures, parameterized tests, and detailed reporting.
 */
#ifndef DFSMSHSM_TEST_FRAMEWORK_HPP
#define DFSMSHSM_TEST_FRAMEWORK_HPP

#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <map>
#include <memory>
#include <cmath>

namespace dfsmshsm { namespace testing {

//=============================================================================
// Test Assertions
//=============================================================================

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        throw std::runtime_error(std::string("REQUIRE failed: ") + #expr + \
            " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
    } \
} while(0)

#define REQUIRE_EQ(a, b) do { \
    if (!((a) == (b))) { \
        std::ostringstream oss; \
        oss << "REQUIRE_EQ failed: " << #a << " (" << (a) << ") != " << #b << " (" << (b) << ")"; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

#define REQUIRE_NE(a, b) do { \
    if (!((a) != (b))) { \
        throw std::runtime_error(std::string("REQUIRE_NE failed: ") + #a + " == " + #b); \
    } \
} while(0)

#define REQUIRE_GT(a, b) do { \
    if (!((a) > (b))) { \
        std::ostringstream oss; \
        oss << "REQUIRE_GT failed: " << #a << " (" << (a) << ") <= " << #b << " (" << (b) << ")"; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

#define REQUIRE_GE(a, b) do { \
    if (!((a) >= (b))) { \
        std::ostringstream oss; \
        oss << "REQUIRE_GE failed: " << #a << " (" << (a) << ") < " << #b << " (" << (b) << ")"; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

#define REQUIRE_LT(a, b) do { \
    if (!((a) < (b))) { \
        std::ostringstream oss; \
        oss << "REQUIRE_LT failed: " << #a << " (" << (a) << ") >= " << #b << " (" << (b) << ")"; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

#define REQUIRE_LE(a, b) do { \
    if (!((a) <= (b))) { \
        std::ostringstream oss; \
        oss << "REQUIRE_LE failed: " << #a << " (" << (a) << ") > " << #b << " (" << (b) << ")"; \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

#define REQUIRE_NEAR(a, b, epsilon) do { \
    if (std::abs((a) - (b)) > (epsilon)) { \
        std::ostringstream oss; \
        oss << "REQUIRE_NEAR failed: |" << (a) << " - " << (b) << "| > " << (epsilon); \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

#define REQUIRE_TRUE(expr) REQUIRE(expr)
#define REQUIRE_FALSE(expr) REQUIRE(!(expr))

#define REQUIRE_NULL(ptr) do { \
    if ((ptr) != nullptr) { \
        throw std::runtime_error(std::string("REQUIRE_NULL failed: ") + #ptr + " is not null"); \
    } \
} while(0)

#define REQUIRE_NOT_NULL(ptr) do { \
    if ((ptr) == nullptr) { \
        throw std::runtime_error(std::string("REQUIRE_NOT_NULL failed: ") + #ptr + " is null"); \
    } \
} while(0)

#define REQUIRE_THROWS(expr) do { \
    bool caught = false; \
    try { expr; } catch (...) { caught = true; } \
    if (!caught) throw std::runtime_error("REQUIRE_THROWS failed: no exception thrown"); \
} while(0)

#define REQUIRE_THROWS_AS(expr, exception_type) do { \
    bool caught = false; \
    try { expr; } catch (const exception_type&) { caught = true; } catch (...) {} \
    if (!caught) throw std::runtime_error("REQUIRE_THROWS_AS failed: wrong exception type"); \
} while(0)

#define REQUIRE_NOTHROW(expr) do { \
    try { expr; } catch (const std::exception& e) { \
        throw std::runtime_error(std::string("REQUIRE_NOTHROW failed: ") + e.what()); \
    } \
} while(0)

#define REQUIRE_CONTAINS(str, substr) do { \
    if (std::string(str).find(substr) == std::string::npos) { \
        throw std::runtime_error(std::string("REQUIRE_CONTAINS failed: '") + \
            std::string(str) + "' does not contain '" + std::string(substr) + "'"); \
    } \
} while(0)

#define REQUIRE_EMPTY(container) do { \
    if (!(container).empty()) { \
        throw std::runtime_error(std::string("REQUIRE_EMPTY failed: ") + #container + " is not empty"); \
    } \
} while(0)

#define REQUIRE_NOT_EMPTY(container) do { \
    if ((container).empty()) { \
        throw std::runtime_error(std::string("REQUIRE_NOT_EMPTY failed: ") + #container + " is empty"); \
    } \
} while(0)

#define REQUIRE_SIZE(container, expected) do { \
    if ((container).size() != (expected)) { \
        std::ostringstream oss; \
        oss << "REQUIRE_SIZE failed: " << #container << ".size() = " << (container).size() \
            << ", expected " << (expected); \
        throw std::runtime_error(oss.str()); \
    } \
} while(0)

// Soft assertions (warnings)
#define CHECK(expr) do { \
    if (!(expr)) { \
        std::cerr << "CHECK warning: " << #expr << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
    } \
} while(0)

//=============================================================================
// Test Structures
//=============================================================================

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> func;
    std::vector<std::string> tags;
};

struct TestResult {
    std::string suite;
    std::string name;
    std::string error;
    bool passed = false;
    double duration_ms = 0.0;
};

struct TestSuiteResult {
    std::string name;
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    double total_duration_ms = 0.0;
    std::vector<TestResult> results;
};

//=============================================================================
// Test Runner
//=============================================================================

class TestRunner {
public:
    static TestRunner& instance() {
        static TestRunner runner;
        return runner;
    }
    
    void addTest(const std::string& name, std::function<void()> func,
                 const std::string& suite = "Default",
                 const std::vector<std::string>& tags = {}) {
        tests_.push_back({suite, name, std::move(func), tags});
    }
    
    int run(const std::string& filter = "", const std::vector<std::string>& tag_filter = {}) {
        printHeader();
        
        std::map<std::string, TestSuiteResult> suites;
        int total_passed = 0, total_failed = 0, total_skipped = 0;
        
        for (const auto& test : tests_) {
            // Apply filters
            if (!filter.empty() && test.name.find(filter) == std::string::npos &&
                test.suite.find(filter) == std::string::npos) {
                continue;
            }
            
            if (!tag_filter.empty()) {
                bool has_tag = false;
                for (const auto& tag : tag_filter) {
                    for (const auto& t : test.tags) {
                        if (t == tag) { has_tag = true; break; }
                    }
                }
                if (!has_tag) {
                    ++total_skipped;
                    continue;
                }
            }
            
            // Initialize suite if needed
            if (suites.find(test.suite) == suites.end()) {
                suites[test.suite].name = test.suite;
            }
            
            // Run test
            std::cout << "[" << test.suite << "] " << test.name << "... ";
            std::cout.flush();
            
            TestResult result;
            result.suite = test.suite;
            result.name = test.name;
            
            auto start = std::chrono::high_resolution_clock::now();
            try {
                test.func();
                result.passed = true;
                ++total_passed;
                ++suites[test.suite].passed;
                std::cout << "\033[32mPASS\033[0m";
            } catch (const std::exception& e) {
                result.passed = false;
                result.error = e.what();
                ++total_failed;
                ++suites[test.suite].failed;
                std::cout << "\033[31mFAIL\033[0m\n  Error: " << e.what();
            }
            auto end = std::chrono::high_resolution_clock::now();
            
            result.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
            std::cout << " (" << std::fixed << std::setprecision(2) 
                      << result.duration_ms << " ms)\n";
            
            suites[test.suite].results.push_back(result);
            suites[test.suite].total_duration_ms += result.duration_ms;
        }
        
        printSummary(suites, total_passed, total_failed, total_skipped);
        return total_failed;
    }
    
    void clear() { tests_.clear(); }
    
    size_t testCount() const { return tests_.size(); }
    
    std::vector<std::string> getSuites() const {
        std::vector<std::string> result;
        std::map<std::string, bool> seen;
        for (const auto& t : tests_) {
            if (!seen[t.suite]) {
                result.push_back(t.suite);
                seen[t.suite] = true;
            }
        }
        return result;
    }

private:
    TestRunner() = default;
    
    void printHeader() {
        std::cout << "\n";
        std::cout << "+==============================================================+\n";
        std::cout << "|        DFSMShsm Test Suite v4.1.0                            |\n";
        std::cout << "|        Hierarchical Storage Management Emulator              |\n";
        std::cout << "+==============================================================+\n\n";
    }
    
    void printSummary(const std::map<std::string, TestSuiteResult>& suites,
                      int passed, int failed, int skipped) {
        std::cout << "\n";
        std::cout << "==============================================================\n";
        std::cout << "                      TEST SUMMARY\n";
        std::cout << "==============================================================\n\n";
        
        for (const auto& [name, suite] : suites) {
            std::cout << "Suite: " << name << "\n";
            std::cout << "  Passed: " << suite.passed << ", Failed: " << suite.failed 
                      << ", Duration: " << std::fixed << std::setprecision(2) 
                      << suite.total_duration_ms << " ms\n";
            
            if (suite.failed > 0) {
                std::cout << "  Failed tests:\n";
                for (const auto& r : suite.results) {
                    if (!r.passed) {
                        std::cout << "    - " << r.name << ": " << r.error << "\n";
                    }
                }
            }
            std::cout << "\n";
        }
        
        std::cout << "--------------------------------------------------------------\n";
        int total = passed + failed;
        std::cout << "Total: " << passed << "/" << total << " passed";
        if (skipped > 0) std::cout << ", " << skipped << " skipped";
        std::cout << "\n";
        
        if (failed == 0) {
            std::cout << "\033[32m[PASS] All tests passed!\033[0m\n";
        } else {
            std::cout << "\033[31m[FAIL] " << failed << " test(s) failed!\033[0m\n";
        }
        std::cout << "==============================================================\n\n";
    }
    
    std::vector<TestCase> tests_;
};

//=============================================================================
// Test Registration Macros
//=============================================================================

#define TEST_CASE(name) \
    static void test_##name(); \
    namespace { \
        struct TestRegistrar_##name { \
            TestRegistrar_##name() { \
                dfsmshsm::testing::TestRunner::instance().addTest(#name, test_##name); \
            } \
        } test_registrar_##name; \
    } \
    static void test_##name()

#define TEST_CASE_SUITE(name, suite) \
    static void test_##suite##_##name(); \
    namespace { \
        struct TestRegistrar_##suite##_##name { \
            TestRegistrar_##suite##_##name() { \
                dfsmshsm::testing::TestRunner::instance().addTest(#name, test_##suite##_##name, #suite); \
            } \
        } test_registrar_##suite##_##name; \
    } \
    static void test_##suite##_##name()

#define TEST_CASE_TAGGED(name, suite, ...) \
    static void test_##suite##_##name(); \
    namespace { \
        struct TestRegistrar_##suite##_##name { \
            TestRegistrar_##suite##_##name() { \
                dfsmshsm::testing::TestRunner::instance().addTest( \
                    #name, test_##suite##_##name, #suite, {__VA_ARGS__}); \
            } \
        } test_registrar_##suite##_##name; \
    } \
    static void test_##suite##_##name()

//=============================================================================
// Test Fixture Support
//=============================================================================

template<typename T>
class TestFixture {
public:
    virtual ~TestFixture() = default;
    virtual void setUp() {}
    virtual void tearDown() {}
    
protected:
    T fixture_;
};

#define TEST_FIXTURE(fixture_class, test_name) \
    class fixture_class##_##test_name : public fixture_class { \
    public: \
        void runTest(); \
    }; \
    static void test_fixture_##fixture_class##_##test_name() { \
        fixture_class##_##test_name fixture; \
        fixture.setUp(); \
        try { fixture.runTest(); } catch (...) { fixture.tearDown(); throw; } \
        fixture.tearDown(); \
    } \
    namespace { \
        struct TestRegistrar_fixture_##fixture_class##_##test_name { \
            TestRegistrar_fixture_##fixture_class##_##test_name() { \
                dfsmshsm::testing::TestRunner::instance().addTest( \
                    #test_name, test_fixture_##fixture_class##_##test_name, #fixture_class); \
            } \
        } test_registrar_fixture_##fixture_class##_##test_name; \
    } \
    void fixture_class##_##test_name::runTest()

}} // namespace dfsmshsm::testing

#endif // DFSMSHSM_TEST_FRAMEWORK_HPP
