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

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <unordered_map>
#include <chrono>
#include "crypto/hash.h"
#include "crypto/crypto.h"
#include "Common/StringTools.h"
#include "CryptoNote.h"

namespace CryptoNote {

// Fable/Able Deposit Types
enum class FableAbleDepositType : uint8_t {
    FABLE_STABLE = 0,        // Fable stablecoin deposit
    ABLE_COLLATERAL = 1,     // Able collateral deposit
    STABILITY_POOL = 2,      // Stability pool deposit
    LIQUIDATION_FUND = 3,    // Liquidation fund deposit
    GOVERNANCE_STAKE = 4     // Governance stake deposit
};

// Deposit Status
enum class DepositStatus : uint8_t {
    ACTIVE = 0,
    LOCKED = 1,
    LIQUIDATED = 2,
    WITHDRAWN = 3,
    SLASHED = 4,
    EXPIRED = 5
};

// Stability Mechanism Types
enum class StabilityMechanism : uint8_t {
    PRICE_PEG = 0,           // Price pegging mechanism
    COLLATERAL_RATIO = 1,    // Collateral ratio mechanism
    ALGORITHMIC_STABILIZER = 2, // Algorithmic stabilizer
    HYBRID_MECHANISM = 3     // Hybrid mechanism
};

// Fable/Able Deposit Data Structure
struct FableAbleDepositData {
    Crypto::Hash depositId;
    FableAbleDepositType depositType;
    StabilityMechanism stabilityMechanism;
    uint64_t depositAmount;              // XFG amount
    uint64_t collateralAmount;           // Collateral amount (if applicable)
    uint64_t stabilityTarget;            // Target stability value
    uint64_t minCollateralRatio;         // Minimum collateral ratio (basis points)
    uint64_t maxCollateralRatio;         // Maximum collateral ratio (basis points)
    uint64_t liquidationThreshold;       // Liquidation threshold (basis points)
    uint64_t depositTimestamp;
    uint64_t maturityTimestamp;
    uint64_t lastUpdateTimestamp;
    std::string depositorAddress;
    std::string collateralAsset;         // Asset used as collateral
    std::string stabilityTargetAsset;    // Asset to stabilize
    DepositStatus status;
    std::vector<uint8_t> metadata;
    std::vector<uint8_t> signature;
    
    // Stability tracking
    double currentPrice;
    double targetPrice;
    double priceDeviation;
    double collateralRatio;
    double stabilityScore;
    
    // Methods
    bool isValid() const;
    bool isMature() const;
    bool isLiquidatable() const;
    bool isStable() const;
    double calculateStabilityScore() const;
    double calculateCollateralRatio() const;
    std::string toString() const;
};

// Stability Pool Data
struct StabilityPoolData {
    Crypto::Hash poolId;
    std::string poolName;
    FableAbleDepositType poolType;
    uint64_t totalDeposits;
    uint64_t totalCollateral;
    uint64_t totalLiquidations;
    double currentStabilityScore;
    double targetStabilityScore;
    uint64_t lastUpdateTimestamp;
    std::vector<Crypto::Hash> activeDeposits;
    std::vector<Crypto::Hash> liquidatedDeposits;
    
    bool isValid() const;
    double calculatePoolHealth() const;
    std::string toString() const;
};

// Liquidation Event
struct LiquidationEvent {
    Crypto::Hash eventId;
    Crypto::Hash depositId;
    uint64_t liquidatedAmount;
    uint64_t collateralRecovered;
    uint64_t liquidationTimestamp;
    std::string liquidatorAddress;
    std::string reason;
    std::vector<uint8_t> evidence;
    
    bool isValid() const;
    std::string toString() const;
};

// Stability Configuration
struct FableAbleStabilityConfig {
    // Deposit limits
    uint64_t minDepositAmount;
    uint64_t maxDepositAmount;
    uint64_t minCollateralAmount;
    uint64_t maxCollateralAmount;
    
    // Stability parameters
    double priceDeviationThreshold;      // Maximum price deviation (percentage)
    double minCollateralRatio;          // Minimum collateral ratio (percentage)
    double maxCollateralRatio;          // Maximum collateral ratio (percentage)
    double liquidationThreshold;        // Liquidation threshold (percentage)
    
    // Time parameters
    uint64_t depositMaturityTime;       // Deposit maturity time (seconds)
    uint64_t liquidationGracePeriod;    // Grace period before liquidation (seconds)
    uint64_t stabilityUpdateInterval;   // Stability update interval (seconds)
    
    // Economic parameters
    uint64_t stabilityFeeRate;          // Stability fee rate (basis points)
    uint64_t liquidationPenalty;        // Liquidation penalty (basis points)
    uint64_t governanceThreshold;       // Governance participation threshold
    
    // Network parameters
    uint32_t consensusThreshold;        // Consensus threshold for stability decisions
    uint32_t maxLiquidationsPerBlock;   // Maximum liquidations per block
    bool enableAutomaticLiquidation;    // Enable automatic liquidation
    bool enableGovernanceVoting;        // Enable governance voting
    
    static FableAbleStabilityConfig getDefault();
    bool isValid() const;
};

// Stability Metrics
struct StabilityMetrics {
    uint64_t totalDeposits;
    uint64_t totalCollateral;
    uint64_t totalLiquidations;
    double averageStabilityScore;
    double averageCollateralRatio;
    double priceStabilityIndex;
    uint64_t activeDeposits;
    uint64_t liquidatedDeposits;
    uint64_t lastUpdateTimestamp;
    
    bool isValid() const;
    std::string toString() const;
};

// Fable/Able Deposit Manager Interface
class IFableAbleDepositManager {
public:
    virtual ~IFableAbleDepositManager() = default;
    
