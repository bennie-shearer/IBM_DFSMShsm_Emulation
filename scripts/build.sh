#!/bin/bash
# DFSMShsm Emulator v4.0.0 Build Script
# Cross-platform build for Windows (MinGW), Linux, and macOS
# Copyright (c) 2025 Bennie Shearer - MIT License

set -e

VERSION="4.0.0"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
BUILD_TYPE="${1:-Release}"

echo "=============================================="
echo "DFSMShsm Emulator v${VERSION} Build Script"
echo "=============================================="
echo ""

# Detect platform
detect_platform() {
    case "$(uname -s)" in
        Linux*)     PLATFORM="Linux";;
        Darwin*)    PLATFORM="macOS";;
        MINGW*|MSYS*|CYGWIN*)   PLATFORM="Windows";;
        *)          PLATFORM="Unknown";;
    esac
    echo "Platform: ${PLATFORM}"
}

# Detect compiler
detect_compiler() {
    if command -v g++ &> /dev/null; then
        CXX="g++"
        CXX_VERSION=$(g++ --version | head -n1)
    elif command -v clang++ &> /dev/null; then
        CXX="clang++"
        CXX_VERSION=$(clang++ --version | head -n1)
    else
        echo "Error: No C++ compiler found. Please install GCC or Clang."
        exit 1
    fi
    echo "Compiler: ${CXX_VERSION}"
}

# Set compiler flags
set_flags() {
    COMMON_FLAGS="-std=c++20 -Wall -Wextra -Wpedantic -I${PROJECT_ROOT}/include"
    
    if [ "${BUILD_TYPE}" = "Debug" ]; then
        BUILD_FLAGS="${COMMON_FLAGS} -g -O0 -DDEBUG"
    else
        BUILD_FLAGS="${COMMON_FLAGS} -O2 -DNDEBUG"
    fi
    
    # Platform-specific flags
    case "${PLATFORM}" in
        Windows)
            BUILD_FLAGS="${BUILD_FLAGS} -D_WIN32_WINNT=0x0A00"
            LINK_FLAGS="-static-libgcc -static-libstdc++"
            ;;
        macOS)
            BUILD_FLAGS="${BUILD_FLAGS}"
            LINK_FLAGS=""
            ;;
        Linux)
            BUILD_FLAGS="${BUILD_FLAGS}"
            LINK_FLAGS="-pthread"
            ;;
    esac
    
    echo "Build type: ${BUILD_TYPE}"
    echo "Flags: ${BUILD_FLAGS}"
}

# Build function
build() {
    echo ""
    echo "Building..."
    mkdir -p "${BUILD_DIR}"
    
    # Build main executable
    echo "  Compiling main.cpp..."
    ${CXX} ${BUILD_FLAGS} -c "${PROJECT_ROOT}/src/main.cpp" -o "${BUILD_DIR}/main.o"
    
    echo "  Compiling hsm_engine.cpp..."
    ${CXX} ${BUILD_FLAGS} -c "${PROJECT_ROOT}/src/hsm_engine.cpp" -o "${BUILD_DIR}/hsm_engine.o"
    
    echo "  Linking dfsmshsm..."
    ${CXX} ${BUILD_FLAGS} "${BUILD_DIR}/main.o" "${BUILD_DIR}/hsm_engine.o" \
        -o "${BUILD_DIR}/dfsmshsm" ${LINK_FLAGS}
    
    echo ""
    echo "Build complete: ${BUILD_DIR}/dfsmshsm"
}

# Build tests
build_tests() {
    echo ""
    echo "Building tests..."
    
    # Core tests
    echo "  Compiling test_all.cpp..."
    ${CXX} ${BUILD_FLAGS} "${PROJECT_ROOT}/tests/test_all.cpp" \
        "${PROJECT_ROOT}/src/hsm_engine.cpp" \
        -o "${BUILD_DIR}/test_all" ${LINK_FLAGS}
    
    # v4.0.0 feature tests
    echo "  Compiling test_v400_features.cpp..."
    ${CXX} ${BUILD_FLAGS} "${PROJECT_ROOT}/tests/test_v400_features.cpp" \
        -o "${BUILD_DIR}/test_v400_features" ${LINK_FLAGS}
    
    # Stress tests
    echo "  Compiling test_stress.cpp..."
    ${CXX} ${BUILD_FLAGS} "${PROJECT_ROOT}/tests/test_stress.cpp" \
        -o "${BUILD_DIR}/test_stress" ${LINK_FLAGS}
    
    echo ""
    echo "Tests built successfully"
}

# Run tests
run_tests() {
    echo ""
    echo "Running tests..."
    echo ""
    
    echo "--- Core Tests ---"
    "${BUILD_DIR}/test_all"
    
    echo ""
    echo "--- v4.0.0 Feature Tests ---"
    "${BUILD_DIR}/test_v400_features"
    
    echo ""
    echo "--- Stress Tests ---"
    "${BUILD_DIR}/test_stress"
}

# Clean build
clean() {
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
    echo "Clean complete"
}

# Main
detect_platform
detect_compiler
set_flags

case "${2:-build}" in
    clean)
        clean
        ;;
    test)
        build
        build_tests
        run_tests
        ;;
    build)
        build
        build_tests
        ;;
    *)
        echo "Usage: $0 [Debug|Release] [build|test|clean]"
        exit 1
        ;;
esac

echo ""
echo "Done!"
