/**
 * @file hsm_binary_protocol.hpp
 * @brief Binary serialization protocol for CDS records
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 *
 * Provides efficient binary serialization for Control Data Set (CDS) records,
 * enabling faster I/O operations, reduced storage requirements, and
 * network transmission efficiency.
 *
 * Features:
 * - Type-safe binary serialization/deserialization
 * - Variable-length encoding for integers (varint)
 * - CRC32 checksums for data integrity
 * - Backward-compatible versioned record format
 * - Zero-copy buffer views where possible
 */
#ifndef DFSMSHSM_HSM_BINARY_PROTOCOL_HPP
#define DFSMSHSM_HSM_BINARY_PROTOCOL_HPP

#include "hsm_types.hpp"
#include "result.hpp"
#include <vector>
#include <string>
#include <cstring>
#include <array>
#include <span>
#include <bit>
#include <cstdint>
#include <optional>
#include <map>
#include <mutex>
#include <sstream>
#include <iomanip>

namespace dfsmshsm {

//=============================================================================
// Binary Protocol Constants
//=============================================================================

namespace binary_protocol {
    constexpr uint32_t MAGIC_NUMBER = 0x48534D42;  // "HSMB" in ASCII
    constexpr uint8_t PROTOCOL_VERSION = 1;
    constexpr size_t MAX_RECORD_SIZE = 16 * 1024 * 1024;  // 16 MB
    constexpr size_t HEADER_SIZE = 16;  // Magic(4) + Version(1) + Type(1) + Flags(2) + Length(4) + CRC(4)
}

//=============================================================================
// Record Types
//=============================================================================

/**
 * @enum BinaryRecordType
 * @brief Types of binary records in the protocol
 */
enum class BinaryRecordType : uint8_t {
    DATASET_ENTRY = 0x01,
    VOLUME_ENTRY = 0x02,
    MIGRATION_RECORD = 0x03,
    BACKUP_RECORD = 0x04,
    CATALOG_ENTRY = 0x05,
    POLICY_ENTRY = 0x06,
    STATISTICS_RECORD = 0x07,
    AUDIT_RECORD = 0x08,
    CHECKPOINT_RECORD = 0x09,
    EVENT_RECORD = 0x0A,
    CONFIG_RECORD = 0x0B,
    INDEX_RECORD = 0x0C,
    JOURNAL_ENTRY = 0x0D,
    SNAPSHOT_RECORD = 0x0E,
    REPLICATION_RECORD = 0x0F,
    BATCH_RECORD = 0x10  // Container for multiple records
};

/**
 * @enum RecordFlags
 * @brief Flags for binary records
 */
enum class RecordFlags : uint16_t {
    NONE = 0x0000,
    COMPRESSED = 0x0001,
    ENCRYPTED = 0x0002,
    CHECKSUMMED = 0x0004,
    CONTINUED = 0x0008,  // Record continues in next block
    DELETED = 0x0010,
    VERSIONED = 0x0020,
    INDEXED = 0x0040
};

inline RecordFlags operator|(RecordFlags a, RecordFlags b) {
    return static_cast<RecordFlags>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline RecordFlags operator&(RecordFlags a, RecordFlags b) {
    return static_cast<RecordFlags>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

inline bool hasFlag(RecordFlags flags, RecordFlags flag) {
    return (static_cast<uint16_t>(flags) & static_cast<uint16_t>(flag)) != 0;
}

//=============================================================================
// CRC32 Implementation
//=============================================================================

/**
 * @class CRC32
 * @brief CRC32 checksum calculator (IEEE polynomial)
 */
class CRC32 {
public:
    CRC32() : value_(0xFFFFFFFF) {
        initTable();
    }
    
    void update(const void* data, size_t length) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < length; ++i) {
            value_ = table_[(value_ ^ bytes[i]) & 0xFF] ^ (value_ >> 8);
        }
    }
    
    void update(const std::vector<uint8_t>& data) {
        update(data.data(), data.size());
    }
    
    void update(const std::string& data) {
        update(data.data(), data.size());
    }
    
    [[nodiscard]] uint32_t finalize() const {
        return value_ ^ 0xFFFFFFFF;
    }
    
    void reset() {
        value_ = 0xFFFFFFFF;
    }
    
    [[nodiscard]] static uint32_t compute(const void* data, size_t length) {
        CRC32 crc;
        crc.update(data, length);
        return crc.finalize();
    }
    
    [[nodiscard]] static uint32_t compute(const std::vector<uint8_t>& data) {
        return compute(data.data(), data.size());
    }

private:
    void initTable() {
        static bool initialized = false;
        if (initialized) return;
        
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {
                crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
            }
            table_[i] = crc;
        }
        initialized = true;
    }
    
    uint32_t value_;
    static inline std::array<uint32_t, 256> table_{};
};

//=============================================================================
// Binary Buffer Writer
//=============================================================================

