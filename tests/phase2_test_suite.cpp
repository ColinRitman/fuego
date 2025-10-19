// Copyright (c) 2024 Fuego Developers
// Phase 2 Test Suite for ARM64 Low-End Devices
// Comprehensive testing for Phase 2 optimizations

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <memory>

// Include Phase 2 components
#include "src/crypto/arm64_advanced_crypto.h"
#include "src/Network/LowEndNetworkManager.h"
#include "src/CryptoNoteCore/LowEndBlockchain.h"
#include "src/Wallet/LowEndWallet.h"
#include "src/Common/LowEndProfiler.h"

using namespace Crypto::ARM64::Advanced;
using namespace Network::LowEnd;
using namespace CryptoNote::LowEnd;
using namespace Wallet::LowEnd;
using namespace Common::LowEnd;

class Phase2TestSuite : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test environment
        m_networkManager = std::make_unique<LowEndNetworkManager>();
        m_blockchain = std::make_unique<LowEndBlockchain>();
        m_wallet = std::make_unique<LowEndWallet>();
        
        // Initialize components
        m_networkManager->initialize();
        m_blockchain->initialize("/tmp/test_blockchain");
        m_wallet->initialize("/tmp/test_wallet");
    }
    
    void TearDown() override {
        // Cleanup test environment
        m_wallet->shutdown();
        m_blockchain->shutdown();
        m_networkManager->shutdown();
    }
    
    std::unique_ptr<LowEndNetworkManager> m_networkManager;
    std::unique_ptr<LowEndBlockchain> m_blockchain;
    std::unique_ptr<LowEndWallet> m_wallet;
};

// ARM64 Advanced Crypto Tests
TEST_F(Phase2TestSuite, ARM64AdvancedCryptoTest) {
    // Test data
    const std::vector<uint8_t> testData = {0x01, 0x02, 0x03, 0x04, 0x05};
    const std::vector<uint8_t> testKey = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                         0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
    const std::vector<uint8_t> testIV = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
                                        0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F};
    
    std::vector<uint8_t> cipher(testData.size());
    std::vector<uint8_t> hash(32);
    
    // Test ChaCha8 encryption
    chacha8_advanced_neon(testData.data(), testData.size(), 
                         testKey.data(), testIV.data(), cipher.data());
    
    // Verify cipher is different from input
    EXPECT_NE(testData, cipher);
    
    // Test hash function
    cn_fast_hash_advanced_neon(testData.data(), testData.size(), hash.data());
    
    // Verify hash is not zero
    bool hashNonZero = false;
    for (uint8_t byte : hash) {
        if (byte != 0) {
            hashNonZero = true;
            break;
        }
    }
    EXPECT_TRUE(hashNonZero);
    
    // Test memory operations
    std::vector<uint8_t> dest(testData.size());
    memcpy_advanced_neon(dest.data(), testData.data(), testData.size());
    EXPECT_EQ(testData, dest);
    
    // Test alignment
    void* aligned = align_advanced_arm64(dest.data(), 16);
    EXPECT_EQ(0, reinterpret_cast<uintptr_t>(aligned) % 16);
}

// Low-End Network Manager Tests
TEST_F(Phase2TestSuite, LowEndNetworkManagerTest) {
    // Test initialization
    EXPECT_TRUE(m_networkManager->getActiveConnections() == 0);
    
    // Test connection limits
    m_networkManager->setMaxConnections(2);
    EXPECT_EQ(2, m_networkManager->getActiveConnections());
    
    // Test message size limits
    m_networkManager->setMaxMessageSize(1024);
    
    // Test memory limits
    m_networkManager->setMemoryLimit(1024 * 1024);  // 1MB
    EXPECT_LE(m_networkManager->getMemoryUsage(), 1024 * 1024);
    
    // Test statistics
    EXPECT_EQ(0, m_networkManager->getBytesSent());
    EXPECT_EQ(0, m_networkManager->getBytesReceived());
    EXPECT_EQ(0, m_networkManager->getMessagesSent());
    EXPECT_EQ(0, m_networkManager->getMessagesReceived());
}

