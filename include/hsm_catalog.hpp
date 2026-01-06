/**
 * @file hsm_catalog.hpp
 * @brief Catalog Integration for DFSMShsm Emulator
 * @version 4.2.0
 * 
 * Provides comprehensive catalog management including:
 * - ICF (Integrated Catalog Facility) catalog emulation
 * - GDG (Generation Data Group) support
 * - Alias management and resolution
 * - Catalog search and navigation
 * - VSAM catalog entries
 * - User catalog management
 * 
 * @copyright Copyright (c) 2024 DFSMShsm Emulator Project
 */

#ifndef HSM_CATALOG_HPP
#define HSM_CATALOG_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <optional>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <stdexcept>

namespace hsm {
namespace catalog {

// ============================================================================
// Catalog Entry Types
// ============================================================================

/**
 * @brief Types of catalog entries
 */
enum class EntryType {
    NON_VSAM,        ///< Non-VSAM dataset
    VSAM_CLUSTER,    ///< VSAM cluster
    VSAM_DATA,       ///< VSAM data component
    VSAM_INDEX,      ///< VSAM index component
    VSAM_PATH,       ///< VSAM path
    VSAM_AIX,        ///< VSAM alternate index
    GDG_BASE,        ///< GDG base entry
    GDG_GENERATION,  ///< GDG generation
    ALIAS,           ///< Alias entry
    USER_CATALOG,    ///< User catalog
    PAGE_SPACE,      ///< Page space dataset
    UNKNOWN
};

/**
 * @brief Convert EntryType to string
 */
inline std::string entryTypeToString(EntryType type) {
    switch (type) {
        case EntryType::NON_VSAM: return "NONVSAM";
        case EntryType::VSAM_CLUSTER: return "CLUSTER";
        case EntryType::VSAM_DATA: return "DATA";
        case EntryType::VSAM_INDEX: return "INDEX";
        case EntryType::VSAM_PATH: return "PATH";
        case EntryType::VSAM_AIX: return "AIX";
        case EntryType::GDG_BASE: return "GDG";
        case EntryType::GDG_GENERATION: return "GDGGEN";
        case EntryType::ALIAS: return "ALIAS";
        case EntryType::USER_CATALOG: return "UCAT";
        case EntryType::PAGE_SPACE: return "PAGESPACE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Dataset organization for non-VSAM
 */
enum class DatasetOrganization {
    PS,      ///< Physical Sequential
    PO,      ///< Partitioned Organization (PDS)
    PDSE,    ///< Partitioned Data Set Extended
    DA,      ///< Direct Access
    IS,      ///< Indexed Sequential
    UNKNOWN
};

/**
 * @brief VSAM organization types
 */
enum class VSAMOrganization {
    KSDS,    ///< Key-Sequenced Data Set
    ESDS,    ///< Entry-Sequenced Data Set
    RRDS,    ///< Relative Record Data Set
    LDS,     ///< Linear Data Set
    UNKNOWN
};

// ============================================================================
// Catalog Entry Structures
// ============================================================================

/**
 * @brief Volume information for catalog entry
 */
struct VolumeInfo {
    std::string volser;           ///< Volume serial
    std::string deviceType;       ///< Device type (3390, etc.)
    uint32_t extents;             ///< Number of extents
    uint64_t allocatedSpace;      ///< Space allocated (bytes)
    uint64_t usedSpace;           ///< Space used (bytes)
    
    VolumeInfo() : extents(0), allocatedSpace(0), usedSpace(0) {}
    
    VolumeInfo(const std::string& vol, uint64_t alloc = 0)
        : volser(vol), deviceType("3390"), extents(1), 
          allocatedSpace(alloc), usedSpace(0) {}
};

/**
 * @brief Base catalog entry
 */
struct CatalogEntry {
    std::string entryName;            ///< Full entry name
    EntryType type;                   ///< Entry type
    std::chrono::system_clock::time_point creationDate;
    std::chrono::system_clock::time_point lastModified;
    std::chrono::system_clock::time_point lastAccessed;
    std::string owningCatalog;        ///< Catalog containing this entry
    std::vector<VolumeInfo> volumes;  ///< Volume information
    std::map<std::string, std::string> attributes;
    
    CatalogEntry() 
        : type(EntryType::UNKNOWN),
          creationDate(std::chrono::system_clock::now()),
          lastModified(std::chrono::system_clock::now()),
          lastAccessed(std::chrono::system_clock::now()) {}
    
    virtual ~CatalogEntry() = default;
    
    virtual std::string toString() const {
        std::ostringstream oss;
        oss << entryTypeToString(type) << ": " << entryName;
        if (!volumes.empty()) {
            oss << " on " << volumes[0].volser;
        }
        return oss.str();
    }
    
    uint64_t getTotalAllocated() const {
        uint64_t total = 0;
        for (const auto& vol : volumes) {
            total += vol.allocatedSpace;
        }
        return total;
    }
};

/**
 * @brief Non-VSAM dataset entry
 */
struct NonVSAMEntry : public CatalogEntry {
    DatasetOrganization dsorg;
    std::string recfm;               ///< Record format
    uint16_t lrecl;                  ///< Logical record length
    uint16_t blksize;                ///< Block size
    uint32_t primaryAlloc;           ///< Primary allocation
    uint32_t secondaryAlloc;         ///< Secondary allocation
    bool smsManaged;                 ///< SMS-managed flag
    std::string storageClass;
    std::string managementClass;
    std::string dataClass;
    
    NonVSAMEntry() 
        : dsorg(DatasetOrganization::PS), lrecl(80), blksize(0),
          primaryAlloc(0), secondaryAlloc(0), smsManaged(false) {
        type = EntryType::NON_VSAM;
    }
    
    std::string toString() const override {
        std::ostringstream oss;
        oss << "NONVSAM: " << entryName << " (";
        switch (dsorg) {
            case DatasetOrganization::PS: oss << "PS"; break;
            case DatasetOrganization::PO: oss << "PO"; break;
            case DatasetOrganization::PDSE: oss << "PDSE"; break;
            case DatasetOrganization::DA: oss << "DA"; break;
            case DatasetOrganization::IS: oss << "IS"; break;
            default: oss << "?"; break;
        }
        oss << ") LRECL=" << lrecl << " BLKSIZE=" << blksize;
        return oss.str();
    }
};

/**
 * @brief VSAM cluster entry
 */
struct VSAMClusterEntry : public CatalogEntry {
    VSAMOrganization organization;
    std::string dataComponent;       ///< Data component name
    std::string indexComponent;      ///< Index component name (KSDS only)
    uint32_t ciSize;                 ///< Control interval size
    uint32_t caSize;                 ///< Control area size
    uint16_t keyLength;              ///< Key length (KSDS)
    uint16_t keyOffset;              ///< Key offset (KSDS)
    uint64_t recordCount;            ///< Number of records
    uint64_t deletedRecords;         ///< Deleted record count
    bool shareoptions12;             ///< SHAREOPTIONS(1,2)
    bool reuse;                      ///< REUSE attribute
    bool spanned;                    ///< SPANNED records
    
    VSAMClusterEntry()
        : organization(VSAMOrganization::KSDS), ciSize(4096), caSize(0),
          keyLength(0), keyOffset(0), recordCount(0), deletedRecords(0),
          shareoptions12(true), reuse(false), spanned(false) {
        type = EntryType::VSAM_CLUSTER;
    }
    
    std::string toString() const override {
        std::ostringstream oss;
        oss << "CLUSTER: " << entryName << " (";
        switch (organization) {
            case VSAMOrganization::KSDS: oss << "KSDS"; break;
            case VSAMOrganization::ESDS: oss << "ESDS"; break;
            case VSAMOrganization::RRDS: oss << "RRDS"; break;
            case VSAMOrganization::LDS: oss << "LDS"; break;
            default: oss << "?"; break;
        }
        oss << ") Records=" << recordCount;
        return oss.str();
    }
};

// ============================================================================
// GDG (Generation Data Group) Support
// ============================================================================

/**
 * @brief GDG attributes
 */
struct GDGAttributes {
    uint16_t limit;              ///< Maximum generations
    bool scratch;                ///< SCRATCH or NOSCRATCH
    bool empty;                  ///< EMPTY or NOEMPTY
    bool purge;                  ///< PURGE or NOPURGE
    bool extended;               ///< Extended format GDG
    std::string modelDsname;     ///< Model DSCB dataset name
    
    GDGAttributes()
        : limit(255), scratch(true), empty(false), purge(false), extended(false) {}
};

/**
 * @brief GDG base entry
 */
struct GDGBaseEntry : public CatalogEntry {
    GDGAttributes attributes;
    std::vector<std::string> generations;  ///< Generation names (newest first)
    int32_t currentGeneration;             ///< Current absolute generation number
    
    GDGBaseEntry() : currentGeneration(0) {
        type = EntryType::GDG_BASE;
    }
    
    /**
     * @brief Get generation by relative number
     * @param relative 0 = current, -1 = previous, +1 = next (new)
     */
    std::string getGeneration(int32_t relative) const {
        if (relative > 0) {
            // Next generation (to be created)
            return formatGenerationName(entryName, currentGeneration + relative);
        } else if (relative == 0) {
            // Current generation
            if (generations.empty()) return "";
            return generations[0];
        } else {
            // Previous generations
            size_t index = static_cast<size_t>(-relative - 1);
            if (index >= generations.size()) return "";
            return generations[index];
        }
    }
    
    /**
     * @brief Format a generation dataset name
     */
    static std::string formatGenerationName(const std::string& baseName, int32_t genNum) {
        std::ostringstream oss;
        oss << baseName << ".G" << std::setfill('0') << std::setw(4) 
            << (genNum % 10000) << "V00";
        return oss.str();
    }
    
    /**
     * @brief Parse generation number from dataset name
     */
    static std::optional<int32_t> parseGenerationNumber(const std::string& dsname) {
        // Look for .GnnnnVnn pattern
        std::regex pattern(R"(\.G(\d{4})V\d{2}$)");
        std::smatch match;
        if (std::regex_search(dsname, match, pattern)) {
            return std::stoi(match[1].str());
        }
        return std::nullopt;
    }
    
    std::string toString() const override {
        std::ostringstream oss;
        oss << "GDG: " << entryName << " LIMIT(" << attributes.limit << ") "
            << "Generations=" << generations.size();
        return oss.str();
    }
};

/**
 * @brief GDG generation entry
 */
struct GDGGenerationEntry : public NonVSAMEntry {
    std::string gdgBase;         ///< GDG base name
    int32_t absoluteGeneration;  ///< Absolute generation number
    int16_t version;             ///< Version number
    
    GDGGenerationEntry() : absoluteGeneration(0), version(0) {
        type = EntryType::GDG_GENERATION;
    }
    
    std::string toString() const override {
        std::ostringstream oss;
        oss << "GDGGEN: " << entryName << " (G" 
            << std::setfill('0') << std::setw(4) << absoluteGeneration
            << "V" << std::setw(2) << version << ")";
        return oss.str();
    }
};

// ============================================================================
// Alias Management
// ============================================================================

/**
 * @brief Alias entry
 */
struct AliasEntry : public CatalogEntry {
    std::string relatedEntryName;    ///< Entry this alias points to
    EntryType relatedEntryType;      ///< Type of related entry
    
    AliasEntry() : relatedEntryType(EntryType::UNKNOWN) {
        type = EntryType::ALIAS;
    }
    
    std::string toString() const override {
        return "ALIAS: " + entryName + " -> " + relatedEntryName;
    }
};

// ============================================================================
// ICF Catalog Implementation
// ============================================================================

/**
 * @brief Catalog search filter
 */
struct CatalogFilter {
    std::string namePattern;         ///< Name pattern (supports *)
    std::optional<EntryType> type;   ///< Filter by type
    std::optional<std::string> volume;  ///< Filter by volume
    std::optional<std::chrono::system_clock::time_point> createdAfter;
    std::optional<std::chrono::system_clock::time_point> createdBefore;
    uint32_t maxResults;             ///< Maximum results to return
    
    CatalogFilter() : namePattern("*"), maxResults(1000) {}
};

/**
 * @brief ICF (Integrated Catalog Facility) catalog
 */
class ICFCatalog {
public:
    /**
     * @brief Create a new catalog
     */
    explicit ICFCatalog(const std::string& catalogName, bool isMasterCatalog = false)
        : name_(catalogName), isMaster_(isMasterCatalog),
          creationTime_(std::chrono::system_clock::now()) {}
    
    // Catalog identification
    const std::string& getName() const { return name_; }
    bool isMasterCatalog() const { return isMaster_; }
    
    // ========================
    // Entry Management
    // ========================
    
    /**
     * @brief Define a non-VSAM dataset
     */
    bool defineNonVSAM(const NonVSAMEntry& entry) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        if (entries_.count(entry.entryName) > 0) {
            return false;  // Already exists
        }
        
        auto newEntry = std::make_shared<NonVSAMEntry>(entry);
        newEntry->owningCatalog = name_;
        entries_[entry.entryName] = newEntry;
        
        updateIndex(entry.entryName);
        return true;
    }
    
    /**
     * @brief Define a VSAM cluster
     */
    bool defineVSAMCluster(const VSAMClusterEntry& entry) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        if (entries_.count(entry.entryName) > 0) {
            return false;
        }
        
        auto newEntry = std::make_shared<VSAMClusterEntry>(entry);
        newEntry->owningCatalog = name_;
        entries_[entry.entryName] = newEntry;
        
        updateIndex(entry.entryName);
        return true;
    }
    
    /**
     * @brief Define a GDG base
     */
    bool defineGDGBase(const GDGBaseEntry& entry) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        if (entries_.count(entry.entryName) > 0) {
            return false;
        }
        
        auto newEntry = std::make_shared<GDGBaseEntry>(entry);
        newEntry->owningCatalog = name_;
        entries_[entry.entryName] = newEntry;
        gdgBases_[entry.entryName] = newEntry;
        
        updateIndex(entry.entryName);
        return true;
    }
    