/**
 * @class BinaryWriter
 * @brief Writes binary data to a buffer with various encoding methods
 */
class BinaryWriter {
public:
    explicit BinaryWriter(size_t initial_capacity = 1024) {
        buffer_.reserve(initial_capacity);
    }
    
    // Primitive types
    void writeUint8(uint8_t value) {
        buffer_.push_back(value);
    }
    
    void writeUint16(uint16_t value) {
        buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
        buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }
    
    void writeUint32(uint32_t value) {
        buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
        buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }
    
    void writeUint64(uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            buffer_.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }
    
    void writeInt32(int32_t value) {
        writeUint32(static_cast<uint32_t>(value));
    }
    
    void writeInt64(int64_t value) {
        writeUint64(static_cast<uint64_t>(value));
    }
    
    void writeFloat(float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        writeUint32(bits);
    }
    
    void writeDouble(double value) {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        writeUint64(bits);
    }
    
    // Variable-length integer encoding (varint)
    void writeVarint(uint64_t value) {
        while (value >= 0x80) {
            buffer_.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
            value >>= 7;
        }
        buffer_.push_back(static_cast<uint8_t>(value));
    }
    
    void writeSignedVarint(int64_t value) {
        // ZigZag encoding for efficient negative number representation
        uint64_t encoded = static_cast<uint64_t>((value << 1) ^ (value >> 63));
        writeVarint(encoded);
    }
    
    // String with length prefix
    void writeString(const std::string& str) {
        writeVarint(str.size());
        buffer_.insert(buffer_.end(), str.begin(), str.end());
    }
    
    // Raw bytes
    void writeBytes(const void* data, size_t length) {
        writeVarint(length);
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        buffer_.insert(buffer_.end(), bytes, bytes + length);
    }
    
    void writeBytes(const std::vector<uint8_t>& data) {
        writeBytes(data.data(), data.size());
    }
    
    // Fixed-size bytes (no length prefix)
    void writeFixedBytes(const void* data, size_t length) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        buffer_.insert(buffer_.end(), bytes, bytes + length);
    }
    
    // Boolean
    void writeBool(bool value) {
        writeUint8(value ? 1 : 0);
    }
    
    // Timestamp (as microseconds since epoch)
    void writeTimestamp(const std::chrono::system_clock::time_point& tp) {
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
            tp.time_since_epoch()).count();
        writeInt64(micros);
    }
    
    // Enum (as underlying type)
    template<typename E>
    void writeEnum(E value) {
        writeVarint(static_cast<uint64_t>(static_cast<std::underlying_type_t<E>>(value)));
    }
    
    // Optional value
    template<typename T, typename WriteFunc>
    void writeOptional(const std::optional<T>& opt, WriteFunc writeFunc) {
        writeBool(opt.has_value());
        if (opt.has_value()) {
            writeFunc(*opt);
        }
    }
    
    // Vector of values
    template<typename T, typename WriteFunc>
    void writeVector(const std::vector<T>& vec, WriteFunc writeFunc) {
        writeVarint(vec.size());
        for (const auto& item : vec) {
            writeFunc(item);
        }
    }
    
    // Map
    template<typename K, typename V, typename WriteKeyFunc, typename WriteValueFunc>
    void writeMap(const std::map<K, V>& map, WriteKeyFunc writeKey, WriteValueFunc writeValue) {
        writeVarint(map.size());
        for (const auto& [k, v] : map) {
            writeKey(k);
            writeValue(v);
        }
    }
    
    [[nodiscard]] const std::vector<uint8_t>& buffer() const { return buffer_; }
    [[nodiscard]] std::vector<uint8_t>&& release() { return std::move(buffer_); }
    [[nodiscard]] size_t size() const { return buffer_.size(); }
    
    void clear() { buffer_.clear(); }
    void reserve(size_t capacity) { buffer_.reserve(capacity); }

private:
    std::vector<uint8_t> buffer_;
};

//=============================================================================
// Binary Buffer Reader
//=============================================================================

/**
 * @class BinaryReader
 * @brief Reads binary data from a buffer with various decoding methods
 */
class BinaryReader {
public:
    BinaryReader(const uint8_t* data, size_t length)
        : data_(data), length_(length), position_(0) {}
    
    explicit BinaryReader(const std::vector<uint8_t>& buffer)
        : data_(buffer.data()), length_(buffer.size()), position_(0) {}
    
    // Primitive types
    [[nodiscard]] Result<uint8_t> readUint8() {
        if (position_ + 1 > length_) {
            return Err<uint8_t>(ErrorCode::OUT_OF_RANGE, "Buffer underflow reading uint8");
        }
        return data_[position_++];
    }
    
