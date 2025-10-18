// Copyright (c) 2024 Fuego Developers
// Hardcore Ultra-Minimal Containers
// Maximum optimization for extreme resource constraints

#pragma once

#include "FuegoHardcoreConfig.h"
#include "HardcoreMemoryPool.h"
#include <array>
#include <cstring>

#ifdef FUEGO_HARDCORE_MODE

namespace Common {

// Ultra-compact vector with fixed capacity
template<typename T, size_t MaxSize>
class HardcoreVector {
public:
    using value_type = T;
    using size_type = size_t;
    using iterator = T*;
    using const_iterator = const T*;
    
    HARDCORE_FORCE_INLINE HardcoreVector() : m_size(0) {}
    
    HARDCORE_FORCE_INLINE void push_back(const T& value) {
        if (m_size < MaxSize) {
            m_data[m_size++] = value;
        }
    }
    
    HARDCORE_FORCE_INLINE void push_back(T&& value) {
        if (m_size < MaxSize) {
            m_data[m_size++] = std::move(value);
        }
    }
    
    HARDCORE_FORCE_INLINE T& operator[](size_type pos) {
        HARDCORE_ASSERT(pos < m_size);
        return m_data[pos];
    }
    
    HARDCORE_FORCE_INLINE const T& operator[](size_type pos) const {
        HARDCORE_ASSERT(pos < m_size);
        return m_data[pos];
    }
    
    HARDCORE_FORCE_INLINE T& front() {
        HARDCORE_ASSERT(m_size > 0);
        return m_data[0];
    }
    
    HARDCORE_FORCE_INLINE const T& front() const {
        HARDCORE_ASSERT(m_size > 0);
        return m_data[0];
    }
    
    HARDCORE_FORCE_INLINE T& back() {
        HARDCORE_ASSERT(m_size > 0);
        return m_data[m_size - 1];
    }
    
    HARDCORE_FORCE_INLINE const T& back() const {
        HARDCORE_ASSERT(m_size > 0);
        return m_data[m_size - 1];
    }
    
    HARDCORE_FORCE_INLINE T* data() { return m_data; }
    HARDCORE_FORCE_INLINE const T* data() const { return m_data; }
    
    HARDCORE_FORCE_INLINE iterator begin() { return m_data; }
    HARDCORE_FORCE_INLINE const_iterator begin() const { return m_data; }
    HARDCORE_FORCE_INLINE const_iterator cbegin() const { return m_data; }
    
    HARDCORE_FORCE_INLINE iterator end() { return m_data + m_size; }
    HARDCORE_FORCE_INLINE const_iterator end() const { return m_data + m_size; }
    HARDCORE_FORCE_INLINE const_iterator cend() const { return m_data + m_size; }
    
    HARDCORE_FORCE_INLINE bool empty() const { return m_size == 0; }
    HARDCORE_FORCE_INLINE size_type size() const { return m_size; }
    HARDCORE_FORCE_INLINE size_type capacity() const { return MaxSize; }
    
    HARDCORE_FORCE_INLINE void clear() { m_size = 0; }
    
    HARDCORE_FORCE_INLINE void pop_back() {
        if (m_size > 0) --m_size;
    }
    
private:
    HARDCORE_ALIGNED T m_data[MaxSize];
    uint8_t m_size;
};

// Ultra-compact hash map with linear probing
template<typename K, typename V, size_t MaxSize>
class HardcoreHashMap {
public:
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<K, V>;
    using size_type = size_t;
    
    HARDCORE_FORCE_INLINE HardcoreHashMap() : m_size(0) {
        std::memset(m_occupied, 0, sizeof(m_occupied));
    }
    
    HARDCORE_FORCE_INLINE V& operator[](const K& key) {
        size_t index = findIndex(key);
        if (index == MaxSize) {
            // Not found, insert new
            index = findEmptySlot();
            if (index != MaxSize) {
                m_keys[index] = key;
                m_occupied[index] = true;
                ++m_size;
            }
        }
        return m_values[index];
    }
    
    HARDCORE_FORCE_INLINE V& at(const K& key) {
        size_t index = findIndex(key);
        HARDCORE_ASSERT(index != MaxSize);
        return m_values[index];
    }
    
    HARDCORE_FORCE_INLINE const V& at(const K& key) const {
        size_t index = findIndex(key);
        HARDCORE_ASSERT(index != MaxSize);
        return m_values[index];
    }
    
    HARDCORE_FORCE_INLINE size_t find(const K& key) const {
        size_t index = findIndex(key);
        return (index == MaxSize) ? MaxSize : index;
    }
    
    HARDCORE_FORCE_INLINE size_t erase(const K& key) {
        size_t index = findIndex(key);
        if (index != MaxSize) {
            m_occupied[index] = false;
            --m_size;
            return 1;
        }
        return 0;
    }
    
