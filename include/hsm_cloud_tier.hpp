/*
 * DFSMShsm Emulator - Cloud Tiering Integration
 * @version 4.2.0
 * 
 * Provides cloud storage tiering capabilities including:
 * - S3-compatible object storage integration
 * - Azure Blob Storage integration
 * - Cloud migration policies
 * - Transparent recall from cloud
 * - Cost optimization and analytics
 * - Hybrid storage management
 * 
 * Author: DFSMShsm Emulator Team
 * License: MIT
 */

#ifndef HSM_CLOUD_TIER_HPP
#define HSM_CLOUD_TIER_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <variant>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <cmath>

namespace dfsmshsm {
namespace cloud {

// ============================================================================
// Forward Declarations
// ============================================================================

class CloudProvider;
class S3Provider;
class AzureProvider;
class CloudMigrationPolicy;
class CloudTierManager;
class CostAnalyzer;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Supported cloud providers
 */
enum class CloudProviderType {
    AWS_S3,           // Amazon S3
    AZURE_BLOB,       // Azure Blob Storage
    GCP_STORAGE,      // Google Cloud Storage
    IBM_COS,          // IBM Cloud Object Storage
    MINIO,            // MinIO (S3-compatible)
    CUSTOM_S3         // Custom S3-compatible
};

/**
 * @brief Cloud storage tiers/classes
 */
enum class CloudStorageClass {
    // AWS S3 classes
    S3_STANDARD,
    S3_STANDARD_IA,       // Infrequent Access
    S3_INTELLIGENT,
    S3_GLACIER,
    S3_GLACIER_DEEP,
    
    // Azure classes
    AZURE_HOT,
    AZURE_COOL,
    AZURE_COLD,
    AZURE_ARCHIVE,
    
    // GCP classes
    GCP_STANDARD,
    GCP_NEARLINE,
    GCP_COLDLINE,
    GCP_ARCHIVE,
    
    // Generic
    HOT,
    WARM,
    COLD,
    ARCHIVE
};

/**
 * @brief Migration status
 */
enum class MigrationStatus {
    PENDING,
    IN_PROGRESS,
    COMPLETED,
    FAILED,
    CANCELLED,
    VALIDATING
};

/**
 * @brief Recall priority
 */
enum class RecallPriority {
    EXPEDITED,    // Fastest (most expensive)
    STANDARD,     // Normal processing
    BULK          // Lowest cost, slowest
};

/**
 * @brief Transfer direction
 */
enum class TransferDirection {
    TO_CLOUD,       // Migration to cloud
    FROM_CLOUD,     // Recall from cloud
    CLOUD_TO_CLOUD  // Inter-cloud transfer
};

/**
 * @brief Connection status
 */
enum class ConnectionStatus {
    CONNECTED,
    DISCONNECTED,
    CONNECTING,
    ERROR,
    RATE_LIMITED
};

// ============================================================================
// Utility Structures
// ============================================================================

/**
 * @brief Cloud provider credentials
 */
struct CloudCredentials {
    std::string accessKeyId;
    std::string secretAccessKey;
    std::string sessionToken;        // For temporary credentials
    std::string region;
    std::string endpoint;            // Custom endpoint for S3-compatible
    std::string accountName;         // Azure account name
    std::string accountKey;          // Azure account key
    std::string connectionString;    // Full connection string
    bool useIAMRole{false};          // Use IAM role instead of keys
    bool useSSL{true};
    uint32_t port{443};
    
    bool isValid() const {
        if (useIAMRole) return true;
        if (!accessKeyId.empty() && !secretAccessKey.empty()) return true;
        if (!accountName.empty() && !accountKey.empty()) return true;
        if (!connectionString.empty()) return true;
        return false;
    }
};

/**
 * @brief Cloud bucket/container configuration
 */
struct CloudBucketConfig {
    std::string name;
    std::string prefix;              // Object key prefix
    CloudStorageClass storageClass{CloudStorageClass::S3_STANDARD};
    bool versioningEnabled{false};
    bool encryptionEnabled{true};
    std::string encryptionKeyId;     // KMS key ID
    bool lifecycleEnabled{false};
    uint32_t lifecycleTransitionDays{90};
    CloudStorageClass lifecycleTargetClass{CloudStorageClass::S3_GLACIER};
    std::map<std::string, std::string> tags;
    
    std::string getFullPrefix() const {
        if (prefix.empty()) return "";
        if (prefix.back() == '/') return prefix;
        return prefix + "/";
    }
};

/**
 * @brief Cloud object metadata
 */
struct CloudObjectMetadata {
    std::string key;
    std::string etag;
    uint64_t size{0};
    CloudStorageClass storageClass{CloudStorageClass::S3_STANDARD};
    std::chrono::system_clock::time_point lastModified;
    std::chrono::system_clock::time_point uploadTime;
    std::map<std::string, std::string> userMetadata;
    std::string contentType;
    std::string checksumSHA256;
    bool isGlacierRestoreInProgress{false};
    std::chrono::system_clock::time_point glacierRestoreExpiry;
    
    // Original dataset info
    std::string datasetName;
    uint64_t originalSize{0};
    std::string compressionType;
    double compressionRatio{1.0};
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Key=" << key << ", Size=" << size 
            << ", ETag=" << etag;
        return oss.str();
    }
};

/**
 * @brief Transfer progress tracking
 */
struct TransferProgress {
    uint64_t bytesTransferred{0};
    uint64_t totalBytes{0};
    double transferRateMBps{0.0};
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point lastUpdateTime;
    uint32_t retryCount{0};
    std::string currentPart;
    uint32_t partsCompleted{0};
    uint32_t totalParts{0};
    
    double getProgressPercent() const {
        if (totalBytes == 0) return 0.0;
        return (static_cast<double>(bytesTransferred) / totalBytes) * 100.0;
    }
    
    std::chrono::seconds getEstimatedTimeRemaining() const {
        if (transferRateMBps <= 0 || bytesTransferred >= totalBytes) {
            return std::chrono::seconds(0);
        }
        double remainingMB = (totalBytes - bytesTransferred) / (1024.0 * 1024.0);
        return std::chrono::seconds(static_cast<int64_t>(remainingMB / transferRateMBps));
    }
};

/**
 * @brief Migration request
 */
struct MigrationRequest {
    std::string requestId;
    std::string datasetName;
    std::string sourceLocation;      // Local path or volume
    std::string targetBucket;
    std::string targetKey;
    CloudStorageClass targetClass{CloudStorageClass::S3_STANDARD};
    RecallPriority priority{RecallPriority::STANDARD};
    bool compressBeforeUpload{true};
    std::string compressionAlgorithm{"ZSTD"};
    bool deleteAfterMigration{false};
    std::chrono::system_clock::time_point requestTime{std::chrono::system_clock::now()};
    std::map<std::string, std::string> metadata;
    