    [[nodiscard]] Result<uint16_t> readUint16() {
        if (position_ + 2 > length_) {
            return Err<uint16_t>(ErrorCode::OUT_OF_RANGE, "Buffer underflow reading uint16");
        }
        uint16_t value = static_cast<uint16_t>(data_[position_]) |
                        (static_cast<uint16_t>(data_[position_ + 1]) << 8);
        position_ += 2;
        return value;
    }
    
    [[nodiscard]] Result<uint32_t> readUint32() {
        if (position_ + 4 > length_) {
            return Err<uint32_t>(ErrorCode::OUT_OF_RANGE, "Buffer underflow reading uint32");
        }
        uint32_t value = static_cast<uint32_t>(data_[position_]) |
                        (static_cast<uint32_t>(data_[position_ + 1]) << 8) |
                        (static_cast<uint32_t>(data_[position_ + 2]) << 16) |
                        (static_cast<uint32_t>(data_[position_ + 3]) << 24);
        position_ += 4;
        return value;
    }
    
    [[nodiscard]] Result<uint64_t> readUint64() {
        if (position_ + 8 > length_) {
            return Err<uint64_t>(ErrorCode::OUT_OF_RANGE, "Buffer underflow reading uint64");
        }
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= static_cast<uint64_t>(data_[position_ + i]) << (i * 8);
        }
        position_ += 8;
        return value;
    }
    
    [[nodiscard]] Result<int32_t> readInt32() {
        auto result = readUint32();
        if (!result) return Err<int32_t>(result.error());
        return static_cast<int32_t>(*result);
    }
    
    [[nodiscard]] Result<int64_t> readInt64() {
        auto result = readUint64();
        if (!result) return Err<int64_t>(result.error());
        return static_cast<int64_t>(*result);
    }
    
    [[nodiscard]] Result<float> readFloat() {
        auto result = readUint32();
        if (!result) return Err<float>(result.error());
        float value;
        uint32_t bits = *result;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    
    [[nodiscard]] Result<double> readDouble() {
        auto result = readUint64();
        if (!result) return Err<double>(result.error());
        double value;
        uint64_t bits = *result;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    
    // Variable-length integer decoding
    [[nodiscard]] Result<uint64_t> readVarint() {
        uint64_t value = 0;
        int shift = 0;
        while (true) {
            if (position_ >= length_) {
                return Err<uint64_t>(ErrorCode::OUT_OF_RANGE, "Buffer underflow reading varint");
            }
            uint8_t byte = data_[position_++];
            value |= static_cast<uint64_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) break;
            shift += 7;
            if (shift >= 64) {
                return Err<uint64_t>(ErrorCode::CORRUPTION_DETECTED, "Varint too long");
            }
        }
        return value;
    }
    
    [[nodiscard]] Result<int64_t> readSignedVarint() {
        auto result = readVarint();
        if (!result) return Err<int64_t>(result.error());
        // ZigZag decoding
        uint64_t encoded = *result;
        return static_cast<int64_t>((encoded >> 1) ^ -(static_cast<int64_t>(encoded) & 1));
    }
    
    // String with length prefix
    [[nodiscard]] Result<std::string> readString() {
        auto lenResult = readVarint();
        if (!lenResult) return Err<std::string>(lenResult.error());
        size_t length = static_cast<size_t>(*lenResult);
        
        if (position_ + length > length_) {
            return Err<std::string>(ErrorCode::OUT_OF_RANGE, "Buffer underflow reading string");
        }
        
        std::string str(reinterpret_cast<const char*>(data_ + position_), length);
        position_ += length;
        return str;
    }
    
    // Raw bytes with length prefix
    [[nodiscard]] Result<std::vector<uint8_t>> readBytes() {
        auto lenResult = readVarint();
        if (!lenResult) return Err<std::vector<uint8_t>>(lenResult.error());
        size_t length = static_cast<size_t>(*lenResult);
        
        if (position_ + length > length_) {
            return Err<std::vector<uint8_t>>(ErrorCode::OUT_OF_RANGE, "Buffer underflow reading bytes");
        }
        
        std::vector<uint8_t> bytes(data_ + position_, data_ + position_ + length);
        position_ += length;
        return bytes;
    }
    
    // Fixed-size bytes
    [[nodiscard]] Result<std::vector<uint8_t>> readFixedBytes(size_t length) {
        if (position_ + length > length_) {
            return Err<std::vector<uint8_t>>(ErrorCode::OUT_OF_RANGE, "Buffer underflow reading fixed bytes");
        }
        std::vector<uint8_t> bytes(data_ + position_, data_ + position_ + length);
        position_ += length;
        return bytes;
    }
    
    // Boolean
    [[nodiscard]] Result<bool> readBool() {
        auto result = readUint8();
        if (!result) return Err<bool>(result.error());
        return *result != 0;
    }
    
    // Timestamp
    [[nodiscard]] Result<std::chrono::system_clock::time_point> readTimestamp() {
        auto result = readInt64();
        if (!result) return Err<std::chrono::system_clock::time_point>(result.error());
        return std::chrono::system_clock::time_point(
            std::chrono::microseconds(*result));
    }
    
    // Enum
    template<typename E>
    [[nodiscard]] Result<E> readEnum() {
        auto result = readVarint();
        if (!result) return Err<E>(result.error());
        return static_cast<E>(static_cast<std::underlying_type_t<E>>(*result));
    }
    
    [[nodiscard]] size_t position() const { return position_; }
    [[nodiscard]] size_t remaining() const { return length_ - position_; }
    [[nodiscard]] bool eof() const { return position_ >= length_; }
    
    void skip(size_t bytes) {
        position_ = std::min(position_ + bytes, length_);
    }
    
    void seek(size_t pos) {
        position_ = std::min(pos, length_);
    }

private:
    const uint8_t* data_;
    size_t length_;
    size_t position_;
};