    /**
     * @brief Add a GDG generation
     */
    bool addGDGGeneration(const std::string& baseName, const GDGGenerationEntry& genEntry) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto baseIt = gdgBases_.find(baseName);
        if (baseIt == gdgBases_.end()) {
            return false;  // GDG base not found
        }
        
        auto& base = baseIt->second;
        
        // Roll off oldest generation if at limit
        while (base->generations.size() >= base->attributes.limit) {
            std::string oldest = base->generations.back();
            base->generations.pop_back();
            
            if (base->attributes.scratch) {
                entries_.erase(oldest);
            }
        }
        
        // Add new generation
        auto newEntry = std::make_shared<GDGGenerationEntry>(genEntry);
        newEntry->owningCatalog = name_;
        newEntry->gdgBase = baseName;
        newEntry->absoluteGeneration = ++base->currentGeneration;
        entries_[genEntry.entryName] = newEntry;
        base->generations.insert(base->generations.begin(), genEntry.entryName);
        
        updateIndex(genEntry.entryName);
        return true;
    }
    
    /**
     * @brief Define an alias
     */
    bool defineAlias(const std::string& aliasName, const std::string& targetName) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        if (entries_.count(aliasName) > 0) {
            return false;
        }
        
        auto targetIt = entries_.find(targetName);
        if (targetIt == entries_.end()) {
            return false;  // Target not found
        }
        