    std::string generateObjectKey() const {
        // Generate hierarchical key: prefix/HLQ/dataset.ext
        std::string key = targetKey;
        if (key.empty()) {
            // Convert DSN.NAME.DATA to DSN/NAME/DATA
            std::string converted = datasetName;
            std::replace(converted.begin(), converted.end(), '.', '/');
            key = converted;
        }
        return key;
    }
};

/**
 * @brief Migration result
 */
struct MigrationResult {
    std::string requestId;
    MigrationStatus status{MigrationStatus::PENDING};
    std::string objectKey;
    std::string etag;
    uint64_t bytesTransferred{0};
    uint64_t originalSize{0};
    uint64_t compressedSize{0};
    double compressionRatio{1.0};
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::chrono::milliseconds duration{0};
    double transferRateMBps{0.0};
    std::string errorMessage;
    double estimatedMonthlyCost{0.0};
    
    bool isSuccess() const {
        return status == MigrationStatus::COMPLETED;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "RequestId=" << requestId
            << ", Status=" << static_cast<int>(status)
            << ", Bytes=" << bytesTransferred;
        if (!errorMessage.empty()) {
            oss << ", Error=" << errorMessage;
        }
        return oss.str();
    }
};

/**
 * @brief Recall request
 */
struct RecallRequest {
    std::string requestId;
    std::string objectKey;
    std::string sourceBucket;
    std::string targetLocation;      // Local destination
    std::string datasetName;
    RecallPriority priority{RecallPriority::STANDARD};
    bool decompressAfterRecall{true};
    std::chrono::system_clock::time_point requestTime{std::chrono::system_clock::now()};
    uint32_t glacierRestoreHours{24};  // Hours to keep restored
};

/**
 * @brief Recall result
 */
struct RecallResult {
    std::string requestId;
    MigrationStatus status{MigrationStatus::PENDING};
    std::string localPath;
    uint64_t bytesTransferred{0};
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::chrono::milliseconds duration{0};
    std::string errorMessage;
    bool glacierRestoreInitiated{false};
    std::chrono::system_clock::time_point glacierRestoreReadyTime;
    
    bool isSuccess() const {
        return status == MigrationStatus::COMPLETED;
    }
};

/**
 * @brief Cost breakdown by category
 */
struct CostBreakdown {
    double storageCost{0.0};
    double requestCost{0.0};
    double transferCost{0.0};
    double retrievalCost{0.0};
    double totalCost{0.0};
    std::string currency{"USD"};
    std::chrono::system_clock::time_point calculatedAt{std::chrono::system_clock::now()};
    
    void calculate() {
        totalCost = storageCost + requestCost + transferCost + retrievalCost;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4)
            << "Storage: $" << storageCost
            << ", Requests: $" << requestCost
            << ", Transfer: $" << transferCost
            << ", Retrieval: $" << retrievalCost
            << ", Total: $" << totalCost;
        return oss.str();
    }
};

/**
 * @brief Storage pricing (per GB/month or per request)
 */
struct StoragePricing {
    CloudProviderType provider{CloudProviderType::AWS_S3};
    CloudStorageClass storageClass{CloudStorageClass::S3_STANDARD};
    std::string region;
    double storagePerGBMonth{0.023};      // Storage cost per GB per month
    double putRequestPer1000{0.005};      // PUT/COPY/POST/LIST per 1000
    double getRequestPer1000{0.0004};     // GET/SELECT per 1000
    double dataTransferOutPerGB{0.09};    // Data transfer out per GB
    double glacierRetrievalPerGB{0.01};   // Glacier retrieval per GB
    double glacierExpedited{0.03};        // Expedited retrieval per GB
    
    static StoragePricing getS3Standard(const std::string& region = "us-east-1") {
        StoragePricing p;
        p.provider = CloudProviderType::AWS_S3;
        p.storageClass = CloudStorageClass::S3_STANDARD;
        p.region = region;
        p.storagePerGBMonth = 0.023;
        p.putRequestPer1000 = 0.005;
        p.getRequestPer1000 = 0.0004;
        p.dataTransferOutPerGB = 0.09;
        return p;
    }
    
    static StoragePricing getS3Glacier(const std::string& region = "us-east-1") {
        StoragePricing p;
        p.provider = CloudProviderType::AWS_S3;
        p.storageClass = CloudStorageClass::S3_GLACIER;
        p.region = region;
        p.storagePerGBMonth = 0.004;
        p.putRequestPer1000 = 0.03;
        p.getRequestPer1000 = 0.0004;
        p.dataTransferOutPerGB = 0.09;
        p.glacierRetrievalPerGB = 0.01;
        p.glacierExpedited = 0.03;
        return p;
    }
    
    static StoragePricing getAzureArchive(const std::string& region = "eastus") {
        StoragePricing p;
        p.provider = CloudProviderType::AZURE_BLOB;
        p.storageClass = CloudStorageClass::AZURE_ARCHIVE;
        p.region = region;
        p.storagePerGBMonth = 0.00099;
        p.putRequestPer1000 = 0.02;
        p.getRequestPer1000 = 5.00;  // Very expensive to read from archive
        p.dataTransferOutPerGB = 0.087;
        return p;
    }
};

// ============================================================================
// Cloud Provider Interface
// ============================================================================

/**
 * @brief Abstract cloud provider interface
 */
class CloudProvider {
public:
    virtual ~CloudProvider() = default;
    
    // Connection management
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual ConnectionStatus getConnectionStatus() const = 0;
    virtual bool testConnection() = 0;
    
    // Bucket/Container operations
    virtual bool createBucket(const std::string& name) = 0;
    virtual bool deleteBucket(const std::string& name) = 0;
    virtual bool bucketExists(const std::string& name) = 0;
    virtual std::vector<std::string> listBuckets() = 0;
    
