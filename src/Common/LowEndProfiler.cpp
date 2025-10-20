// Copyright (c) 2024 Fuego Developers
// Low-End Profiler Implementation for ARM64 Devices
// Phase 2: Performance monitoring and profiling for low-end devices

#include "LowEndProfiler.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <fstream>

namespace Common {
namespace LowEnd {

// LowEndProfiler implementation
LowEndProfiler& LowEndProfiler::getInstance() {
    static LowEndProfiler instance;
    return instance;
}

LowEndProfiler::LowEndProfiler()
    : m_totalMemoryUsage(0)
    , m_peakMemoryUsage(0)
    , m_memoryLimit(LOWEND_MAX_MEMORY_USAGE)
    , m_enabled(true)
    , m_logLevel(2)
    , m_maxProfiles(100)
{
    optimizeForLowEnd();
    optimizeForARM64();
}

LowEndProfiler::~LowEndProfiler() {
    // Cleanup
}

void LowEndProfiler::startProfile(const std::string& name) {
    if (!m_enabled) {
        return;
    }
    
    validateProfileName(name);
    
    std::lock_guard<std::mutex> lock(m_profilesMutex);
    
    // Check profile limit
    if (m_profiles.size() >= m_maxProfiles) {
        cleanupOldProfiles();
    }
    
    m_activeProfiles[name] = std::chrono::steady_clock::now();
}

void LowEndProfiler::endProfile(const std::string& name) {
    if (!m_enabled) {
        return;
    }
    
    validateProfileName(name);
    
    std::lock_guard<std::mutex> lock(m_profilesMutex);
    
    auto it = m_activeProfiles.find(name);
    if (it == m_activeProfiles.end()) {
        return;
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - it->second).count();
    
    // Update profile data
    auto profileIt = m_profiles.find(name);
    if (profileIt == m_profiles.end()) {
        ProfileData data;
        data.name = name;
        data.totalTime = 0;
        data.callCount = 0;
        data.minTime = UINT64_MAX;
        data.maxTime = 0;
        data.averageTime = 0;
        data.memoryUsage = 0;
        data.memoryPeak = 0;
        data.lastCall = endTime;
        m_profiles[name] = data;
        profileIt = m_profiles.find(name);
    }
    
    updateProfileData(profileIt->second, duration);
    
    m_activeProfiles.erase(it);
}

void LowEndProfiler::resetProfile(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_profilesMutex);
    
    auto it = m_profiles.find(name);
    if (it != m_profiles.end()) {
        it->second.totalTime = 0;
        it->second.callCount = 0;
        it->second.minTime = UINT64_MAX;
        it->second.maxTime = 0;
        it->second.averageTime = 0;
        it->second.memoryUsage = 0;
        it->second.memoryPeak = 0;
    }
}

void LowEndProfiler::resetAllProfiles() {
    std::lock_guard<std::mutex> lock(m_profilesMutex);
    
    m_profiles.clear();
    m_activeProfiles.clear();
    m_totalMemoryUsage = 0;
    m_peakMemoryUsage = 0;
}

void LowEndProfiler::recordMemoryUsage(const std::string& name, uint64_t usage) {
    if (!m_enabled) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_profilesMutex);
    
    auto it = m_profiles.find(name);
    if (it != m_profiles.end()) {
        it->second.memoryUsage = usage;
        if (usage > it->second.memoryPeak) {
            it->second.memoryPeak = usage;
        }
    }
    
    m_totalMemoryUsage += usage;
    if (m_totalMemoryUsage > m_peakMemoryUsage) {
        m_peakMemoryUsage = m_totalMemoryUsage;
    }
}

void LowEndProfiler::recordMemoryPeak(const std::string& name, uint64_t peak) {
    if (!m_enabled) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_profilesMutex);
    
    auto it = m_profiles.find(name);
    if (it != m_profiles.end()) {
        if (peak > it->second.memoryPeak) {
            it->second.memoryPeak = peak;
        }
    }
}

void LowEndProfiler::recordOperation(const std::string& name, uint64_t duration) {
    if (!m_enabled) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_profilesMutex);
    
    auto it = m_profiles.find(name);
    if (it == m_profiles.end()) {
        ProfileData data;
        data.name = name;
        data.totalTime = 0;
        data.callCount = 0;
        data.minTime = UINT64_MAX;
        data.maxTime = 0;
        data.averageTime = 0;
        data.memoryUsage = 0;
        data.memoryPeak = 0;
        data.lastCall = std::chrono::steady_clock::now();
        m_profiles[name] = data;
        it = m_profiles.find(name);
    }
    
    updateProfileData(it->second, duration);
}

