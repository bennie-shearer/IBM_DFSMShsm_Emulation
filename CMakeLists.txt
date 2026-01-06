# IBM DFSMShsm (Hierarchical Storage Management) Emulation
# Author: Bennie Shearer
# Copyright (c) 2025 Bennie Shearer - MIT License

cmake_minimum_required(VERSION 3.16)
project(IBM_DFSMShsm_Emulation
    VERSION 4.2.0
    DESCRIPTION "IBM DFSMShsm Emulation"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(ENABLE_WARNINGS "Enable compiler warnings" ON)
option(ENABLE_COVERAGE "Enable code coverage (GCC/Clang only)" OFF)

# Code coverage configuration
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Code coverage enabled")
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    else()
        message(WARNING "Code coverage only supported with GCC/Clang")
    endif()
endif()

if(ENABLE_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
    elseif(MSVC)
        add_compile_options(/W4 /permissive-)
    endif()
endif()

if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601 -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
endif()

find_package(Threads REQUIRED)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(dfsmshsm STATIC src/hsm_engine.cpp)
target_link_libraries(dfsmshsm PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(dfsmshsm PUBLIC ws2_32)
endif()
target_include_directories(dfsmshsm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

add_executable(dfsmshsm_cli src/main.cpp)
target_link_libraries(dfsmshsm_cli PRIVATE dfsmshsm)

if(BUILD_TESTS)
    enable_testing()
    
    # Core test suite
    add_executable(test_all tests/test_all.cpp)
    target_link_libraries(test_all PRIVATE dfsmshsm)
    add_test(NAME AllTests COMMAND test_all)
    
    add_executable(test_extended tests/test_extended.cpp)
    target_link_libraries(test_extended PRIVATE dfsmshsm)
    add_test(NAME ExtendedTests COMMAND test_extended)
    
    # Version-specific test suites
    add_executable(test_v3_5_0 tests/test_v3_5_0.cpp)
    target_link_libraries(test_v3_5_0 PRIVATE dfsmshsm)
    add_test(NAME V350Tests COMMAND test_v3_5_0)
    
    add_executable(test_v350_features tests/test_v350_features.cpp)
    target_link_libraries(test_v350_features PRIVATE dfsmshsm)
    add_test(NAME V350FeaturesTests COMMAND test_v350_features)
    
    add_executable(test_v360_features tests/test_v360_features.cpp)
    target_link_libraries(test_v360_features PRIVATE dfsmshsm)
    add_test(NAME V360FeaturesTests COMMAND test_v360_features)
    
    add_executable(test_v370_features tests/test_v370_features.cpp)
    target_link_libraries(test_v370_features PRIVATE dfsmshsm)
    add_test(NAME V370FeaturesTests COMMAND test_v370_features)
    
    add_executable(test_v400_features tests/test_v400_features.cpp)
    target_link_libraries(test_v400_features PRIVATE dfsmshsm)
    add_test(NAME V400FeaturesTests COMMAND test_v400_features)
    
    # v4.1.0 comprehensive test suites
    add_executable(test_v410_features tests/test_v410_features.cpp)
    target_link_libraries(test_v410_features PRIVATE dfsmshsm)
    add_test(NAME V410FeaturesTests COMMAND test_v410_features)
endif()

if(BUILD_EXAMPLES)
    # Core examples
    foreach(example basic migration deduplication quota encryption snapshots replication scheduling compression metrics rest_api)
        add_executable(example_${example} examples/example_${example}.cpp)
        target_link_libraries(example_${example} PRIVATE dfsmshsm)
    endforeach()
    
    # Version-specific feature demos
    add_executable(v3_5_0_demo examples/v3_5_0_demo.cpp)
    target_link_libraries(v3_5_0_demo PRIVATE dfsmshsm)
    
    add_executable(example_v350_features examples/example_v350_features.cpp)
    target_link_libraries(example_v350_features PRIVATE dfsmshsm)
    
    add_executable(example_v360_features examples/example_v360_features.cpp)
    target_link_libraries(example_v360_features PRIVATE dfsmshsm)
    
    add_executable(example_v370_features examples/example_v370_features.cpp)
    target_link_libraries(example_v370_features PRIVATE dfsmshsm)
    
    add_executable(example_hsm_core examples/example_hsm_core.cpp)
    target_link_libraries(example_hsm_core PRIVATE dfsmshsm)
    
    add_executable(example_v410_features examples/example_v410_features.cpp)
    target_link_libraries(example_v410_features PRIVATE dfsmshsm)
endif()

install(TARGETS dfsmshsm dfsmshsm_cli ARCHIVE DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include/dfsmshsm)
install(DIRECTORY docs/ DESTINATION share/doc/dfsmshsm)

# Code coverage targets
if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    find_program(GCOV_PATH gcov)
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    
    if(LCOV_PATH AND GENHTML_PATH)
        add_custom_target(coverage
            COMMAND ${LCOV_PATH} --directory . --zerocounters
            COMMAND ${CMAKE_COMMAND} --build . --target all
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND ${LCOV_PATH} --directory . --capture --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info.cleaned
            COMMAND ${GENHTML_PATH} -o coverage_report coverage.info.cleaned
            COMMAND ${CMAKE_COMMAND} -E echo "Coverage report: ${CMAKE_BINARY_DIR}/coverage_report/index.html"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating code coverage report..."
        )
        
        add_custom_target(coverage-clean
            COMMAND ${CMAKE_COMMAND} -E remove_directory coverage_report
            COMMAND ${CMAKE_COMMAND} -E remove coverage.info coverage.info.cleaned
            COMMAND find . -name "*.gcda" -delete
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Cleaning coverage data..."
        )
    else()
        message(STATUS "lcov/genhtml not found - coverage target not available")
        message(STATUS "Install with: apt-get install lcov (Linux) or brew install lcov (macOS)")
    endif()
endif()

message(STATUS "IBM DFSMShsm Emulation v${PROJECT_VERSION} - C++${CMAKE_CXX_STANDARD}")