//=============================================================================
// Record Header
//=============================================================================

/**
 * @struct BinaryRecordHeader
 * @brief Header for all binary records
 */
struct BinaryRecordHeader {
    uint32_t magic = binary_protocol::MAGIC_NUMBER;
    uint8_t version = binary_protocol::PROTOCOL_VERSION;
    BinaryRecordType type = BinaryRecordType::DATASET_ENTRY;
    RecordFlags flags = RecordFlags::NONE;
    uint32_t payload_length = 0;
    uint32_t checksum = 0;
    
    void write(BinaryWriter& writer) const {
        writer.writeUint32(magic);
        writer.writeUint8(version);
        writer.writeUint8(static_cast<uint8_t>(type));
        writer.writeUint16(static_cast<uint16_t>(flags));
        writer.writeUint32(payload_length);
        writer.writeUint32(checksum);
    }
    
    [[nodiscard]] static Result<BinaryRecordHeader> read(BinaryReader& reader) {
        BinaryRecordHeader header;
        
        auto magic = reader.readUint32();
        if (!magic) return Err<BinaryRecordHeader>(magic.error());
        header.magic = *magic;
        
        if (header.magic != binary_protocol::MAGIC_NUMBER) {
            return Err<BinaryRecordHeader>(ErrorCode::CORRUPTION_DETECTED, "Invalid magic number");
        }
        
        auto version = reader.readUint8();
        if (!version) return Err<BinaryRecordHeader>(version.error());
        header.version = *version;
        
        auto type = reader.readUint8();
        if (!type) return Err<BinaryRecordHeader>(type.error());
        header.type = static_cast<BinaryRecordType>(*type);
        
        auto flags = reader.readUint16();
        if (!flags) return Err<BinaryRecordHeader>(flags.error());
        header.flags = static_cast<RecordFlags>(*flags);
        
        auto length = reader.readUint32();
        if (!length) return Err<BinaryRecordHeader>(length.error());
        header.payload_length = *length;
        
        auto checksum = reader.readUint32();
        if (!checksum) return Err<BinaryRecordHeader>(checksum.error());
        header.checksum = *checksum;
        
        return header;
    }
};

//=============================================================================
// Binary Record Serializers
//=============================================================================

/**
 * @class BinarySerializer
 * @brief Serializes HSM data structures to binary format
 */
class BinarySerializer {
public:
    // Serialize DatasetInfo
    [[nodiscard]] static std::vector<uint8_t> serializeDataset(const DatasetInfo& ds) {
        BinaryWriter payload;
        
        payload.writeString(ds.name);
        payload.writeEnum(ds.tier);
        payload.writeEnum(ds.state);
        payload.writeUint64(ds.size);
        payload.writeTimestamp(ds.created);
        payload.writeTimestamp(ds.last_accessed);
        payload.writeTimestamp(ds.last_modified);
        payload.writeString(ds.volume);
        payload.writeUint32(ds.access_count);
        payload.writeString(ds.owner);
        payload.writeString(ds.dsorg);
        payload.writeUint32(ds.record_length);
        payload.writeUint32(ds.block_size);
        payload.writeBool(ds.compressed);
        payload.writeBool(ds.encrypted);
        payload.writeBool(ds.deduplicated);
        payload.writeString(ds.encryption_key_id);
        payload.writeVarint(ds.retention_days);
        payload.writeTimestamp(ds.expiration_time);
        
        // Write tags
        payload.writeVarint(ds.tags.size());
        for (const auto& tag : ds.tags) {
            payload.writeString(tag);
        }
        
        return wrapWithHeader(BinaryRecordType::DATASET_ENTRY, payload.buffer());
    }
    
