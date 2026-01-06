/**
 * @file hsm_dasd.hpp
 * @brief Extended DASD Support for DFSMShsm Emulator
 * @version 4.2.0
 * 
 * Provides comprehensive DASD (Direct Access Storage Device) emulation including:
 * - Track and cylinder allocation management
 * - 3390 geometry emulation (Model 1, 2, 3, 9, 27, 54)
 * - Space management and fragmentation analysis
 * - VTOC (Volume Table of Contents) management
 * - Dataset extent tracking
 * - Free space management
 * 
 * @copyright Copyright (c) 2024 DFSMShsm Emulator Project
 */

#ifndef HSM_DASD_HPP
#define HSM_DASD_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <cstdint>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace hsm {
namespace dasd {

// ============================================================================
// DASD Device Types and Geometry
// ============================================================================

/**
 * @brief DASD device types supported
 */
enum class DeviceType {
    IBM_3390_1,      ///< 3390 Model 1 - 1.1 GB
    IBM_3390_2,      ///< 3390 Model 2 - 2.2 GB
    IBM_3390_3,      ///< 3390 Model 3 - 3.3 GB
    IBM_3390_9,      ///< 3390 Model 9 - 10 GB (extended)
    IBM_3390_27,     ///< 3390 Model 27 - 30 GB
    IBM_3390_54,     ///< 3390 Model 54 - 60 GB
    IBM_3380_E,      ///< 3380 Model E - legacy
    IBM_3380_K,      ///< 3380 Model K - legacy
    VIRTUAL_DASD,    ///< Virtual DASD (configurable)
    UNKNOWN
};

/**
 * @brief Convert DeviceType to string
 */
inline std::string deviceTypeToString(DeviceType type) {
    switch (type) {
        case DeviceType::IBM_3390_1: return "3390-1";
        case DeviceType::IBM_3390_2: return "3390-2";
        case DeviceType::IBM_3390_3: return "3390-3";
        case DeviceType::IBM_3390_9: return "3390-9";
        case DeviceType::IBM_3390_27: return "3390-27";
        case DeviceType::IBM_3390_54: return "3390-54";
        case DeviceType::IBM_3380_E: return "3380-E";
        case DeviceType::IBM_3380_K: return "3380-K";
        case DeviceType::VIRTUAL_DASD: return "VIRTUAL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief DASD geometry parameters
 */
struct DeviceGeometry {
    uint32_t cylinders;           ///< Total cylinders
    uint32_t headsPerCylinder;    ///< Tracks per cylinder
    uint32_t bytesPerTrack;       ///< Bytes per track
    uint32_t blocksPerTrack;      ///< Maximum blocks per track
    uint64_t totalBytes;          ///< Total device capacity
    std::string modelName;        ///< Device model name
    
    DeviceGeometry() : cylinders(0), headsPerCylinder(0), bytesPerTrack(0),
                       blocksPerTrack(0), totalBytes(0), modelName("UNKNOWN") {}
    
    DeviceGeometry(uint32_t cyls, uint32_t heads, uint32_t bytesTrack, 
                   uint32_t blocks, const std::string& name)
        : cylinders(cyls), headsPerCylinder(heads), bytesPerTrack(bytesTrack),
          blocksPerTrack(blocks), modelName(name) {
        totalBytes = static_cast<uint64_t>(cylinders) * headsPerCylinder * bytesPerTrack;
    }
    
    uint32_t totalTracks() const { return cylinders * headsPerCylinder; }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << modelName << " [" << cylinders << " cyls x " << headsPerCylinder 
            << " heads = " << totalTracks() << " tracks, "
            << (totalBytes / (1024 * 1024)) << " MB]";
        return oss.str();
    }
};

/**
 * @brief Factory for creating device geometries
 */
class GeometryFactory {
public:
    /**
     * @brief Get geometry for a specific device type
     */
    static DeviceGeometry getGeometry(DeviceType type) {
        switch (type) {
            case DeviceType::IBM_3390_1:
                return DeviceGeometry(1113, 15, 56664, 12, "3390-1");
            case DeviceType::IBM_3390_2:
                return DeviceGeometry(2226, 15, 56664, 12, "3390-2");
            case DeviceType::IBM_3390_3:
                return DeviceGeometry(3339, 15, 56664, 12, "3390-3");
            case DeviceType::IBM_3390_9:
                return DeviceGeometry(10017, 15, 56664, 12, "3390-9");
            case DeviceType::IBM_3390_27:
                return DeviceGeometry(32760, 15, 56664, 12, "3390-27");
            case DeviceType::IBM_3390_54:
                return DeviceGeometry(65520, 15, 56664, 12, "3390-54");
            case DeviceType::IBM_3380_E:
                return DeviceGeometry(1770, 15, 47476, 10, "3380-E");
            case DeviceType::IBM_3380_K:
                return DeviceGeometry(2655, 15, 47476, 10, "3380-K");
            case DeviceType::VIRTUAL_DASD:
                // Default virtual geometry - configurable
                return DeviceGeometry(10000, 15, 56664, 12, "VIRTUAL");
            default:
                return DeviceGeometry();
        }
    }
    
    /**
     * @brief Create custom geometry for virtual DASD
     */
    static DeviceGeometry createCustom(uint32_t cylinders, uint32_t heads,
                                        uint32_t bytesPerTrack, const std::string& name) {
        uint32_t blocksPerTrack = bytesPerTrack / 4096;  // Assume 4K blocks
        return DeviceGeometry(cylinders, heads, bytesPerTrack, blocksPerTrack, name);
    }
};

// ============================================================================
// Track and Extent Management
// ============================================================================

/**
 * @brief CCHH address (Cylinder-Cylinder-Head-Head)
 */
struct CCHHAddress {
    uint16_t cylinder;
    uint16_t head;
    
    CCHHAddress() : cylinder(0), head(0) {}
    CCHHAddress(uint16_t cyl, uint16_t hd) : cylinder(cyl), head(hd) {}
    
    bool operator<(const CCHHAddress& other) const {
        if (cylinder != other.cylinder) return cylinder < other.cylinder;
        return head < other.head;
    }
    
    bool operator==(const CCHHAddress& other) const {
        return cylinder == other.cylinder && head == other.head;
    }
    
    bool operator<=(const CCHHAddress& other) const {
        return *this < other || *this == other;
    }
    
    uint32_t toAbsoluteTrack(uint16_t headsPerCyl) const {
        return static_cast<uint32_t>(cylinder) * headsPerCyl + head;
    }
    
    static CCHHAddress fromAbsoluteTrack(uint32_t track, uint16_t headsPerCyl) {
        return CCHHAddress(static_cast<uint16_t>(track / headsPerCyl),
                          static_cast<uint16_t>(track % headsPerCyl));
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << std::setfill('0') << std::hex << std::uppercase
            << std::setw(4) << cylinder << std::setw(4) << head;
        return oss.str();
    }
};

/**
 * @brief Dataset extent - contiguous space allocation
 */
struct Extent {
    CCHHAddress start;        ///< Starting CCHH
    CCHHAddress end;          ///< Ending CCHH
    uint8_t extentNumber;     ///< Extent sequence number (0-15)
    uint8_t extentType;       ///< Extent type (0=data, 1=overflow, 2=index)
    
    Extent() : extentNumber(0), extentType(0) {}
    
    Extent(const CCHHAddress& s, const CCHHAddress& e, uint8_t num = 0, uint8_t type = 0)
        : start(s), end(e), extentNumber(num), extentType(type) {}
    
    /**
     * @brief Calculate tracks in extent
     */
    uint32_t tracks(uint16_t headsPerCyl) const {
        uint32_t startTrack = start.toAbsoluteTrack(headsPerCyl);
        uint32_t endTrack = end.toAbsoluteTrack(headsPerCyl);
        return endTrack - startTrack + 1;
    }
    
    /**
     * @brief Calculate cylinders spanned
     */
    uint32_t cylinders() const {
        return end.cylinder - start.cylinder + 1;
    }
    
    /**
     * @brief Check if extent contains an address
     */
    bool contains(const CCHHAddress& addr, uint16_t headsPerCyl) const {
        uint32_t track = addr.toAbsoluteTrack(headsPerCyl);
        uint32_t startTrack = start.toAbsoluteTrack(headsPerCyl);
        uint32_t endTrack = end.toAbsoluteTrack(headsPerCyl);
        return track >= startTrack && track <= endTrack;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Extent[" << static_cast<int>(extentNumber) << "]: "
            << start.toString() << "-" << end.toString();
        return oss.str();
    }
};

/**
 * @brief Allocation unit specification
 */
enum class AllocationUnit {
    TRACKS,          ///< Allocate by tracks
    CYLINDERS,       ///< Allocate by cylinders
    BLOCKS,          ///< Allocate by blocks
    KILOBYTES,       ///< Allocate by KB
    MEGABYTES,       ///< Allocate by MB
    RECORDS,         ///< Allocate by record count
    AVERAGE_BLOCKS   ///< Average block length
};

/**
 * @brief Space allocation request
 */
struct AllocationRequest {
    AllocationUnit unit;
    uint32_t primaryQuantity;
    uint32_t secondaryQuantity;
    uint32_t directoryBlocks;     ///< For PDS
    uint16_t recordLength;        ///< LRECL
    uint16_t blockSize;           ///< BLKSIZE
    std::string recfm;            ///< Record format (FB, VB, etc.)
    bool contig;                  ///< Require contiguous space
    bool round;                   ///< Round to cylinder boundary
    bool rlse;                    ///< Release unused space
    std::string volser;           ///< Specific volume request
    
    AllocationRequest()
        : unit(AllocationUnit::TRACKS), primaryQuantity(1), secondaryQuantity(0),
          directoryBlocks(0), recordLength(80), blockSize(0), recfm("FB"),
          contig(false), round(false), rlse(false) {}
    
    /**
     * @brief Convert request to tracks
     */
    uint32_t toTracks(const DeviceGeometry& geom) const {
        switch (unit) {
            case AllocationUnit::CYLINDERS:
                return primaryQuantity * geom.headsPerCylinder;
            case AllocationUnit::TRACKS:
                return primaryQuantity;
            case AllocationUnit::BLOCKS:
                return (primaryQuantity + geom.blocksPerTrack - 1) / geom.blocksPerTrack;
            case AllocationUnit::KILOBYTES:
                return (primaryQuantity * 1024 + geom.bytesPerTrack - 1) / geom.bytesPerTrack;
            case AllocationUnit::MEGABYTES:
                return (primaryQuantity * 1024 * 1024 + geom.bytesPerTrack - 1) / geom.bytesPerTrack;
            case AllocationUnit::RECORDS:
                if (blockSize == 0 || recordLength == 0) return primaryQuantity;
                return (primaryQuantity * recordLength + geom.bytesPerTrack - 1) / geom.bytesPerTrack;
            default:
                return primaryQuantity;
        }
    }
    
    /**
     * @brief Convert secondary to tracks
     */
    uint32_t secondaryToTracks(const DeviceGeometry& geom) const {
        AllocationRequest sec = *this;
        sec.primaryQuantity = secondaryQuantity;
        return sec.toTracks(geom);
    }
};

// ============================================================================
// VTOC Management
// ============================================================================

/**
 * @brief Format types for DSCB
 */
enum class DSCBFormat {
    FORMAT_1,    ///< Standard dataset DSCB
    FORMAT_2,    ///< ISAM dataset DSCB
    FORMAT_3,    ///< Additional extents
    FORMAT_4,    ///< VTOC DSCB
    FORMAT_5,    ///< Free space DSCB
    FORMAT_7,    ///< Extended free space
    FORMAT_8,    ///< Extended format DSCB
    FORMAT_9     ///< Large format DSCB
};

/**
 * @brief Dataset organization types
 */
enum class DatasetOrg {
    PS,          ///< Physical Sequential
    PO,          ///< Partitioned (PDS)
    PDSE,        ///< Partitioned Extended
    DA,          ///< Direct Access
    IS,          ///< Indexed Sequential (ISAM)
    VSAM,        ///< VSAM (various types)
    HFS,         ///< Hierarchical File System
    UNKNOWN
};

/**
 * @brief Format 1 DSCB - Primary dataset descriptor
 */
struct Format1DSCB {
    std::string datasetName;      ///< 44-byte dataset name
    std::string volser;           ///< 6-byte volume serial
    uint16_t creationDate;        ///< Julian date (YYDDD or YYYYDDD)
    uint16_t expirationDate;      ///< Expiration date
    uint8_t extentCount;          ///< Number of extents (0-16)
    DatasetOrg dsorg;             ///< Dataset organization
    std::string recfm;            ///< Record format
    uint16_t blockSize;           ///< Block size
    uint16_t lrecl;               ///< Logical record length
    uint8_t keyLength;            ///< Key length (ISAM/VSAM)
    std::vector<Extent> extents;  ///< Up to 3 extents in F1
    uint64_t bytesUsed;           ///< Space used
    bool smsManaged;              ///< SMS-managed flag
    std::string storageClass;     ///< SMS storage class
    std::string managementClass;  ///< SMS management class
    std::string dataClass;        ///< SMS data class
    
    Format1DSCB() 
        : creationDate(0), expirationDate(0), extentCount(0),
          dsorg(DatasetOrg::PS), blockSize(0), lrecl(80), keyLength(0),
          bytesUsed(0), smsManaged(false) {}
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "F1DSCB: " << datasetName << " on " << volser
            << " [" << static_cast<int>(extentCount) << " extents, "
            << (bytesUsed / 1024) << " KB used]";
        return oss.str();
    }
};

/**
 * @brief Format 4 DSCB - VTOC descriptor
 */
struct Format4DSCB {
    std::string volser;           ///< Volume serial
    CCHHAddress vtocStart;        ///< VTOC starting address
    CCHHAddress vtocEnd;          ///< VTOC ending address
    uint32_t dscbCount;           ///< Number of DSCBs
    uint32_t freeDscbs;           ///< Free DSCB slots
    uint32_t freeTracks;          ///< Free tracks on volume
    uint32_t freeCylinders;       ///< Free cylinders
    uint32_t largestFreeExtent;   ///< Largest free extent (tracks)
    DeviceType deviceType;        ///< Device type
    bool indexed;                 ///< VTOC indexed flag
    
