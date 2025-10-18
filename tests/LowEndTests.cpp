// Copyright (c) 2024 Fuego Developers
// Low-End Device Test Suite

#include <gtest/gtest.h>
#include "FuegoLowEndConfig.h"

#ifdef FUEGO_LOWEND_DEVICE

#include "Common/MemoryPool.h"
#include "Common/LowEndContainers.h"
#include "crypto/arm64_crypto.h"
#include "Logging/LowEndLogger.h"

class LowEndDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize low-end components
        Common::MemoryPoolManager::getInstance();
        Logging::g_lowEndLogger = new Logging::LowEndLogger();
        Logging::g_lowEndLogger->initialize();
    }
    
    void TearDown() override {
        if (Logging::g_lowEndLogger) {
            Logging::g_lowEndLogger->shutdown();
            delete Logging::g_lowEndLogger;
            Logging::g_lowEndLogger = nullptr;
        }
    }
};

// Test memory pool functionality
TEST_F(LowEndDeviceTest, MemoryPoolAllocation) {
    auto& pool = Common::MemoryPoolManager::getInstance();
    
    // Test small allocation
    void* ptr1 = pool.allocateSmall(64);
    ASSERT_NE(ptr1, nullptr);
    pool.deallocateSmall(ptr1, 64);
    
    // Test medium allocation
    void* ptr2 = pool.allocateSmall(256);
    ASSERT_NE(ptr2, nullptr);
    pool.deallocateSmall(ptr2, 256);
    
    // Test large allocation
    void* ptr3 = pool.allocateLarge(2048);
    ASSERT_NE(ptr3, nullptr);
    pool.deallocateLarge(ptr3);
}

// Test low-end containers
TEST_F(LowEndDeviceTest, LowEndContainers) {
    // Test LowEndVector
    Common::LowEndVector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);
    
    // Test LowEndUnorderedMap
    Common::LowEndUnorderedMap<std::string, int> map;
    map["key1"] = 100;
    map["key2"] = 200;
    
    EXPECT_EQ(map["key1"], 100);
    EXPECT_EQ(map["key2"], 200);
    EXPECT_EQ(map.size(), 2);
}

// Test ARM64 crypto optimizations
TEST_F(LowEndDeviceTest, ARM64Crypto) {
    const uint8_t testData[64] = {0};
    uint8_t hash[32];
    
    // Test ARM64 NEON hash function
    Crypto::ARM64::cn_fast_hash_arm64_neon(testData, 64, hash);
    
    // Verify hash is not all zeros
    bool allZero = true;
    for (int i = 0; i < 32; i++) {
        if (hash[i] != 0) {
            allZero = false;
            break;
        }
    }
    EXPECT_FALSE(allZero);
}

// Test memory usage limits
TEST_F(LowEndDeviceTest, MemoryUsageLimits) {
    Common::LowEndVector<int> vec;
    
    // Test that container respects memory limits
    for (int i = 0; i < 10000; i++) {
        vec.push_back(i);
    }
    
    // Should not exceed maximum size
    EXPECT_LE(vec.size(), LOWEND_CONSTANT(LOWEND_MAX_WALLET_CACHE));
}

// Test logging system
TEST_F(LowEndDeviceTest, LowEndLogging) {
    ASSERT_NE(Logging::g_lowEndLogger, nullptr);
    
    // Test logging at different levels
    Logging::g_lowEndLogger->logError("Test error message");
    Logging::g_lowEndLogger->logWarning("Test warning message");
    Logging::g_lowEndLogger->logInfo("Test info message");
    
    // Verify log count increased
    EXPECT_GT(Logging::g_lowEndLogger->getLogCount(), 0);
}

// Test performance under memory pressure
TEST_F(LowEndDeviceTest, MemoryPressureTest) {
    auto& pool = Common::MemoryPoolManager::getInstance();
    
    // Allocate until pool is exhausted
    std::vector<void*> allocations;
    void* ptr = nullptr;
    
    while ((ptr = pool.allocateSmall(64)) != nullptr) {
        allocations.push_back(ptr);
    }
    
    // Verify we got some allocations
    EXPECT_GT(allocations.size(), 0);
    
    // Clean up
    for (void* p : allocations) {
        pool.deallocateSmall(p, 64);
    }
}

#endif // FUEGO_LOWEND_DEVICE