    // Deserialize DatasetInfo
    [[nodiscard]] static Result<DatasetInfo> deserializeDataset(const std::vector<uint8_t>& data) {
        auto headerResult = validateAndStripHeader(data, BinaryRecordType::DATASET_ENTRY);
        if (!headerResult) return Err<DatasetInfo>(headerResult.error());
        
        BinaryReader reader(*headerResult);
        DatasetInfo ds;
        
        auto name = reader.readString();
        if (!name) return Err<DatasetInfo>(name.error());
        ds.name = *name;
        
        auto tier = reader.readEnum<StorageTier>();
        if (!tier) return Err<DatasetInfo>(tier.error());
        ds.tier = *tier;
        
        auto state = reader.readEnum<DatasetState>();
        if (!state) return Err<DatasetInfo>(state.error());
        ds.state = *state;
        
        auto size = reader.readUint64();
        if (!size) return Err<DatasetInfo>(size.error());
        ds.size = *size;
        
        auto created = reader.readTimestamp();
        if (!created) return Err<DatasetInfo>(created.error());
        ds.created = *created;
        
        auto lastAccessed = reader.readTimestamp();
        if (!lastAccessed) return Err<DatasetInfo>(lastAccessed.error());
        ds.last_accessed = *lastAccessed;
        
        auto lastModified = reader.readTimestamp();
        if (!lastModified) return Err<DatasetInfo>(lastModified.error());
        ds.last_modified = *lastModified;
        
        auto volume = reader.readString();
        if (!volume) return Err<DatasetInfo>(volume.error());
        ds.volume = *volume;
        
        auto accessCount = reader.readUint32();
        if (!accessCount) return Err<DatasetInfo>(accessCount.error());
        ds.access_count = *accessCount;
        
        auto owner = reader.readString();
        if (!owner) return Err<DatasetInfo>(owner.error());
        ds.owner = *owner;
        
        auto dsorg = reader.readString();
        if (!dsorg) return Err<DatasetInfo>(dsorg.error());
        ds.dsorg = *dsorg;
        
        auto recordLength = reader.readUint32();
        if (!recordLength) return Err<DatasetInfo>(recordLength.error());
        ds.record_length = *recordLength;
        
        auto blockSize = reader.readUint32();
        if (!blockSize) return Err<DatasetInfo>(blockSize.error());
        ds.block_size = *blockSize;
        
        auto compressed = reader.readBool();
        if (!compressed) return Err<DatasetInfo>(compressed.error());
        ds.compressed = *compressed;
        
        auto encrypted = reader.readBool();
        if (!encrypted) return Err<DatasetInfo>(encrypted.error());
        ds.encrypted = *encrypted;
        
        auto deduplicated = reader.readBool();
        if (!deduplicated) return Err<DatasetInfo>(deduplicated.error());
        ds.deduplicated = *deduplicated;
        
        auto keyId = reader.readString();
        if (!keyId) return Err<DatasetInfo>(keyId.error());
        ds.encryption_key_id = *keyId;
        
        auto retention = reader.readVarint();
        if (!retention) return Err<DatasetInfo>(retention.error());
        ds.retention_days = static_cast<int>(*retention);
        
        auto expiration = reader.readTimestamp();
        if (!expiration) return Err<DatasetInfo>(expiration.error());
        ds.expiration_time = *expiration;
        
        auto tagCount = reader.readVarint();
        if (!tagCount) return Err<DatasetInfo>(tagCount.error());
        for (size_t i = 0; i < *tagCount; ++i) {
            auto tag = reader.readString();
            if (!tag) return Err<DatasetInfo>(tag.error());
            ds.tags.push_back(*tag);
        }
        
        return ds;
    }
    
    // Serialize VolumeInfo
    [[nodiscard]] static std::vector<uint8_t> serializeVolume(const VolumeInfo& vol) {
        BinaryWriter payload;
        
        payload.writeString(vol.name);
        payload.writeEnum(vol.tier);
        payload.writeUint64(vol.total_space);
        payload.writeUint64(vol.used_space);
        payload.writeUint64(vol.free_space);
        payload.writeBool(vol.online);
        payload.writeTimestamp(vol.created);
        payload.writeTimestamp(vol.last_accessed);
        payload.writeUint32(vol.dataset_count);
        payload.writeString(vol.device_type);
        payload.writeString(vol.storage_group);
        
        return wrapWithHeader(BinaryRecordType::VOLUME_ENTRY, payload.buffer());
    }
    
