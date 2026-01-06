/**
 * @file compression.hpp
 * @brief Compression utilities (RLE, LZ4, ZSTD)
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_COMPRESSION_HPP
#define DFSMSHSM_COMPRESSION_HPP

#include "hsm_types.hpp"
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <algorithm>

namespace dfsmshsm {

class Compression {
public:
    static std::vector<uint8_t> compressRLE(const std::vector<uint8_t>& data) {
        if (data.empty()) return {};
        std::vector<uint8_t> result;
        result.reserve(data.size());
        const uint8_t ESC = 0xFF;
        for (size_t i = 0; i < data.size();) {
            size_t run = 1;
            while (i + run < data.size() && data[i + run] == data[i] && run < 255) ++run;
            if (run >= 4) {
                result.push_back(ESC);
                result.push_back(static_cast<uint8_t>(run));
                result.push_back(data[i]);
                i += run;
            } else {
                if (data[i] == ESC) { result.push_back(ESC); result.push_back(1); result.push_back(ESC); }
                else result.push_back(data[i]);
                ++i;
            }
        }
        return result;
    }

    static std::vector<uint8_t> decompressRLE(const std::vector<uint8_t>& data) {
        if (data.empty()) return {};
        std::vector<uint8_t> result;
        result.reserve(data.size() * 2);
        const uint8_t ESC = 0xFF;
        for (size_t i = 0; i < data.size();) {
            if (data[i] == ESC) {
                if (i + 2 >= data.size()) throw std::runtime_error("Invalid RLE data");
                for (uint8_t j = 0; j < data[i + 1]; ++j) result.push_back(data[i + 2]);
                i += 3;
            } else { result.push_back(data[i]); ++i; }
        }
        return result;
    }

    // LZ4-style compression (simplified but working)
    static std::vector<uint8_t> compressLZ4(const std::vector<uint8_t>& data) {
        if (data.empty()) return {};
        std::vector<uint8_t> result;
        result.reserve(data.size() + 8);
        
        // Header: magic + size
        result.push_back('L'); result.push_back('Z'); result.push_back('4'); result.push_back('!');
        uint32_t sz = static_cast<uint32_t>(data.size());
        result.push_back(sz & 0xFF); result.push_back((sz >> 8) & 0xFF);
        result.push_back((sz >> 16) & 0xFF); result.push_back((sz >> 24) & 0xFF);
        
        // Simple compression: look for matches
        std::unordered_map<uint32_t, size_t> ht;
        size_t pos = 0;
        std::vector<uint8_t> literals;
        
        while (pos < data.size()) {
            bool matched = false;
            if (pos + 4 <= data.size()) {
                uint32_t h = data[pos] | (data[pos+1] << 8) | (data[pos+2] << 16) | (data[pos+3] << 24);
                auto it = ht.find(h);
                if (it != ht.end() && pos - it->second < 65535) {
                    size_t mpos = it->second;
                    size_t mlen = 0;
                    size_t maxlen = std::min(data.size() - pos, size_t(255));
                    while (mlen < maxlen && data[mpos + mlen] == data[pos + mlen]) mlen++;
                    
                    if (mlen >= 4) {
                        // Flush literals
                        if (!literals.empty()) {
                            result.push_back(0x00); // literal marker
                            result.push_back(static_cast<uint8_t>(literals.size()));
                            result.insert(result.end(), literals.begin(), literals.end());
                            literals.clear();
                        }
                        // Write match
                        uint16_t off = static_cast<uint16_t>(pos - mpos);
                        result.push_back(0x01); // match marker
                        result.push_back(static_cast<uint8_t>(mlen));
                        result.push_back(off & 0xFF);
                        result.push_back((off >> 8) & 0xFF);
                        pos += mlen;
                        matched = true;
                    }
                }
                ht[h] = pos;
            }
            if (!matched) {
                literals.push_back(data[pos++]);
                if (literals.size() == 255) {
                    result.push_back(0x00);
                    result.push_back(255);
                    result.insert(result.end(), literals.begin(), literals.end());
                    literals.clear();
                }
            }
        }
        // Flush remaining literals
        if (!literals.empty()) {
            result.push_back(0x00);
            result.push_back(static_cast<uint8_t>(literals.size()));
            result.insert(result.end(), literals.begin(), literals.end());
        }
        
        // If no compression, store raw
        if (result.size() >= data.size() + 8) {
            result.clear();
            result.push_back('U'); result.push_back('N'); result.push_back('C'); result.push_back('!');
            result.insert(result.end(), data.begin(), data.end());
        }
        return result;
    }

    static std::vector<uint8_t> decompressLZ4(const std::vector<uint8_t>& data) {
        if (data.size() < 4) return data;
        if (data[0] == 'U' && data[1] == 'N' && data[2] == 'C' && data[3] == '!')
            return std::vector<uint8_t>(data.begin() + 4, data.end());
        if (data[0] != 'L' || data[1] != 'Z' || data[2] != '4' || data[3] != '!')
            throw std::runtime_error("Invalid LZ4 header");
        
        uint32_t origSize = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
        std::vector<uint8_t> result;
        result.reserve(origSize);
        
        size_t i = 8;
        while (i < data.size()) {
            uint8_t type = data[i++];
            if (type == 0x00) {
                // Literals
                if (i >= data.size()) break;
                uint8_t len = data[i++];
                for (uint8_t j = 0; j < len && i < data.size(); j++) result.push_back(data[i++]);
            } else if (type == 0x01) {
                // Match
                if (i + 2 >= data.size()) break;
                uint8_t len = data[i++];
                uint16_t off = data[i] | (data[i+1] << 8);
                i += 2;
                size_t mpos = result.size() - off;
                for (uint8_t j = 0; j < len; j++) result.push_back(result[mpos + j]);
            }
        }
        return result;
    }

    // ZSTD-style compression (dictionary-based)
    static std::vector<uint8_t> compressZSTD(const std::vector<uint8_t>& data) {
        if (data.empty()) return {};
        std::vector<uint8_t> result;
        result.reserve(data.size() + 32);
        
        result.push_back('Z'); result.push_back('S'); result.push_back('T'); result.push_back('D');
        uint32_t sz = static_cast<uint32_t>(data.size());
        result.push_back(sz & 0xFF); result.push_back((sz >> 8) & 0xFF);
        result.push_back((sz >> 16) & 0xFF); result.push_back((sz >> 24) & 0xFF);
        
        // Build frequency table
        std::vector<size_t> freq(256, 0);
        for (uint8_t b : data) freq[b]++;
        
        // Find top 16 bytes
        std::vector<std::pair<size_t, uint8_t>> sorted;
        for (size_t i = 0; i < 256; ++i)
            if (freq[i] > 0) sorted.emplace_back(freq[i], static_cast<uint8_t>(i));
        std::sort(sorted.rbegin(), sorted.rend());
        
        uint8_t numCodes = std::min<uint8_t>(16, static_cast<uint8_t>(sorted.size()));
        result.push_back(numCodes);
        
        std::unordered_map<uint8_t, uint8_t> encMap;
        for (uint8_t i = 0; i < numCodes; ++i) {
            encMap[sorted[i].second] = i;
            result.push_back(sorted[i].second);
        }
        
        // Encode data
        for (uint8_t b : data) {
            auto it = encMap.find(b);
            if (it != encMap.end()) result.push_back(0xE0 | it->second);
            else result.push_back(b);
        }
        
        if (result.size() >= data.size() + 8) {
            result.clear();
            result.push_back('U'); result.push_back('N'); result.push_back('C'); result.push_back('!');
            result.insert(result.end(), data.begin(), data.end());
        }
        return result;
    }

    static std::vector<uint8_t> decompressZSTD(const std::vector<uint8_t>& data) {
        if (data.size() < 4) return data;
        if (data[0] == 'U' && data[1] == 'N' && data[2] == 'C' && data[3] == '!')
            return std::vector<uint8_t>(data.begin() + 4, data.end());
        if (data[0] != 'Z' || data[1] != 'S' || data[2] != 'T' || data[3] != 'D')
            throw std::runtime_error("Invalid ZSTD header");
        
        uint32_t origSize = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
        std::vector<uint8_t> result;
        result.reserve(origSize);
        
        uint8_t numCodes = data[8];
        std::vector<uint8_t> decTable(numCodes);
        for (uint8_t i = 0; i < numCodes; ++i) decTable[i] = data[9 + i];
        
        size_t i = 9 + numCodes;
        while (i < data.size()) {
            uint8_t b = data[i++];
            if ((b & 0xF0) == 0xE0) {
                uint8_t idx = b & 0x0F;
                if (idx < decTable.size()) result.push_back(decTable[idx]);
            } else result.push_back(b);
        }
        return result;
    }

    static std::vector<uint8_t> compress(const std::vector<uint8_t>& data, CompressionType type) {
        switch (type) {
            case CompressionType::RLE:  return compressRLE(data);
            case CompressionType::LZ4:  return compressLZ4(data);
            case CompressionType::ZSTD: return compressZSTD(data);
            default: return data;
        }
    }

    static std::vector<uint8_t> decompress(const std::vector<uint8_t>& data, CompressionType type) {
        switch (type) {
            case CompressionType::RLE:  return decompressRLE(data);
            case CompressionType::LZ4:  return decompressLZ4(data);
            case CompressionType::ZSTD: return decompressZSTD(data);
            default: return data;
        }
    }

    static double ratio(size_t orig, size_t comp) { return orig > 0 ? 100.0 * double(comp) / double(orig) : 100.0; }

    static std::string typeToString(CompressionType type) {
        switch (type) {
            case CompressionType::NONE: return "NONE";
            case CompressionType::RLE:  return "RLE";
            case CompressionType::LZ4:  return "LZ4";
            case CompressionType::ZSTD: return "ZSTD";
            default: return "UNKNOWN";
        }
    }
};

class CompressedBuffer {
public:
    void setData(const std::vector<uint8_t>& data, CompressionType type = CompressionType::NONE) {
        orig_size_ = data.size(); type_ = type;
        data_ = (type == CompressionType::NONE) ? data : Compression::compress(data, type);
    }
    std::vector<uint8_t> getData() const { return (type_ == CompressionType::NONE) ? data_ : Compression::decompress(data_, type_); }
    size_t compressedSize() const { return data_.size(); }
    size_t originalSize() const { return orig_size_; }
    double ratio() const { return Compression::ratio(orig_size_, data_.size()); }
    CompressionType type() const { return type_; }
private:
    std::vector<uint8_t> data_;
    size_t orig_size_ = 0;
    CompressionType type_ = CompressionType::NONE;
};

} // namespace dfsmshsm
#endif
