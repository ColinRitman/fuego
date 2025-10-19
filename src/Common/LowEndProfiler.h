// Copyright (c) 2024 Fuego Developers
// Low-End Profiler for ARM64 Devices
// Phase 2: Performance monitoring and profiling for low-end devices

#pragma once

#include <chrono>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

#include "LowEndConfig.h"

namespace Common {
namespace LowEnd {

struct ProfileData {
    std::string name;
    uint64_t totalTime;
    uint64_t callCount;
    uint64_t minTime;
    uint64_t maxTime;
    uint64_t averageTime;
    uint64_t memoryUsage;
    uint64_t memoryPeak;
    std::chrono::steady_clock::time_point lastCall;
};

class LowEndProfiler {
public:
    static LowEndProfiler& getInstance();
    
    // Profiling operations
    void startProfile(const std::string& name);
    void endProfile(const std::string& name);
    void resetProfile(const std::string& name);
    void resetAllProfiles();
    
    // Memory profiling
    void recordMemoryUsage(const std::string& name, uint64_t usage);
    void recordMemoryPeak(const std::string& name, uint64_t peak);
    
    // Performance monitoring
    void recordOperation(const std::string& name, uint64_t duration);
    void recordMemoryAllocation(const std::string& name, uint64_t size);
    void recordMemoryDeallocation(const std::string& name, uint64_t size);
    
    // Statistics
    ProfileData getProfileData(const std::string& name) const;
    std::vector<ProfileData> getAllProfileData() const;
    uint64_t getTotalMemoryUsage() const;
    uint64_t getPeakMemoryUsage() const;
    
    // Low-end optimizations
    void setMaxProfiles(uint32_t max);
    void setMemoryLimit(uint64_t limit);
    void optimizeForLowEnd();
    
    // ARM64 optimizations
    void optimizeForARM64();
    void useNEONOperations();
    
    // Reporting
    void generateReport(const std::string& filename) const;
    void printReport() const;
    
    // Configuration
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setLogLevel(uint32_t level);
    uint32_t getLogLevel() const;

private:
    LowEndProfiler();
    ~LowEndProfiler();
    
    // Core data
    std::unordered_map<std::string, ProfileData> m_profiles;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_activeProfiles;
    mutable std::mutex m_profilesMutex;
    
    // Memory tracking
    std::atomic<uint64_t> m_totalMemoryUsage;
    std::atomic<uint64_t> m_peakMemoryUsage;
    std::atomic<uint64_t> m_memoryLimit;
    
    // Configuration
    std::atomic<bool> m_enabled;
    std::atomic<uint32_t> m_logLevel;
    std::atomic<uint32_t> m_maxProfiles;
    
    // Low-end optimizations
    void reduceMemoryUsage();
    void limitProfileCount();
    void optimizeDataStructures();
    
    // ARM64 optimizations
    void useNEONForCalculations();
    void optimizeMemoryAlignment();
    
    // Internal methods
    void updateProfileData(ProfileData& data, uint64_t duration);
    void cleanupOldProfiles();
    void validateProfileName(const std::string& name) const;
};

// RAII profiler for automatic timing
class ProfileScope {
public:
    ProfileScope(const std::string& name);
    ~ProfileScope();
    
private:
    std::string m_name;
    std::chrono::steady_clock::time_point m_startTime;
};

// Memory profiler for tracking allocations
class MemoryProfiler {
public:
    static MemoryProfiler& getInstance();
    
    void recordAllocation(const std::string& name, uint64_t size);
    void recordDeallocation(const std::string& name, uint64_t size);
    void recordPeakUsage(const std::string& name, uint64_t peak);
    
    uint64_t getTotalAllocated(const std::string& name) const;
    uint64_t getPeakUsage(const std::string& name) const;
    uint64_t getCurrentUsage(const std::string& name) const;
    
    void reset(const std::string& name);
    void resetAll();
    
    void setMemoryLimit(uint64_t limit);
    uint64_t getMemoryLimit() const;
    
    void optimizeForLowEnd();
    void optimizeForARM64();

private:
    MemoryProfiler();
    ~MemoryProfiler();
    