    // Deserialize VolumeInfo
    [[nodiscard]] static Result<VolumeInfo> deserializeVolume(const std::vector<uint8_t>& data) {
        auto headerResult = validateAndStripHeader(data, BinaryRecordType::VOLUME_ENTRY);
        if (!headerResult) return Err<VolumeInfo>(headerResult.error());
        
        BinaryReader reader(*headerResult);
        VolumeInfo vol;
        
        auto name = reader.readString();
        if (!name) return Err<VolumeInfo>(name.error());
        vol.name = *name;
        
        auto tier = reader.readEnum<StorageTier>();
        if (!tier) return Err<VolumeInfo>(tier.error());
        vol.tier = *tier;
        
        auto totalSpace = reader.readUint64();
        if (!totalSpace) return Err<VolumeInfo>(totalSpace.error());
        vol.total_space = *totalSpace;
        
        auto usedSpace = reader.readUint64();
        if (!usedSpace) return Err<VolumeInfo>(usedSpace.error());
        vol.used_space = *usedSpace;
        
        auto freeSpace = reader.readUint64();
        if (!freeSpace) return Err<VolumeInfo>(freeSpace.error());
        vol.free_space = *freeSpace;
        
        auto online = reader.readBool();
        if (!online) return Err<VolumeInfo>(online.error());
        vol.online = *online;
        
        auto created = reader.readTimestamp();
        if (!created) return Err<VolumeInfo>(created.error());
        vol.created = *created;
        
        auto lastAccessed = reader.readTimestamp();
        if (!lastAccessed) return Err<VolumeInfo>(lastAccessed.error());
        vol.last_accessed = *lastAccessed;
        
        auto datasetCount = reader.readUint32();
        if (!datasetCount) return Err<VolumeInfo>(datasetCount.error());
        vol.dataset_count = *datasetCount;
        
        auto deviceType = reader.readString();
        if (!deviceType) return Err<VolumeInfo>(deviceType.error());
        vol.device_type = *deviceType;
        
        auto storageGroup = reader.readString();
        if (!storageGroup) return Err<VolumeInfo>(storageGroup.error());
        vol.storage_group = *storageGroup;
        
        return vol;
    }
    
    // Serialize OperationRecord
    [[nodiscard]] static std::vector<uint8_t> serializeOperation(const OperationRecord& op) {
        BinaryWriter payload;
        
        payload.writeString(op.operation_id);
        payload.writeEnum(op.type);
        payload.writeEnum(op.status);
        payload.writeString(op.dataset_name);
        payload.writeString(op.source_volume);
        payload.writeString(op.target_volume);
        payload.writeEnum(op.source_tier);
        payload.writeEnum(op.target_tier);
        payload.writeUint64(op.bytes_processed);
        payload.writeTimestamp(op.start_time);
        payload.writeTimestamp(op.end_time);
        payload.writeString(op.error_message);
        payload.writeString(op.user);
        payload.writeInt32(op.priority);
        
        return wrapWithHeader(BinaryRecordType::MIGRATION_RECORD, payload.buffer());
    }
    
    // Deserialize OperationRecord
    [[nodiscard]] static Result<OperationRecord> deserializeOperation(const std::vector<uint8_t>& data) {
        auto headerResult = validateAndStripHeader(data, BinaryRecordType::MIGRATION_RECORD);
        if (!headerResult) return Err<OperationRecord>(headerResult.error());
        
        BinaryReader reader(*headerResult);
        OperationRecord op;
        
        auto opId = reader.readString();
        if (!opId) return Err<OperationRecord>(opId.error());
        op.operation_id = *opId;
        
        auto type = reader.readEnum<OperationType>();
        if (!type) return Err<OperationRecord>(type.error());
        op.type = *type;
        
        auto status = reader.readEnum<OperationStatus>();
        if (!status) return Err<OperationRecord>(status.error());
        op.status = *status;
        
        auto dsName = reader.readString();
        if (!dsName) return Err<OperationRecord>(dsName.error());
        op.dataset_name = *dsName;
        
        auto srcVol = reader.readString();
        if (!srcVol) return Err<OperationRecord>(srcVol.error());
        op.source_volume = *srcVol;
        
        auto tgtVol = reader.readString();
        if (!tgtVol) return Err<OperationRecord>(tgtVol.error());
        op.target_volume = *tgtVol;
        
        auto srcTier = reader.readEnum<StorageTier>();
        if (!srcTier) return Err<OperationRecord>(srcTier.error());
        op.source_tier = *srcTier;
        
        auto tgtTier = reader.readEnum<StorageTier>();
        if (!tgtTier) return Err<OperationRecord>(tgtTier.error());
        op.target_tier = *tgtTier;
        
        auto bytesProcessed = reader.readUint64();
        if (!bytesProcessed) return Err<OperationRecord>(bytesProcessed.error());
        op.bytes_processed = *bytesProcessed;
        
        auto startTime = reader.readTimestamp();
        if (!startTime) return Err<OperationRecord>(startTime.error());
        op.start_time = *startTime;
        
        auto endTime = reader.readTimestamp();
        if (!endTime) return Err<OperationRecord>(endTime.error());
        op.end_time = *endTime;
        
        auto errorMsg = reader.readString();
        if (!errorMsg) return Err<OperationRecord>(errorMsg.error());
        op.error_message = *errorMsg;
        
        auto user = reader.readString();
        if (!user) return Err<OperationRecord>(user.error());
        op.user = *user;
        
        auto priority = reader.readInt32();
        if (!priority) return Err<OperationRecord>(priority.error());
        op.priority = *priority;
        
        return op;
    }
    
