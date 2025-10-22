// Copyright (c) 2017-2025 Elderfire Privacy Council
//
// This file is part of Fuego.
//
// Fuego is free software distributed in the hope that it
// will be useful, but WITHOUT ANY WARRANTY; without even the
// implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. You can redistribute it and/or modify it under the terms
// of the GNU General Public License v3 or later versions as published
// by the Free Software Foundation. Fuego includes elements written
// by third parties. See file labeled LICENSE for more details.
// You should have received a copy of the GNU General Public License
// along with Fuego. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "CryptoNote.h"
#include "CryptoTypes.h"
#include "Common/Logging.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <chrono>

namespace CryptoNote {

// Fable deposit types
enum class FableDepositType {
    FABLE_STABLE = 0x01,      // Fable stablecoin deposits
    ABLE_COLLATERAL = 0x02,    // Able collateral deposits
    STABILITY_POOL = 0x03,     // Stability pool deposits
    LIQUIDATION_FUND = 0x04,   // Liquidation fund deposits
    GOVERNANCE_STAKE = 0x05    // Governance stake deposits
};

// Deposit status
enum class DepositStatus {
    ACTIVE = 0x01,
    LOCKED = 0x02,
    LIQUIDATED = 0x03,
    WITHDRAWN = 0x04,
    SLASHED = 0x05,
    EXPIRED = 0x06
};

// Fable deposit data structure
struct FableDepositData {
    Crypto::Hash depositId;           // Unique deposit identifier
    FableDepositType depositType;     // Type of deposit
    uint64_t xfgAmount;               // XFG amount burned
    uint64_t abelAmount;              // ABEL tokens received
    uint64_t depositTimestamp;        // Deposit creation timestamp
    uint64_t maturityTimestamp;       // Deposit maturity timestamp
    std::string depositorAddress;     // Depositor address
    DepositStatus status;             // Current deposit status
    std::vector<uint8_t> metadata;    // Additional metadata
    std::vector<uint8_t> signature;   // Deposit signature
    
    // Methods
    bool isValid() const;
    bool isMature() const;
    bool isActive() const;
    std::string toString() const;
};

// Fable deposit index entry
struct FableDepositIndexEntry {
    uint64_t totalXfgBurned;          // Total XFG burned
    uint64_t totalAbelMinted;         // Total ABEL tokens minted
    uint64_t activeDeposits;          // Number of active deposits
    uint64_t liquidatedDeposits;      // Number of liquidated deposits
    uint64_t withdrawnDeposits;       // Number of withdrawn deposits
    uint64_t timestamp;               // Block timestamp
    
    bool serialize(ISerializer& serializer);
    bool isValid() const;
    std::string toString() const;
};

// Fable deposit configuration
struct FableDepositConfig {
    uint64_t minDepositAmount;        // Minimum deposit amount (XFG)
    uint64_t maxDepositAmount;        // Maximum deposit amount (XFG)
    uint64_t minMaturityTime;         // Minimum maturity time (seconds)
    uint64_t maxMaturityTime;         // Maximum maturity time (seconds)
    double abelExchangeRate;          // XFG to ABEL exchange rate
    uint64_t liquidationThreshold;    // Liquidation threshold (basis points)
    bool enableAutomaticLiquidation;  // Enable automatic liquidation
    bool enableGovernanceVoting;      // Enable governance voting
    
    static FableDepositConfig getDefault();
    bool isValid() const;
    std::string toString() const;
};

// Fable deposit manager interface
class IFableDepositManager {
public:
    virtual ~IFableDepositManager() = default;
    
    // Deposit management
    virtual bool createDeposit(const FableDepositData& deposit) = 0;
    virtual bool updateDeposit(const Crypto::Hash& depositId, const FableDepositData& deposit) = 0;
    virtual bool liquidateDeposit(const Crypto::Hash& depositId, const std::string& reason) = 0;
    virtual bool withdrawDeposit(const Crypto::Hash& depositId) = 0;
    