        auto alias = std::make_shared<AliasEntry>();
        alias->entryName = aliasName;
        alias->relatedEntryName = targetName;
        alias->relatedEntryType = targetIt->second->type;
        alias->owningCatalog = name_;
        
        entries_[aliasName] = alias;
        aliases_[aliasName] = targetName;
        
        updateIndex(aliasName);
        return true;
    }
    
    /**
     * @brief Delete an entry
     */
    bool deleteEntry(const std::string& entryName, bool force = false) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto it = entries_.find(entryName);
        if (it == entries_.end()) {
            return false;
        }
        
        // Check for dependencies
        if (!force) {
            // Check if GDG base with generations
            if (it->second->type == EntryType::GDG_BASE) {
                auto gdgIt = gdgBases_.find(entryName);
                if (gdgIt != gdgBases_.end() && !gdgIt->second->generations.empty()) {
                    return false;  // Has generations
                }
            }
            
            // Check if aliased
            for (const auto& [alias, target] : aliases_) {
                if (target == entryName) {
                    return false;  // Has alias
                }
            }
        }
        
        // Remove from indexes
        gdgBases_.erase(entryName);
        aliases_.erase(entryName);
        
        // Remove aliases pointing to this entry
        for (auto aliasIt = aliases_.begin(); aliasIt != aliases_.end(); ) {
            if (aliasIt->second == entryName) {
                entries_.erase(aliasIt->first);
                aliasIt = aliases_.erase(aliasIt);
            } else {
                ++aliasIt;
            }
        }
        
        entries_.erase(it);
        return true;
    }
    
    // ========================
    // Lookup and Search
    // ========================
    
    /**
     * @brief Locate an entry by name
     */
    std::shared_ptr<CatalogEntry> locate(const std::string& entryName) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        auto it = entries_.find(entryName);
        if (it != entries_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    /**
     * @brief Resolve alias to actual entry
     */
    std::shared_ptr<CatalogEntry> resolveAlias(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::string current = name;
        std::set<std::string> visited;
        
        while (true) {
            auto aliasIt = aliases_.find(current);
            if (aliasIt == aliases_.end()) {
                // Not an alias, look up actual entry
                auto entryIt = entries_.find(current);
                return entryIt != entries_.end() ? entryIt->second : nullptr;
            }
            
            // Prevent circular references
            if (visited.count(current) > 0) {
                return nullptr;
            }
            visited.insert(current);
            
            current = aliasIt->second;
        }
    }
    
    /**
     * @brief Search catalog with filter
     */
    std::vector<std::shared_ptr<CatalogEntry>> search(const CatalogFilter& filter) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::vector<std::shared_ptr<CatalogEntry>> results;
        
        for (const auto& [name, entry] : entries_) {
            if (results.size() >= filter.maxResults) break;
            
            // Name pattern matching
            if (!matchPattern(name, filter.namePattern)) continue;
            
            // Type filter
            if (filter.type && entry->type != *filter.type) continue;
            
            // Volume filter
            if (filter.volume) {
                bool foundVolume = false;
                for (const auto& vol : entry->volumes) {
                    if (vol.volser == *filter.volume) {
                        foundVolume = true;
                        break;
                    }
                }
                if (!foundVolume) continue;
            }
            
            // Date filters
            if (filter.createdAfter && entry->creationDate < *filter.createdAfter) continue;
            if (filter.createdBefore && entry->creationDate > *filter.createdBefore) continue;
            
            results.push_back(entry);
        }
        
        return results;
    }
    
    /**
     * @brief List entries by high-level qualifier
     */
    std::vector<std::shared_ptr<CatalogEntry>> listByHLQ(const std::string& hlq) const {
        CatalogFilter filter;
        filter.namePattern = hlq + ".*";
        return search(filter);
    }
    
    /**
     * @brief Get GDG generation by relative reference
     */
    std::optional<std::string> resolveGDGRelative(const std::string& baseName, 
                                                   int32_t relative) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        auto it = gdgBases_.find(baseName);
        if (it == gdgBases_.end()) {
            return std::nullopt;
        }
        
        std::string gen = it->second->getGeneration(relative);
        return gen.empty() ? std::nullopt : std::optional<std::string>(gen);
    }
    
    // ========================
    // Statistics and Reporting
    // ========================
    
    /**
     * @brief Get catalog statistics
     */
    std::string getStatistics() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::map<EntryType, uint32_t> typeCounts;
        uint64_t totalSpace = 0;
        
        for (const auto& [name, entry] : entries_) {
            typeCounts[entry->type]++;
            totalSpace += entry->getTotalAllocated();
        }
        
        std::ostringstream oss;
        oss << "Catalog: " << name_ << (isMaster_ ? " (MASTER)" : "") << "\n"
            << "Created: " << formatTime(creationTime_) << "\n"
            << "Total Entries: " << entries_.size() << "\n"
            << "  Non-VSAM: " << typeCounts[EntryType::NON_VSAM] << "\n"
            << "  VSAM Clusters: " << typeCounts[EntryType::VSAM_CLUSTER] << "\n"
            << "  GDG Bases: " << typeCounts[EntryType::GDG_BASE] << "\n"
            << "  GDG Generations: " << typeCounts[EntryType::GDG_GENERATION] << "\n"
            << "  Aliases: " << typeCounts[EntryType::ALIAS] << "\n"
            << "Total Space: " << (totalSpace / (1024 * 1024)) << " MB\n";
        
        return oss.str();
    }
    
    /**
     * @brief Export catalog to LISTCAT format
     */
    std::string exportListcat(const std::string& pattern = "*") const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::ostringstream oss;
        oss << "LISTCAT -- CATALOG = " << name_ << "\n";
        oss << std::string(70, '-') << "\n";
        
        for (const auto& [name, entry] : entries_) {
            if (!matchPattern(name, pattern)) continue;
            
            oss << entry->toString() << "\n";
            
            for (const auto& vol : entry->volumes) {
                oss << "     VOLSER=" << vol.volser 
                    << " DEVT=" << vol.deviceType
                    << " EXT=" << vol.extents << "\n";
            }
        }
        
        return oss.str();
    }