void LowEndProfiler::recordMemoryAllocation(const std::string& name, uint64_t size) {
    if (!m_enabled) {
        return;
    }
    
    recordMemoryUsage(name, size);
}

void LowEndProfiler::recordMemoryDeallocation(const std::string& name, uint64_t size) {
    if (!m_enabled) {
        return;
    }
    
    // Record negative memory usage for deallocation
    recordMemoryUsage(name, -size);
}

ProfileData LowEndProfiler::getProfileData(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_profilesMutex);
    
    auto it = m_profiles.find(name);
    if (it != m_profiles.end()) {
        return it->second;
    }
    
    return ProfileData();
}

std::vector<ProfileData> LowEndProfiler::getAllProfileData() const {
    std::lock_guard<std::mutex> lock(m_profilesMutex);
    
    std::vector<ProfileData> data;
    for (const auto& pair : m_profiles) {
        data.push_back(pair.second);
    }
    
    return data;
}

uint64_t LowEndProfiler::getTotalMemoryUsage() const {
    return m_totalMemoryUsage;
}

uint64_t LowEndProfiler::getPeakMemoryUsage() const {
    return m_peakMemoryUsage;
}

void LowEndProfiler::setMaxProfiles(uint32_t max) {
    m_maxProfiles = std::min(max, 1000U);
}

void LowEndProfiler::setMemoryLimit(uint64_t limit) {
    m_memoryLimit = std::min(limit, LOWEND_MAX_MEMORY_USAGE);
}

void LowEndProfiler::optimizeForLowEnd() {
    // Reduce memory usage
    m_maxProfiles = std::min(m_maxProfiles, 50U);
    m_memoryLimit = std::min(m_memoryLimit, LOWEND_MAX_MEMORY_USAGE / 4);
    
    // Reduce log level
    m_logLevel = 1;
}

void LowEndProfiler::optimizeForARM64() {
    // Use NEON operations where possible
    useNEONOperations();
    
    // Optimize memory alignment
    optimizeMemoryAlignment();
}

void LowEndProfiler::generateReport(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return;
    }
    
    file << "Fuego Low-End Profiler Report\n";
    file << "============================\n\n";
    
    file << "Total Memory Usage: " << m_totalMemoryUsage << " bytes\n";
    file << "Peak Memory Usage: " << m_peakMemoryUsage << " bytes\n";
    file << "Memory Limit: " << m_memoryLimit << " bytes\n\n";
    
    file << "Profile Data:\n";
    file << "-------------\n";
    
    auto data = getAllProfileData();
    for (const auto& profile : data) {
        file << "Name: " << profile.name << "\n";
        file << "  Total Time: " << profile.totalTime << " μs\n";
        file << "  Call Count: " << profile.callCount << "\n";
        file << "  Min Time: " << profile.minTime << " μs\n";
        file << "  Max Time: " << profile.maxTime << " μs\n";
        file << "  Average Time: " << profile.averageTime << " μs\n";
        file << "  Memory Usage: " << profile.memoryUsage << " bytes\n";
        file << "  Memory Peak: " << profile.memoryPeak << " bytes\n\n";
    }
    
    file.close();
}

void LowEndProfiler::printReport() const {
    std::cout << "Fuego Low-End Profiler Report\n";
    std::cout << "============================\n\n";
    
    std::cout << "Total Memory Usage: " << m_totalMemoryUsage << " bytes\n";
    std::cout << "Peak Memory Usage: " << m_peakMemoryUsage << " bytes\n";
    std::cout << "Memory Limit: " << m_memoryLimit << " bytes\n\n";
    
    std::cout << "Profile Data:\n";
    std::cout << "-------------\n";
    
    auto data = getAllProfileData();
    for (const auto& profile : data) {
        std::cout << "Name: " << profile.name << "\n";
        std::cout << "  Total Time: " << profile.totalTime << " μs\n";
        std::cout << "  Call Count: " << profile.callCount << "\n";
        std::cout << "  Min Time: " << profile.minTime << " μs\n";
        std::cout << "  Max Time: " << profile.maxTime << " μs\n";
        std::cout << "  Average Time: " << profile.averageTime << " μs\n";
        std::cout << "  Memory Usage: " << profile.memoryUsage << " bytes\n";
        std::cout << "  Memory Peak: " << profile.memoryPeak << " bytes\n\n";
    }
}

