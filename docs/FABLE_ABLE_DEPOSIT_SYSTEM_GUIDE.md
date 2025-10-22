# Fable/Able Deposit System Implementation Guide

## Overview

The Fable/Able Deposit System is a comprehensive stable mechanism implementation for the Fuego blockchain that enables various types of deposits with built-in stability mechanisms, collateral management, and risk management features. This system supports multiple deposit types including Fable stablecoin deposits, Able collateral deposits, stability pools, liquidation funds, and governance stakes.

## Table of Contents

1. [System Architecture](#system-architecture)
2. [Deposit Types](#deposit-types)
3. [Stability Mechanisms](#stability-mechanisms)
4. [Implementation Details](#implementation-details)
5. [Transaction Extra Integration](#transaction-extra-integration)
6. [API Reference](#api-reference)
7. [Configuration](#configuration)
8. [Testing Strategy](#testing-strategy)
9. [Security Considerations](#security-considerations)
10. [Usage Examples](#usage-examples)

---

## System Architecture

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                Fable/Able Deposit System                    │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │   Deposit   │  │  Stability  │  │ Liquidation │        │
│  │  Manager    │  │  Validator  │  │   Manager    │        │
│  └─────────────┘  └─────────────┘  └─────────────┘        │
├─────────────────────────────────────────────────────────────┤
│              Transaction Extra System                       │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │   Tag 0x0A  │  │   Tag 0x0B  │  │   Tag 0x0C  │        │
│  │ Fable/Able  │  │ Stability   │  │ Liquidation │        │
│  │  Deposits   │  │   Pools     │  │   Events    │        │
│  └─────────────┘  └─────────────┘  └─────────────┘        │
└─────────────────────────────────────────────────────────────┘
```

### Data Flow

```
Deposit Creation → Validation → Stability Check → Pool Management → Liquidation (if needed)
```

---

## Deposit Types

### 1. Fable Stablecoin Deposits (FABLE_STABLE)

Fable stablecoin deposits are designed to maintain price stability through various mechanisms:

- **Purpose**: Create and manage stablecoin deposits
- **Collateral**: XFG or other supported assets
- **Stability Target**: Maintain 1:1 peg with target asset
- **Mechanism**: Price pegging with collateral backing

### 2. Able Collateral Deposits (ABLE_COLLATERAL)

Able collateral deposits provide collateral for various DeFi operations:

- **Purpose**: Provide collateral for lending, borrowing, and other DeFi operations
- **Collateral**: XFG or other supported assets
- **Stability Target**: Maintain collateral ratio above minimum threshold
- **Mechanism**: Collateral ratio management

### 3. Stability Pool Deposits (STABILITY_POOL)

Stability pools provide liquidity and stability for the entire system:

- **Purpose**: Pool funds to provide system-wide stability
- **Collateral**: XFG or other supported assets
- **Stability Target**: Maintain pool health above threshold
- **Mechanism**: Pool-based stability management

### 4. Liquidation Fund Deposits (LIQUIDATION_FUND)

Liquidation funds provide capital for liquidating undercollateralized positions:

- **Purpose**: Provide capital for liquidation operations
- **Collateral**: XFG or other supported assets
- **Stability Target**: Maintain sufficient liquidity for liquidations
- **Mechanism**: Liquidation capital management

### 5. Governance Stake Deposits (GOVERNANCE_STAKE)

Governance stakes enable participation in system governance:

- **Purpose**: Enable governance participation
- **Collateral**: XFG or other supported assets
- **Stability Target**: Maintain governance participation threshold
- **Mechanism**: Governance participation management

---

## Stability Mechanisms

### 1. Price Pegging (PRICE_PEG)

Maintains price stability by pegging to a target asset:

```cpp
bool checkPriceStability(const FableAbleDepositData& deposit) const {
    if (deposit.targetPrice <= 0) return false;
    
    double priceDeviation = std::abs(deposit.currentPrice - deposit.targetPrice) / deposit.targetPrice * 100.0;
    return priceDeviation <= m_config.priceDeviationThreshold;
}
```

### 2. Collateral Ratio Management (COLLATERAL_RATIO)

Maintains collateral ratios within acceptable ranges:

```cpp
bool checkCollateralRatio(const FableAbleDepositData& deposit) const {
    double collateralRatio = deposit.calculateCollateralRatio();
    double minRatio = m_config.minCollateralRatio / 100.0;
    double maxRatio = m_config.maxCollateralRatio / 100.0;
    
    return collateralRatio >= minRatio && collateralRatio <= maxRatio;
}
```

### 3. Algorithmic Stabilizer (ALGORITHMIC_STABILIZER)

Uses algorithmic mechanisms to maintain stability:

```cpp
double calculateStabilityScore() const {
    if (targetPrice == 0) return 0.0;
    
    double priceStability = 1.0 - (priceDeviation / 100.0);
    double collateralStability = std::min(1.0, collateralRatio / (minCollateralRatio / 100.0));
    
    return (priceStability * 0.6 + collateralStability * 0.4) * 100.0;
}
```

### 4. Hybrid Mechanism (HYBRID_MECHANISM)

Combines multiple stability mechanisms:

```cpp
bool isDepositStable(const FableAbleDepositData& deposit) const {
    return checkPriceStability(deposit) &&
           checkCollateralRatio(deposit) &&
           checkLiquidationThreshold(deposit);
}
```

---

## Implementation Details

### Core Classes

#### 1. FableAbleDepositManager

Main manager class for deposit operations:

```cpp
class FableAbleDepositManager : public IFableAbleDepositManager {
public:
    // Deposit management
    bool createDeposit(const FableAbleDepositData& deposit) override;
    bool updateDeposit(const Crypto::Hash& depositId, const FableAbleDepositData& deposit) override;
    bool liquidateDeposit(const Crypto::Hash& depositId, const std::string& reason) override;
    bool withdrawDeposit(const Crypto::Hash& depositId) override;
    
    // Deposit queries
    std::optional<FableAbleDepositData> getDeposit(const Crypto::Hash& depositId) const override;
    std::vector<FableAbleDepositData> getDepositsByAddress(const std::string& address) const override;
    std::vector<FableAbleDepositData> getDepositsByType(FableAbleDepositType type) const override;
    
    // Stability management
    bool updateStabilityMetrics() override;
    StabilityMetrics getStabilityMetrics() const override;
    bool checkStabilityThresholds() override;
    std::vector<Crypto::Hash> getLiquidatableDeposits() const override;
    
    // Pool management
    bool createStabilityPool(const StabilityPoolData& pool) override;
    std::optional<StabilityPoolData> getStabilityPool(const Crypto::Hash& poolId) const override;
    std::vector<StabilityPoolData> getAllStabilityPools() const override;
};
```

#### 2. FableAbleStabilityValidator

Validator class for stability checks:

```cpp
class FableAbleStabilityValidator {
public:
    // Validation methods
    bool validateDepositData(const FableAbleDepositData& deposit) const;
    bool validateStabilityPool(const StabilityPoolData& pool) const;
    bool validateLiquidationEvent(const LiquidationEvent& event) const;
    
    // Stability checks
    bool isDepositStable(const FableAbleDepositData& deposit) const;
    bool isDepositLiquidatable(const FableAbleDepositData& deposit) const;
    bool isPoolHealthy(const StabilityPoolData& pool) const;
    
    // Price stability checks
    bool checkPriceStability(const FableAbleDepositData& deposit) const;
    bool checkCollateralRatio(const FableAbleDepositData& deposit) const;
    bool checkLiquidationThreshold(const FableAbleDepositData& deposit) const;
};
```

### Data Structures

#### FableAbleDepositData

Main deposit data structure:

```cpp
struct FableAbleDepositData {
    Crypto::Hash depositId;
    FableAbleDepositType depositType;
    StabilityMechanism stabilityMechanism;
    uint64_t depositAmount;              // XFG amount
    uint64_t collateralAmount;           // Collateral amount
    uint64_t stabilityTarget;            // Target stability value
    uint64_t minCollateralRatio;         // Minimum collateral ratio (basis points)
    uint64_t maxCollateralRatio;         // Maximum collateral ratio (basis points)
    uint64_t liquidationThreshold;       // Liquidation threshold (basis points)
    uint64_t depositTimestamp;
    uint64_t maturityTimestamp;
    std::string depositorAddress;
    std::string collateralAsset;         // Asset used as collateral
    std::string stabilityTargetAsset;    // Asset to stabilize
    DepositStatus status;
    
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
};
```

#### StabilityPoolData

Stability pool data structure:

```cpp
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
};
```

---

## Transaction Extra Integration

### TX_EXTRA Tags

The system uses three new transaction extra tags:

- **TX_EXTRA_FABLE_ABLE_DEPOSIT (0x0A)**: For Fable/Able deposits
- **TX_EXTRA_STABILITY_POOL_DEPOSIT (0x0B)**: For stability pool deposits
- **TX_EXTRA_LIQUIDATION_EVENT (0x0C)**: For liquidation events

### Transaction Extra Structures

#### TransactionExtraFableAbleDeposit

```cpp
struct TransactionExtraFableAbleDeposit {
    Crypto::Hash depositId;           // Unique deposit identifier
    uint8_t depositType;              // FableAbleDepositType
    uint8_t stabilityMechanism;       // StabilityMechanism
    uint64_t depositAmount;           // XFG deposit amount
    uint64_t collateralAmount;        // Collateral amount
    uint64_t stabilityTarget;         // Target stability value
    uint64_t minCollateralRatio;      // Minimum collateral ratio (basis points)
    uint64_t maxCollateralRatio;      // Maximum collateral ratio (basis points)
    uint64_t liquidationThreshold;    // Liquidation threshold (basis points)
    uint64_t maturityTimestamp;       // Deposit maturity timestamp
    std::string depositorAddress;     // Depositor address
    std::string collateralAsset;      // Collateral asset identifier
    std::string stabilityTargetAsset; // Asset to stabilize
    std::vector<uint8_t> metadata;    // Additional metadata
    std::vector<uint8_t> signature;   // Deposit signature
    
    bool serialize(ISerializer& serializer);
    bool isValid() const;
    std::string toString() const;
};
```

#### TransactionExtraStabilityPoolDeposit

```cpp
struct TransactionExtraStabilityPoolDeposit {
    Crypto::Hash poolId;              // Stability pool identifier
    std::string poolName;             // Pool name
    uint8_t poolType;                 // FableAbleDepositType
    uint64_t depositAmount;           // Deposit amount
    uint64_t collateralAmount;        // Collateral amount
    std::string depositorAddress;     // Depositor address
    uint64_t depositTimestamp;        // Deposit timestamp
    std::vector<uint8_t> metadata;    // Additional metadata
    std::vector<uint8_t> signature;   // Deposit signature
    
    bool serialize(ISerializer& serializer);
    bool isValid() const;
    std::string toString() const;
};
```

#### TransactionExtraLiquidationEvent

```cpp
struct TransactionExtraLiquidationEvent {
    Crypto::Hash eventId;             // Liquidation event identifier
    Crypto::Hash depositId;           // Liquidated deposit identifier
    uint64_t liquidatedAmount;        // Liquidated amount
    uint64_t collateralRecovered;     // Collateral recovered
    std::string liquidatorAddress;    // Liquidator address
    std::string reason;               // Liquidation reason
    std::vector<uint8_t> evidence;    // Liquidation evidence
    std::vector<uint8_t> signature;   // Event signature
    
    bool serialize(ISerializer& serializer);
    bool isValid() const;
    std::string toString() const;
};
```

### Helper Functions

#### Creating Transaction Extra

```cpp
// Create Fable/Able deposit extra
bool createTxExtraWithFableAbleDeposit(
    const Crypto::Hash& depositId,
    uint8_t depositType,
    uint8_t stabilityMechanism,
    uint64_t depositAmount,
    uint64_t collateralAmount,
    uint64_t stabilityTarget,
    uint64_t minCollateralRatio,
    uint64_t maxCollateralRatio,
    uint64_t liquidationThreshold,
    uint64_t maturityTimestamp,
    const std::string& depositorAddress,
    const std::string& collateralAsset,
    const std::string& stabilityTargetAsset,
    const std::vector<uint8_t>& metadata,
    std::vector<uint8_t>& extra
);

// Create stability pool deposit extra
bool createTxExtraWithStabilityPoolDeposit(
    const Crypto::Hash& poolId,
    const std::string& poolName,
    uint8_t poolType,
    uint64_t depositAmount,
    uint64_t collateralAmount,
    const std::string& depositorAddress,
    uint64_t depositTimestamp,
    const std::vector<uint8_t>& metadata,
    std::vector<uint8_t>& extra
);

// Create liquidation event extra
bool createTxExtraWithLiquidationEvent(
    const Crypto::Hash& eventId,
    const Crypto::Hash& depositId,
    uint64_t liquidatedAmount,
    uint64_t collateralRecovered,
    const std::string& liquidatorAddress,
    const std::string& reason,
    const std::vector<uint8_t>& evidence,
    std::vector<uint8_t>& extra
);
```

#### Parsing Transaction Extra

```cpp
// Parse Fable/Able deposit from extra
bool getFableAbleDepositFromExtra(
    const std::vector<uint8_t>& tx_extra,
    TransactionExtraFableAbleDeposit& deposit
);

// Parse stability pool deposit from extra
bool getStabilityPoolDepositFromExtra(
    const std::vector<uint8_t>& tx_extra,
    TransactionExtraStabilityPoolDeposit& deposit
);

// Parse liquidation event from extra
bool getLiquidationEventFromExtra(
    const std::vector<uint8_t>& tx_extra,
    TransactionExtraLiquidationEvent& event
);
```

---

## API Reference

### Deposit Management

#### Creating Deposits

```cpp
// Create a new Fable stablecoin deposit
FableAbleDepositData deposit;
deposit.depositId = Crypto::Hash::random();
deposit.depositType = FableAbleDepositType::FABLE_STABLE;
deposit.stabilityMechanism = StabilityMechanism::PRICE_PEG;
deposit.depositAmount = 1000000000; // 1 XFG
deposit.collateralAmount = 1500000000; // 1.5 XFG
deposit.stabilityTarget = 1000000000; // 1 XFG target
deposit.minCollateralRatio = 15000; // 150% (15000 basis points)
deposit.maxCollateralRatio = 50000; // 500% (50000 basis points)
deposit.liquidationThreshold = 12000; // 120% (12000 basis points)
deposit.depositTimestamp = getCurrentTimestamp();
deposit.maturityTimestamp = deposit.depositTimestamp + 86400; // 24 hours
deposit.depositorAddress = "depositor_address";
deposit.collateralAsset = "XFG";
deposit.stabilityTargetAsset = "FABLE";
deposit.status = DepositStatus::ACTIVE;

bool success = depositManager->createDeposit(deposit);
```

#### Updating Deposits

```cpp
// Update deposit stability metrics
auto deposit = depositManager->getDeposit(depositId);
if (deposit.has_value()) {
    deposit->currentPrice = 1.02; // 2% above target
    deposit->targetPrice = 1.0;
    deposit->priceDeviation = 2.0;
    deposit->collateralRatio = deposit->calculateCollateralRatio();
    deposit->stabilityScore = deposit->calculateStabilityScore();
    
    bool success = depositManager->updateDeposit(depositId, *deposit);
}
```

#### Liquidating Deposits

```cpp
// Liquidate undercollateralized deposit
std::vector<Crypto::Hash> liquidatableDeposits = depositManager->getLiquidatableDeposits();
for (const auto& depositId : liquidatableDeposits) {
    bool success = depositManager->liquidateDeposit(depositId, "Low collateral ratio");
    if (success) {
        logger(Logging::INFO) << "Liquidated deposit: " << Common::podToHex(depositId);
    }
}
```

#### Withdrawing Deposits

```cpp
// Withdraw mature deposit
auto deposit = depositManager->getDeposit(depositId);
if (deposit.has_value() && deposit->isMature()) {
    bool success = depositManager->withdrawDeposit(depositId);
    if (success) {
        logger(Logging::INFO) << "Withdrew deposit: " << Common::podToHex(depositId);
    }
}
```

### Stability Management

#### Updating Stability Metrics

```cpp
// Update all stability metrics
bool success = depositManager->updateStabilityMetrics();
if (success) {
    auto metrics = depositManager->getStabilityMetrics();
    logger(Logging::INFO) << "Stability metrics: " << metrics.toString();
}
```

#### Checking Stability Thresholds

```cpp
// Check if all deposits are stable
bool allStable = depositManager->checkStabilityThresholds();
if (!allStable) {
    logger(Logging::WARNING) << "Some deposits are not stable";
}
```

### Pool Management

#### Creating Stability Pools

```cpp
// Create a new stability pool
StabilityPoolData pool;
pool.poolId = Crypto::Hash::random();
pool.poolName = "Main Stability Pool";
pool.poolType = FableAbleDepositType::STABILITY_POOL;
pool.totalDeposits = 0;
pool.totalCollateral = 0;
pool.totalLiquidations = 0;
pool.currentStabilityScore = 100.0;
pool.targetStabilityScore = 100.0;
pool.lastUpdateTimestamp = getCurrentTimestamp();

bool success = depositManager->createStabilityPool(pool);
```

#### Querying Pools

```cpp
// Get all stability pools
auto pools = depositManager->getAllStabilityPools();
for (const auto& pool : pools) {
    logger(Logging::INFO) << "Pool: " << pool.toString();
}
```

---

## Configuration

### FableAbleStabilityConfig

The system uses a comprehensive configuration structure:

```cpp
struct FableAbleStabilityConfig {
    // Deposit limits
    uint64_t minDepositAmount;        // Minimum deposit amount
    uint64_t maxDepositAmount;        // Maximum deposit amount
    uint64_t minCollateralAmount;     // Minimum collateral amount
    uint64_t maxCollateralAmount;     // Maximum collateral amount
    
    // Stability parameters
    double priceDeviationThreshold;   // Maximum price deviation (percentage)
    double minCollateralRatio;        // Minimum collateral ratio (percentage)
    double maxCollateralRatio;        // Maximum collateral ratio (percentage)
    double liquidationThreshold;      // Liquidation threshold (percentage)
    
    // Time parameters
    uint64_t depositMaturityTime;     // Deposit maturity time (seconds)
    uint64_t liquidationGracePeriod;  // Grace period before liquidation (seconds)
    uint64_t stabilityUpdateInterval; // Stability update interval (seconds)
    
    // Economic parameters
    uint64_t stabilityFeeRate;        // Stability fee rate (basis points)
    uint64_t liquidationPenalty;      // Liquidation penalty (basis points)
    uint64_t governanceThreshold;     // Governance participation threshold
    
    // Network parameters
    uint32_t consensusThreshold;      // Consensus threshold for stability decisions
    uint32_t maxLiquidationsPerBlock; // Maximum liquidations per block
    bool enableAutomaticLiquidation;  // Enable automatic liquidation
    bool enableGovernanceVoting;      // Enable governance voting
};
```

### Default Configuration

```cpp
FableAbleStabilityConfig FableAbleStabilityConfig::getDefault() {
    FableAbleStabilityConfig config;
    config.minDepositAmount = 1000000000;        // 1 XFG
    config.maxDepositAmount = 1000000000000;     // 1000 XFG
    config.minCollateralAmount = 500000000;      // 0.5 XFG
    config.maxCollateralAmount = 500000000000;   // 500 XFG
    config.priceDeviationThreshold = 5.0;        // 5%
    config.minCollateralRatio = 15000;           // 150% (15000 basis points)
    config.maxCollateralRatio = 50000;           // 500% (50000 basis points)
    config.liquidationThreshold = 12000;         // 120% (12000 basis points)
    config.depositMaturityTime = 86400;          // 24 hours
    config.liquidationGracePeriod = 3600;        // 1 hour
    config.stabilityUpdateInterval = 300;        // 5 minutes
    config.stabilityFeeRate = 100;               // 1% (100 basis points)
    config.liquidationPenalty = 500;             // 5% (500 basis points)
    config.governanceThreshold = 1000000000;     // 1 XFG
    config.consensusThreshold = 3;               // 3/5 consensus
    config.maxLiquidationsPerBlock = 10;
    config.enableAutomaticLiquidation = true;
    config.enableGovernanceVoting = true;
    return config;
}
```

### Setting Configuration

```cpp
// Set custom configuration
FableAbleStabilityConfig config = FableAbleStabilityConfig::getDefault();
config.minDepositAmount = 500000000; // 0.5 XFG
config.priceDeviationThreshold = 3.0; // 3%
config.enableAutomaticLiquidation = false;

depositManager->setStabilityConfig(config);
```

---

## Testing Strategy

### Unit Tests

The system includes comprehensive unit tests covering:

1. **Deposit Data Validation**
   - Valid deposit creation
   - Invalid deposit rejection
   - Maturity checking
   - Liquidation eligibility

2. **Stability Checks**
   - Price stability validation
   - Collateral ratio validation
   - Liquidation threshold checks
   - Stability score calculation

3. **Deposit Management**
   - Deposit creation, update, liquidation, withdrawal
   - Query operations by address, type, status
   - Stability metrics updates

4. **Pool Management**
   - Pool creation and validation
   - Pool health calculation
   - Pool query operations

5. **Configuration**
   - Default configuration validation
   - Custom configuration setting
   - Configuration parameter validation

### Integration Tests

Integration tests cover:

1. **End-to-End Deposit Flow**
   - Create deposit → Update stability → Liquidate if needed
   - Create deposit → Wait for maturity → Withdraw

2. **Stability Pool Operations**
   - Create pool → Add deposits → Check pool health
   - Pool-based stability management

3. **Transaction Extra Integration**
   - Create transaction with deposit extra
   - Parse deposit from transaction extra
   - Validate transaction extra data

### Test Examples

```cpp
TEST_F(FableAbleDepositSystemTest, DepositDataValidation) {
    auto deposit = createTestDeposit();
    
    EXPECT_TRUE(deposit.isValid());
    EXPECT_FALSE(deposit.isMature());
    EXPECT_FALSE(deposit.isLiquidatable());
    EXPECT_TRUE(deposit.isStable());
}

TEST_F(FableAbleDepositSystemTest, DepositManagerCreateDeposit) {
    auto deposit = createTestDeposit();
    
    bool result = depositManager->createDeposit(deposit);
    EXPECT_TRUE(result);
    
    auto retrievedDeposit = depositManager->getDeposit(deposit.depositId);
    EXPECT_TRUE(retrievedDeposit.has_value());
    EXPECT_EQ(retrievedDeposit->depositId, deposit.depositId);
}

TEST_F(FableAbleDepositSystemTest, StabilityValidatorPriceStability) {
    auto deposit = createTestDeposit();
    deposit.currentPrice = 1.05; // 5% above target
    deposit.targetPrice = 1.0;
    
    bool isStable = stabilityValidator->checkPriceStability(deposit);
    EXPECT_TRUE(isStable); // Within 5% threshold
}
```

---

## Security Considerations

### 1. Deposit Security

- **Amount Validation**: Enforce minimum and maximum deposit amounts
- **Collateral Validation**: Ensure sufficient collateral for deposits
- **Signature Validation**: Validate all deposit signatures
- **Address Validation**: Prevent duplicate or invalid addresses

### 2. Stability Security

- **Price Manipulation Protection**: Implement price deviation thresholds
- **Collateral Ratio Protection**: Enforce minimum collateral ratios
- **Liquidation Protection**: Implement grace periods and fair liquidation
- **Governance Protection**: Require consensus for stability decisions

### 3. Transaction Security

- **Extra Field Validation**: Validate all transaction extra fields
- **Signature Verification**: Verify all transaction signatures
- **Amount Validation**: Validate all transaction amounts
- **Timestamp Validation**: Prevent timestamp manipulation

### 4. Network Security

- **Sybil Attack Prevention**: Prevent single entity from controlling multiple deposits
- **DDoS Protection**: Implement rate limiting for deposit operations
- **Consensus Security**: Ensure stability decisions cannot be manipulated
- **Audit Trail**: Maintain comprehensive logs of all operations

---

## Usage Examples

### Example 1: Creating a Fable Stablecoin Deposit

```cpp
#include "FableAbleDepositSystem.h"

// Create deposit manager
auto logger = Logging::createLogger("FableAbleExample");
FableAbleDepositManager depositManager(*logger);

// Create Fable stablecoin deposit
FableAbleDepositData deposit;
deposit.depositId = Crypto::Hash::random();
deposit.depositType = FableAbleDepositType::FABLE_STABLE;
deposit.stabilityMechanism = StabilityMechanism::PRICE_PEG;
deposit.depositAmount = 1000000000; // 1 XFG
deposit.collateralAmount = 1500000000; // 1.5 XFG (150% collateral)
deposit.stabilityTarget = 1000000000; // 1 XFG target
deposit.minCollateralRatio = 15000; // 150% (15000 basis points)
deposit.maxCollateralRatio = 50000; // 500% (50000 basis points)
deposit.liquidationThreshold = 12000; // 120% (12000 basis points)
deposit.depositTimestamp = getCurrentTimestamp();
deposit.maturityTimestamp = deposit.depositTimestamp + 86400; // 24 hours
deposit.depositorAddress = "depositor_address_123";
deposit.collateralAsset = "XFG";
deposit.stabilityTargetAsset = "FABLE";
deposit.status = DepositStatus::ACTIVE;
deposit.currentPrice = 1.0;
deposit.targetPrice = 1.0;
deposit.priceDeviation = 0.0;
deposit.collateralRatio = 150.0;
deposit.stabilityScore = 100.0;
deposit.metadata = {0x01, 0x02, 0x03};
deposit.signature = {0x04, 0x05, 0x06};

// Create deposit
bool success = depositManager.createDeposit(deposit);
if (success) {
    logger(Logging::INFO) << "Created Fable stablecoin deposit: " << deposit.toString();
} else {
    logger(Logging::ERROR) << "Failed to create deposit";
}
```

### Example 2: Creating a Stability Pool

```cpp
// Create stability pool
StabilityPoolData pool;
pool.poolId = Crypto::Hash::random();
pool.poolName = "Main Stability Pool";
pool.poolType = FableAbleDepositType::STABILITY_POOL;
pool.totalDeposits = 0;
pool.totalCollateral = 0;
pool.totalLiquidations = 0;
pool.currentStabilityScore = 100.0;
pool.targetStabilityScore = 100.0;
pool.lastUpdateTimestamp = getCurrentTimestamp();
pool.activeDeposits = {};
pool.liquidatedDeposits = {};

// Create pool
bool success = depositManager.createStabilityPool(pool);
if (success) {
    logger(Logging::INFO) << "Created stability pool: " << pool.toString();
} else {
    logger(Logging::ERROR) << "Failed to create stability pool";
}
```

### Example 3: Monitoring Stability

```cpp
// Update stability metrics
bool success = depositManager.updateStabilityMetrics();
if (success) {
    auto metrics = depositManager.getStabilityMetrics();
    logger(Logging::INFO) << "Stability metrics: " << metrics.toString();
    
    // Check if all deposits are stable
    bool allStable = depositManager.checkStabilityThresholds();
    if (!allStable) {
        logger(Logging::WARNING) << "Some deposits are not stable";
        
        // Get liquidatable deposits
        auto liquidatableDeposits = depositManager.getLiquidatableDeposits();
        for (const auto& depositId : liquidatableDeposits) {
            logger(Logging::WARNING) << "Deposit liquidatable: " << Common::podToHex(depositId);
        }
    }
}
```

### Example 4: Transaction Extra Integration

```cpp
#include "CryptoNoteCore/TransactionExtra.h"

// Create transaction extra with Fable/Able deposit
std::vector<uint8_t> tx_extra;
Crypto::Hash depositId = Crypto::Hash::random();

bool success = createTxExtraWithFableAbleDeposit(
    depositId,
    static_cast<uint8_t>(FableAbleDepositType::FABLE_STABLE),
    static_cast<uint8_t>(StabilityMechanism::PRICE_PEG),
    1000000000, // 1 XFG
    1500000000, // 1.5 XFG
    1000000000, // 1 XFG target
    15000,      // 150% min collateral ratio
    50000,      // 500% max collateral ratio
    12000,      // 120% liquidation threshold
    getCurrentTimestamp() + 86400, // 24 hours maturity
    "depositor_address",
    "XFG",
    "FABLE",
    {0x01, 0x02, 0x03}, // metadata
    tx_extra
);

if (success) {
    logger(Logging::INFO) << "Created transaction extra with Fable/Able deposit";
    
    // Parse deposit from transaction extra
    TransactionExtraFableAbleDeposit deposit;
    if (getFableAbleDepositFromExtra(tx_extra, deposit)) {
        logger(Logging::INFO) << "Parsed deposit: " << deposit.toString();
    }
} else {
    logger(Logging::ERROR) << "Failed to create transaction extra";
}
```

---

## Conclusion

The Fable/Able Deposit System provides a comprehensive, secure, and flexible foundation for stable mechanisms on the Fuego blockchain. Key benefits include:

### Advantages:
- **Multiple Deposit Types**: Support for various deposit types and use cases
- **Flexible Stability Mechanisms**: Multiple stability mechanisms to choose from
- **Comprehensive Validation**: Extensive validation and security checks
- **Transaction Integration**: Seamless integration with Fuego transaction system
- **Configurable Parameters**: Highly configurable for different use cases
- **Comprehensive Testing**: Extensive test coverage for reliability

### Security Features:
- **Cryptographic Validation**: All deposits must be cryptographically signed
- **Stability Monitoring**: Continuous monitoring of stability metrics
- **Liquidation Protection**: Fair and transparent liquidation mechanisms
- **Governance Integration**: Built-in governance participation
- **Audit Trail**: Comprehensive logging of all operations

This system enables the creation of sophisticated stable mechanisms while maintaining the security and privacy features that make Fuego unique in the cryptocurrency space.