    // Serialize a batch of records
    template<typename T, typename SerializeFunc>
    [[nodiscard]] static std::vector<uint8_t> serializeBatch(
            const std::vector<T>& records, SerializeFunc serializeFunc) {
        BinaryWriter payload;
        
        payload.writeVarint(records.size());
        for (const auto& record : records) {
            auto serialized = serializeFunc(record);
            payload.writeBytes(serialized);
        }
        
        return wrapWithHeader(BinaryRecordType::BATCH_RECORD, payload.buffer());
    }

private:
    [[nodiscard]] static std::vector<uint8_t> wrapWithHeader(
            BinaryRecordType type, const std::vector<uint8_t>& payload) {
        BinaryWriter writer;
        
        BinaryRecordHeader header;
        header.type = type;
        header.flags = RecordFlags::CHECKSUMMED;
        header.payload_length = static_cast<uint32_t>(payload.size());
        header.checksum = CRC32::compute(payload);
        
        header.write(writer);
        writer.writeFixedBytes(payload.data(), payload.size());
        
        return writer.release();
    }
    
    [[nodiscard]] static Result<std::vector<uint8_t>> validateAndStripHeader(
            const std::vector<uint8_t>& data, BinaryRecordType expectedType) {
        if (data.size() < binary_protocol::HEADER_SIZE) {
            return Err<std::vector<uint8_t>>(ErrorCode::CORRUPTION_DETECTED, "Data too small for header");
        }
        
        BinaryReader reader(data);
        auto headerResult = BinaryRecordHeader::read(reader);
        if (!headerResult) return Err<std::vector<uint8_t>>(headerResult.error());
        
        const auto& header = *headerResult;
        
        if (header.type != expectedType) {
            return Err<std::vector<uint8_t>>(ErrorCode::INVALID_ARGUMENT, "Record type mismatch");
        }
        
        if (data.size() < binary_protocol::HEADER_SIZE + header.payload_length) {
            return Err<std::vector<uint8_t>>(ErrorCode::CORRUPTION_DETECTED, "Payload truncated");
        }
        
        std::vector<uint8_t> payload(
            data.begin() + binary_protocol::HEADER_SIZE,
            data.begin() + binary_protocol::HEADER_SIZE + header.payload_length);
        
        if (hasFlag(header.flags, RecordFlags::CHECKSUMMED)) {
            uint32_t computed = CRC32::compute(payload);
            if (computed != header.checksum) {
                return Err<std::vector<uint8_t>>(ErrorCode::CORRUPTION_DETECTED, "Checksum mismatch");
            }
        }
        
        return payload;
    }
};

//=============================================================================
// Binary Protocol Statistics
//=============================================================================

/**
 * @struct BinaryProtocolStats
 * @brief Statistics for binary protocol operations
 */
struct BinaryProtocolStats {
    std::atomic<uint64_t> records_serialized{0};
    std::atomic<uint64_t> records_deserialized{0};
    std::atomic<uint64_t> bytes_written{0};
    std::atomic<uint64_t> bytes_read{0};
    std::atomic<uint64_t> checksum_failures{0};
    std::atomic<uint64_t> parse_errors{0};
    
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << "=== Binary Protocol Statistics ===\n"
            << "Records Serialized: " << records_serialized.load() << "\n"
            << "Records Deserialized: " << records_deserialized.load() << "\n"
            << "Bytes Written: " << bytes_written.load() << "\n"
            << "Bytes Read: " << bytes_read.load() << "\n"
            << "Checksum Failures: " << checksum_failures.load() << "\n"
            << "Parse Errors: " << parse_errors.load() << "\n";
        return oss.str();
    }
};

//=============================================================================
// Binary File Writer/Reader
//=============================================================================

/**
 * @class BinaryFileWriter
 * @brief Writes binary records to a file
 */
class BinaryFileWriter {
public:
    explicit BinaryFileWriter(const std::string& filename)
        : filename_(filename), records_written_(0) {}
    
    [[nodiscard]] Result<void> open() {
        file_.open(filename_, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!file_.is_open()) {
            return Err<void>(ErrorCode::IO_ERROR, "Failed to open file: " + filename_);
        }
        
        // Write file header
        BinaryWriter header;
        header.writeUint32(binary_protocol::MAGIC_NUMBER);
        header.writeUint8(binary_protocol::PROTOCOL_VERSION);
        header.writeUint64(0);  // Placeholder for record count
        
        const auto& buf = header.buffer();
        file_.write(reinterpret_cast<const char*>(buf.data()), buf.size());
        
        return Ok();
    }
    
    [[nodiscard]] Result<void> write(const std::vector<uint8_t>& record) {
        if (!file_.is_open()) {
            return Err<void>(ErrorCode::INVALID_STATE, "File not open");
        }
        
        file_.write(reinterpret_cast<const char*>(record.data()), record.size());
        if (file_.fail()) {
            return Err<void>(ErrorCode::IO_ERROR, "Write failed");
        }
        
        ++records_written_;
        stats_.records_serialized++;
        stats_.bytes_written += record.size();
        
        return Ok();
    }
    