private:
    std::string name_;
    bool isMaster_;
    std::chrono::system_clock::time_point creationTime_;
    
    std::map<std::string, std::shared_ptr<CatalogEntry>> entries_;
    std::map<std::string, std::shared_ptr<GDGBaseEntry>> gdgBases_;
    std::map<std::string, std::string> aliases_;  // alias -> target
    
    // Index by HLQ for faster searches
    std::map<std::string, std::set<std::string>> hlqIndex_;
    
    mutable std::shared_mutex mutex_;
    
    void updateIndex(const std::string& entryName) {
        auto dotPos = entryName.find('.');
        std::string hlq = dotPos != std::string::npos ? 
                          entryName.substr(0, dotPos) : entryName;
        hlqIndex_[hlq].insert(entryName);
    }
    
    bool matchPattern(const std::string& name, const std::string& pattern) const {
        if (pattern == "*") return true;
        
        // Convert pattern to regex
        std::string regexPattern;
        for (char c : pattern) {
            switch (c) {
                case '*': regexPattern += ".*"; break;
                case '?': regexPattern += "."; break;
                case '.': regexPattern += "\\."; break;
                default: regexPattern += c; break;
            }
        }
        
        try {
            std::regex re(regexPattern, std::regex::icase);
            return std::regex_match(name, re);
        } catch (...) {
            return false;
        }
    }
    
    std::string formatTime(const std::chrono::system_clock::time_point& tp) const {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::tm* tm = std::localtime(&time);
        std::ostringstream oss;
        oss << std::put_time(tm, "%Y/%m/%d %H:%M:%S");
        return oss.str();
    }
};