    // Object operations
    virtual MigrationResult uploadObject(const MigrationRequest& request,
                                        std::function<void(const TransferProgress&)> progressCallback = nullptr) = 0;
    virtual RecallResult downloadObject(const RecallRequest& request,
                                       std::function<void(const TransferProgress&)> progressCallback = nullptr) = 0;
    virtual bool deleteObject(const std::string& bucket, const std::string& key) = 0;
    virtual std::optional<CloudObjectMetadata> getObjectMetadata(const std::string& bucket, 
                                                                  const std::string& key) = 0;
    virtual std::vector<CloudObjectMetadata> listObjects(const std::string& bucket,
                                                         const std::string& prefix = "",
                                                         uint32_t maxKeys = 1000) = 0;
    
    // Glacier/Archive operations
    virtual bool initiateGlacierRestore(const std::string& bucket, const std::string& key,
                                        uint32_t restoreDays, RecallPriority priority) = 0;
    virtual bool isGlacierRestoreComplete(const std::string& bucket, const std::string& key) = 0;
    
    // Storage class operations
    virtual bool changeStorageClass(const std::string& bucket, const std::string& key,
                                   CloudStorageClass newClass) = 0;
    
    // Provider info
    virtual CloudProviderType getType() const = 0;
    virtual std::string getName() const = 0;
    virtual std::string getRegion() const = 0;
};

// ============================================================================
// S3 Provider Implementation
// ============================================================================

/**
 * @brief S3-compatible cloud provider
 */
class S3Provider : public CloudProvider {
public:
    S3Provider() = default;
    
    explicit S3Provider(const CloudCredentials& credentials,
                       const CloudBucketConfig& defaultBucket = CloudBucketConfig())
        : credentials_(credentials), defaultBucket_(defaultBucket) {}
    
    // Connection management
    bool connect() override {
        if (!credentials_.isValid()) {
            lastError_ = "Invalid credentials";
            status_ = ConnectionStatus::ERROR;
            return false;
        }
        
        // Simulate connection
        status_ = ConnectionStatus::CONNECTED;
        connectedAt_ = std::chrono::system_clock::now();
        return true;
    }
    
    void disconnect() override {
        status_ = ConnectionStatus::DISCONNECTED;
    }
    
    ConnectionStatus getConnectionStatus() const override {
        return status_;
    }
    
    bool testConnection() override {
        if (status_ != ConnectionStatus::CONNECTED) {
            return connect();
        }
        // Test by listing buckets
        return true;
    }
    
    // Bucket operations
    bool createBucket(const std::string& name) override {
        if (status_ != ConnectionStatus::CONNECTED) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if (buckets_.find(name) != buckets_.end()) return false;
        buckets_[name] = {};
        return true;
    }
    
    bool deleteBucket(const std::string& name) override {
        if (status_ != ConnectionStatus::CONNECTED) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        return buckets_.erase(name) > 0;
    }
    
    bool bucketExists(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return buckets_.find(name) != buckets_.end();
    }
    