    Format4DSCB()
        : dscbCount(0), freeDscbs(0), freeTracks(0), freeCylinders(0),
          largestFreeExtent(0), deviceType(DeviceType::IBM_3390_3), indexed(true) {}
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "F4DSCB: Vol=" << volser << " VTOC at " 
            << vtocStart.toString() << "-" << vtocEnd.toString()
            << " [" << dscbCount << " DSCBs, " << freeTracks << " free tracks]";
        return oss.str();
    }
};

/**
 * @brief Format 5 DSCB - Free space descriptor
 */
struct Format5DSCB {
    struct FreeExtent {
        CCHHAddress start;
        uint16_t tracks;
    };
    
    std::vector<FreeExtent> freeExtents;  ///< Up to 26 free extents
    
    uint32_t totalFreeTracks() const {
        uint32_t total = 0;
        for (const auto& ext : freeExtents) {
            total += ext.tracks;
        }
        return total;
    }
};

/**
 * @brief VTOC (Volume Table of Contents) manager
 */
class VTOCManager {
public:
    /**
     * @brief Initialize VTOC for a volume
     */
    VTOCManager(const std::string& volser, const DeviceGeometry& geometry)
        : volser_(volser), geometry_(geometry) {
        initializeVTOC();
    }
    
    /**
     * @brief Get Format 4 DSCB (volume descriptor)
     */
    const Format4DSCB& getFormat4() const { return format4_; }
    