// ============================================================================
// Catalog Manager - Multi-Catalog Support
// ============================================================================

/**
 * @brief Catalog connector for user catalogs
 */
struct CatalogConnector {
    std::string alias;           ///< Alias for user catalog
    std::string catalogName;     ///< Actual catalog name
    std::string hlqPattern;      ///< HLQ pattern to route to this catalog
    bool updateOnly;             ///< Connector only for updates
    
    CatalogConnector() : updateOnly(false) {}
};

/**
 * @brief Manages multiple catalogs in a catalog structure
 */
class CatalogManager {
public:
    CatalogManager() {
        // Create master catalog
        masterCatalog_ = std::make_shared<ICFCatalog>("MASTER.CATALOG", true);
    }
    
    /**
     * @brief Get the master catalog
     */
    ICFCatalog& getMasterCatalog() { return *masterCatalog_; }
    const ICFCatalog& getMasterCatalog() const { return *masterCatalog_; }
    
    /**
     * @brief Define a user catalog
     */
    bool defineUserCatalog(const std::string& catalogName, 
                           const std::string& alias = "",
                           const std::string& hlqPattern = "") {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        if (userCatalogs_.count(catalogName) > 0) {
            return false;
        }
        
        auto catalog = std::make_shared<ICFCatalog>(catalogName, false);
        userCatalogs_[catalogName] = catalog;
        
        // Create connector
        if (!alias.empty() || !hlqPattern.empty()) {
            CatalogConnector connector;
            connector.catalogName = catalogName;
            connector.alias = alias.empty() ? catalogName : alias;
            connector.hlqPattern = hlqPattern;
            connectors_.push_back(connector);
            
            // Define alias in master catalog
            if (!alias.empty()) {
                aliasToUserCatalog_[alias] = catalogName;
            }
        }
        
        return true;
    }
    
