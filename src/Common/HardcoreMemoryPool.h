// Copyright (c) 2024 Fuego Developers
// Hardcore Ultra-Aggressive Memory Pool System
// Maximum optimization for extreme resource constraints

#pragma once

#include <memory>
#include <array>
#include <atomic>
#include <cstring>
#include "FuegoHardcoreConfig.h"

#ifdef FUEGO_HARDCORE_MODE

namespace Common {

// Ultra-compact memory pool with fixed-size blocks
template<size_t ObjectSize, size_t PoolSize>
class HardcoreMemoryPool {
public:
    HARDCORE_ALIGNED char m_blocks[PoolSize * ObjectSize];
    HARDCORE_ALIGNED std::array<uint8_t, PoolSize> m_freeList;
    std::atomic<uint8_t> m_nextFree;
    std::atomic<uint8_t> m_available;
    
    HardcoreMemoryPool() : m_nextFree(0), m_available(PoolSize) {
        // Initialize free list
        for (size_t i = 0; i < PoolSize - 1; ++i) {
            m_freeList[i] = i + 1;
        }
        m_freeList[PoolSize - 1] = 0xFF; // End marker
    }
    
    HARDCORE_FORCE_INLINE void* allocate() {
        uint8_t index = m_nextFree.load(std::memory_order_acquire);
        if (index == 0xFF || m_available.load(std::memory_order_acquire) == 0) {
            return nullptr; // Pool exhausted
        }
        
        m_nextFree.store(m_freeList[index], std::memory_order_release);
        m_available.fetch_sub(1, std::memory_order_release);
        
        return &m_blocks[index * ObjectSize];
    }
    
    HARDCORE_FORCE_INLINE void deallocate(void* ptr) {
        if (!ptr) return;
        
        // Calculate index
        size_t index = (static_cast<char*>(ptr) - m_blocks) / ObjectSize;
        if (index >= PoolSize) return;
        
        // Add back to free list
        uint8_t oldNext = m_nextFree.load(std::memory_order_acquire);
        m_freeList[index] = oldNext;
        m_nextFree.store(index, std::memory_order_release);
        m_available.fetch_add(1, std::memory_order_release);
    }
    
    HARDCORE_FORCE_INLINE size_t getAvailable() const {
        return m_available.load(std::memory_order_acquire);
    }
    
    HARDCORE_FORCE_INLINE size_t getAllocated() const {
        return PoolSize - m_available.load(std::memory_order_acquire);
    }
};

// Ultra-compact memory pool manager
class HardcoreMemoryPoolManager {
public:
    static HardcoreMemoryPoolManager& getInstance() {
        static HardcoreMemoryPoolManager instance;
        return instance;
    }
    
    HARDCORE_FORCE_INLINE void* allocateTiny(size_t size) {
        if (size <= 32) return m_tinyPool.allocate();
        if (size <= 64) return m_smallPool.allocate();
        return m_mediumPool.allocate();
    }
    
    HARDCORE_FORCE_INLINE void deallocateTiny(void* ptr, size_t size) {
        if (size <= 32) m_tinyPool.deallocate(ptr);
        else if (size <= 64) m_smallPool.deallocate(ptr);
        else m_mediumPool.deallocate(ptr);
    }
    
    HARDCORE_FORCE_INLINE void* allocateLarge(size_t size) {
        // For large allocations, use aligned allocation
        return std::aligned_alloc(FUEGO_ARM64_ALIGNMENT, size);
    }
    
    HARDCORE_FORCE_INLINE void deallocateLarge(void* ptr) {
        std::free(ptr);
    }
    
    HARDCORE_FORCE_INLINE void printStats() const {
        // Ultra-minimal stats printing
    }
    
private:
    HardcoreMemoryPoolManager() = default;
    
    HardcoreMemoryPool<32, HARDCORE_CONSTANT(HARDCORE_TINY_POOL_SIZE)> m_tinyPool;
    HardcoreMemoryPool<64, HARDCORE_CONSTANT(HARDCORE_SMALL_POOL_SIZE)> m_smallPool;
    HardcoreMemoryPool<128, HARDCORE_CONSTANT(HARDCORE_MEDIUM_POOL_SIZE)> m_mediumPool;
};

// Ultra-aggressive custom allocator
template<typename T>
class HardcoreAllocator {
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
        using other = HardcoreAllocator<U>;
    };
    
    HardcoreAllocator() = default;
    template<typename U>
    HardcoreAllocator(const HardcoreAllocator<U>&) {}
    
    HARDCORE_FORCE_INLINE pointer allocate(size_type n) {
        size_t total_size = n * sizeof(T);
        if (total_size <= 128) {
            return static_cast<pointer>(HardcoreMemoryPoolManager::getInstance().allocateTiny(total_size));
        } else {
            return static_cast<pointer>(HardcoreMemoryPoolManager::getInstance().allocateLarge(total_size));
        }
    }
    
    HARDCORE_FORCE_INLINE void deallocate(pointer p, size_type n) {
        size_t total_size = n * sizeof(T);
        if (total_size <= 128) {
            HardcoreMemoryPoolManager::getInstance().deallocateTiny(p, total_size);
        } else {
            HardcoreMemoryPoolManager::getInstance().deallocateLarge(p);
        }
    }
    
    template<typename U>
    bool operator==(const HardcoreAllocator<U>&) const { return true; }
    
    template<typename U>
    bool operator!=(const HardcoreAllocator<U>&) const { return false; }
};

// Ultra-compact string with minimal overhead
class HardcoreString {
public:
    static constexpr size_t MAX_SIZE = 255;
    
    HARDCORE_FORCE_INLINE HardcoreString() : m_size(0) {
        m_data[0] = '\0';
    }
    
    HARDCORE_FORCE_INLINE HardcoreString(const char* str) : m_size(0) {
        if (str) {
            size_t len = std::strlen(str);
            if (len < MAX_SIZE) {
                std::memcpy(m_data, str, len);
                m_size = len;
                m_data[len] = '\0';
            }
        }
    }
    
    HARDCORE_FORCE_INLINE const char* c_str() const { return m_data; }
    HARDCORE_FORCE_INLINE size_t size() const { return m_size; }
    HARDCORE_FORCE_INLINE bool empty() const { return m_size == 0; }
    
    HARDCORE_FORCE_INLINE void clear() {
        m_size = 0;
        m_data[0] = '\0';
    }
    
private:
    char m_data[MAX_SIZE + 1];
    uint8_t m_size;
};

} // namespace Common

#endif // FUEGO_HARDCORE_MODE