// Low-End Blockchain Tests
TEST_F(Phase2TestSuite, LowEndBlockchainTest) {
    // Test initialization
    EXPECT_EQ(0, m_blockchain->getCurrentHeight());
    EXPECT_EQ(0, m_blockchain->getTotalDifficulty());
    
    // Test memory limits
    m_blockchain->setMemoryLimit(1024 * 1024);  // 1MB
    EXPECT_LE(m_blockchain->getMemoryUsage(), 1024 * 1024);
    
    // Test block cache limits
    m_blockchain->setMaxBlocksInMemory(10);
    m_blockchain->setMaxTransactionsInMemory(100);
    
    // Test compression
    m_blockchain->setStorageCompression(true);
    m_blockchain->setIndexingEnabled(true);
    
    // Test statistics
    EXPECT_EQ(0, m_blockchain->getBlocksProcessed());
    EXPECT_EQ(0, m_blockchain->getTransactionsProcessed());
    EXPECT_EQ(0, m_blockchain->getBlockCount());
    EXPECT_EQ(0, m_blockchain->getTransactionCount());
}

// Low-End Wallet Tests
TEST_F(Phase2TestSuite, LowEndWalletTest) {
    // Test initialization
    EXPECT_FALSE(m_wallet->isWalletSynced());
    
    // Test memory limits
    m_wallet->setMemoryLimit(1024 * 1024);  // 1MB
    EXPECT_LE(m_wallet->getMemoryUsage(), 1024 * 1024);
    
    // Test address cache limits
    m_wallet->setMaxAddressesInMemory(50);
    m_wallet->setMaxTransactionsInMemory(100);
    
    // Test compression
    m_wallet->setStorageCompression(true);
    m_wallet->setIndexingEnabled(true);
    
    // Test wallet state
    auto state = m_wallet->getWalletState();
    EXPECT_EQ(0, state.totalBalance);
    EXPECT_EQ(0, state.unlockedBalance);
    EXPECT_EQ(0, state.transactionCount);
    EXPECT_EQ(0, state.addressCount);
    EXPECT_FALSE(state.isSynced);
    
    // Test statistics
    EXPECT_EQ(0, m_wallet->getAddressesProcessed());
    EXPECT_EQ(0, m_wallet->getTransactionsProcessed());
    EXPECT_EQ(0, m_wallet->getAddressCount());
    EXPECT_EQ(0, m_wallet->getTransactionCount());
}

// Low-End Profiler Tests
TEST_F(Phase2TestSuite, LowEndProfilerTest) {
    auto& profiler = LowEndProfiler::getInstance();
    
    // Test initialization
    EXPECT_TRUE(profiler.isEnabled());
    EXPECT_EQ(2, profiler.getLogLevel());
    
    // Test profiling
    profiler.startProfile("test_profile");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    profiler.endProfile("test_profile");
    
    // Test memory profiling
    profiler.recordMemoryUsage("test_memory", 1024);
    profiler.recordMemoryPeak("test_memory", 2048);
    
    // Test operation recording
    profiler.recordOperation("test_operation", 1000);
    
    // Test statistics
    auto profileData = profiler.getProfileData("test_profile");
    EXPECT_EQ("test_profile", profileData.name);
    EXPECT_GT(profileData.totalTime, 0);
    EXPECT_EQ(1, profileData.callCount);
    
    // Test memory limits
    profiler.setMemoryLimit(1024 * 1024);  // 1MB
    EXPECT_LE(profiler.getTotalMemoryUsage(), 1024 * 1024);
    
    // Test profile limits
    profiler.setMaxProfiles(10);
    
    // Test reset
    profiler.resetProfile("test_profile");
    profileData = profiler.getProfileData("test_profile");
    EXPECT_EQ(0, profileData.totalTime);
    EXPECT_EQ(0, profileData.callCount);
}

// Performance Tests
TEST_F(Phase2TestSuite, PerformanceTest) {
    const int iterations = 1000;
    const int dataSize = 1024;
    
    // Test crypto performance
    std::vector<uint8_t> testData(dataSize);
    std::vector<uint8_t> cipher(dataSize);
    std::vector<uint8_t> hash(32);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        cn_fast_hash_advanced_neon(testData.data(), testData.size(), hash.data());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Verify performance is reasonable (less than 1ms per operation)
    EXPECT_LT(duration.count() / iterations, 1000);
    
    // Test memory operations performance
    std::vector<uint8_t> dest(dataSize);
    
    start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        memcpy_advanced_neon(dest.data(), testData.data(), testData.size());
    }
    
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Verify performance is reasonable (less than 100μs per operation)
    EXPECT_LT(duration.count() / iterations, 100);
}

// Memory Usage Tests
TEST_F(Phase2TestSuite, MemoryUsageTest) {
    // Test network manager memory usage
    EXPECT_LE(m_networkManager->getMemoryUsage(), 1024 * 1024);
    
    // Test blockchain memory usage
    EXPECT_LE(m_blockchain->getMemoryUsage(), 1024 * 1024);
    
    // Test wallet memory usage
    EXPECT_LE(m_wallet->getMemoryUsage(), 1024 * 1024);
    
    // Test profiler memory usage
    auto& profiler = LowEndProfiler::getInstance();
    EXPECT_LE(profiler.getTotalMemoryUsage(), 1024 * 1024);
}