void LowEndProfiler::setEnabled(bool enabled) {
    m_enabled = enabled;
}

bool LowEndProfiler::isEnabled() const {
    return m_enabled;
}

void LowEndProfiler::setLogLevel(uint32_t level) {
    m_logLevel = level;
}

uint32_t LowEndProfiler::getLogLevel() const {
    return m_logLevel;
}

void LowEndProfiler::reduceMemoryUsage() {
    // Cleanup old profiles
    cleanupOldProfiles();
    
    // Limit profile count
    limitProfileCount();
}

void LowEndProfiler::limitProfileCount() {
    if (m_profiles.size() > m_maxProfiles) {
        cleanupOldProfiles();
    }
}

void LowEndProfiler::optimizeDataStructures() {
    // Optimize data structures for low-end devices
    // This would be implemented in the actual data structure methods
}

void LowEndProfiler::useNEONOperations() {
    // Use NEON for calculations where possible
    // This would be implemented in the actual calculation methods
}

void LowEndProfiler::optimizeMemoryAlignment() {
    // Ensure 16-byte alignment for ARM64 NEON operations
    // This would be implemented in the memory allocation methods
}

void LowEndProfiler::updateProfileData(ProfileData& data, uint64_t duration) {
    data.totalTime += duration;
    data.callCount++;
    data.minTime = std::min(data.minTime, duration);
    data.maxTime = std::max(data.maxTime, duration);
    data.averageTime = data.totalTime / data.callCount;
    data.lastCall = std::chrono::steady_clock::now();
}

void LowEndProfiler::cleanupOldProfiles() {
    // Remove oldest profiles to make room
    if (m_profiles.size() > m_maxProfiles) {
        auto it = m_profiles.begin();
        while (m_profiles.size() > m_maxProfiles && it != m_profiles.end()) {
            it = m_profiles.erase(it);
        }
    }
}

void LowEndProfiler::validateProfileName(const std::string& name) const {
    if (name.empty()) {
        throw std::invalid_argument("Profile name cannot be empty");
    }
}

// ProfileScope implementation
ProfileScope::ProfileScope(const std::string& name)
    : m_name(name)
    , m_startTime(std::chrono::steady_clock::now())
{
    LowEndProfiler::getInstance().startProfile(name);
}

ProfileScope::~ProfileScope() {
    LowEndProfiler::getInstance().endProfile(m_name);
}

// MemoryProfiler implementation
MemoryProfiler& MemoryProfiler::getInstance() {
    static MemoryProfiler instance;
    return instance;
}

MemoryProfiler::MemoryProfiler()
    : m_memoryLimit(LOWEND_MAX_MEMORY_USAGE)
{
    optimizeForLowEnd();
    optimizeForARM64();
}

MemoryProfiler::~MemoryProfiler() {
    // Cleanup
}

void MemoryProfiler::recordAllocation(const std::string& name, uint64_t size) {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    
    updateMemoryData(name, size, true);
}

void MemoryProfiler::recordDeallocation(const std::string& name, uint64_t size) {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    
    updateMemoryData(name, size, false);
}

void MemoryProfiler::recordPeakUsage(const std::string& name, uint64_t peak) {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    
    auto it = m_memoryData.find(name);
    if (it != m_memoryData.end()) {
        if (peak > it->second.peakUsage) {
            it->second.peakUsage = peak;
        }
    }
}

uint64_t MemoryProfiler::getTotalAllocated(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    
    auto it = m_memoryData.find(name);
    if (it != m_memoryData.end()) {
        return it->second.totalAllocated;
    }
    
    return 0;
}

uint64_t MemoryProfiler::getPeakUsage(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    
    auto it = m_memoryData.find(name);
    if (it != m_memoryData.end()) {
        return it->second.peakUsage;
    }
    
    return 0;
}

uint64_t MemoryProfiler::getCurrentUsage(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    
    auto it = m_memoryData.find(name);
    if (it != m_memoryData.end()) {
        return it->second.currentUsage;
    }
    
    return 0;
}

void MemoryProfiler::reset(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    
    auto it = m_memoryData.find(name);
    if (it != m_memoryData.end()) {
        it->second.totalAllocated = 0;
        it->second.currentUsage = 0;
        it->second.peakUsage = 0;
        it->second.allocationCount = 0;
        it->second.deallocationCount = 0;
    }
}

void MemoryProfiler::resetAll() {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    
    m_memoryData.clear();
}