    HARDCORE_FORCE_INLINE void clear() {
        std::memset(m_occupied, 0, sizeof(m_occupied));
        m_size = 0;
    }
    
    HARDCORE_FORCE_INLINE bool empty() const { return m_size == 0; }
    HARDCORE_FORCE_INLINE size_type size() const { return m_size; }
    
private:
    HARDCORE_FORCE_INLINE size_t findIndex(const K& key) const {
        size_t hash = std::hash<K>{}(key) % MaxSize;
        for (size_t i = 0; i < MaxSize; ++i) {
            size_t index = (hash + i) % MaxSize;
            if (m_occupied[index] && m_keys[index] == key) {
                return index;
            }
        }
        return MaxSize;
    }
    
    HARDCORE_FORCE_INLINE size_t findEmptySlot() const {
        for (size_t i = 0; i < MaxSize; ++i) {
            if (!m_occupied[i]) {
                return i;
            }
        }
        return MaxSize;
    }
    
    HARDCORE_ALIGNED K m_keys[MaxSize];
    HARDCORE_ALIGNED V m_values[MaxSize];
    bool m_occupied[MaxSize];
    uint8_t m_size;
};

// Ultra-compact circular buffer
template<typename T, size_t MaxSize>
class HardcoreCircularBuffer {
public:
    using value_type = T;
    using size_type = size_t;
    
    HARDCORE_FORCE_INLINE HardcoreCircularBuffer() : m_head(0), m_tail(0), m_size(0) {}
    
    HARDCORE_FORCE_INLINE void push_back(const T& value) {
        if (m_size < MaxSize) {
            m_data[m_tail] = value;
            m_tail = (m_tail + 1) % MaxSize;
            ++m_size;
        } else {
            // Overwrite oldest
            m_data[m_tail] = value;
            m_tail = (m_tail + 1) % MaxSize;
            m_head = (m_head + 1) % MaxSize;
        }
    }
    
    HARDCORE_FORCE_INLINE void push_front(const T& value) {
        if (m_size < MaxSize) {
            m_head = (m_head - 1 + MaxSize) % MaxSize;
            m_data[m_head] = value;
            ++m_size;
        } else {
            // Overwrite newest
            m_tail = (m_tail - 1 + MaxSize) % MaxSize;
            m_data[m_tail] = value;
        }
    }
    
    HARDCORE_FORCE_INLINE void pop_back() {
        if (m_size > 0) {
            m_tail = (m_tail - 1 + MaxSize) % MaxSize;
            --m_size;
        }
    }
    
    HARDCORE_FORCE_INLINE void pop_front() {
        if (m_size > 0) {
            m_head = (m_head + 1) % MaxSize;
            --m_size;
        }
    }
    
    HARDCORE_FORCE_INLINE T& front() {
        HARDCORE_ASSERT(m_size > 0);
        return m_data[m_head];
    }
    
    HARDCORE_FORCE_INLINE const T& front() const {
        HARDCORE_ASSERT(m_size > 0);
        return m_data[m_head];
    }
    
    HARDCORE_FORCE_INLINE T& back() {
        HARDCORE_ASSERT(m_size > 0);
        size_t last = (m_tail - 1 + MaxSize) % MaxSize;
        return m_data[last];
    }
    
    HARDCORE_FORCE_INLINE const T& back() const {
        HARDCORE_ASSERT(m_size > 0);
        size_t last = (m_tail - 1 + MaxSize) % MaxSize;
        return m_data[last];
    }
    
    HARDCORE_FORCE_INLINE T& operator[](size_type pos) {
        HARDCORE_ASSERT(pos < m_size);
        size_t index = (m_head + pos) % MaxSize;
        return m_data[index];
    }
    
    HARDCORE_FORCE_INLINE const T& operator[](size_type pos) const {
        HARDCORE_ASSERT(pos < m_size);
        size_t index = (m_head + pos) % MaxSize;
        return m_data[index];
    }
    
    HARDCORE_FORCE_INLINE bool empty() const { return m_size == 0; }
    HARDCORE_FORCE_INLINE size_type size() const { return m_size; }
    HARDCORE_FORCE_INLINE size_type capacity() const { return MaxSize; }
    
    HARDCORE_FORCE_INLINE void clear() {
        m_head = m_tail = m_size = 0;
    }
    
private:
    HARDCORE_ALIGNED T m_data[MaxSize];
    uint8_t m_head;
    uint8_t m_tail;
    uint8_t m_size;
};

} // namespace Common

#else
// Standard containers for non-hardcore builds
namespace Common {
    template<typename T, size_t MaxSize> using HardcoreVector = std::vector<T>;
    template<typename K, typename V, size_t MaxSize> using HardcoreHashMap = std::unordered_map<K, V>;
    template<typename T, size_t MaxSize> using HardcoreCircularBuffer = std::deque<T>;
}

#endif // FUEGO_HARDCORE_MODE