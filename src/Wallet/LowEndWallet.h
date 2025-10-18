// Copyright (c) 2024 Fuego Developers
// Low-End Device Wallet Implementation

#pragma once

#include "FuegoLowEndConfig.h"
#include "Common/LowEndContainers.h"
#include "IWallet.h"
#include "Common/MemoryPool.h"

#ifdef FUEGO_LOWEND_DEVICE

namespace CryptoNote {

class LowEndWallet : public IWallet {
public:
    LowEndWallet();
    ~LowEndWallet();
    
    // IWallet interface implementation
    void initialize(const std::string& password) override;
    void shutdown() override;
    
    // Transaction management with limits
    TransactionId sendTransaction(const SendTransaction& transaction) override;
    void getTransaction(TransactionId transactionId, Transaction& transaction) override;
    void getTransactions(TransactionId firstTransactionId, size_t count, 
                        std::vector<Transaction>& transactions) override;
    
    // Balance management
    uint64_t getBalance() const override;
    uint64_t getUnlockedBalance() const override;
    
    // Address management
    std::string getAddress() const override;
    std::vector<std::string> getAddresses() const override;
    
    // Memory optimization
    void optimizeMemory();
    void clearOldTransactions();
    
    // Resource monitoring
    size_t getMemoryUsage() const;
    size_t getTransactionCount() const;
    
private:
    struct TransactionData {
        TransactionId id;
        Transaction transaction;
        uint64_t timestamp;
        bool confirmed;
    };
    
    // Memory-optimized storage
    Common::LowEndVector<TransactionData> m_transactions;
    Common::LowEndUnorderedMap<TransactionId, size_t> m_transactionIndex;
    
    // Wallet state
    std::string m_address;
    uint64_t m_balance;
    uint64_t m_unlockedBalance;
    std::string m_password;
    
    // Memory limits
    static constexpr size_t MAX_TRANSACTIONS = 1000;
    static constexpr size_t MAX_MEMORY_USAGE = 16 * 1024 * 1024; // 16MB limit
    
    // Internal methods
    void updateBalance();
    void cleanupOldTransactions();
    bool isMemoryLimitExceeded() const;
    void compressTransaction(TransactionData& txData);
    void decompressTransaction(TransactionData& txData);
};

} // namespace CryptoNote

#endif // FUEGO_LOWEND_DEVICE