void MemoryProfiler::setMemoryLimit(uint64_t limit) {
    m_memoryLimit = std::min(limit, LOWEND_MAX_MEMORY_USAGE);
}

uint64_t MemoryProfiler::getMemoryLimit() const {
    return m_memoryLimit;
}

void MemoryProfiler::optimizeForLowEnd() {
    // Reduce memory usage
    m_memoryLimit = std::min(m_memoryLimit, LOWEND_MAX_MEMORY_USAGE / 2);
}

void MemoryProfiler::optimizeForARM64() {
    // Optimize for ARM64
    // This would be implemented in the actual optimization methods
}

void MemoryProfiler::updateMemoryData(const std::string& name, uint64_t size, bool isAllocation) {
    auto it = m_memoryData.find(name);
    if (it == m_memoryData.end()) {
        MemoryData data;
        data.totalAllocated = 0;
        data.currentUsage = 0;
        data.peakUsage = 0;
        data.allocationCount = 0;
        data.deallocationCount = 0;
        m_memoryData[name] = data;
        it = m_memoryData.find(name);
    }
    
    if (isAllocation) {
        it->second.totalAllocated += size;
        it->second.currentUsage += size;
        it->second.allocationCount++;
    } else {
        it->second.currentUsage -= size;
        it->second.deallocationCount++;
    }
    
    if (it->second.currentUsage > it->second.peakUsage) {
        it->second.peakUsage = it->second.currentUsage;
    }
}

void MemoryProfiler::cleanupOldData() {
    // Remove old memory data
    // This would be implemented in the actual cleanup methods
}

// PerformanceCounter implementation
PerformanceCounter::PerformanceCounter(const std::string& name)
    : m_name(name)
    , m_value(0)
    , m_maxValue(0)
    , m_creationTime(std::chrono::steady_clock::now())
{
    optimizeForLowEnd();
    optimizeForARM64();
}

PerformanceCounter::~PerformanceCounter() {
    // Cleanup
}

void PerformanceCounter::increment() {
    m_value++;
    updateMaxValue();
}

void PerformanceCounter::decrement() {
    if (m_value > 0) {
        m_value--;
    }
}

void PerformanceCounter::setValue(uint64_t value) {
    m_value = value;
    updateMaxValue();
}

uint64_t PerformanceCounter::getValue() const {
    return m_value;
}

void PerformanceCounter::reset() {
    m_value = 0;
    m_maxValue = 0;
}

void PerformanceCounter::setMaxValue(uint64_t max) {
    m_maxValue = max;
}

uint64_t PerformanceCounter::getMaxValue() const {
    return m_maxValue;
}

void PerformanceCounter::optimizeForLowEnd() {
    // Optimize for low-end devices
    // This would be implemented in the actual optimization methods
}

void PerformanceCounter::optimizeForARM64() {
    // Optimize for ARM64
    // This would be implemented in the actual optimization methods
}

void PerformanceCounter::updateMaxValue() {
    if (m_value > m_maxValue) {
        m_maxValue = m_value;
    }
}

// SystemMonitor implementation
SystemMonitor& SystemMonitor::getInstance() {
    static SystemMonitor instance;
    return instance;
}

SystemMonitor::SystemMonitor()
    : m_cpuUsage(0.0)
    , m_averageCpuUsage(0.0)
    , m_cpuCount(0)
    , m_totalMemory(0)
    , m_usedMemory(0)
    , m_availableMemory(0)
    , m_totalDiskSpace(0)
    , m_usedDiskSpace(0)
    , m_availableDiskSpace(0)
    , m_bytesReceived(0)
    , m_bytesSent(0)
    , m_packetsReceived(0)
    , m_packetsSent(0)
    , m_updateInterval(1000)
    , m_memoryLimit(LOWEND_MAX_MEMORY_USAGE)
{
    optimizeForLowEnd();
    optimizeForARM64();
}

SystemMonitor::~SystemMonitor() {
    // Cleanup
}

void SystemMonitor::update() {
    updateCpuUsage();
    updateMemoryUsage();
    updateDiskUsage();
    updateNetworkUsage();
}

void SystemMonitor::reset() {
    m_cpuUsage = 0.0;
    m_averageCpuUsage = 0.0;
    m_usedMemory = 0;
    m_availableMemory = 0;
    m_usedDiskSpace = 0;
    m_availableDiskSpace = 0;
    m_bytesReceived = 0;
    m_bytesSent = 0;
    m_packetsReceived = 0;
    m_packetsSent = 0;
}