    /**
     * @brief Add a dataset DSCB
     */
    bool addDataset(const Format1DSCB& dscb) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        if (datasets_.count(dscb.datasetName) > 0) {
            return false;  // Dataset already exists
        }
        
        datasets_[dscb.datasetName] = dscb;
        format4_.dscbCount++;
        format4_.freeDscbs--;
        
        // Update free space
        updateFreeSpace(dscb);
        
        return true;
    }
    
    /**
     * @brief Remove a dataset DSCB
     */
    bool removeDataset(const std::string& dsname) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto it = datasets_.find(dsname);
        if (it == datasets_.end()) {
            return false;
        }
        
        // Return extents to free space
        returnExtentsToFreeSpace(it->second);
        
        datasets_.erase(it);
        format4_.dscbCount--;
        format4_.freeDscbs++;
        
        return true;
    }
    
    /**
     * @brief Find a dataset
     */
    std::optional<Format1DSCB> findDataset(const std::string& dsname) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        auto it = datasets_.find(dsname);
        if (it != datasets_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    /**
     * @brief List all datasets matching pattern
     */
    std::vector<Format1DSCB> listDatasets(const std::string& pattern = "*") const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::vector<Format1DSCB> result;
        for (const auto& [name, dscb] : datasets_) {
            if (matchPattern(name, pattern)) {
                result.push_back(dscb);
            }
        }
        return result;
    }
    
    /**
     * @brief Get free space information
     */
    const Format5DSCB& getFreeSpace() const { return format5_; }
    
    /**
     * @brief Get total free tracks
     */
    uint32_t getFreeTracks() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return format4_.freeTracks;
    }
    
    /**
     * @brief Get largest free extent
     */
    uint32_t getLargestFreeExtent() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return format4_.largestFreeExtent;
    }
    
    /**
     * @brief Allocate space for a dataset
     */
    std::optional<std::vector<Extent>> allocateSpace(const AllocationRequest& request) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        uint32_t tracksNeeded = request.toTracks(geometry_);
        
        // Round to cylinder if requested
        if (request.round) {
            tracksNeeded = ((tracksNeeded + geometry_.headsPerCylinder - 1) 
                           / geometry_.headsPerCylinder) * geometry_.headsPerCylinder;
        }
        
        if (request.contig) {
            return allocateContiguous(tracksNeeded);
        } else {
            return allocateNonContiguous(tracksNeeded);
        }
    }
    
    /**
     * @brief Get VTOC statistics
     */
    std::string getStatistics() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::ostringstream oss;
        oss << "VTOC Statistics for " << volser_ << ":\n"
            << "  Device: " << geometry_.toString() << "\n"
            << "  Datasets: " << datasets_.size() << "\n"
            << "  Free DSCBs: " << format4_.freeDscbs << "\n"
            << "  Free Tracks: " << format4_.freeTracks << "\n"
            << "  Free Cylinders: " << format4_.freeCylinders << "\n"
            << "  Largest Free Extent: " << format4_.largestFreeExtent << " tracks\n"
            << "  Fragmentation: " << calculateFragmentation() << "%\n";
        
        return oss.str();
    }