    [[nodiscard]] Result<void> close() {
        if (!file_.is_open()) return Ok();
        
        // Update record count in header
        file_.seekp(5);  // After magic + version
        BinaryWriter countWriter;
        countWriter.writeUint64(records_written_);
        const auto& buf = countWriter.buffer();
        file_.write(reinterpret_cast<const char*>(buf.data()), buf.size());
        
        file_.close();
        return Ok();
    }
    
    [[nodiscard]] uint64_t recordsWritten() const { return records_written_; }
    [[nodiscard]] const BinaryProtocolStats& stats() const { return stats_; }

private:
    std::string filename_;
    std::ofstream file_;
    uint64_t records_written_;
    BinaryProtocolStats stats_;
};

/**
 * @class BinaryFileReader
 * @brief Reads binary records from a file
 */
class BinaryFileReader {
public:
    explicit BinaryFileReader(const std::string& filename)
        : filename_(filename), record_count_(0), records_read_(0) {}
    
    [[nodiscard]] Result<void> open() {
        file_.open(filename_, std::ios::binary | std::ios::in);
        if (!file_.is_open()) {
            return Err<void>(ErrorCode::IO_ERROR, "Failed to open file: " + filename_);
        }
        
        // Read file header
        uint8_t header[13];
        file_.read(reinterpret_cast<char*>(header), sizeof(header));
        if (file_.gcount() != sizeof(header)) {
            return Err<void>(ErrorCode::CORRUPTION_DETECTED, "Failed to read header");
        }
        
        BinaryReader reader(header, sizeof(header));
        
        auto magic = reader.readUint32();
        if (!magic || *magic != binary_protocol::MAGIC_NUMBER) {
            return Err<void>(ErrorCode::CORRUPTION_DETECTED, "Invalid file magic");
        }
        
        auto version = reader.readUint8();
        if (!version || *version > binary_protocol::PROTOCOL_VERSION) {
            return Err<void>(ErrorCode::NOT_SUPPORTED, "Unsupported protocol version");
        }
        
        auto count = reader.readUint64();
        if (!count) return Err<void>(count.error());
        record_count_ = *count;
        
        return Ok();
    }
    
    [[nodiscard]] Result<std::vector<uint8_t>> readNext() {
        if (!file_.is_open()) {
            return Err<std::vector<uint8_t>>(ErrorCode::INVALID_STATE, "File not open");
        }
        
        if (file_.eof()) {
            return Err<std::vector<uint8_t>>(ErrorCode::NOT_FOUND, "End of file");
        }
        
        // Read header
        uint8_t headerBuf[binary_protocol::HEADER_SIZE];
        file_.read(reinterpret_cast<char*>(headerBuf), sizeof(headerBuf));
        if (file_.gcount() == 0) {
            return Err<std::vector<uint8_t>>(ErrorCode::NOT_FOUND, "End of file");
        }
        if (file_.gcount() != sizeof(headerBuf)) {
            stats_.parse_errors++;
            return Err<std::vector<uint8_t>>(ErrorCode::CORRUPTION_DETECTED, "Truncated header");
        }
        
        BinaryReader headerReader(headerBuf, sizeof(headerBuf));
        auto header = BinaryRecordHeader::read(headerReader);
        if (!header) {
            stats_.parse_errors++;
            return Err<std::vector<uint8_t>>(header.error());
        }
        
        // Read payload
        std::vector<uint8_t> record(binary_protocol::HEADER_SIZE + header->payload_length);
        std::memcpy(record.data(), headerBuf, sizeof(headerBuf));
        
        if (header->payload_length > 0) {
            file_.read(reinterpret_cast<char*>(record.data() + sizeof(headerBuf)),
                      header->payload_length);
            if (file_.gcount() != static_cast<std::streamsize>(header->payload_length)) {
                stats_.parse_errors++;
                return Err<std::vector<uint8_t>>(ErrorCode::CORRUPTION_DETECTED, "Truncated payload");
            }
        }
        
        ++records_read_;
        stats_.records_deserialized++;
        stats_.bytes_read += record.size();
        
        return record;
    }
    
    void close() {
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    [[nodiscard]] uint64_t recordCount() const { return record_count_; }
    [[nodiscard]] uint64_t recordsRead() const { return records_read_; }
    [[nodiscard]] const BinaryProtocolStats& stats() const { return stats_; }

private:
    std::string filename_;
    std::ifstream file_;
    uint64_t record_count_;
    uint64_t records_read_;
    BinaryProtocolStats stats_;
};

} // namespace dfsmshsm
#endif // DFSMSHSM_HSM_BINARY_PROTOCOL_HPP