    struct MemoryData {
        uint64_t totalAllocated;
        uint64_t currentUsage;
        uint64_t peakUsage;
        uint64_t allocationCount;
        uint64_t deallocationCount;
    };
    
    std::unordered_map<std::string, MemoryData> m_memoryData;
    mutable std::mutex m_memoryMutex;
    std::atomic<uint64_t> m_memoryLimit;
    
    void updateMemoryData(const std::string& name, uint64_t size, bool isAllocation);
    void cleanupOldData();
};

// Performance counter for real-time monitoring
class PerformanceCounter {
public:
    PerformanceCounter(const std::string& name);
    ~PerformanceCounter();
    
    void increment();
    void decrement();
    void setValue(uint64_t value);
    uint64_t getValue() const;
    
    void reset();
    void setMaxValue(uint64_t max);
    uint64_t getMaxValue() const;
    
    void optimizeForLowEnd();
    void optimizeForARM64();

private:
    std::string m_name;
    std::atomic<uint64_t> m_value;
    std::atomic<uint64_t> m_maxValue;
    std::chrono::steady_clock::time_point m_creationTime;
    
    void updateMaxValue();
};

// System monitor for overall system health
class SystemMonitor {
public:
    static SystemMonitor& getInstance();
    
    void update();
    void reset();
    
    // CPU monitoring
    double getCpuUsage() const;
    double getAverageCpuUsage() const;
    uint32_t getCpuCount() const;
    
    // Memory monitoring
    uint64_t getTotalMemory() const;
    uint64_t getUsedMemory() const;
    uint64_t getAvailableMemory() const;
    double getMemoryUsagePercent() const;
    
    // Disk monitoring
    uint64_t getTotalDiskSpace() const;
    uint64_t getUsedDiskSpace() const;
    uint64_t getAvailableDiskSpace() const;
    double getDiskUsagePercent() const;
    
    // Network monitoring
    uint64_t getBytesReceived() const;
    uint64_t getBytesSent() const;
    uint64_t getPacketsReceived() const;
    uint64_t getPacketsSent() const;
    
    // Low-end optimizations
    void setUpdateInterval(uint32_t intervalMs);
    void setMemoryLimit(uint64_t limit);
    void optimizeForLowEnd();
    
    // ARM64 optimizations
    void optimizeForARM64();
    void useNEONOperations();

private:
    SystemMonitor();
    ~SystemMonitor();
    
    // System data
    std::atomic<double> m_cpuUsage;
    std::atomic<double> m_averageCpuUsage;
    std::atomic<uint32_t> m_cpuCount;
    
    std::atomic<uint64_t> m_totalMemory;
    std::atomic<uint64_t> m_usedMemory;
    std::atomic<uint64_t> m_availableMemory;
    
    std::atomic<uint64_t> m_totalDiskSpace;
    std::atomic<uint64_t> m_usedDiskSpace;
    std::atomic<uint64_t> m_availableDiskSpace;
    
    std::atomic<uint64_t> m_bytesReceived;
    std::atomic<uint64_t> m_bytesSent;
    std::atomic<uint64_t> m_packetsReceived;
    std::atomic<uint64_t> m_packetsSent;
    
    // Configuration
    std::atomic<uint32_t> m_updateInterval;
    std::atomic<uint64_t> m_memoryLimit;
    
    // Internal methods
    void updateCpuUsage();
    void updateMemoryUsage();
    void updateDiskUsage();
    void updateNetworkUsage();
    
    // Low-end optimizations
    void reduceUpdateFrequency();
    void limitDataCollection();
    void optimizeDataStructures();
    
    // ARM64 optimizations
    void useNEONForCalculations();
    void optimizeMemoryLayout();
};

} // namespace LowEnd
} // namespace Common

// Convenience macros for profiling
#define PROFILE_SCOPE(name) ProfileScope _profile_scope(name)
#define PROFILE_START(name) LowEndProfiler::getInstance().startProfile(name)
#define PROFILE_END(name) LowEndProfiler::getInstance().endProfile(name)
#define PROFILE_MEMORY(name, size) LowEndProfiler::getInstance().recordMemoryUsage(name, size)
#define PROFILE_OPERATION(name, duration) LowEndProfiler::getInstance().recordOperation(name, duration)