    std::vector<std::string> listBuckets() override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        for (const auto& [name, objects] : buckets_) {
            result.push_back(name);
        }
        return result;
    }
    
    // Object operations
    MigrationResult uploadObject(const MigrationRequest& request,
                                std::function<void(const TransferProgress&)> progressCallback) override {
        MigrationResult result;
        result.requestId = request.requestId;
        result.startTime = std::chrono::system_clock::now();
        
        if (status_ != ConnectionStatus::CONNECTED) {
            result.status = MigrationStatus::FAILED;
            result.errorMessage = "Not connected";
            return result;
        }
        
        std::string bucket = request.targetBucket.empty() ? defaultBucket_.name : request.targetBucket;
        if (!bucketExists(bucket)) {
            createBucket(bucket);
        }
        
        std::string key = request.generateObjectKey();
        
        // Simulate upload with progress
        result.status = MigrationStatus::IN_PROGRESS;
        result.originalSize = 1024 * 1024 * 100;  // Simulated 100MB
        result.compressedSize = static_cast<uint64_t>(result.originalSize * 0.4);  // 60% compression
        result.compressionRatio = static_cast<double>(result.originalSize) / result.compressedSize;
        
        TransferProgress progress;
        progress.totalBytes = result.compressedSize;
        progress.startTime = result.startTime;
        
        uint64_t chunkSize = 5 * 1024 * 1024;  // 5MB chunks
        progress.totalParts = static_cast<uint32_t>((progress.totalBytes + chunkSize - 1) / chunkSize);
        
        for (uint32_t part = 0; part < progress.totalParts; ++part) {
            progress.partsCompleted = part + 1;
            progress.bytesTransferred = std::min(progress.totalBytes, 
                                                  static_cast<uint64_t>((part + 1) * chunkSize));
            progress.lastUpdateTime = std::chrono::system_clock::now();
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                progress.lastUpdateTime - progress.startTime).count();
            if (elapsed > 0) {
                progress.transferRateMBps = (progress.bytesTransferred / (1024.0 * 1024.0)) / 
                                           (elapsed / 1000.0);
            }
            
            if (progressCallback) {
                progressCallback(progress);
            }
        }
        
        // Create object metadata
        CloudObjectMetadata meta;
        meta.key = key;
        meta.size = result.compressedSize;
        meta.storageClass = request.targetClass;
        meta.uploadTime = std::chrono::system_clock::now();
        meta.lastModified = meta.uploadTime;
        meta.datasetName = request.datasetName;
        meta.originalSize = result.originalSize;
        meta.compressionType = request.compressionAlgorithm;
        meta.compressionRatio = result.compressionRatio;
        meta.etag = generateETag();
        meta.userMetadata = request.metadata;
        
        // Store object
        {
            std::lock_guard<std::mutex> lock(mutex_);
            buckets_[bucket][key] = meta;
        }
        
        result.status = MigrationStatus::COMPLETED;
        result.objectKey = key;
        result.etag = meta.etag;
        result.bytesTransferred = result.compressedSize;
        result.endTime = std::chrono::system_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.endTime - result.startTime);
        result.transferRateMBps = (result.bytesTransferred / (1024.0 * 1024.0)) / 
                                  (result.duration.count() / 1000.0);
        
        // Estimate monthly storage cost
        double gbMonth = result.compressedSize / (1024.0 * 1024.0 * 1024.0);
        result.estimatedMonthlyCost = gbMonth * pricing_.storagePerGBMonth;
        
        stats_.objectsUploaded++;
        stats_.bytesUploaded += result.bytesTransferred;
        
        return result;
    }
    
    RecallResult downloadObject(const RecallRequest& request,
                               std::function<void(const TransferProgress&)> progressCallback) override {
        RecallResult result;
        result.requestId = request.requestId;
        result.startTime = std::chrono::system_clock::now();
        
        if (status_ != ConnectionStatus::CONNECTED) {
            result.status = MigrationStatus::FAILED;
            result.errorMessage = "Not connected";
            return result;
        }
        
        std::string bucket = request.sourceBucket.empty() ? defaultBucket_.name : request.sourceBucket;
        
        std::optional<CloudObjectMetadata> metaOpt;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto bucketIt = buckets_.find(bucket);
            if (bucketIt != buckets_.end()) {
                auto objIt = bucketIt->second.find(request.objectKey);
                if (objIt != bucketIt->second.end()) {
                    metaOpt = objIt->second;
                }
            }
        }
        
        if (!metaOpt) {
            result.status = MigrationStatus::FAILED;
            result.errorMessage = "Object not found: " + request.objectKey;
            return result;
        }
        
        const auto& meta = *metaOpt;
        
        // Check if Glacier restore is needed
        if (meta.storageClass == CloudStorageClass::S3_GLACIER ||
            meta.storageClass == CloudStorageClass::S3_GLACIER_DEEP) {
            if (!meta.isGlacierRestoreInProgress && 
                meta.glacierRestoreExpiry < std::chrono::system_clock::now()) {
                result.glacierRestoreInitiated = true;
                result.status = MigrationStatus::PENDING;
                result.errorMessage = "Glacier restore initiated, object will be available in " +
                                     std::to_string(request.glacierRestoreHours) + " hours";
                
                // In real implementation, initiate restore
                initiateGlacierRestore(bucket, request.objectKey, 
                                      request.glacierRestoreHours, request.priority);
                return result;
            }
        }
        
        // Simulate download with progress
        result.status = MigrationStatus::IN_PROGRESS;
        
        TransferProgress progress;
        progress.totalBytes = meta.size;
        progress.startTime = result.startTime;
        progress.totalParts = static_cast<uint32_t>((progress.totalBytes + 5*1024*1024 - 1) / (5*1024*1024));
        
        for (uint32_t part = 0; part < progress.totalParts; ++part) {
            progress.partsCompleted = part + 1;
            progress.bytesTransferred = std::min(progress.totalBytes,
                                                  static_cast<uint64_t>((part + 1) * 5*1024*1024));
            progress.lastUpdateTime = std::chrono::system_clock::now();
            
            if (progressCallback) {
                progressCallback(progress);
            }
        }
        
        result.status = MigrationStatus::COMPLETED;
        result.localPath = request.targetLocation;
        result.bytesTransferred = meta.size;
        result.endTime = std::chrono::system_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.endTime - result.startTime);
        
        stats_.objectsDownloaded++;
        stats_.bytesDownloaded += result.bytesTransferred;
        
        return result;
    }
    
    bool deleteObject(const std::string& bucket, const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto bucketIt = buckets_.find(bucket);
        if (bucketIt == buckets_.end()) return false;
        return bucketIt->second.erase(key) > 0;
    }
    
    std::optional<CloudObjectMetadata> getObjectMetadata(const std::string& bucket,
                                                          const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto bucketIt = buckets_.find(bucket);
        if (bucketIt == buckets_.end()) return std::nullopt;
        auto objIt = bucketIt->second.find(key);
        if (objIt == bucketIt->second.end()) return std::nullopt;
        return objIt->second;
    }
    
    std::vector<CloudObjectMetadata> listObjects(const std::string& bucket,
                                                  const std::string& prefix,
                                                  uint32_t maxKeys) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<CloudObjectMetadata> result;
        
        auto bucketIt = buckets_.find(bucket);
        if (bucketIt == buckets_.end()) return result;
        
        for (const auto& [key, meta] : bucketIt->second) {
            if (prefix.empty() || key.substr(0, prefix.size()) == prefix) {
                result.push_back(meta);
                if (result.size() >= maxKeys) break;
            }
        }
        
        return result;
    }
    
    // Glacier operations
    bool initiateGlacierRestore(const std::string& bucket, const std::string& key,
                                uint32_t restoreDays, RecallPriority priority) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto bucketIt = buckets_.find(bucket);
        if (bucketIt == buckets_.end()) return false;
        auto objIt = bucketIt->second.find(key);
        if (objIt == bucketIt->second.end()) return false;
        
        objIt->second.isGlacierRestoreInProgress = true;
        
        // Simulate restore time based on priority
        int hours = 24;
        switch (priority) {
            case RecallPriority::EXPEDITED: hours = 1; break;
            case RecallPriority::STANDARD: hours = 5; break;
            case RecallPriority::BULK: hours = 12; break;
        }
        
        // Use hours to simulate restore completion time, expiry is based on restoreDays
        objIt->second.glacierRestoreExpiry = std::chrono::system_clock::now() + 
                                             std::chrono::hours(hours) +
                                             std::chrono::hours(restoreDays * 24);
        return true;
    }
    
    bool isGlacierRestoreComplete(const std::string& bucket, const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto bucketIt = buckets_.find(bucket);
        if (bucketIt == buckets_.end()) return false;
        auto objIt = bucketIt->second.find(key);
        if (objIt == bucketIt->second.end()) return false;
        
        // Simulate: restore is complete if in progress
        return objIt->second.isGlacierRestoreInProgress;
    }
    
    bool changeStorageClass(const std::string& bucket, const std::string& key,
                           CloudStorageClass newClass) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto bucketIt = buckets_.find(bucket);
        if (bucketIt == buckets_.end()) return false;
        auto objIt = bucketIt->second.find(key);
        if (objIt == bucketIt->second.end()) return false;
        
        objIt->second.storageClass = newClass;
        return true;
    }
    
    // Provider info
    CloudProviderType getType() const override { return CloudProviderType::AWS_S3; }
    std::string getName() const override { return "Amazon S3"; }
    std::string getRegion() const override { return credentials_.region; }
    
    // Configuration
    void setCredentials(const CloudCredentials& creds) { credentials_ = creds; }
    void setDefaultBucket(const CloudBucketConfig& config) { defaultBucket_ = config; }
    void setPricing(const StoragePricing& pricing) { pricing_ = pricing; }
    
    // Statistics
    struct Statistics {
        uint64_t objectsUploaded{0};
        uint64_t objectsDownloaded{0};
        uint64_t bytesUploaded{0};
        uint64_t bytesDownloaded{0};
        uint64_t requestCount{0};
    };
    
    const Statistics& getStatistics() const { return stats_; }

