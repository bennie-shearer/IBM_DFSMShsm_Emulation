/**
 * @file hsm_memory.hpp
 * @brief Memory management utilities for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - Memory pool allocators
 * - Object pooling
 * - Memory statistics tracking
 * - Buffer management
 * - Smart pointer utilities
 */

#ifndef HSM_MEMORY_HPP
#define HSM_MEMORY_HPP

#include <memory>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>

namespace dfsmshsm {
namespace memory {

// ============================================================================
// Memory Statistics
// ============================================================================

/**
 * @brief Memory usage statistics
 */
struct MemoryStats {
    std::atomic<uint64_t> allocations{0};
    std::atomic<uint64_t> deallocations{0};
    std::atomic<uint64_t> bytesAllocated{0};
    std::atomic<uint64_t> bytesDeallocated{0};
    std::atomic<uint64_t> peakUsage{0};
    std::atomic<uint64_t> currentUsage{0};
    
    /**
     * @brief Reset all statistics
     */
    void reset() noexcept {
        allocations = 0;
        deallocations = 0;
        bytesAllocated = 0;
        bytesDeallocated = 0;
        peakUsage = 0;
        currentUsage = 0;
    }
    
    /**
     * @brief Record an allocation
     * @param bytes Number of bytes allocated
     */
    void recordAllocation(uint64_t bytes) noexcept {
        allocations.fetch_add(1, std::memory_order_relaxed);
        bytesAllocated.fetch_add(bytes, std::memory_order_relaxed);
        uint64_t current = currentUsage.fetch_add(bytes, std::memory_order_relaxed) + bytes;
        
        // Update peak if necessary
        uint64_t peak = peakUsage.load(std::memory_order_relaxed);
        while (current > peak && 
               !peakUsage.compare_exchange_weak(peak, current, std::memory_order_relaxed));
    }
    