private:
    std::string volser_;
    DeviceGeometry geometry_;
    Format4DSCB format4_;
    Format5DSCB format5_;
    std::map<std::string, Format1DSCB> datasets_;
    mutable std::shared_mutex mutex_;
    
    void initializeVTOC() {
        format4_.volser = volser_;
        format4_.vtocStart = CCHHAddress(0, 1);  // Cylinder 0, track 1
        format4_.vtocEnd = CCHHAddress(0, 14);   // Standard VTOC size
        format4_.dscbCount = 1;  // Format 4 DSCB
        format4_.freeDscbs = 500;  // Typical VTOC capacity
        format4_.deviceType = DeviceType::IBM_3390_3;
        format4_.indexed = true;
        
        // Initialize free space - entire volume minus VTOC
        uint32_t vtocTracks = 14;
        format4_.freeTracks = geometry_.totalTracks() - vtocTracks;
        format4_.freeCylinders = format4_.freeTracks / geometry_.headsPerCylinder;
        format4_.largestFreeExtent = format4_.freeTracks;
        
        // Create initial free extent
        Format5DSCB::FreeExtent freeExt;
        freeExt.start = CCHHAddress(1, 0);  // Start after VTOC
        freeExt.tracks = static_cast<uint16_t>(std::min(format4_.freeTracks, 65535u));
        format5_.freeExtents.push_back(freeExt);
    }
    
    void updateFreeSpace(const Format1DSCB& dscb) {
        uint32_t tracksUsed = 0;
        for (const auto& ext : dscb.extents) {
            tracksUsed += ext.tracks(geometry_.headsPerCylinder);
        }
        
        format4_.freeTracks -= tracksUsed;
        format4_.freeCylinders = format4_.freeTracks / geometry_.headsPerCylinder;
        recalculateLargestFreeExtent();
    }
    
    void returnExtentsToFreeSpace(const Format1DSCB& dscb) {
        for (const auto& ext : dscb.extents) {
            Format5DSCB::FreeExtent freeExt;
            freeExt.start = ext.start;
            freeExt.tracks = static_cast<uint16_t>(ext.tracks(geometry_.headsPerCylinder));
            format5_.freeExtents.push_back(freeExt);
        }
        
        uint32_t tracksFreed = 0;
        for (const auto& ext : dscb.extents) {
            tracksFreed += ext.tracks(geometry_.headsPerCylinder);
        }
        
        format4_.freeTracks += tracksFreed;
        format4_.freeCylinders = format4_.freeTracks / geometry_.headsPerCylinder;
        coalesceFreeSpace();
        recalculateLargestFreeExtent();
    }
    
    void coalesceFreeSpace() {
        if (format5_.freeExtents.size() < 2) return;
        
        // Sort by starting address
        std::sort(format5_.freeExtents.begin(), format5_.freeExtents.end(),
            [](const Format5DSCB::FreeExtent& a, const Format5DSCB::FreeExtent& b) {
                return a.start < b.start;
            });
        
        // Merge adjacent extents
        std::vector<Format5DSCB::FreeExtent> merged;
        merged.push_back(format5_.freeExtents[0]);
        
        for (size_t i = 1; i < format5_.freeExtents.size(); i++) {
            auto& last = merged.back();
            auto& curr = format5_.freeExtents[i];
            
            // Check if adjacent
            uint32_t lastEnd = last.start.toAbsoluteTrack(geometry_.headsPerCylinder) + last.tracks;
            uint32_t currStart = curr.start.toAbsoluteTrack(geometry_.headsPerCylinder);
            
            if (lastEnd == currStart) {
                // Merge
                last.tracks = static_cast<uint16_t>(std::min(
                    static_cast<uint32_t>(last.tracks) + curr.tracks, 65535u));
            } else {
                merged.push_back(curr);
            }
        }
        
        format5_.freeExtents = std::move(merged);
    }
    
    void recalculateLargestFreeExtent() {
        format4_.largestFreeExtent = 0;
        for (const auto& ext : format5_.freeExtents) {
            if (ext.tracks > format4_.largestFreeExtent) {
                format4_.largestFreeExtent = ext.tracks;
            }
        }
    }
    
    std::optional<std::vector<Extent>> allocateContiguous(uint32_t tracks) {
        // Find a single free extent large enough
        for (auto it = format5_.freeExtents.begin(); it != format5_.freeExtents.end(); ++it) {
            if (it->tracks >= tracks) {
                Extent allocated(it->start, 
                    CCHHAddress::fromAbsoluteTrack(
                        it->start.toAbsoluteTrack(geometry_.headsPerCylinder) + tracks - 1,
                        geometry_.headsPerCylinder));
                
                // Update free extent
                if (it->tracks == tracks) {
                    format5_.freeExtents.erase(it);
                } else {
                    it->tracks -= static_cast<uint16_t>(tracks);
                    it->start = CCHHAddress::fromAbsoluteTrack(
                        it->start.toAbsoluteTrack(geometry_.headsPerCylinder) + tracks,
                        geometry_.headsPerCylinder);
                }
                
                recalculateLargestFreeExtent();
                return std::vector<Extent>{allocated};
            }
        }
        
        return std::nullopt;  // No contiguous space available
    }
    
    std::optional<std::vector<Extent>> allocateNonContiguous(uint32_t tracks) {
        std::vector<Extent> allocated;
        uint32_t remaining = tracks;
        uint8_t extentNum = 0;
        
        for (auto it = format5_.freeExtents.begin(); 
             it != format5_.freeExtents.end() && remaining > 0 && extentNum < 16; ) {
            
            uint32_t toAllocate = std::min(remaining, static_cast<uint32_t>(it->tracks));
            
            Extent ext(it->start,
                CCHHAddress::fromAbsoluteTrack(
                    it->start.toAbsoluteTrack(geometry_.headsPerCylinder) + toAllocate - 1,
                    geometry_.headsPerCylinder),
                extentNum++);
            allocated.push_back(ext);
            
            remaining -= toAllocate;
            
            if (it->tracks == toAllocate) {
                it = format5_.freeExtents.erase(it);
            } else {
                it->tracks -= static_cast<uint16_t>(toAllocate);
                it->start = CCHHAddress::fromAbsoluteTrack(
                    it->start.toAbsoluteTrack(geometry_.headsPerCylinder) + toAllocate,
                    geometry_.headsPerCylinder);
                ++it;
            }
        }
        
        if (remaining > 0) {
            // Not enough space - return extents
            for (const auto& ext : allocated) {
                Format5DSCB::FreeExtent freeExt;
                freeExt.start = ext.start;
                freeExt.tracks = static_cast<uint16_t>(ext.tracks(geometry_.headsPerCylinder));
                format5_.freeExtents.push_back(freeExt);
            }
            coalesceFreeSpace();
            return std::nullopt;
        }
        
        recalculateLargestFreeExtent();
        return allocated;
    }
    
    double calculateFragmentation() const {
        if (format4_.freeTracks == 0) return 0.0;
        
        // Fragmentation = 1 - (largest_extent / total_free)
        double frag = 1.0 - (static_cast<double>(format4_.largestFreeExtent) / format4_.freeTracks);
        return frag * 100.0;
    }
    
    bool matchPattern(const std::string& name, const std::string& pattern) const {
        if (pattern == "*") return true;
        
        // Simple wildcard matching
        size_t nameIdx = 0, patIdx = 0;
        size_t starIdx = std::string::npos, matchIdx = 0;
        
        while (nameIdx < name.size()) {
            if (patIdx < pattern.size() && 
                (pattern[patIdx] == name[nameIdx] || pattern[patIdx] == '?')) {
                nameIdx++;
                patIdx++;
            } else if (patIdx < pattern.size() && pattern[patIdx] == '*') {
                starIdx = patIdx;
                matchIdx = nameIdx;
                patIdx++;
            } else if (starIdx != std::string::npos) {
                patIdx = starIdx + 1;
                matchIdx++;
                nameIdx = matchIdx;
            } else {
                return false;
            }
        }
        
        while (patIdx < pattern.size() && pattern[patIdx] == '*') {
            patIdx++;
        }
        
        return patIdx == pattern.size();
    }
};