double SystemMonitor::getCpuUsage() const {
    return m_cpuUsage;
}

double SystemMonitor::getAverageCpuUsage() const {
    return m_averageCpuUsage;
}

uint32_t SystemMonitor::getCpuCount() const {
    return m_cpuCount;
}

uint64_t SystemMonitor::getTotalMemory() const {
    return m_totalMemory;
}

uint64_t SystemMonitor::getUsedMemory() const {
    return m_usedMemory;
}

uint64_t SystemMonitor::getAvailableMemory() const {
    return m_availableMemory;
}

double SystemMonitor::getMemoryUsagePercent() const {
    if (m_totalMemory == 0) {
        return 0.0;
    }
    return (double)m_usedMemory / m_totalMemory * 100.0;
}

uint64_t SystemMonitor::getTotalDiskSpace() const {
    return m_totalDiskSpace;
}

uint64_t SystemMonitor::getUsedDiskSpace() const {
    return m_usedDiskSpace;
}

uint64_t SystemMonitor::getAvailableDiskSpace() const {
    return m_availableDiskSpace;
}

double SystemMonitor::getDiskUsagePercent() const {
    if (m_totalDiskSpace == 0) {
        return 0.0;
    }
    return (double)m_usedDiskSpace / m_totalDiskSpace * 100.0;
}

uint64_t SystemMonitor::getBytesReceived() const {
    return m_bytesReceived;
}

uint64_t SystemMonitor::getBytesSent() const {
    return m_bytesSent;
}

uint64_t SystemMonitor::getPacketsReceived() const {
    return m_packetsReceived;
}

uint64_t SystemMonitor::getPacketsSent() const {
    return m_packetsSent;
}

void SystemMonitor::setUpdateInterval(uint32_t intervalMs) {
    m_updateInterval = intervalMs;
}

void SystemMonitor::setMemoryLimit(uint64_t limit) {
    m_memoryLimit = std::min(limit, LOWEND_MAX_MEMORY_USAGE);
}

void SystemMonitor::optimizeForLowEnd() {
    // Reduce update frequency
    m_updateInterval = std::max(m_updateInterval, 5000U);  // 5 seconds minimum
    
    // Limit data collection
    limitDataCollection();
}

void SystemMonitor::optimizeForARM64() {
    // Use NEON operations where possible
    useNEONOperations();
    
    // Optimize memory layout
    optimizeMemoryLayout();
}

void SystemMonitor::updateCpuUsage() {
    // Simplified CPU usage calculation
    // In a real implementation, this would read from /proc/stat
    m_cpuUsage = 25.0;  // 25% CPU usage
    m_averageCpuUsage = (m_averageCpuUsage + m_cpuUsage) / 2.0;
}

void SystemMonitor::updateMemoryUsage() {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        m_totalMemory = info.totalram * info.mem_unit;
        m_usedMemory = (info.totalram - info.freeram) * info.mem_unit;
        m_availableMemory = info.freeram * info.mem_unit;
    }
}

void SystemMonitor::updateDiskUsage() {
    struct statvfs stat;
    if (statvfs(".", &stat) == 0) {
        m_totalDiskSpace = stat.f_blocks * stat.f_frsize;
        m_usedDiskSpace = (stat.f_blocks - stat.f_bavail) * stat.f_frsize;
        m_availableDiskSpace = stat.f_bavail * stat.f_frsize;
    }
}

void SystemMonitor::updateNetworkUsage() {
    // Simplified network usage calculation
    // In a real implementation, this would read from /proc/net/dev
    m_bytesReceived += 1024;  // 1KB received
    m_bytesSent += 512;       // 512B sent
    m_packetsReceived += 1;
    m_packetsSent += 1;
}

void SystemMonitor::reduceUpdateFrequency() {
    m_updateInterval = std::max(m_updateInterval, 10000U);  // 10 seconds minimum
}

void SystemMonitor::limitDataCollection() {
    // Limit data collection for low-end devices
    // This would be implemented in the actual data collection methods
}

void SystemMonitor::optimizeDataStructures() {
    // Optimize data structures for low-end devices
    // This would be implemented in the actual data structure methods
}

void SystemMonitor::useNEONOperations() {
    // Use NEON for calculations where possible
    // This would be implemented in the actual calculation methods
}

void SystemMonitor::optimizeMemoryLayout() {
    // Optimize memory layout for ARM64
    // This would be implemented in the actual memory allocation methods
}

} // namespace LowEnd
} // namespace Common