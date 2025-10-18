// Copyright (c) 2024 Fuego Developers
// Low-End Device Blockchain Optimizations

#pragma once

#include "FuegoLowEndConfig.h"
#include "Common/LowEndContainers.h"
#include "Blockchain.h"

#ifdef FUEGO_LOWEND_DEVICE

namespace CryptoNote {

class LowEndBlockchain : public Blockchain {
public:
    LowEndBlockchain(const Currency& currency, tx_memory_pool& tx_pool, 
                     ILogger& logger, bool blockchainIndexesEnabled = false, 
                     bool blockchainAutosaveEnabled = false);
    
    // Override methods with low-end optimizations
    bool addNewBlock(const Block& block, block_verification_context& bvc) override;
    bool getBlocks(uint32_t start_offset, uint32_t count, std::vector<Block>& blocks) override;
    bool getBlocks(const std::vector<Crypto::Hash>& block_ids, std::vector<Block>& blocks) override;
    
    // Memory-optimized block storage
    bool storeBlock(const Block& block);
    bool loadBlock(uint32_t height, Block& block);
    
    // Reduced cache management
    void optimizeCache();
    void clearOldCache();
    
    // Resource monitoring
    size_t getMemoryUsage() const;
    size_t getCacheSize() const;
    
private:
    // Reduced block cache
    Common::LowEndUnorderedMap<uint32_t, Block> m_blockCache;
    Common::LowEndDeque<uint32_t> m_cacheOrder;
    
    // Memory limits
    static constexpr size_t MAX_BLOCK_CACHE_SIZE = LOWEND_CONSTANT(LOWEND_MAX_BLOCK_CACHE);
    static constexpr size_t MAX_MEMORY_USAGE = 32 * 1024 * 1024; // 32MB limit
    
    // Internal methods
    void evictOldBlocks();
    bool isMemoryLimitExceeded() const;
    void compressBlock(Block& block);
    void decompressBlock(Block& block);
};

} // namespace CryptoNote

#endif // FUEGO_LOWEND_DEVICE