// ============================================================================
// DASD Volume Manager
// ============================================================================

/**
 * @brief Volume status
 */
enum class VolumeStatus {
    ONLINE,
    OFFLINE,
    PENDING,
    RESERVED,
    BOXED,
    MAINTENANCE
};

/**
 * @brief DASD volume representation
 */
class DASDVolume {
public:
    /**
     * @brief Create a new DASD volume
     */
    DASDVolume(const std::string& volser, DeviceType deviceType)
        : volser_(volser), deviceType_(deviceType), 
          geometry_(GeometryFactory::getGeometry(deviceType)),
          status_(VolumeStatus::ONLINE),
          vtoc_(std::make_unique<VTOCManager>(volser, geometry_)) {}
    
    // Getters
    const std::string& getVolser() const { return volser_; }
    DeviceType getDeviceType() const { return deviceType_; }
    const DeviceGeometry& getGeometry() const { return geometry_; }
    VolumeStatus getStatus() const { return status_; }
    VTOCManager& getVTOC() { return *vtoc_; }
    const VTOCManager& getVTOC() const { return *vtoc_; }
    
    // Status management
    void setStatus(VolumeStatus status) { status_ = status; }
    bool isOnline() const { return status_ == VolumeStatus::ONLINE; }
    
    /**
     * @brief Get volume utilization percentage
     */
    double getUtilization() const {
        uint32_t totalTracks = geometry_.totalTracks();
        uint32_t freeTracks = vtoc_->getFreeTracks();
        return 100.0 * (totalTracks - freeTracks) / totalTracks;
    }
    
