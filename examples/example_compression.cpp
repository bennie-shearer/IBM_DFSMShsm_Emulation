/**
 * @file example_compression.cpp
 * @brief Compression algorithms example (RLE, LZ4, ZSTD)
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#include "hsm_engine.hpp"
#include "compression.hpp"
#include <iostream>
#include <iomanip>
#include <random>

using namespace dfsmshsm;

std::vector<uint8_t> generateData(size_t size, bool repetitive) {
    std::vector<uint8_t> data(size);
    std::mt19937 rng(42);
    if (repetitive) {
        for (size_t i = 0; i < size; ++i) data[i] = static_cast<uint8_t>((i / 100) % 10);
    } else {
        std::uniform_int_distribution<int> dist(0, 255);
        for (size_t i = 0; i < size; ++i) data[i] = static_cast<uint8_t>(dist(rng));
    }
    return data;
}

void testCompression(const std::string& name, CompressionType type, const std::vector<uint8_t>& data) {
    std::cout << "\n=== " << name << " Compression ===\n";
    auto compressed = Compression::compress(data, type);
    auto decompressed = Compression::decompress(compressed, type);
    bool valid = (decompressed == data);
    double ratio = Compression::ratio(data.size(), compressed.size());
    std::cout << "Original:   " << data.size() << " bytes\n";
    std::cout << "Compressed: " << compressed.size() << " bytes\n";
    std::cout << "Ratio:      " << std::fixed << std::setprecision(2) << ratio << "%\n";
    std::cout << "Saved:      " << (100.0 - ratio) << "%\n";
    std::cout << "Integrity:  " << (valid ? "PASSED" : "FAILED") << "\n";
}

int main() {
    std::cout << "DFSMShsm v4.1.0 - Compression Algorithms Demo\n";
    std::cout << "==============================================\n";

    std::cout << "\n--- Highly Compressible Data (100 KB) ---\n";
    auto repetitiveData = generateData(102400, true);
    testCompression("RLE", CompressionType::RLE, repetitiveData);
    testCompression("LZ4", CompressionType::LZ4, repetitiveData);
    testCompression("ZSTD", CompressionType::ZSTD, repetitiveData);
    
    std::cout << "\n--- Random Data (100 KB) ---\n";
    auto randomData = generateData(102400, false);
    testCompression("RLE", CompressionType::RLE, randomData);
    testCompression("LZ4", CompressionType::LZ4, randomData);
    testCompression("ZSTD", CompressionType::ZSTD, randomData);
    
    std::cout << "\n--- Algorithm Summary ---\n";
    std::cout << "RLE:  Best for repetitive byte sequences\n";
    std::cout << "LZ4:  Fast compression, good for real-time\n";
    std::cout << "ZSTD: High compression for archival\n";
    return 0;
}