    /**
     * @brief Get a user catalog
     */
    std::shared_ptr<ICFCatalog> getUserCatalog(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        auto it = userCatalogs_.find(name);
        return it != userCatalogs_.end() ? it->second : nullptr;
    }
    
    /**
     * @brief Locate an entry across all catalogs
     */
    std::pair<std::shared_ptr<ICFCatalog>, std::shared_ptr<CatalogEntry>> 
    locate(const std::string& entryName) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        // Determine which catalog to search based on HLQ
        auto catalog = findCatalogForEntry(entryName);
        if (!catalog) {
            catalog = masterCatalog_;
        }
        
        auto entry = catalog->locate(entryName);
        if (entry) {
            return {catalog, entry};
        }
        
        // Search master catalog if not found
        if (catalog != masterCatalog_) {
            entry = masterCatalog_->locate(entryName);
            if (entry) {
                return {masterCatalog_, entry};
            }
        }
        
        // Search all user catalogs
        for (const auto& [name, ucat] : userCatalogs_) {
            if (ucat != catalog) {
                entry = ucat->locate(entryName);
                if (entry) {
                    return {ucat, entry};
                }
            }
        }
        
        return {nullptr, nullptr};
    }
    
    /**
     * @brief Define entry in appropriate catalog
     */
    bool defineNonVSAM(const NonVSAMEntry& entry) {
        auto catalog = findCatalogForEntry(entry.entryName);
        if (!catalog) {
            catalog = masterCatalog_;
        }
        return catalog->defineNonVSAM(entry);
    }
    
    bool defineVSAMCluster(const VSAMClusterEntry& entry) {
        auto catalog = findCatalogForEntry(entry.entryName);
        if (!catalog) {
            catalog = masterCatalog_;
        }
        return catalog->defineVSAMCluster(entry);
    }
    
    bool defineGDGBase(const GDGBaseEntry& entry) {
        auto catalog = findCatalogForEntry(entry.entryName);
        if (!catalog) {
            catalog = masterCatalog_;
        }
        return catalog->defineGDGBase(entry);
    }
    
    /**
     * @brief Delete entry from appropriate catalog
     */
    bool deleteEntry(const std::string& entryName, bool force = false) {
        auto [catalog, entry] = locate(entryName);
        if (!catalog || !entry) {
            return false;
        }
        return catalog->deleteEntry(entryName, force);
    }
    
    /**
     * @brief Search across all catalogs
     */
    std::vector<std::pair<std::string, std::shared_ptr<CatalogEntry>>> 
    searchAll(const CatalogFilter& filter) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::vector<std::pair<std::string, std::shared_ptr<CatalogEntry>>> results;
        
        // Search master catalog
        for (const auto& entry : masterCatalog_->search(filter)) {
            results.push_back({masterCatalog_->getName(), entry});
        }
        
        // Search user catalogs
        for (const auto& [name, catalog] : userCatalogs_) {
            for (const auto& entry : catalog->search(filter)) {
                results.push_back({name, entry});
            }
        }
        
        return results;
    }
    
    /**
     * @brief Get overall statistics
     */
    std::string getStatistics() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::ostringstream oss;
        oss << "=== Catalog Manager Statistics ===\n\n"
            << "Master Catalog:\n"
            << masterCatalog_->getStatistics() << "\n";
        
        for (const auto& [name, catalog] : userCatalogs_) {
            oss << "User Catalog " << name << ":\n"
                << catalog->getStatistics() << "\n";
        }
        
        return oss.str();
    }
    
    /**
     * @brief List all catalogs
     */
    std::vector<std::string> listCatalogs() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::vector<std::string> result;
        result.push_back(masterCatalog_->getName());
        
        for (const auto& [name, catalog] : userCatalogs_) {
            result.push_back(name);
        }
        
        return result;
    }