    /**
     * @brief Get volume summary
     */
    std::string getSummary() const {
        std::ostringstream oss;
        oss << "Volume " << volser_ << " (" << deviceTypeToString(deviceType_) << ")\n"
            << "  Status: " << statusToString(status_) << "\n"
            << "  Capacity: " << (geometry_.totalBytes / (1024 * 1024)) << " MB\n"
            << "  Utilization: " << std::fixed << std::setprecision(1) 
            << getUtilization() << "%\n"
            << "  Free Tracks: " << vtoc_->getFreeTracks() << "\n"
            << "  Largest Free: " << vtoc_->getLargestFreeExtent() << " tracks\n";
        return oss.str();
    }
    
private:
    std::string volser_;
    DeviceType deviceType_;
    DeviceGeometry geometry_;
    VolumeStatus status_;
    std::unique_ptr<VTOCManager> vtoc_;
    
    std::string statusToString(VolumeStatus s) const {
        switch (s) {
            case VolumeStatus::ONLINE: return "ONLINE";
            case VolumeStatus::OFFLINE: return "OFFLINE";
            case VolumeStatus::PENDING: return "PENDING";
            case VolumeStatus::RESERVED: return "RESERVED";
            case VolumeStatus::BOXED: return "BOXED";
            case VolumeStatus::MAINTENANCE: return "MAINTENANCE";
            default: return "UNKNOWN";
        }
    }
};

// ============================================================================
// Space Management and Fragmentation Analysis
// ============================================================================

/**
 * @brief Fragmentation analysis results
 */
struct FragmentationAnalysis {
    double fragmentationPercent;      ///< Overall fragmentation
    uint32_t freeExtentCount;         ///< Number of free extents
    uint32_t averageExtentSize;       ///< Average free extent size
    uint32_t largestFreeExtent;       ///< Largest contiguous free space
    uint32_t smallExtentCount;        ///< Extents < 15 tracks
    std::string recommendation;       ///< Defragmentation recommendation
    
    FragmentationAnalysis()
        : fragmentationPercent(0), freeExtentCount(0), averageExtentSize(0),
          largestFreeExtent(0), smallExtentCount(0) {}
};

/**
 * @brief Space reclamation request
 */
struct ReclaimRequest {
    std::string volser;
    uint32_t targetFreeTracks;
    bool aggressive;               ///< Use aggressive reclamation
    bool migrateIfNeeded;          ///< Allow migration to free space
    std::vector<std::string> excludeDatasets;
    
    ReclaimRequest() : targetFreeTracks(0), aggressive(false), migrateIfNeeded(true) {}
};

/**
 * @brief Space management for DASD volumes
 */
class SpaceManager {
public:
    SpaceManager() = default;
    
    /**
     * @brief Add a volume to management
     */
    void addVolume(std::shared_ptr<DASDVolume> volume) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        volumes_[volume->getVolser()] = volume;
    }
    
    /**
     * @brief Remove a volume from management
     */
    bool removeVolume(const std::string& volser) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        return volumes_.erase(volser) > 0;
    }
    
    /**
     * @brief Get a volume
     */
    std::shared_ptr<DASDVolume> getVolume(const std::string& volser) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = volumes_.find(volser);
        return it != volumes_.end() ? it->second : nullptr;
    }
    
    /**
     * @brief Analyze volume fragmentation
     */
    FragmentationAnalysis analyzeFragmentation(const std::string& volser) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        FragmentationAnalysis analysis;
        auto it = volumes_.find(volser);
        if (it == volumes_.end()) {
            return analysis;
        }
        
        const auto& vtoc = it->second->getVTOC();
        const auto& freeSpace = vtoc.getFreeSpace();
        
        analysis.freeExtentCount = static_cast<uint32_t>(freeSpace.freeExtents.size());
        analysis.largestFreeExtent = vtoc.getLargestFreeExtent();
        
        uint32_t totalFree = freeSpace.totalFreeTracks();
        if (totalFree > 0 && analysis.freeExtentCount > 0) {
            analysis.averageExtentSize = totalFree / analysis.freeExtentCount;
            analysis.fragmentationPercent = 
                100.0 * (1.0 - static_cast<double>(analysis.largestFreeExtent) / totalFree);
        }
        
        // Count small extents
        for (const auto& ext : freeSpace.freeExtents) {
            if (ext.tracks < 15) {
                analysis.smallExtentCount++;
            }
        }
        
        // Generate recommendation
        if (analysis.fragmentationPercent < 20) {
            analysis.recommendation = "No action needed - fragmentation is low";
        } else if (analysis.fragmentationPercent < 50) {
            analysis.recommendation = "Consider scheduling defragmentation during maintenance window";
        } else {
            analysis.recommendation = "High fragmentation - immediate defragmentation recommended";
        }
        
        return analysis;
    }
    
    /**
     * @brief Find volumes with available space
     */
    std::vector<std::string> findVolumesWithSpace(uint32_t tracksNeeded, 
                                                   bool contiguous = false) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::vector<std::string> result;
        for (const auto& [volser, volume] : volumes_) {
            if (!volume->isOnline()) continue;
            
            const auto& vtoc = volume->getVTOC();
            if (contiguous) {
                if (vtoc.getLargestFreeExtent() >= tracksNeeded) {
                    result.push_back(volser);
                }
            } else {
                if (vtoc.getFreeTracks() >= tracksNeeded) {
                    result.push_back(volser);
                }
            }
        }
        
        // Sort by available space (descending)
        std::sort(result.begin(), result.end(),
            [this](const std::string& a, const std::string& b) {
                return volumes_.at(a)->getVTOC().getFreeTracks() >
                       volumes_.at(b)->getVTOC().getFreeTracks();
            });
        
        return result;
    }
    
    /**
     * @brief Get aggregate space statistics
     */
    std::string getAggregateStatistics() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        uint64_t totalCapacity = 0;
        uint64_t totalFree = 0;
        uint32_t onlineCount = 0;
        uint32_t offlineCount = 0;
        
        for (const auto& [volser, volume] : volumes_) {
            totalCapacity += volume->getGeometry().totalBytes;
            totalFree += static_cast<uint64_t>(volume->getVTOC().getFreeTracks()) * 
                         volume->getGeometry().bytesPerTrack;
            
            if (volume->isOnline()) {
                onlineCount++;
            } else {
                offlineCount++;
            }
        }
        
        std::ostringstream oss;
        oss << "DASD Space Management Summary:\n"
            << "  Total Volumes: " << volumes_.size() << "\n"
            << "  Online: " << onlineCount << ", Offline: " << offlineCount << "\n"
            << "  Total Capacity: " << (totalCapacity / (1024 * 1024 * 1024)) << " GB\n"
            << "  Total Free: " << (totalFree / (1024 * 1024 * 1024)) << " GB\n"
            << "  Overall Utilization: " << std::fixed << std::setprecision(1)
            << (100.0 * (totalCapacity - totalFree) / totalCapacity) << "%\n";
        
        return oss.str();
    }
    
    /**
     * @brief Recommend volume for allocation
     */
    std::optional<std::string> recommendVolume(const AllocationRequest& request) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::vector<std::pair<std::string, double>> candidates;
        
        for (const auto& [volser, volume] : volumes_) {
            if (!volume->isOnline()) continue;
            
            // Check if volume matches request
            if (!request.volser.empty() && request.volser != volser) continue;
            
            uint32_t tracksNeeded = request.toTracks(volume->getGeometry());
            const auto& vtoc = volume->getVTOC();
            
            if (request.contig) {
                if (vtoc.getLargestFreeExtent() < tracksNeeded) continue;
            } else {
                if (vtoc.getFreeTracks() < tracksNeeded) continue;
            }
            
            // Score based on available space and fragmentation
            double utilizationScore = 1.0 - volume->getUtilization() / 100.0;
            double fragScore = 1.0 - analyzeFragmentationInternal(volser).fragmentationPercent / 100.0;
            double score = utilizationScore * 0.6 + fragScore * 0.4;
            
            candidates.push_back({volser, score});
        }
        
        if (candidates.empty()) {
            return std::nullopt;
        }
        
        // Return highest scored volume
        std::sort(candidates.begin(), candidates.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        return candidates[0].first;
    }

