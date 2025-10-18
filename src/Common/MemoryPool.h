// Copyright (c) 2024 Fuego Developers
// Memory Pool System for Low-End Devices

#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include "FuegoLowEndConfig.h"

namespace Common {

#ifdef FUEGO_LOWEND_DEVICE

template<size_t ObjectSize, size_t PoolSize>
class MemoryPool {
public:
    MemoryPool() : m_available(0), m_totalAllocated(0) {
        m_blocks.reserve(PoolSize);
        m_freeBlocks.reserve(PoolSize);
        
        // Pre-allocate blocks
        for (size_t i = 0; i < PoolSize; ++i) {
            m_blocks.emplace_back(new uint8_t[ObjectSize]);
            m_freeBlocks.push_back(m_blocks.back().get());
        }
        m_available = PoolSize;
    }
    
    ~MemoryPool() = default;
    
    void* allocate() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_freeBlocks.empty()) {
            return nullptr; // Pool exhausted
        }
        
        void* ptr = m_freeBlocks.back();
        m_freeBlocks.pop_back();
        --m_available;
        ++m_totalAllocated;
        
        return ptr;
    }
    
    void deallocate(void* ptr) {
        if (!ptr) return;
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_freeBlocks.push_back(static_cast<uint8_t*>(ptr));
        ++m_available;
        --m_totalAllocated;
    }
    
    size_t getAvailable() const { return m_available; }
    size_t getAllocated() const { return m_totalAllocated; }
    
private:
    std::vector<std::unique_ptr<uint8_t[]>> m_blocks;
    std::vector<uint8_t*> m_freeBlocks;
    std::mutex m_mutex;
    std::atomic<size_t> m_available;
    std::atomic<size_t> m_totalAllocated;
};

// Memory pool manager for different object sizes
class MemoryPoolManager {
public:
    static MemoryPoolManager& getInstance() {
        static MemoryPoolManager instance;
        return instance;
    }
    
    void* allocateSmall(size_t size) {
        if (size <= 64) return m_smallPool.allocate();
        if (size <= 256) return m_mediumPool.allocate();
        return m_largePool.allocate();
    }
    
    void deallocateSmall(void* ptr, size_t size) {
        if (size <= 64) m_smallPool.deallocate(ptr);
        else if (size <= 256) m_mediumPool.deallocate(ptr);
        else m_largePool.deallocate(ptr);
    }
    
    void* allocateLarge(size_t size) {
        // For large allocations, use standard allocator
        return std::aligned_alloc(FUEGO_ARM64_ALIGNMENT, size);
    }
    
    void deallocateLarge(void* ptr) {
        std::free(ptr);
    }
    
    void printStats() const {
        // Implementation for debugging memory usage
    }
    
private:
    MemoryPoolManager() = default;
    
    MemoryPool<64, LOWEND_CONSTANT(LOWEND_SMALL_POOL_SIZE)> m_smallPool;
    MemoryPool<256, LOWEND_CONSTANT(LOWEND_MEDIUM_POOL_SIZE)> m_mediumPool;
    MemoryPool<1024, LOWEND_CONSTANT(LOWEND_LARGE_POOL_SIZE)> m_largePool;
};

// Custom allocator for low-end devices
template<typename T>
class LowEndAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    template<typename U>
    struct rebind {
        using other = LowEndAllocator<U>;
    };
    
    LowEndAllocator() = default;
    template<typename U>
    LowEndAllocator(const LowEndAllocator<U>&) {}
    
    pointer allocate(size_type n) {
        if (n * sizeof(T) <= 1024) {
            return static_cast<pointer>(MemoryPoolManager::getInstance().allocateSmall(n * sizeof(T)));
        } else {
            return static_cast<pointer>(MemoryPoolManager::getInstance().allocateLarge(n * sizeof(T)));
        }
    }
    
    void deallocate(pointer p, size_type n) {
        if (n * sizeof(T) <= 1024) {
            MemoryPoolManager::getInstance().deallocateSmall(p, n * sizeof(T));
        } else {
            MemoryPoolManager::getInstance().deallocateLarge(p);
        }
    }
    
    template<typename U>
    bool operator==(const LowEndAllocator<U>&) const { return true; }
    
    template<typename U>
    bool operator!=(const LowEndAllocator<U>&) const { return false; }
};

#endif // FUEGO_LOWEND_DEVICE

} // namespace Common