// ARM64 Optimization Tests
TEST_F(Phase2TestSuite, ARM64OptimizationTest) {
    // Test NEON operations
    uint32x4_t a = vdupq_n_u32(1);
    uint32x4_t b = vdupq_n_u32(2);
    uint32x4_t c = vaddq_u32_advanced(a, b);
    
    // Verify NEON operation result
    uint32_t result[4];
    vst1q_u32(result, c);
    EXPECT_EQ(3, result[0]);
    EXPECT_EQ(3, result[1]);
    EXPECT_EQ(3, result[2]);
    EXPECT_EQ(3, result[3]);
    
    // Test memory alignment
    std::vector<uint8_t> data(64);
    void* aligned = align_advanced_arm64(data.data(), 16);
    EXPECT_EQ(0, reinterpret_cast<uintptr_t>(aligned) % 16);
    
    // Test prefetching
    prefetch_read_arm64(data.data());
    prefetch_write_arm64(data.data());
    
    // Test cache operations
    cache_flush_arm64(data.data(), data.size());
    cache_invalidate_arm64(data.data(), data.size());
}

// Low-End Device Simulation Tests
TEST_F(Phase2TestSuite, LowEndDeviceSimulationTest) {
    // Simulate low-end device constraints
    const uint64_t lowMemoryLimit = 512 * 1024;  // 512KB
    const uint32_t lowConnectionLimit = 2;
    const uint32_t lowMessageSizeLimit = 512;
    
    // Test network manager with low-end constraints
    m_networkManager->setMemoryLimit(lowMemoryLimit);
    m_networkManager->setMaxConnections(lowConnectionLimit);
    m_networkManager->setMaxMessageSize(lowMessageSizeLimit);
    
    EXPECT_LE(m_networkManager->getMemoryUsage(), lowMemoryLimit);
    
    // Test blockchain with low-end constraints
    m_blockchain->setMemoryLimit(lowMemoryLimit);
    m_blockchain->setMaxBlocksInMemory(5);
    m_blockchain->setMaxTransactionsInMemory(50);
    
    EXPECT_LE(m_blockchain->getMemoryUsage(), lowMemoryLimit);
    
    // Test wallet with low-end constraints
    m_wallet->setMemoryLimit(lowMemoryLimit);
    m_wallet->setMaxAddressesInMemory(25);
    m_wallet->setMaxTransactionsInMemory(50);
    
    EXPECT_LE(m_wallet->getMemoryUsage(), lowMemoryLimit);
}

// Integration Tests
TEST_F(Phase2TestSuite, IntegrationTest) {
    // Test complete system integration
    EXPECT_TRUE(m_networkManager->initialize());
    EXPECT_TRUE(m_blockchain->initialize("/tmp/test_blockchain"));
    EXPECT_TRUE(m_wallet->initialize("/tmp/test_wallet"));
    
    // Test system shutdown
    m_wallet->shutdown();
    m_blockchain->shutdown();
    m_networkManager->shutdown();
    
    // Verify shutdown
    EXPECT_FALSE(m_wallet->isWalletSynced());
    EXPECT_EQ(0, m_blockchain->getCurrentHeight());
    EXPECT_EQ(0, m_networkManager->getActiveConnections());
}

// Stress Tests
TEST_F(Phase2TestSuite, StressTest) {
    const int stressIterations = 10000;
    
    // Test crypto operations under stress
    std::vector<uint8_t> testData(1024);
    std::vector<uint8_t> hash(32);
    
    for (int i = 0; i < stressIterations; ++i) {
        cn_fast_hash_advanced_neon(testData.data(), testData.size(), hash.data());
    }
    
    // Test memory operations under stress
    std::vector<uint8_t> dest(1024);
    
    for (int i = 0; i < stressIterations; ++i) {
        memcpy_advanced_neon(dest.data(), testData.data(), testData.size());
    }
    
    // Test profiler under stress
    auto& profiler = LowEndProfiler::getInstance();
    
    for (int i = 0; i < stressIterations; ++i) {
        profiler.recordOperation("stress_test", 1000);
    }
    
    // Verify system stability
    EXPECT_TRUE(profiler.isEnabled());
    EXPECT_LE(profiler.getTotalMemoryUsage(), 1024 * 1024);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}