private:
    std::map<std::string, std::shared_ptr<DASDVolume>> volumes_;
    mutable std::shared_mutex mutex_;
    
    FragmentationAnalysis analyzeFragmentationInternal(const std::string& volser) const {
        FragmentationAnalysis analysis;
        auto it = volumes_.find(volser);
        if (it == volumes_.end()) return analysis;
        
        const auto& vtoc = it->second->getVTOC();
        const auto& freeSpace = vtoc.getFreeSpace();
        
        analysis.freeExtentCount = static_cast<uint32_t>(freeSpace.freeExtents.size());
        analysis.largestFreeExtent = vtoc.getLargestFreeExtent();
        
        uint32_t totalFree = freeSpace.totalFreeTracks();
        if (totalFree > 0 && analysis.freeExtentCount > 0) {
            analysis.fragmentationPercent = 
                100.0 * (1.0 - static_cast<double>(analysis.largestFreeExtent) / totalFree);
        }
        
        return analysis;
    }
};

// ============================================================================
// Track I/O Operations
// ============================================================================

/**
 * @brief Track data representation
 */
struct TrackData {
    CCHHAddress address;
    std::vector<uint8_t> data;
    bool modified;
    std::chrono::steady_clock::time_point lastAccess;
    
    TrackData() : modified(false), lastAccess(std::chrono::steady_clock::now()) {}
    
    explicit TrackData(const CCHHAddress& addr, size_t size = 56664)
        : address(addr), data(size, 0), modified(false),
          lastAccess(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Count-Key-Data record format
 */
struct CKDRecord {
    uint8_t count[8];        ///< CCHHR + key length + data length
    std::vector<uint8_t> key;
    std::vector<uint8_t> data;
    
    CKDRecord() {
        std::fill(std::begin(count), std::end(count), 0);
    }
    
    uint16_t getKeyLength() const { return key.size(); }
    uint16_t getDataLength() const { return data.size(); }
    size_t totalSize() const { return 8 + key.size() + data.size(); }
};

/**
 * @brief Track I/O manager
 */
class TrackIOManager {
public:
    /**
     * @brief Create track I/O manager
     */
    explicit TrackIOManager(const DeviceGeometry& geometry, size_t cacheSize = 100)
        : geometry_(geometry), maxCacheSize_(cacheSize) {}
    
    /**
     * @brief Read a track
     */
    std::optional<TrackData> readTrack(const CCHHAddress& address) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        auto it = cache_.find(addressToKey(address));
        if (it != cache_.end()) {
            it->second.lastAccess = std::chrono::steady_clock::now();
            readHits_++;
            return it->second;
        }
        
        readMisses_++;
        // In emulation, create empty track if not cached
        TrackData track(address, geometry_.bytesPerTrack);
        cacheTrack(track);
        return track;
    }
    
    /**
     * @brief Write a track
     */
    bool writeTrack(const TrackData& track) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        TrackData t = track;
        t.modified = true;
        t.lastAccess = std::chrono::steady_clock::now();
        cacheTrack(t);
        
        writeCount_++;
        return true;
    }
    
    /**
     * @brief Write CKD records to a track
     */
    bool writeRecords(const CCHHAddress& address, const std::vector<CKDRecord>& records) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        TrackData track(address, geometry_.bytesPerTrack);
        
        size_t offset = 0;
        for (const auto& record : records) {
            if (offset + record.totalSize() > track.data.size()) {
                return false;  // Track overflow
            }
            
            // Write count field
            std::copy(std::begin(record.count), std::end(record.count),
                     track.data.begin() + offset);
            offset += 8;
            
            // Write key
            if (!record.key.empty()) {
                std::copy(record.key.begin(), record.key.end(),
                         track.data.begin() + offset);
                offset += record.key.size();
            }
            
            // Write data
            std::copy(record.data.begin(), record.data.end(),
                     track.data.begin() + offset);
            offset += record.data.size();
        }
        
