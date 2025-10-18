// Copyright (c) 2024 Fuego Developers
// Hardcore Ultra-Minimal Core Implementation
// Maximum optimization for extreme resource constraints

#pragma once

#include "FuegoHardcoreConfig.h"
#include "Common/HardcoreContainers.h"
#include "Common/HardcoreMemoryPool.h"
#include "Blockchain.h"

#ifdef FUEGO_HARDCORE_MODE

namespace CryptoNote {

class HardcoreCore {
public:
    HardcoreCore();
    ~HardcoreCore();
    
    // Ultra-minimal initialization
    HARDCORE_FORCE_INLINE bool initialize();
    HARDCORE_FORCE_INLINE void shutdown();
    
    // Ultra-minimal block operations
    HARDCORE_FORCE_INLINE bool addBlock(const Block& block);
    HARDCORE_FORCE_INLINE bool getBlock(uint32_t height, Block& block);
    
    // Ultra-minimal transaction operations
    HARDCORE_FORCE_INLINE bool addTransaction(const Transaction& transaction);
    HARDCORE_FORCE_INLINE bool getTransaction(const Crypto::Hash& hash, Transaction& transaction);
    
    // Ultra-minimal balance operations
    HARDCORE_FORCE_INLINE uint64_t getBalance(const std::string& address);
    HARDCORE_FORCE_INLINE uint64_t getUnlockedBalance(const std::string& address);
    
    // Ultra-minimal mining operations
    HARDCORE_FORCE_INLINE bool startMining(const std::string& address);
    HARDCORE_FORCE_INLINE void stopMining();
    HARDCORE_FORCE_INLINE bool isMining() const;
    
    // Ultra-minimal network operations
    HARDCORE_FORCE_INLINE bool connectToNetwork();
    HARDCORE_FORCE_INLINE void disconnectFromNetwork();
    HARDCORE_FORCE_INLINE bool isConnected() const;
    
    // Ultra-minimal resource monitoring
    HARDCORE_FORCE_INLINE size_t getMemoryUsage() const;
    HARDCORE_FORCE_INLINE size_t getCpuUsage() const;
    
private:
    // Ultra-minimal state
    bool m_initialized;
    bool m_mining;
    bool m_connected;
    
    // Ultra-minimal storage
    HardcoreVector<Block, HARDCORE_CONSTANT(HARDCORE_MAX_BLOCK_CACHE)> m_blocks;
    HardcoreHashMap<Crypto::Hash, Transaction, HARDCORE_CONSTANT(HARDCORE_MAX_TX_POOL_SIZE)> m_transactions;
    HardcoreHashMap<HardcoreString, uint64_t, HARDCORE_CONSTANT(HARDCORE_MAX_WALLET_CACHE)> m_balances;
    
    // Ultra-minimal mining state
    HardcoreString m_miningAddress;
    uint32_t m_miningHeight;
    
    // Ultra-minimal network state
    HardcoreString m_networkAddress;
    uint16_t m_networkPort;
    
    // Ultra-minimal resource monitoring
    size_t m_memoryUsage;
    size_t m_cpuUsage;
    
    // Internal methods
    HARDCORE_FORCE_INLINE void updateMemoryUsage();
    HARDCORE_FORCE_INLINE void updateCpuUsage();
    HARDCORE_FORCE_INLINE bool validateBlock(const Block& block);
    HARDCORE_FORCE_INLINE bool validateTransaction(const Transaction& transaction);
};

} // namespace CryptoNote

#endif // FUEGO_HARDCORE_MODE