private:
    CloudCredentials credentials_;
    CloudBucketConfig defaultBucket_;
    StoragePricing pricing_{StoragePricing::getS3Standard()};
    ConnectionStatus status_{ConnectionStatus::DISCONNECTED};
    std::chrono::system_clock::time_point connectedAt_;
    std::string lastError_;
    Statistics stats_;
    
    // Simulated storage
    std::map<std::string, std::map<std::string, CloudObjectMetadata>> buckets_;
    mutable std::mutex mutex_;
    
    std::string generateETag() const {
        static uint64_t counter = 0;
        std::ostringstream oss;
        oss << "\"" << std::hex << std::setfill('0') << std::setw(32) 
            << (++counter * 0x9E3779B97F4A7C15ULL) << "\"";
        return oss.str();
    }
};

// ============================================================================
// Azure Provider Implementation
// ============================================================================

/**
 * @brief Azure Blob Storage provider
 */
class AzureProvider : public CloudProvider {
public:
    AzureProvider() = default;
    
    explicit AzureProvider(const CloudCredentials& credentials)
        : credentials_(credentials) {}
    
    bool connect() override {
        if (!credentials_.isValid()) {
            status_ = ConnectionStatus::ERROR;
            return false;
        }
        status_ = ConnectionStatus::CONNECTED;
        return true;
    }
    
    void disconnect() override {
        status_ = ConnectionStatus::DISCONNECTED;
    }
    
    ConnectionStatus getConnectionStatus() const override { return status_; }
    bool testConnection() override { return status_ == ConnectionStatus::CONNECTED; }
    
    bool createBucket(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);
        containers_[name] = {};
        return true;
    }
    
    bool deleteBucket(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return containers_.erase(name) > 0;
    }
    
    bool bucketExists(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return containers_.find(name) != containers_.end();
    }
    
    std::vector<std::string> listBuckets() override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        for (const auto& [name, blobs] : containers_) {
            result.push_back(name);
        }
        return result;
    }
    
    MigrationResult uploadObject(const MigrationRequest& request,
                                std::function<void(const TransferProgress&)> progressCallback) override {
        MigrationResult result;
        result.requestId = request.requestId;
        result.startTime = std::chrono::system_clock::now();
        
        if (status_ != ConnectionStatus::CONNECTED) {
            result.status = MigrationStatus::FAILED;
            result.errorMessage = "Not connected";
            return result;
        }
        
        std::string container = request.targetBucket.empty() ? "default" : request.targetBucket;
        if (!bucketExists(container)) {
            createBucket(container);
        }
        
        std::string blobName = request.generateObjectKey();
        
        // Simulate upload
        CloudObjectMetadata meta;
        meta.key = blobName;
        meta.size = 1024 * 1024 * 50;  // 50MB
        meta.storageClass = CloudStorageClass::AZURE_HOT;
        meta.uploadTime = std::chrono::system_clock::now();
        meta.datasetName = request.datasetName;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            containers_[container][blobName] = meta;
        }
        
        result.status = MigrationStatus::COMPLETED;
        result.objectKey = blobName;
        result.bytesTransferred = meta.size;
        result.endTime = std::chrono::system_clock::now();
        
        return result;
    }
    
    RecallResult downloadObject(const RecallRequest& request,
                               std::function<void(const TransferProgress&)> progressCallback) override {
        RecallResult result;
        result.requestId = request.requestId;
        result.startTime = std::chrono::system_clock::now();
        
        if (status_ != ConnectionStatus::CONNECTED) {
            result.status = MigrationStatus::FAILED;
            result.errorMessage = "Not connected";
            return result;
        }
        
        result.status = MigrationStatus::COMPLETED;
        result.localPath = request.targetLocation;
        result.endTime = std::chrono::system_clock::now();
        
        return result;
    }
    
    bool deleteObject(const std::string& bucket, const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = containers_.find(bucket);
        if (it == containers_.end()) return false;
        return it->second.erase(key) > 0;
    }
    
    std::optional<CloudObjectMetadata> getObjectMetadata(const std::string& bucket,
                                                          const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto contIt = containers_.find(bucket);
        if (contIt == containers_.end()) return std::nullopt;
        auto blobIt = contIt->second.find(key);
        if (blobIt == contIt->second.end()) return std::nullopt;
        return blobIt->second;
    }
    
    std::vector<CloudObjectMetadata> listObjects(const std::string& bucket,
                                                  const std::string& prefix,
                                                  uint32_t maxKeys) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<CloudObjectMetadata> result;
        auto contIt = containers_.find(bucket);
        if (contIt == containers_.end()) return result;
        
        for (const auto& [name, meta] : contIt->second) {
            if (prefix.empty() || name.substr(0, prefix.size()) == prefix) {
                result.push_back(meta);
                if (result.size() >= maxKeys) break;
            }
        }
        return result;
    }
    
    bool initiateGlacierRestore(const std::string& bucket, const std::string& key,
                                uint32_t restoreDays, RecallPriority priority) override {
        // Azure uses "rehydrate" for archive tier
        return changeStorageClass(bucket, key, CloudStorageClass::AZURE_HOT);
    }
    
    bool isGlacierRestoreComplete(const std::string& bucket, const std::string& key) override {
        auto meta = getObjectMetadata(bucket, key);
        if (!meta) return false;
        return meta->storageClass != CloudStorageClass::AZURE_ARCHIVE;
    }
    
    bool changeStorageClass(const std::string& bucket, const std::string& key,
                           CloudStorageClass newClass) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto contIt = containers_.find(bucket);
        if (contIt == containers_.end()) return false;
        auto blobIt = contIt->second.find(key);
        if (blobIt == contIt->second.end()) return false;
        blobIt->second.storageClass = newClass;
        return true;
    }
    
    CloudProviderType getType() const override { return CloudProviderType::AZURE_BLOB; }
    std::string getName() const override { return "Azure Blob Storage"; }
    std::string getRegion() const override { return credentials_.region; }

private:
    CloudCredentials credentials_;
    ConnectionStatus status_{ConnectionStatus::DISCONNECTED};
    std::map<std::string, std::map<std::string, CloudObjectMetadata>> containers_;
    mutable std::mutex mutex_;
};