    // Deposit queries
    virtual std::optional<FableDepositData> getDeposit(const Crypto::Hash& depositId) const = 0;
    virtual std::vector<FableDepositData> getDepositsByAddress(const std::string& address) const = 0;
    virtual std::vector<FableDepositData> getDepositsByType(FableDepositType type) const = 0;
    virtual std::vector<FableDepositData> getActiveDeposits() const = 0;
    virtual std::vector<FableDepositData> getLiquidatableDeposits() const = 0;
    
    // Deposit index management
    virtual bool addDepositToIndex(const FableDepositData& deposit) = 0;
    virtual bool removeDepositFromIndex(const Crypto::Hash& depositId) = 0;
    virtual FableDepositIndexEntry getDepositIndex(uint32_t blockHeight) const = 0;
    virtual std::vector<FableDepositIndexEntry> getDepositIndexRange(uint32_t startHeight, uint32_t endHeight) const = 0;
    
    // Configuration
    virtual void setConfig(const FableDepositConfig& config) = 0;
    virtual FableDepositConfig getConfig() const = 0;
};

// Fable deposit manager implementation
class FableDepositManager : public IFableDepositManager {
public:
    explicit FableDepositManager(Logging::ILogger& logger);
    virtual ~FableDepositManager() = default;
    
    // Deposit management
    bool createDeposit(const FableDepositData& deposit) override;
    bool updateDeposit(const Crypto::Hash& depositId, const FableDepositData& deposit) override;
    bool liquidateDeposit(const Crypto::Hash& depositId, const std::string& reason) override;
    bool withdrawDeposit(const Crypto::Hash& depositId) override;
    
    // Deposit queries
    std::optional<FableDepositData> getDeposit(const Crypto::Hash& depositId) const override;
    std::vector<FableDepositData> getDepositsByAddress(const std::string& address) const override;
    std::vector<FableDepositData> getDepositsByType(FableDepositType type) const override;
    std::vector<FableDepositData> getActiveDeposits() const override;
    std::vector<FableDepositData> getLiquidatableDeposits() const override;
    
    // Deposit index management
    bool addDepositToIndex(const FableDepositData& deposit) override;
    bool removeDepositFromIndex(const Crypto::Hash& depositId) override;
    FableDepositIndexEntry getDepositIndex(uint32_t blockHeight) const override;
    std::vector<FableDepositIndexEntry> getDepositIndexRange(uint32_t startHeight, uint32_t endHeight) const override;
    
    // Configuration
    void setConfig(const FableDepositConfig& config) override;
    FableDepositConfig getConfig() const override;
    
private:
    Logging::LoggerRef logger;
    FableDepositConfig m_config;
    std::map<Crypto::Hash, FableDepositData> m_deposits;
    std::map<uint32_t, FableDepositIndexEntry> m_depositIndex;
    
    // Helper methods
    bool validateDeposit(const FableDepositData& deposit) const;
    bool processLiquidation(const Crypto::Hash& depositId, const std::string& reason);
    Crypto::Hash generateDepositId(const FableDepositData& deposit) const;
    uint64_t calculateAbelAmount(uint64_t xfgAmount) const;
};

// Fable deposit validator
class FableDepositValidator {
public:
    explicit FableDepositValidator(Logging::ILogger& logger);
    virtual ~FableDepositValidator() = default;
    
    // Validation methods
    bool validateDeposit(const FableDepositData& deposit) const;
    bool validateDepositIndex(const FableDepositIndexEntry& entry) const;
    bool validateConfig(const FableDepositConfig& config) const;
    
    // Deposit checks
    bool isDepositValid(const FableDepositData& deposit) const;
    bool isDepositMature(const FableDepositData& deposit) const;
    bool isDepositLiquidatable(const FableDepositData& deposit) const;
    bool isDepositActive(const FableDepositData& deposit) const;
    
private:
    Logging::LoggerRef logger;
    FableDepositConfig m_config;
    
    // Helper methods
    bool checkDepositAmounts(const FableDepositData& deposit) const;
    bool checkDepositTimestamps(const FableDepositData& deposit) const;
    bool checkDepositAddress(const FableDepositData& deposit) const;
    bool checkDepositSignature(const FableDepositData& deposit) const;
};

} // namespace CryptoNote