    // Deposit management
    virtual bool createDeposit(const FableAbleDepositData& deposit) = 0;
    virtual bool updateDeposit(const Crypto::Hash& depositId, const FableAbleDepositData& deposit) = 0;
    virtual bool liquidateDeposit(const Crypto::Hash& depositId, const std::string& reason) = 0;
    virtual bool withdrawDeposit(const Crypto::Hash& depositId) = 0;
    
    // Deposit queries
    virtual std::optional<FableAbleDepositData> getDeposit(const Crypto::Hash& depositId) const = 0;
    virtual std::vector<FableAbleDepositData> getDepositsByAddress(const std::string& address) const = 0;
    virtual std::vector<FableAbleDepositData> getDepositsByType(FableAbleDepositType type) const = 0;
    virtual std::vector<FableAbleDepositData> getActiveDeposits() const = 0;
    
    // Stability management
    virtual bool updateStabilityMetrics() = 0;
    virtual StabilityMetrics getStabilityMetrics() const = 0;
    virtual bool checkStabilityThresholds() = 0;
    virtual std::vector<Crypto::Hash> getLiquidatableDeposits() const = 0;
    
    // Pool management
    virtual bool createStabilityPool(const StabilityPoolData& pool) = 0;
    virtual std::optional<StabilityPoolData> getStabilityPool(const Crypto::Hash& poolId) const = 0;
    virtual std::vector<StabilityPoolData> getAllStabilityPools() const = 0;
    
    // Configuration
    virtual void setStabilityConfig(const FableAbleStabilityConfig& config) = 0;
    virtual FableAbleStabilityConfig getStabilityConfig() const = 0;
};

// Fable/Able Deposit Manager Implementation
class FableAbleDepositManager : public IFableAbleDepositManager {
public:
    explicit FableAbleDepositManager(Logging::ILogger& logger);
    ~FableAbleDepositManager();
    
    // Deposit management
    bool createDeposit(const FableAbleDepositData& deposit) override;
    bool updateDeposit(const Crypto::Hash& depositId, const FableAbleDepositData& deposit) override;
    bool liquidateDeposit(const Crypto::Hash& depositId, const std::string& reason) override;
    bool withdrawDeposit(const Crypto::Hash& depositId) override;
    
    // Deposit queries
    std::optional<FableAbleDepositData> getDeposit(const Crypto::Hash& depositId) const override;
    std::vector<FableAbleDepositData> getDepositsByAddress(const std::string& address) const override;
    std::vector<FableAbleDepositData> getDepositsByType(FableAbleDepositType type) const override;
    std::vector<FableAbleDepositData> getActiveDeposits() const override;
    
    // Stability management
    bool updateStabilityMetrics() override;
    StabilityMetrics getStabilityMetrics() const override;
    bool checkStabilityThresholds() override;
    std::vector<Crypto::Hash> getLiquidatableDeposits() const override;
    
    // Pool management
    bool createStabilityPool(const StabilityPoolData& pool) override;
    std::optional<StabilityPoolData> getStabilityPool(const Crypto::Hash& poolId) const override;
    std::vector<StabilityPoolData> getAllStabilityPools() const override;
    
    // Configuration
    void setStabilityConfig(const FableAbleStabilityConfig& config) override;
    FableAbleStabilityConfig getStabilityConfig() const override;

private:
    Logging::LoggerRef logger;
    FableAbleStabilityConfig m_config;
    std::unordered_map<Crypto::Hash, FableAbleDepositData> m_deposits;
    std::unordered_map<Crypto::Hash, StabilityPoolData> m_stabilityPools;
    std::vector<LiquidationEvent> m_liquidationEvents;
    StabilityMetrics m_metrics;
    
    // Helper methods
    bool validateDeposit(const FableAbleDepositData& deposit) const;
    bool validateStabilityPool(const StabilityPoolData& pool) const;
    bool checkDepositLiquidation(const FableAbleDepositData& deposit) const;
    void updateDepositStability(FableAbleDepositData& deposit);
    void processLiquidation(const Crypto::Hash& depositId, const std::string& reason);
    void updateStabilityMetricsInternal();
    Crypto::Hash generateDepositId(const FableAbleDepositData& deposit) const;
    bool verifyDepositSignature(const FableAbleDepositData& deposit) const;
};

// Stability Validator
class FableAbleStabilityValidator {
public:
    explicit FableAbleStabilityValidator(Logging::ILogger& logger);
    ~FableAbleStabilityValidator();
    
    // Validation methods
    bool validateDepositData(const FableAbleDepositData& deposit) const;
    bool validateStabilityPool(const StabilityPoolData& pool) const;
    bool validateLiquidationEvent(const LiquidationEvent& event) const;
    bool validateStabilityConfig(const FableAbleStabilityConfig& config) const;
    
    // Stability checks
    bool isDepositStable(const FableAbleDepositData& deposit) const;
    bool isDepositLiquidatable(const FableAbleDepositData& deposit) const;
    bool isPoolHealthy(const StabilityPoolData& pool) const;
    
    // Price stability checks
    bool checkPriceStability(const FableAbleDepositData& deposit) const;
    bool checkCollateralRatio(const FableAbleDepositData& deposit) const;
    bool checkLiquidationThreshold(const FableAbleDepositData& deposit) const;

private:
    Logging::LoggerRef logger;
    FableAbleStabilityConfig m_config;
    
    // Helper methods
    double calculatePriceDeviation(double currentPrice, double targetPrice) const;
    double calculateCollateralRatio(uint64_t collateralAmount, uint64_t depositAmount) const;
    bool isWithinThreshold(double value, double threshold) const;
};

} // namespace CryptoNote