// ============================================================================
// Cloud Migration Policy
// ============================================================================

/**
 * @brief Cloud migration policy configuration
 */
class CloudMigrationPolicy {
public:
    struct Rule {
        int priority{0};
        std::string namePattern;           // Dataset name pattern
        uint32_t daysUnused{90};           // Days since last access
        uint64_t minSizeBytes{0};          // Minimum size to migrate
        uint64_t maxSizeBytes{UINT64_MAX}; // Maximum size to migrate
        CloudStorageClass targetClass{CloudStorageClass::S3_GLACIER};
        std::string targetBucket;
        bool compressBeforeMigration{true};
        bool deleteLocalAfterMigration{false};
        bool enabled{true};
        
        bool matches(const std::string& dsn, uint64_t size, uint32_t daysUnused) const {
            if (!enabled) return false;
            if (size < minSizeBytes || size > maxSizeBytes) return false;
            if (daysUnused < this->daysUnused) return false;
            
            // Simple pattern matching (supports * wildcard)
            if (!namePattern.empty()) {
                std::string pattern = namePattern;
                std::string::size_type pos = 0;
                while ((pos = pattern.find('*', pos)) != std::string::npos) {
                    pattern.replace(pos, 1, ".*");
                    pos += 2;
                }
                try {
                    std::regex re(pattern, std::regex::icase);
                    if (!std::regex_match(dsn, re)) return false;
                } catch (...) {
                    return false;
                }
            }
            return true;
        }
    };
    
    CloudMigrationPolicy() = default;
    
    explicit CloudMigrationPolicy(const std::string& name)
        : name_(name) {}
    
    // Getters
    const std::string& getName() const { return name_; }
    const std::vector<Rule>& getRules() const { return rules_; }
    bool isEnabled() const { return enabled_; }
    
    // Setters
    CloudMigrationPolicy& setName(const std::string& name) {
        name_ = name;
        return *this;
    }
    
    CloudMigrationPolicy& setEnabled(bool enabled) {
        enabled_ = enabled;
        return *this;
    }
    
    CloudMigrationPolicy& addRule(const Rule& rule) {
        rules_.push_back(rule);
        std::sort(rules_.begin(), rules_.end(),
            [](const Rule& a, const Rule& b) { return a.priority < b.priority; });
        return *this;
    }
    
    // Find matching rule
    const Rule* findMatchingRule(const std::string& dsn, uint64_t size, 
                                 uint32_t daysUnused) const {
        if (!enabled_) return nullptr;
        
        for (const auto& rule : rules_) {
            if (rule.matches(dsn, size, daysUnused)) {
                return &rule;
            }
        }
        return nullptr;
    }
    
    // Create common policies
    static CloudMigrationPolicy createDefaultPolicy() {
        CloudMigrationPolicy policy("DEFAULT");
        
        Rule archiveRule;
        archiveRule.priority = 10;
        archiveRule.namePattern = "*.ARCHIVE.*";
        archiveRule.daysUnused = 30;
        archiveRule.targetClass = CloudStorageClass::S3_GLACIER;
        archiveRule.compressBeforeMigration = true;
        policy.addRule(archiveRule);
        
        Rule oldDataRule;
        oldDataRule.priority = 20;
        oldDataRule.daysUnused = 180;
        oldDataRule.minSizeBytes = 100 * 1024 * 1024;  // 100MB minimum
        oldDataRule.targetClass = CloudStorageClass::S3_GLACIER_DEEP;
        policy.addRule(oldDataRule);
        
        Rule largeFileRule;
        largeFileRule.priority = 30;
        largeFileRule.daysUnused = 90;
        largeFileRule.minSizeBytes = 1024ULL * 1024 * 1024;  // 1GB minimum
        largeFileRule.targetClass = CloudStorageClass::S3_STANDARD_IA;
        policy.addRule(largeFileRule);
        
        return policy;
    }

private:
    std::string name_{"DEFAULT"};
    std::vector<Rule> rules_;
    bool enabled_{true};
};

// ============================================================================
// Cost Analyzer
// ============================================================================

/**
 * @brief Cloud cost analyzer
 */
class CostAnalyzer {
public:
    CostAnalyzer() = default;
    
    void setPricing(const StoragePricing& pricing) {
        pricing_ = pricing;
    }
    
    // Calculate storage cost for given size and duration
    CostBreakdown calculateStorageCost(uint64_t sizeBytes, uint32_t daysStored) const {
        CostBreakdown cost;
        
        double sizeGB = sizeBytes / (1024.0 * 1024.0 * 1024.0);
        double months = daysStored / 30.0;
        
        cost.storageCost = sizeGB * pricing_.storagePerGBMonth * months;
        cost.calculate();
        
        return cost;
    }
    
    // Calculate transfer cost
    CostBreakdown calculateTransferCost(uint64_t sizeBytes, TransferDirection direction,
                                        RecallPriority priority = RecallPriority::STANDARD) const {
        CostBreakdown cost;
        
        double sizeGB = sizeBytes / (1024.0 * 1024.0 * 1024.0);
        
        switch (direction) {
            case TransferDirection::TO_CLOUD:
                // Ingress is usually free
                cost.requestCost = pricing_.putRequestPer1000 / 1000.0;
                break;
                
            case TransferDirection::FROM_CLOUD:
                cost.transferCost = sizeGB * pricing_.dataTransferOutPerGB;
                cost.requestCost = pricing_.getRequestPer1000 / 1000.0;
                
                // Add retrieval cost for Glacier
                if (pricing_.storageClass == CloudStorageClass::S3_GLACIER ||
                    pricing_.storageClass == CloudStorageClass::S3_GLACIER_DEEP) {
                    switch (priority) {
                        case RecallPriority::EXPEDITED:
                            cost.retrievalCost = sizeGB * pricing_.glacierExpedited;
                            break;
                        case RecallPriority::STANDARD:
                        case RecallPriority::BULK:
                            cost.retrievalCost = sizeGB * pricing_.glacierRetrievalPerGB;
                            break;
                    }
                }
                break;
                
            case TransferDirection::CLOUD_TO_CLOUD:
                cost.transferCost = sizeGB * pricing_.dataTransferOutPerGB * 0.5;  // Usually cheaper
                break;
        }
        
        cost.calculate();
        return cost;
    }
    