private:
    std::shared_ptr<ICFCatalog> masterCatalog_;
    std::map<std::string, std::shared_ptr<ICFCatalog>> userCatalogs_;
    std::vector<CatalogConnector> connectors_;
    std::map<std::string, std::string> aliasToUserCatalog_;
    mutable std::shared_mutex mutex_;
    
    std::shared_ptr<ICFCatalog> findCatalogForEntry(const std::string& entryName) const {
        // Extract HLQ
        auto dotPos = entryName.find('.');
        std::string hlq = dotPos != std::string::npos ? 
                          entryName.substr(0, dotPos) : entryName;
        
        // Check aliases
        auto aliasIt = aliasToUserCatalog_.find(hlq);
        if (aliasIt != aliasToUserCatalog_.end()) {
            auto catIt = userCatalogs_.find(aliasIt->second);
            if (catIt != userCatalogs_.end()) {
                return catIt->second;
            }
        }
        
        // Check connectors by pattern
        for (const auto& connector : connectors_) {
            if (!connector.hlqPattern.empty()) {
                if (matchHLQ(hlq, connector.hlqPattern)) {
                    auto catIt = userCatalogs_.find(connector.catalogName);
                    if (catIt != userCatalogs_.end()) {
                        return catIt->second;
                    }
                }
            }
        }
        
        return nullptr;  // Use master catalog
    }
    
    bool matchHLQ(const std::string& hlq, const std::string& pattern) const {
        if (pattern == "*") return true;
        if (pattern.back() == '*') {
            return hlq.compare(0, pattern.size() - 1, pattern, 0, pattern.size() - 1) == 0;
        }
        return hlq == pattern;
    }
};

// ============================================================================
// GDG Management Utilities
// ============================================================================

/**
 * @brief GDG management operations
 */
class GDGManager {
public:
    explicit GDGManager(CatalogManager& catalogManager)
        : catalogManager_(catalogManager) {}
    
    /**
     * @brief Define a new GDG base
     */
    bool defineGDG(const std::string& baseName, const GDGAttributes& attrs) {
        GDGBaseEntry base;
        base.entryName = baseName;
        base.attributes = attrs;
        return catalogManager_.defineGDGBase(base);
    }
    
    /**
     * @brief Create a new generation
     */
    std::optional<std::string> createGeneration(const std::string& baseName,
                                                 const NonVSAMEntry& template_entry = NonVSAMEntry()) {
        auto [catalog, entry] = catalogManager_.locate(baseName);
        if (!entry || entry->type != EntryType::GDG_BASE) {
            return std::nullopt;
        }
        
        auto gdgBase = std::dynamic_pointer_cast<GDGBaseEntry>(entry);
        if (!gdgBase) {
            return std::nullopt;
        }
        
        // Create new generation name
        std::string genName = GDGBaseEntry::formatGenerationName(
            baseName, gdgBase->currentGeneration + 1);
        
        GDGGenerationEntry genEntry;
        static_cast<NonVSAMEntry&>(genEntry) = template_entry;
        genEntry.entryName = genName;
        genEntry.gdgBase = baseName;
        
        if (catalog->addGDGGeneration(baseName, genEntry)) {
            return genName;
        }
        
        return std::nullopt;
    }
    
    /**
     * @brief Roll GDG (remove oldest generation)
     */
    bool rollGDG(const std::string& baseName) {
        auto [catalog, entry] = catalogManager_.locate(baseName);
        if (!entry || entry->type != EntryType::GDG_BASE) {
            return false;
        }
        
        auto gdgBase = std::dynamic_pointer_cast<GDGBaseEntry>(entry);
        if (!gdgBase || gdgBase->generations.empty()) {
            return false;
        }
        
        // Delete oldest generation
        std::string oldest = gdgBase->generations.back();
        return catalog->deleteEntry(oldest, true);
    }
    