    /**
     * @brief Record a deallocation
     * @param bytes Number of bytes deallocated
     */
    void recordDeallocation(uint64_t bytes) noexcept {
        deallocations.fetch_add(1, std::memory_order_relaxed);
        bytesDeallocated.fetch_add(bytes, std::memory_order_relaxed);
        currentUsage.fetch_sub(bytes, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get allocation count
     */
    [[nodiscard]] uint64_t getAllocationCount() const noexcept {
        return allocations.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get deallocation count
     */
    [[nodiscard]] uint64_t getDeallocationCount() const noexcept {
        return deallocations.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get current memory usage
     */
    [[nodiscard]] uint64_t getCurrentUsage() const noexcept {
        return currentUsage.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get peak memory usage
     */
    [[nodiscard]] uint64_t getPeakUsage() const noexcept {
        return peakUsage.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get total bytes allocated
     */
    [[nodiscard]] uint64_t getTotalAllocated() const noexcept {
        return bytesAllocated.load(std::memory_order_relaxed);
    }
};

// Global memory statistics instance
inline MemoryStats& getGlobalStats() noexcept {
    static MemoryStats stats;
    return stats;
}

// ============================================================================
// Buffer Class
// ============================================================================

/**
 * @brief Fixed-size buffer with automatic memory management
 */
class Buffer {
public:
    /**
     * @brief Default constructor - creates empty buffer
     */
    Buffer() noexcept = default;
    
    /**
     * @brief Construct buffer with specified size
     * @param size Buffer size in bytes
     */
    explicit Buffer(std::size_t size) 
        : data_(std::make_unique<uint8_t[]>(size)), size_(size), capacity_(size) {
        getGlobalStats().recordAllocation(size);
    }
    
    /**
     * @brief Construct buffer from data
     * @param data Source data
     * @param size Data size
     */
    Buffer(const void* data, std::size_t size)
        : data_(std::make_unique<uint8_t[]>(size)), size_(size), capacity_(size) {
        std::memcpy(data_.get(), data, size);
        getGlobalStats().recordAllocation(size);
    }
    
    /**
     * @brief Copy constructor
     */
    Buffer(const Buffer& other) 
        : data_(std::make_unique<uint8_t[]>(other.size_)), 
          size_(other.size_), capacity_(other.size_) {
        std::memcpy(data_.get(), other.data_.get(), size_);
        getGlobalStats().recordAllocation(size_);
    }
    
    /**
     * @brief Move constructor
     */
    Buffer(Buffer&& other) noexcept
        : data_(std::move(other.data_)), size_(other.size_), capacity_(other.capacity_) {
        other.size_ = 0;
        other.capacity_ = 0;
    }
    
    /**
     * @brief Copy assignment
     */
    Buffer& operator=(const Buffer& other) {
        if (this != &other) {
            if (capacity_ < other.size_) {
                getGlobalStats().recordDeallocation(capacity_);
                data_ = std::make_unique<uint8_t[]>(other.size_);
                capacity_ = other.size_;
                getGlobalStats().recordAllocation(capacity_);
            }
            size_ = other.size_;
            std::memcpy(data_.get(), other.data_.get(), size_);
        }
        return *this;
    }
    
    /**
     * @brief Move assignment
     */
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            if (data_) {
                getGlobalStats().recordDeallocation(capacity_);
            }
            data_ = std::move(other.data_);
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }
    
    /**
     * @brief Destructor
     */
    ~Buffer() {
        if (data_) {
            getGlobalStats().recordDeallocation(capacity_);
        }
    }
    
    /**
     * @brief Get raw data pointer
     */
    [[nodiscard]] uint8_t* data() noexcept { return data_.get(); }
    [[nodiscard]] const uint8_t* data() const noexcept { return data_.get(); }
    
    /**
     * @brief Get buffer size
     */
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    
    /**
     * @brief Get buffer capacity
     */
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    
    /**
     * @brief Check if buffer is empty
     */
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    
    /**
     * @brief Resize buffer
     * @param newSize New size
     */
    void resize(std::size_t newSize) {
        if (newSize > capacity_) {
            auto newData = std::make_unique<uint8_t[]>(newSize);
            if (data_) {
                std::memcpy(newData.get(), data_.get(), size_);
                getGlobalStats().recordDeallocation(capacity_);
            }
            data_ = std::move(newData);
            capacity_ = newSize;
            getGlobalStats().recordAllocation(capacity_);
        }
        size_ = newSize;
    }
    
    /**
     * @brief Clear buffer (set size to 0)
     */
    void clear() noexcept { size_ = 0; }
    
    /**
     * @brief Fill buffer with value
     * @param value Fill value
     */
    void fill(uint8_t value) noexcept {
        if (data_) {
            std::memset(data_.get(), value, size_);
        }
    }
    
    /**
     * @brief Array subscript operator
     */
    [[nodiscard]] uint8_t& operator[](std::size_t index) noexcept {
        return data_[index];
    }
    [[nodiscard]] const uint8_t& operator[](std::size_t index) const noexcept {
        return data_[index];
    }
    
    /**
     * @brief Convert to vector
     */
    [[nodiscard]] std::vector<uint8_t> toVector() const {
        return std::vector<uint8_t>(data_.get(), data_.get() + size_);
    }
    
    /**
     * @brief Create from vector
     */
    static Buffer fromVector(const std::vector<uint8_t>& vec) {
        return Buffer(vec.data(), vec.size());
    }

private:
    std::unique_ptr<uint8_t[]> data_;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};

// ============================================================================
// Object Pool
// ============================================================================

/**
 * @brief Thread-safe object pool for frequently allocated objects
 * @tparam T Object type
 */
template<typename T>
class ObjectPool {
public:
    /**
     * @brief Constructor
     * @param initialCapacity Initial pool capacity
     */
    explicit ObjectPool(std::size_t initialCapacity = 32) {
        pool_.reserve(initialCapacity);
    }
    
    /**
     * @brief Acquire an object from the pool
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Unique pointer to object with custom deleter
     */
    template<typename... Args>
    [[nodiscard]] std::unique_ptr<T, std::function<void(T*)>> acquire(Args&&... args) {
        T* obj = nullptr;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!pool_.empty()) {
                obj = pool_.back();
                pool_.pop_back();
            }
        }
        
        if (obj) {
            // Reinitialize with placement new
            new (obj) T(std::forward<Args>(args)...);
        } else {
            // Allocate new object
            obj = new T(std::forward<Args>(args)...);
            totalAllocated_.fetch_add(1, std::memory_order_relaxed);
        }
        
        activeCount_.fetch_add(1, std::memory_order_relaxed);
        
        // Return with custom deleter that returns to pool
        return std::unique_ptr<T, std::function<void(T*)>>(
            obj,
            [this](T* p) { this->release(p); }
        );
    }
    
    /**
     * @brief Get number of active objects
     */
    [[nodiscard]] std::size_t activeCount() const noexcept {
        return activeCount_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get number of pooled objects
     */
    [[nodiscard]] std::size_t pooledCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }
    
    /**
     * @brief Get total objects allocated
     */
    [[nodiscard]] std::size_t totalAllocated() const noexcept {
        return totalAllocated_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Clear the pool
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (T* obj : pool_) {
            delete obj;
        }
        pool_.clear();
    }
    
    /**
     * @brief Destructor
     */
    ~ObjectPool() {
        clear();
    }

private:
    /**
     * @brief Release object back to pool
     */
    void release(T* obj) {
        if (obj) {
            obj->~T();  // Call destructor
            
            {
                std::lock_guard<std::mutex> lock(mutex_);
                pool_.push_back(obj);
            }
            
            activeCount_.fetch_sub(1, std::memory_order_relaxed);
        }
    }
    
    mutable std::mutex mutex_;
    std::vector<T*> pool_;
    std::atomic<std::size_t> activeCount_{0};
    std::atomic<std::size_t> totalAllocated_{0};
};

// ============================================================================
// Smart Pointer Utilities
// ============================================================================

/**
 * @brief Create a shared_ptr with custom deleter for C resources
 * @tparam T Resource type
 * @param ptr Raw pointer
 * @param deleter Custom deleter function
 * @return Shared pointer with custom deleter
 */
template<typename T, typename Deleter>
[[nodiscard]] std::shared_ptr<T> makeSharedWithDeleter(T* ptr, Deleter deleter) {
    return std::shared_ptr<T>(ptr, deleter);
}

/**
 * @brief Create a unique_ptr with custom deleter for C resources
 * @tparam T Resource type
 * @param ptr Raw pointer
 * @param deleter Custom deleter function
 * @return Unique pointer with custom deleter
 */
template<typename T, typename Deleter>
[[nodiscard]] auto makeUniqueWithDeleter(T* ptr, Deleter deleter) {
    return std::unique_ptr<T, Deleter>(ptr, deleter);
}

/**
 * @brief Safe dynamic_pointer_cast with null check
 * @tparam To Target type
 * @tparam From Source type
 * @param ptr Source shared_ptr
 * @return Cast shared_ptr or nullptr
 */
template<typename To, typename From>
[[nodiscard]] std::shared_ptr<To> safeDynamicCast(const std::shared_ptr<From>& ptr) noexcept {
    return std::dynamic_pointer_cast<To>(ptr);
}

// ============================================================================
// Memory Utilities
// ============================================================================

/**
 * @brief Zero-fill memory securely (prevents compiler optimization)
 * @param ptr Pointer to memory
 * @param size Size in bytes
 */
inline void secureZero(void* ptr, std::size_t size) noexcept {
    if (ptr && size > 0) {
        volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
        while (size--) {
            *p++ = 0;
        }
    }
}

/**
 * @brief Align size to boundary
 * @param size Size to align
 * @param alignment Alignment boundary (must be power of 2)
 * @return Aligned size
 */
[[nodiscard]] constexpr std::size_t alignSize(std::size_t size, std::size_t alignment) noexcept {
    return (size + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief Check if pointer is aligned
 * @param ptr Pointer to check
 * @param alignment Required alignment
 * @return true if aligned
 */
[[nodiscard]] inline bool isAligned(const void* ptr, std::size_t alignment) noexcept {
    return (reinterpret_cast<std::uintptr_t>(ptr) % alignment) == 0;
}

/**
 * @brief Calculate padding needed for alignment
 * @param ptr Current pointer
 * @param alignment Required alignment
 * @return Padding bytes needed
 */
[[nodiscard]] inline std::size_t alignmentPadding(const void* ptr, std::size_t alignment) noexcept {
    auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    auto remainder = addr % alignment;
    return remainder == 0 ? 0 : alignment - remainder;
}

// ============================================================================
// Ring Buffer
// ============================================================================

/**
 * @brief Fixed-size ring buffer (circular buffer)
 * @tparam T Element type
 */
template<typename T>
class RingBuffer {
public:
    /**
     * @brief Constructor
     * @param capacity Buffer capacity
     */
    explicit RingBuffer(std::size_t capacity)
        : buffer_(capacity), capacity_(capacity) {}
    
    /**
     * @brief Push element to buffer
     * @param value Value to push
     * @return true if successful, false if full
     */
    bool push(const T& value) noexcept {
        if (full()) return false;
        buffer_[tail_] = value;
        tail_ = (tail_ + 1) % capacity_;
        ++size_;
        return true;
    }
    
    /**
     * @brief Push element to buffer (move)
     * @param value Value to push
     * @return true if successful, false if full
     */
    bool push(T&& value) noexcept {
        if (full()) return false;
        buffer_[tail_] = std::move(value);
        tail_ = (tail_ + 1) % capacity_;
        ++size_;
        return true;
    }
    
    /**
     * @brief Pop element from buffer
     * @param value Output value
     * @return true if successful, false if empty
     */
    bool pop(T& value) noexcept {
        if (empty()) return false;
        value = std::move(buffer_[head_]);
        head_ = (head_ + 1) % capacity_;
        --size_;
        return true;
    }
    
    /**
     * @brief Peek at front element
     * @return Pointer to front element or nullptr if empty
     */
    [[nodiscard]] T* front() noexcept {
        return empty() ? nullptr : &buffer_[head_];
    }
    [[nodiscard]] const T* front() const noexcept {
        return empty() ? nullptr : &buffer_[head_];
    }
    
    /**
     * @brief Check if buffer is empty
     */
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    
    /**
     * @brief Check if buffer is full
     */
    [[nodiscard]] bool full() const noexcept { return size_ == capacity_; }
    
    /**
     * @brief Get current size
     */
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    
    /**
     * @brief Get capacity
     */
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    
    /**
     * @brief Clear buffer
     */
    void clear() noexcept {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

private:
    std::vector<T> buffer_;
    std::size_t capacity_;
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t size_ = 0;
};

// ============================================================================
// Scoped Memory Guard
// ============================================================================

/**
 * @brief RAII guard for temporary memory allocations
 * @tparam T Element type
 */
template<typename T>
class ScopedMemory {
public:
    /**
     * @brief Constructor - allocates memory
     * @param count Number of elements
     */
    explicit ScopedMemory(std::size_t count)
        : ptr_(new T[count]), count_(count) {
        getGlobalStats().recordAllocation(count * sizeof(T));
    }
    
    /**
     * @brief Destructor - deallocates memory
     */
    ~ScopedMemory() {
        delete[] ptr_;
        getGlobalStats().recordDeallocation(count_ * sizeof(T));
    }
    
    // Non-copyable
    ScopedMemory(const ScopedMemory&) = delete;
    ScopedMemory& operator=(const ScopedMemory&) = delete;
    
    // Non-movable
    ScopedMemory(ScopedMemory&&) = delete;
    ScopedMemory& operator=(ScopedMemory&&) = delete;
    
    /**
     * @brief Get raw pointer
     */
    [[nodiscard]] T* get() noexcept { return ptr_; }
    [[nodiscard]] const T* get() const noexcept { return ptr_; }
    
    /**
     * @brief Array subscript
     */
    [[nodiscard]] T& operator[](std::size_t index) noexcept { return ptr_[index]; }
    [[nodiscard]] const T& operator[](std::size_t index) const noexcept { return ptr_[index]; }
    
    /**
     * @brief Get element count
     */
    [[nodiscard]] std::size_t count() const noexcept { return count_; }
    
    /**
     * @brief Get size in bytes
     */
    [[nodiscard]] std::size_t sizeBytes() const noexcept { return count_ * sizeof(T); }

private:
    T* ptr_;
    std::size_t count_;
};

} // namespace memory
} // namespace dfsmshsm

#endif // HSM_MEMORY_HPP
