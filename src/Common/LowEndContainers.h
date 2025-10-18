// Copyright (c) 2024 Fuego Developers
// Memory-optimized containers for low-end devices

#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <list>
#include "FuegoLowEndConfig.h"

#ifdef FUEGO_LOWEND_DEVICE

namespace Common {

// Memory-optimized vector with reduced capacity
template<typename T>
class LowEndVector {
public:
    using value_type = T;
    using size_type = size_t;
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;
    
    LowEndVector() { m_data.reserve(16); } // Small initial capacity
    explicit LowEndVector(size_type count) { m_data.reserve(std::min(count, size_t(64))); }
    
    void push_back(const T& value) {
        if (m_data.size() >= LOWEND_CONSTANT(LOWEND_MAX_WALLET_CACHE)) {
            return; // Prevent excessive growth
        }
        m_data.push_back(value);
    }
    
    void push_back(T&& value) {
        if (m_data.size() >= LOWEND_CONSTANT(LOWEND_MAX_WALLET_CACHE)) {
            return;
        }
        m_data.push_back(std::move(value));
    }
    
    void reserve(size_type new_cap) {
        // Limit maximum capacity
        m_data.reserve(std::min(new_cap, size_t(LOWEND_CONSTANT(LOWEND_MAX_WALLET_CACHE))));
    }
    
    void shrink_to_fit() { m_data.shrink_to_fit(); }
    
    // Forward other methods to underlying vector
    T& operator[](size_type pos) { return m_data[pos]; }
    const T& operator[](size_type pos) const { return m_data[pos]; }
    
    T& at(size_type pos) { return m_data.at(pos); }
    const T& at(size_type pos) const { return m_data.at(pos); }
    
    T& front() { return m_data.front(); }
    const T& front() const { return m_data.front(); }
    
    T& back() { return m_data.back(); }
    const T& back() const { return m_data.back(); }
    
    T* data() { return m_data.data(); }
    const T* data() const { return m_data.data(); }
    
    iterator begin() { return m_data.begin(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator cbegin() const { return m_data.cbegin(); }
    
    iterator end() { return m_data.end(); }
    const_iterator end() const { return m_data.end(); }
    const_iterator cend() const { return m_data.cend(); }
    
    bool empty() const { return m_data.empty(); }
    size_type size() const { return m_data.size(); }
    size_type capacity() const { return m_data.capacity(); }
    
    void clear() { m_data.clear(); }
    void resize(size_type count) { m_data.resize(std::min(count, size_t(LOWEND_CONSTANT(LOWEND_MAX_WALLET_CACHE)))); }
    
    void pop_back() { m_data.pop_back(); }
    
private:
    std::vector<T> m_data;
};

// Memory-optimized unordered_map with limited bucket count
template<typename K, typename V>
class LowEndUnorderedMap {
public:
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<const K, V>;
    using size_type = size_t;
    using iterator = typename std::unordered_map<K, V>::iterator;
    using const_iterator = typename std::unordered_map<K, V>::const_iterator;
    
    LowEndUnorderedMap() {
        m_data.max_load_factor(0.75f);
        m_data.reserve(64); // Small initial capacity
    }
    
    V& operator[](const K& key) {
        if (m_data.size() >= LOWEND_CONSTANT(LOWEND_MAX_PEER_LIST)) {
            // Remove oldest entries if at capacity
            auto it = m_data.begin();
            m_data.erase(it);
        }
        return m_data[key];
    }
    
    V& at(const K& key) { return m_data.at(key); }
    const V& at(const K& key) const { return m_data.at(key); }
    
    iterator find(const K& key) { return m_data.find(key); }
    const_iterator find(const K& key) const { return m_data.find(key); }
    
    std::pair<iterator, bool> insert(const value_type& value) {
        if (m_data.size() >= LOWEND_CONSTANT(LOWEND_MAX_PEER_LIST)) {
            auto it = m_data.begin();
            m_data.erase(it);
        }
        return m_data.insert(value);
    }
    
    size_type erase(const K& key) { return m_data.erase(key); }
    iterator erase(iterator pos) { return m_data.erase(pos); }
    
    void clear() { m_data.clear(); }
    bool empty() const { return m_data.empty(); }
    size_type size() const { return m_data.size(); }
    
    iterator begin() { return m_data.begin(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator cbegin() const { return m_data.cbegin(); }
    
    iterator end() { return m_data.end(); }
    const_iterator end() const { return m_data.end(); }
    const_iterator cend() const { return m_data.cend(); }
    
private:
    std::unordered_map<K, V> m_data;
};

// Memory-optimized deque with limited capacity
template<typename T>
class LowEndDeque {
public:
    using value_type = T;
    using size_type = size_t;
    using iterator = typename std::deque<T>::iterator;
    using const_iterator = typename std::deque<T>::const_iterator;
    
    LowEndDeque() { m_data.reserve(32); }
    
    void push_back(const T& value) {
        if (m_data.size() >= LOWEND_CONSTANT(LOWEND_MAX_BLOCK_CACHE)) {
            m_data.pop_front(); // Remove oldest if at capacity
        }
        m_data.push_back(value);
    }
    
    void push_front(const T& value) {
        if (m_data.size() >= LOWEND_CONSTANT(LOWEND_MAX_BLOCK_CACHE)) {
            m_data.pop_back(); // Remove newest if at capacity
        }
        m_data.push_front(value);
    }
    
    void pop_back() { m_data.pop_back(); }
    void pop_front() { m_data.pop_front(); }
    
    T& front() { return m_data.front(); }
    const T& front() const { return m_data.front(); }
    
    T& back() { return m_data.back(); }
    const T& back() const { return m_data.back(); }
    
    T& operator[](size_type pos) { return m_data[pos]; }
    const T& operator[](size_type pos) const { return m_data[pos]; }
    
    bool empty() const { return m_data.empty(); }
    size_type size() const { return m_data.size(); }
    
    void clear() { m_data.clear(); }
    
    iterator begin() { return m_data.begin(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator cbegin() const { return m_data.cbegin(); }
    
    iterator end() { return m_data.end(); }
    const_iterator end() const { return m_data.end(); }
    const_iterator cend() const { return m_data.cend(); }
    
private:
    std::deque<T> m_data;
};

} // namespace Common

#else
// Standard containers for non-lowend builds
namespace Common {
    template<typename T> using LowEndVector = std::vector<T>;
    template<typename K, typename V> using LowEndUnorderedMap = std::unordered_map<K, V>;
    template<typename T> using LowEndDeque = std::deque<T>;
}

#endif // FUEGO_LOWEND_DEVICE