    // Compare storage class costs
    struct StorageClassComparison {
        CloudStorageClass storageClass;
        CostBreakdown monthlyCost;
        CostBreakdown yearlyCost;
        CostBreakdown recallCost;
        double totalYearlyCost{0.0};
    };
    
    std::vector<StorageClassComparison> compareStorageClasses(
            uint64_t sizeBytes, 
            uint32_t expectedRecallsPerYear = 1) const {
        
        std::vector<StorageClassComparison> comparisons;
        
        std::vector<StoragePricing> pricings = {
            StoragePricing::getS3Standard(),
            StoragePricing::getS3Glacier(),
            StoragePricing::getAzureArchive()
        };
        
        for (const auto& p : pricings) {
            CostAnalyzer analyzer;
            analyzer.setPricing(p);
            
            StorageClassComparison comp;
            comp.storageClass = p.storageClass;
            comp.monthlyCost = analyzer.calculateStorageCost(sizeBytes, 30);
            comp.yearlyCost = analyzer.calculateStorageCost(sizeBytes, 365);
            comp.recallCost = analyzer.calculateTransferCost(sizeBytes, 
                                                             TransferDirection::FROM_CLOUD);
            comp.totalYearlyCost = comp.yearlyCost.totalCost + 
                                   (comp.recallCost.totalCost * expectedRecallsPerYear);
            
            comparisons.push_back(comp);
        }
        
        // Sort by total yearly cost
        std::sort(comparisons.begin(), comparisons.end(),
            [](const auto& a, const auto& b) {
                return a.totalYearlyCost < b.totalYearlyCost;
            });
        
        return comparisons;
    }
    
    // Recommend optimal storage class
    CloudStorageClass recommendStorageClass(uint64_t sizeBytes,
                                            uint32_t expectedDaysStored,
                                            uint32_t expectedRecallsPerYear) const {
        auto comparisons = compareStorageClasses(sizeBytes, expectedRecallsPerYear);
        
        if (!comparisons.empty()) {
            // Consider retrieval time constraints
            if (expectedRecallsPerYear > 12) {
                // Frequent access - use standard
                return CloudStorageClass::S3_STANDARD;
            } else if (expectedRecallsPerYear > 4) {
                // Moderate access - use IA
                return CloudStorageClass::S3_STANDARD_IA;
            } else {
                // Infrequent access - use cheapest
                return comparisons[0].storageClass;
            }
        }
        
        return CloudStorageClass::S3_STANDARD;
    }

private:
    StoragePricing pricing_{StoragePricing::getS3Standard()};
};

// ============================================================================
// Cloud Tier Manager
// ============================================================================

/**
 * @brief Cloud Tier Manager - Main entry point
 * 
 * Manages cloud tiering operations including:
 * - Multi-provider support
 * - Migration scheduling
 * - Transparent recall
 * - Cost optimization
 */
class CloudTierManager {
public:
    /**
     * @brief Manager statistics
     */
    struct Statistics {
        uint64_t migrationsCompleted{0};
        uint64_t migrationsFailed{0};
        uint64_t recallsCompleted{0};
        uint64_t recallsFailed{0};
        uint64_t bytesToCloud{0};
        uint64_t bytesFromCloud{0};
        uint64_t objectsInCloud{0};
        double totalCloudStorageGB{0.0};
        double estimatedMonthlyCost{0.0};
        std::chrono::system_clock::time_point startTime{std::chrono::system_clock::now()};
        
        std::string toString() const {
            std::ostringstream oss;
            oss << "Migrations: " << migrationsCompleted << " completed, " 
                << migrationsFailed << " failed\n"
                << "Recalls: " << recallsCompleted << " completed, "
                << recallsFailed << " failed\n"
                << "Data: " << std::fixed << std::setprecision(2)
                << (bytesToCloud / (1024.0 * 1024.0 * 1024.0)) << " GB to cloud, "
                << (bytesFromCloud / (1024.0 * 1024.0 * 1024.0)) << " GB from cloud\n"
                << "Storage: " << totalCloudStorageGB << " GB in cloud\n"
                << "Est. Monthly Cost: $" << estimatedMonthlyCost;
            return oss.str();
        }
    };
    
    CloudTierManager() {
        costAnalyzer_ = std::make_unique<CostAnalyzer>();
    }
    
    // Provider management
    void addProvider(const std::string& name, std::shared_ptr<CloudProvider> provider) {
        std::lock_guard<std::mutex> lock(mutex_);
        providers_[name] = provider;
        if (!defaultProvider_) {
            defaultProvider_ = provider;
        }
    }
    
    void removeProvider(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        providers_.erase(name);
    }
    
    std::shared_ptr<CloudProvider> getProvider(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = providers_.find(name);
        if (it != providers_.end()) return it->second;
        return nullptr;
    }
    
    void setDefaultProvider(std::shared_ptr<CloudProvider> provider) {
        std::lock_guard<std::mutex> lock(mutex_);
        defaultProvider_ = provider;
    }
    
    // Policy management
    void setMigrationPolicy(const CloudMigrationPolicy& policy) {
        std::lock_guard<std::mutex> lock(mutex_);
        policy_ = policy;
    }
    
    const CloudMigrationPolicy& getMigrationPolicy() const {
        return policy_;
    }
    
    // Migration operations
    MigrationResult migrateToCloud(const MigrationRequest& request,
                                   const std::string& providerName = "",
                                   std::function<void(const TransferProgress&)> progressCallback = nullptr) {
        auto provider = providerName.empty() ? defaultProvider_ : getProvider(providerName);
        
        if (!provider) {
            MigrationResult result;
            result.requestId = request.requestId;
            result.status = MigrationStatus::FAILED;
            result.errorMessage = "No provider available";
            return result;
        }
        
        auto result = provider->uploadObject(request, progressCallback);
        
        std::lock_guard<std::mutex> lock(statsMutex_);
        if (result.isSuccess()) {
            stats_.migrationsCompleted++;
            stats_.bytesToCloud += result.bytesTransferred;
            stats_.objectsInCloud++;
            stats_.totalCloudStorageGB += result.compressedSize / (1024.0 * 1024.0 * 1024.0);
            
            // Track in catalog
            cloudCatalog_[request.datasetName] = CloudCatalogEntry{
                request.datasetName,
                result.objectKey,
                request.targetBucket,
                providerName.empty() ? "default" : providerName,
                result.compressedSize,
                std::chrono::system_clock::now()
            };
        } else {
            stats_.migrationsFailed++;
        }
        
        return result;
    }
    