    /**
     * @brief Resolve relative GDG reference
     */
    std::optional<std::string> resolve(const std::string& gdgReference) const {
        // Parse GDG reference like "BASE.GDG(0)" or "BASE.GDG(-1)"
        std::regex pattern(R"((.+)\(([+-]?\d+)\)$)");
        std::smatch match;
        
        if (!std::regex_match(gdgReference, match, pattern)) {
            return std::nullopt;
        }
        
        std::string baseName = match[1].str();
        int32_t relative = std::stoi(match[2].str());
        
        auto [catalog, entry] = catalogManager_.locate(baseName);
        if (!entry || entry->type != EntryType::GDG_BASE) {
            return std::nullopt;
        }
        
        auto gdgBase = std::dynamic_pointer_cast<GDGBaseEntry>(entry);
        if (!gdgBase) {
            return std::nullopt;
        }
        
        std::string gen = gdgBase->getGeneration(relative);
        return gen.empty() ? std::nullopt : std::optional<std::string>(gen);
    }
    
    /**
     * @brief List all generations for a GDG
     */
    std::vector<std::string> listGenerations(const std::string& baseName) const {
        auto [catalog, entry] = catalogManager_.locate(baseName);
        if (!entry || entry->type != EntryType::GDG_BASE) {
            return {};
        }
        
        auto gdgBase = std::dynamic_pointer_cast<GDGBaseEntry>(entry);
        return gdgBase ? gdgBase->generations : std::vector<std::string>();
    }
    
    /**
     * @brief Get GDG information
     */
    std::string getGDGInfo(const std::string& baseName) const {
        auto [catalog, entry] = catalogManager_.locate(baseName);
        if (!entry || entry->type != EntryType::GDG_BASE) {
            return "GDG base not found: " + baseName;
        }
        
        auto gdgBase = std::dynamic_pointer_cast<GDGBaseEntry>(entry);
        if (!gdgBase) {
            return "Invalid GDG base: " + baseName;
        }
        
        std::ostringstream oss;
        oss << "GDG Base: " << baseName << "\n"
            << "  Limit: " << gdgBase->attributes.limit << "\n"
            << "  SCRATCH: " << (gdgBase->attributes.scratch ? "YES" : "NO") << "\n"
            << "  EMPTY: " << (gdgBase->attributes.empty ? "YES" : "NO") << "\n"
            << "  Current Generation: " << gdgBase->currentGeneration << "\n"
            << "  Active Generations: " << gdgBase->generations.size() << "\n";
        
        for (size_t i = 0; i < gdgBase->generations.size(); i++) {
            oss << "    (" << -(static_cast<int>(i)) << ") " 
                << gdgBase->generations[i] << "\n";
        }
        
        return oss.str();
    }

private:
    CatalogManager& catalogManager_;
};

// ============================================================================
// Alias Manager
// ============================================================================

/**
 * @brief Manages catalog aliases
 */
class AliasManager {
public:
    explicit AliasManager(CatalogManager& catalogManager)
        : catalogManager_(catalogManager) {}
    
    /**
     * @brief Define an alias for a dataset
     */
    bool defineAlias(const std::string& aliasName, const std::string& targetName) {
        auto [catalog, entry] = catalogManager_.locate(targetName);
        if (!catalog || !entry) {
            return false;
        }
        return catalog->defineAlias(aliasName, targetName);
    }
    
    /**
     * @brief Delete an alias
     */
    bool deleteAlias(const std::string& aliasName) {
        auto [catalog, entry] = catalogManager_.locate(aliasName);
        if (!entry || entry->type != EntryType::ALIAS) {
            return false;
        }
        return catalog->deleteEntry(aliasName, false);
    }
    
    /**
     * @brief Resolve an alias to its target
     */
    std::optional<std::string> resolveAlias(const std::string& name) const {
        auto [catalog, entry] = catalogManager_.locate(name);
        if (!entry) {
            return std::nullopt;
        }
        
        if (entry->type != EntryType::ALIAS) {
            return name;  // Not an alias, return original
        }
        
        auto aliasEntry = std::dynamic_pointer_cast<AliasEntry>(entry);
        if (!aliasEntry) {
            return std::nullopt;
        }
        
        return aliasEntry->relatedEntryName;
    }
    
    /**
     * @brief List all aliases in a catalog
     */
    std::vector<std::pair<std::string, std::string>> listAliases(
            const std::string& catalogName = "") const {
        CatalogFilter filter;
        filter.type = EntryType::ALIAS;
        
        std::vector<std::pair<std::string, std::string>> result;
        
        auto entries = catalogManager_.searchAll(filter);
        for (const auto& [catName, entry] : entries) {
            if (!catalogName.empty() && catName != catalogName) continue;
            
            auto aliasEntry = std::dynamic_pointer_cast<AliasEntry>(entry);
            if (aliasEntry) {
                result.push_back({aliasEntry->entryName, aliasEntry->relatedEntryName});
            }
        }
        
        return result;
    }

private:
    CatalogManager& catalogManager_;
};

} // namespace catalog
} // namespace hsm

#endif // HSM_CATALOG_HPP