        track.modified = true;
        cacheTrack(track);
        return true;
    }
    
    /**
     * @brief Flush modified tracks
     */
    size_t flushModified() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        size_t flushed = 0;
        for (auto& [key, track] : cache_) {
            if (track.modified) {
                // In real implementation, write to storage
                track.modified = false;
                flushed++;
            }
        }
        
        return flushed;
    }
    
    /**
     * @brief Get cache statistics
     */
    std::string getCacheStatistics() const {
        std::unique_lock<std::mutex> lock(mutex_);
        
        double hitRate = (readHits_ + readMisses_ > 0) ?
            100.0 * readHits_ / (readHits_ + readMisses_) : 0.0;
        
        std::ostringstream oss;
        oss << "Track I/O Cache Statistics:\n"
            << "  Cache Size: " << cache_.size() << "/" << maxCacheSize_ << "\n"
            << "  Read Hits: " << readHits_ << "\n"
            << "  Read Misses: " << readMisses_ << "\n"
            << "  Hit Rate: " << std::fixed << std::setprecision(1) << hitRate << "%\n"
            << "  Writes: " << writeCount_ << "\n";
        
        return oss.str();
    }

private:
    DeviceGeometry geometry_;
    size_t maxCacheSize_;
    std::map<uint64_t, TrackData> cache_;
    mutable std::mutex mutex_;
    
    uint64_t readHits_ = 0;
    uint64_t readMisses_ = 0;
    uint64_t writeCount_ = 0;
    
    uint64_t addressToKey(const CCHHAddress& addr) const {
        return (static_cast<uint64_t>(addr.cylinder) << 16) | addr.head;
    }
    
    void cacheTrack(const TrackData& track) {
        if (cache_.size() >= maxCacheSize_) {
            evictLRU();
        }
        cache_[addressToKey(track.address)] = track;
    }
    
    void evictLRU() {
        if (cache_.empty()) return;
        
        auto oldest = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it->second.lastAccess < oldest->second.lastAccess) {
                oldest = it;
            }
        }
        
        // Flush if modified
        if (oldest->second.modified) {
            // In real implementation, write to storage
        }
        
        cache_.erase(oldest);
    }
};

// ============================================================================
// Integrated DASD Manager
// ============================================================================

/**
 * @brief Comprehensive DASD management system
 */
class DASDManager {
public:
    DASDManager() : spaceManager_(std::make_unique<SpaceManager>()) {}
    
    /**
     * @brief Initialize a new volume
     */
    std::shared_ptr<DASDVolume> initializeVolume(const std::string& volser,
                                                   DeviceType deviceType) {
        auto volume = std::make_shared<DASDVolume>(volser, deviceType);
        spaceManager_->addVolume(volume);
        
        // Create track I/O manager for the volume
        ioManagers_[volser] = std::make_unique<TrackIOManager>(volume->getGeometry());
        
        return volume;
    }
    
    /**
     * @brief Get a volume
     */
    std::shared_ptr<DASDVolume> getVolume(const std::string& volser) const {
        return spaceManager_->getVolume(volser);
    }
    
    /**
     * @brief Get space manager
     */
    SpaceManager& getSpaceManager() { return *spaceManager_; }
    const SpaceManager& getSpaceManager() const { return *spaceManager_; }
    
    /**
     * @brief Get track I/O manager for a volume
     */
    TrackIOManager* getIOManager(const std::string& volser) {
        auto it = ioManagers_.find(volser);
        return it != ioManagers_.end() ? it->second.get() : nullptr;
    }
    
    /**
     * @brief Allocate a dataset
     */
    bool allocateDataset(const std::string& dsname, const AllocationRequest& request) {
        auto volser = spaceManager_->recommendVolume(request);
        if (!volser) {
            return false;
        }
        
        auto volume = spaceManager_->getVolume(*volser);
        if (!volume) {
            return false;
        }
        
        auto extents = volume->getVTOC().allocateSpace(request);
        if (!extents) {
            return false;
        }
        
        // Create DSCB
        Format1DSCB dscb;
        dscb.datasetName = dsname;
        dscb.volser = *volser;
        dscb.extents = *extents;
        dscb.extentCount = static_cast<uint8_t>(extents->size());
        dscb.blockSize = request.blockSize;
        dscb.lrecl = request.recordLength;
        dscb.recfm = request.recfm;
        dscb.creationDate = getCurrentJulianDate();
        
        return volume->getVTOC().addDataset(dscb);
    }
    
    /**
     * @brief Delete a dataset
     */
    bool deleteDataset(const std::string& dsname, const std::string& volser) {
        auto volume = spaceManager_->getVolume(volser);
        if (!volume) {
            return false;
        }
        
        return volume->getVTOC().removeDataset(dsname);
    }
    
    /**
     * @brief Get system statistics
     */
    std::string getStatistics() const {
        std::ostringstream oss;
        oss << "=== DASD Manager Statistics ===\n\n"
            << spaceManager_->getAggregateStatistics() << "\n";
        
        for (const auto& [volser, ioMgr] : ioManagers_) {
            oss << "Volume " << volser << ":\n"
                << ioMgr->getCacheStatistics() << "\n";
        }
        
        return oss.str();
    }

private:
    std::unique_ptr<SpaceManager> spaceManager_;
    std::map<std::string, std::unique_ptr<TrackIOManager>> ioManagers_;
    
    uint16_t getCurrentJulianDate() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&time);
        return static_cast<uint16_t>((tm->tm_year % 100) * 1000 + tm->tm_yday + 1);
    }
};

} // namespace dasd
} // namespace hsm

#endif // HSM_DASD_HPP
