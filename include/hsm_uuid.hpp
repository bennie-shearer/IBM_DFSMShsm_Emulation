/**
 * @file hsm_uuid.hpp
 * @brief UUID generation utilities for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - UUID v4 (random) generation
 * - UUID parsing and formatting
 * - UUID comparison and hashing
 */

#ifndef HSM_UUID_HPP
#define HSM_UUID_HPP

#include <string>
#include <string_view>
#include <array>
#include <random>
#include <optional>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <functional>

namespace dfsmshsm {
namespace uuid {

/**
 * @brief UUID (Universally Unique Identifier)
 * 
 * Represents a 128-bit UUID in the standard format:
 * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
 */
class UUID {
public:
    /**
     * @brief Default constructor - creates nil UUID
     */
    constexpr UUID() noexcept : data_{} {}
    
    /**
     * @brief Construct from byte array
     */
    explicit constexpr UUID(std::array<uint8_t, 16> data) noexcept 
        : data_(data) {}
    
    /**
     * @brief Generate random UUID v4
     */
    [[nodiscard]] static UUID generate() {
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 gen(rd());
        static thread_local std::uniform_int_distribution<uint64_t> dist;
        
        std::array<uint8_t, 16> data;
        
        // Fill with random bytes
        uint64_t r1 = dist(gen);
        uint64_t r2 = dist(gen);
        
        for (int i = 0; i < 8; ++i) {
            data[i] = static_cast<uint8_t>(r1 >> (i * 8));
            data[i + 8] = static_cast<uint8_t>(r2 >> (i * 8));
        }
        
        // Set version to 4 (random)
        data[6] = (data[6] & 0x0F) | 0x40;
        
        // Set variant to RFC 4122
        data[8] = (data[8] & 0x3F) | 0x80;
        
        return UUID(data);
    }
    
    /**
     * @brief Create nil UUID (all zeros)
     */
    [[nodiscard]] static constexpr UUID nil() noexcept {
        return UUID();
    }
    
    /**
     * @brief Parse UUID from string
     * @param str UUID string (with or without hyphens)
     * @return Parsed UUID or nullopt on error
     */
    [[nodiscard]] static std::optional<UUID> parse(std::string_view str) {
        std::array<uint8_t, 16> data{};
        std::size_t byteIndex = 0;
        std::size_t nibbleIndex = 0;
        
        for (char c : str) {
            if (c == '-') continue;
            
            int nibble = hexValue(c);
            if (nibble < 0) return std::nullopt;
            
            if (nibbleIndex % 2 == 0) {
                data[byteIndex] = static_cast<uint8_t>(nibble << 4);
            } else {
                data[byteIndex] |= static_cast<uint8_t>(nibble);
                ++byteIndex;
            }
            ++nibbleIndex;
            
            if (byteIndex > 16) return std::nullopt;
        }
        
        if (byteIndex != 16) return std::nullopt;
        
        return UUID(data);
    }
    
    /**
     * @brief Check if UUID is nil
     */
    [[nodiscard]] constexpr bool isNil() const noexcept {
        for (uint8_t b : data_) {
            if (b != 0) return false;
        }
        return true;
    }
    
    /**
     * @brief Get UUID version
     */
    [[nodiscard]] constexpr int version() const noexcept {
        return (data_[6] >> 4) & 0x0F;
    }
    
    /**
     * @brief Get UUID variant
     */
    [[nodiscard]] constexpr int variant() const noexcept {
        uint8_t v = data_[8];
        if ((v & 0x80) == 0x00) return 0;      // NCS backward compatibility
        if ((v & 0xC0) == 0x80) return 1;      // RFC 4122
        if ((v & 0xE0) == 0xC0) return 2;      // Microsoft backward compatibility
        return 3;                               // Future definition
    }
    
    /**
     * @brief Get raw byte data
     */
    [[nodiscard]] constexpr const std::array<uint8_t, 16>& data() const noexcept {
        return data_;
    }
    
    /**
     * @brief Convert to string (lowercase with hyphens)
     */
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        
        for (int i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                oss << '-';
            }
            oss << std::setw(2) << static_cast<int>(data_[i]);
        }
        
        return oss.str();
    }
    
    /**
     * @brief Convert to uppercase string
     */
    [[nodiscard]] std::string toUpperString() const {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setfill('0');
        
        for (int i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                oss << '-';
            }
            oss << std::setw(2) << static_cast<int>(data_[i]);
        }
        
        return oss.str();
    }
    
    /**
     * @brief Convert to compact string (no hyphens)
     */
    [[nodiscard]] std::string toCompactString() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        
        for (int i = 0; i < 16; ++i) {
            oss << std::setw(2) << static_cast<int>(data_[i]);
        }
        
        return oss.str();
    }
    
    /**
     * @brief Compute hash value
     */
    [[nodiscard]] std::size_t hash() const noexcept {
        std::size_t h = 0;
        for (std::size_t i = 0; i < 16; ++i) {
            h ^= std::hash<uint8_t>{}(data_[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
    
    /**
     * @brief Equality comparison
     */
    [[nodiscard]] constexpr bool operator==(const UUID& other) const noexcept {
        return data_ == other.data_;
    }
    
    /**
     * @brief Inequality comparison
     */
    [[nodiscard]] constexpr bool operator!=(const UUID& other) const noexcept {
        return data_ != other.data_;
    }
    
    /**
     * @brief Less-than comparison (for ordering)
     */
    [[nodiscard]] constexpr bool operator<(const UUID& other) const noexcept {
        return data_ < other.data_;
    }
    
    /**
     * @brief Greater-than comparison
     */
    [[nodiscard]] constexpr bool operator>(const UUID& other) const noexcept {
        return data_ > other.data_;
    }
    
    /**
     * @brief Less-or-equal comparison
     */
    [[nodiscard]] constexpr bool operator<=(const UUID& other) const noexcept {
        return data_ <= other.data_;
    }
    
    /**
     * @brief Greater-or-equal comparison
     */
    [[nodiscard]] constexpr bool operator>=(const UUID& other) const noexcept {
        return data_ >= other.data_;
    }

private:
    std::array<uint8_t, 16> data_;
    
    [[nodiscard]] static constexpr int hexValue(char c) noexcept {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
};

/**
 * @brief Generate a new random UUID
 */
[[nodiscard]] inline UUID generate() {
    return UUID::generate();
}

/**
 * @brief Parse UUID from string
 */
[[nodiscard]] inline std::optional<UUID> parse(std::string_view str) {
    return UUID::parse(str);
}

/**
 * @brief Create nil UUID
 */
[[nodiscard]] inline constexpr UUID nil() noexcept {
    return UUID::nil();
}

/**
 * @brief Stream output operator
 */
inline std::ostream& operator<<(std::ostream& os, const UUID& uuid) {
    return os << uuid.toString();
}

} // namespace uuid
} // namespace dfsmshsm

// Hash specialization for std::hash
namespace std {
template<>
struct hash<dfsmshsm::uuid::UUID> {
    std::size_t operator()(const dfsmshsm::uuid::UUID& uuid) const noexcept {
        return uuid.hash();
    }
};
} // namespace std

#endif // HSM_UUID_HPP