    // Recall operations
    RecallResult recallFromCloud(const RecallRequest& request,
                                 const std::string& providerName = "",
                                 std::function<void(const TransferProgress&)> progressCallback = nullptr) {
        // Look up in catalog first
        std::string actualProvider = providerName;
        std::string bucket;
        std::string key = request.objectKey;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = cloudCatalog_.find(request.datasetName);
            if (it != cloudCatalog_.end()) {
                actualProvider = it->second.providerName;
                bucket = it->second.bucket;
                key = it->second.objectKey;
            }
        }
        
        auto provider = actualProvider.empty() ? defaultProvider_ : getProvider(actualProvider);
        
        if (!provider) {
            RecallResult result;
            result.requestId = request.requestId;
            result.status = MigrationStatus::FAILED;
            result.errorMessage = "No provider available";
            return result;
        }
        
        RecallRequest actualRequest = request;
        actualRequest.sourceBucket = bucket;
        actualRequest.objectKey = key;
        
        auto result = provider->downloadObject(actualRequest, progressCallback);
        
        std::lock_guard<std::mutex> lock(statsMutex_);
        if (result.isSuccess()) {
            stats_.recallsCompleted++;
            stats_.bytesFromCloud += result.bytesTransferred;
        } else {
            stats_.recallsFailed++;
        }
        
        return result;
    }
    
    // Check if dataset is in cloud
    bool isInCloud(const std::string& datasetName) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cloudCatalog_.find(datasetName) != cloudCatalog_.end();
    }
    
    // Get cloud location for dataset
    std::optional<std::string> getCloudLocation(const std::string& datasetName) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cloudCatalog_.find(datasetName);
        if (it != cloudCatalog_.end()) {
            return it->second.bucket + "/" + it->second.objectKey;
        }
        return std::nullopt;
    }
    
    // Policy-based migration check
    std::optional<MigrationRequest> shouldMigrateToCloud(const std::string& datasetName,
                                                          uint64_t size,
                                                          uint32_t daysUnused) const {
        const auto* rule = policy_.findMatchingRule(datasetName, size, daysUnused);
        if (!rule) return std::nullopt;
        
        MigrationRequest request;
        request.requestId = generateRequestId();
        request.datasetName = datasetName;
        request.targetClass = rule->targetClass;
        request.targetBucket = rule->targetBucket;
        request.compressBeforeUpload = rule->compressBeforeMigration;
        request.deleteAfterMigration = rule->deleteLocalAfterMigration;
        
        return request;
    }
    
    // Cost analysis
    CostBreakdown estimateMigrationCost(uint64_t sizeBytes, 
                                        CloudStorageClass targetClass) const {
        StoragePricing pricing;
        switch (targetClass) {
            case CloudStorageClass::S3_GLACIER:
            case CloudStorageClass::S3_GLACIER_DEEP:
                pricing = StoragePricing::getS3Glacier();
                break;
            case CloudStorageClass::AZURE_ARCHIVE:
                pricing = StoragePricing::getAzureArchive();
                break;
            default:
                pricing = StoragePricing::getS3Standard();
                break;
        }
        
        CostAnalyzer analyzer;
        analyzer.setPricing(pricing);
        return analyzer.calculateStorageCost(sizeBytes, 30);
    }
    
    // Statistics
    const Statistics& getStatistics() const { return stats_; }
    
    void resetStatistics() {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_ = Statistics{};
    }
    
    // Generate summary report
    std::string generateSummaryReport() const {
        std::ostringstream oss;
        oss << "Cloud Tier Manager Summary\n"
            << "==========================\n"
            << "Providers: " << providers_.size() << "\n"
            << "Objects in Cloud: " << cloudCatalog_.size() << "\n\n"
            << stats_.toString();
        return oss.str();
    }

private:
    struct CloudCatalogEntry {
        std::string datasetName;
        std::string objectKey;
        std::string bucket;
        std::string providerName;
        uint64_t size{0};
        std::chrono::system_clock::time_point migratedAt;
    };
    
    std::map<std::string, std::shared_ptr<CloudProvider>> providers_;
    std::shared_ptr<CloudProvider> defaultProvider_;
    CloudMigrationPolicy policy_;
    std::unique_ptr<CostAnalyzer> costAnalyzer_;
    std::map<std::string, CloudCatalogEntry> cloudCatalog_;
    Statistics stats_;
    mutable std::mutex mutex_;
    mutable std::mutex statsMutex_;
    
    std::string generateRequestId() const {
        static std::atomic<uint64_t> counter{0};
        std::ostringstream oss;
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        oss << "MIG" << std::put_time(std::localtime(&time), "%Y%m%d%H%M%S")
            << "-" << std::setfill('0') << std::setw(6) << (++counter);
        return oss.str();
    }
};

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create S3 provider with credentials
 */
inline std::shared_ptr<S3Provider> createS3Provider(
        const std::string& accessKey,
        const std::string& secretKey,
        const std::string& region = "us-east-1",
        const std::string& bucket = "") {
    
    CloudCredentials creds;
    creds.accessKeyId = accessKey;
    creds.secretAccessKey = secretKey;
    creds.region = region;
    
    CloudBucketConfig config;
    config.name = bucket;
    
    return std::make_shared<S3Provider>(creds, config);
}

/**
 * @brief Create Azure provider with credentials
 */
inline std::shared_ptr<AzureProvider> createAzureProvider(
        const std::string& accountName,
        const std::string& accountKey,
        const std::string& region = "eastus") {
    
    CloudCredentials creds;
    creds.accountName = accountName;
    creds.accountKey = accountKey;
    creds.region = region;
    
    return std::make_shared<AzureProvider>(creds);
}

/**
 * @brief Create MinIO provider (S3-compatible)
 */
inline std::shared_ptr<S3Provider> createMinIOProvider(
        const std::string& endpoint,
        const std::string& accessKey,
        const std::string& secretKey,
        const std::string& bucket = "") {
    
    CloudCredentials creds;
    creds.accessKeyId = accessKey;
    creds.secretAccessKey = secretKey;
    creds.endpoint = endpoint;
    creds.useSSL = false;
    creds.port = 9000;
    
    CloudBucketConfig config;
    config.name = bucket;
    
    return std::make_shared<S3Provider>(creds, config);
}

} // namespace cloud
} // namespace dfsmshsm

#endif // HSM_CLOUD_TIER_HPP
