# Fable Deposit System Implementation Guide

## Overview

The Fable Deposit System is a simplified burn deposit system for the Fuego blockchain that follows the same pattern as HEAT burns (tag `0x08`) but uses the ABEL token contract instead. This system enables users to burn XFG tokens in exchange for ABEL tokens, providing a foundation for stable mechanisms and DeFi operations.

## Table of Contents

1. [System Architecture](#system-architecture)
2. [Deposit Types](#deposit-types)
3. [Transaction Extra Integration](#transaction-extra-integration)
4. [Implementation Details](#implementation-details)
5. [API Reference](#api-reference)
6. [Configuration](#configuration)
7. [Testing Strategy](#testing-strategy)
8. [Security Considerations](#security-considerations)
9. [Usage Examples](#usage-examples)

---

## System Architecture

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                Fable Deposit System                         │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │   Deposit   │  │  Deposit    │  │  Deposit    │        │
│  │  Manager    │  │  Validator  │  │   Index     │        │
│  └─────────────┘  └─────────────┘  └─────────────┘        │
├─────────────────────────────────────────────────────────────┤
│              Transaction Extra System                       │
│  ┌─────────────┐                                           │
│  │   Tag 0xAB  │                                           │
│  │ Fable       │                                           │
│  │ Commitments │                                           │
│  └─────────────┘                                           │
└─────────────────────────────────────────────────────────────┘
```

### Data Flow

```
XFG Burn → Fable Commitment → ABEL Mint → Deposit Index → Withdrawal/Liquidation
```

---

## Deposit Types

### 1. Fable Stablecoin Deposits (FABLE_STABLE)

Fable stablecoin deposits are designed to create stablecoin tokens:

- **Purpose**: Create and manage stablecoin deposits
- **Collateral**: XFG tokens burned
- **Reward**: ABEL tokens minted
- **Mechanism**: Burn XFG to mint ABEL at 1:1 ratio

### 2. Able Collateral Deposits (ABLE_COLLATERAL)

Able collateral deposits provide collateral for various DeFi operations:

- **Purpose**: Provide collateral for lending, borrowing, and other DeFi operations
- **Collateral**: XFG tokens burned
- **Reward**: ABEL tokens minted
- **Mechanism**: Burn XFG to mint ABEL for collateral

### 3. Stability Pool Deposits (STABILITY_POOL)

Stability pools provide liquidity and stability for the entire system:

- **Purpose**: Pool funds to provide system-wide stability
- **Collateral**: XFG tokens burned
- **Reward**: ABEL tokens minted
- **Mechanism**: Burn XFG to mint ABEL for stability

### 4. Liquidation Fund Deposits (LIQUIDATION_FUND)

Liquidation funds provide capital for liquidating undercollateralized positions:

- **Purpose**: Provide capital for liquidation operations
- **Collateral**: XFG tokens burned
- **Reward**: ABEL tokens minted
- **Mechanism**: Burn XFG to mint ABEL for liquidation capital

### 5. Governance Stake Deposits (GOVERNANCE_STAKE)

Governance stakes enable participation in system governance:

- **Purpose**: Enable governance participation
- **Collateral**: XFG tokens burned
- **Reward**: ABEL tokens minted
- **Mechanism**: Burn XFG to mint ABEL for governance

---

## Transaction Extra Integration

### TX_EXTRA Tag

The system uses a single transaction extra tag:

- **TX_EXTRA_FABLE_COMMITMENT (0xAB)**: For Fable commitments

### Transaction Extra Structure

#### TransactionExtraFableCommitment

```cpp
struct TransactionExtraFableCommitment {
    Crypto::Hash commitment;          // 🔒 SECURE: Only commitment hash on blockchain
    uint64_t amount;                  // XFG amount burned for ABEL tokens
    std::vector<uint8_t> metadata;    // Additional metadata
    
    bool serialize(ISerializer& serializer);
    bool isValid() const;
    std::string toString() const;
};
```

### Helper Functions

#### Creating Transaction Extra

```cpp
// Create Fable commitment extra
bool createTxExtraWithFableCommitment(
    const Crypto::Hash& commitment,
    uint64_t amount,
    const std::vector<uint8_t>& metadata,
    std::vector<uint8_t>& extra
);

// Add Fable commitment to extra
bool addFableCommitmentToExtra(
    std::vector<uint8_t>& tx_extra,
    const TransactionExtraFableCommitment& commitment
);
```

#### Parsing Transaction Extra

```cpp
// Parse Fable commitment from extra
bool getFableCommitmentFromExtra(
    const std::vector<uint8_t>& tx_extra,
    TransactionExtraFableCommitment& commitment
);
```

---

## Implementation Details

### Core Classes

#### 1. FableDepositManager

Main manager class for deposit operations:

```cpp
class FableDepositManager : public IFableDepositManager {
public:
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
};
```

#### 2. FableDepositValidator

Validator class for deposit validation:

```cpp
class FableDepositValidator {
public:
    // Validation methods
    bool validateDeposit(const FableDepositData& deposit) const;
    bool validateDepositIndex(const FableDepositIndexEntry& entry) const;
    bool validateConfig(const FableDepositConfig& config) const;
    
    // Deposit checks
    bool isDepositValid(const FableDepositData& deposit) const;
    bool isDepositMature(const FableDepositData& deposit) const;
    bool isDepositLiquidatable(const FableDepositData& deposit) const;
    bool isDepositActive(const FableDepositData& deposit) const;
};
```

### Data Structures

#### FableDepositData

Main deposit data structure:

```cpp
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
```

#### FableDepositIndexEntry

Deposit index entry for tracking cumulative values:

```cpp
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
```

---

## API Reference

### Deposit Management

#### Creating Deposits

```cpp
// Create a new Fable stablecoin deposit
FableDepositData deposit;
deposit.depositId = Crypto::Hash::random();
deposit.depositType = FableDepositType::FABLE_STABLE;
deposit.xfgAmount = 1000000000; // 1 XFG
deposit.abelAmount = 1000000000; // 1 ABEL (1:1 exchange rate)
deposit.depositTimestamp = getCurrentTimestamp();
deposit.maturityTimestamp = deposit.depositTimestamp + 86400; // 24 hours
deposit.depositorAddress = "depositor_address";
deposit.status = DepositStatus::ACTIVE;
deposit.metadata = {0x01, 0x02, 0x03};
deposit.signature = {0x04, 0x05, 0x06};

bool success = depositManager->createDeposit(deposit);
```

#### Updating Deposits

```cpp
// Update deposit status
auto deposit = depositManager->getDeposit(depositId);
if (deposit.has_value()) {
    deposit->status = DepositStatus::LOCKED;
    
    bool success = depositManager->updateDeposit(depositId, *deposit);
}
```

#### Liquidating Deposits

```cpp
// Liquidate undercollateralized deposit
std::vector<FableDepositData> liquidatableDeposits = depositManager->getLiquidatableDeposits();
for (const auto& deposit : liquidatableDeposits) {
    bool success = depositManager->liquidateDeposit(deposit.depositId, "Low collateral ratio");
    if (success) {
        logger(Logging::INFO) << "Liquidated deposit: " << Common::podToHex(deposit.depositId);
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

### Deposit Queries

#### Querying by Address

```cpp
// Get all deposits for an address
std::vector<FableDepositData> deposits = depositManager->getDepositsByAddress("depositor_address");
for (const auto& deposit : deposits) {
    logger(Logging::INFO) << "Deposit: " << deposit.toString();
}
```

#### Querying by Type

```cpp
// Get all Fable stablecoin deposits
std::vector<FableDepositData> fableDeposits = depositManager->getDepositsByType(FableDepositType::FABLE_STABLE);
logger(Logging::INFO) << "Found " << fableDeposits.size() << " Fable deposits";
```

#### Querying Active Deposits

```cpp
// Get all active deposits
std::vector<FableDepositData> activeDeposits = depositManager->getActiveDeposits();
logger(Logging::INFO) << "Found " << activeDeposits.size() << " active deposits";
```

### Deposit Index Management

#### Getting Deposit Index

```cpp
// Get deposit index for a specific block height
FableDepositIndexEntry index = depositManager->getDepositIndex(blockHeight);
logger(Logging::INFO) << "Deposit index: " << index.toString();
```

#### Getting Deposit Index Range

```cpp
// Get deposit index for a range of block heights
std::vector<FableDepositIndexEntry> indexRange = depositManager->getDepositIndexRange(startHeight, endHeight);
for (const auto& entry : indexRange) {
    logger(Logging::INFO) << "Index entry: " << entry.toString();
}
```

---

## Configuration

### FableDepositConfig

The system uses a comprehensive configuration structure:

```cpp
struct FableDepositConfig {
    uint64_t minDepositAmount;        // Minimum deposit amount (XFG)
    uint64_t maxDepositAmount;        // Maximum deposit amount (XFG)
    uint64_t minMaturityTime;         // Minimum maturity time (seconds)
    uint64_t maxMaturityTime;         // Maximum maturity time (seconds)
    double abelExchangeRate;          // XFG to ABEL exchange rate
    uint64_t liquidationThreshold;    // Liquidation threshold (basis points)
    bool enableAutomaticLiquidation;  // Enable automatic liquidation
    bool enableGovernanceVoting;      // Enable governance voting
};
```

### Default Configuration

```cpp
FableDepositConfig FableDepositConfig::getDefault() {
    FableDepositConfig config;
    config.minDepositAmount = 1000000000;        // 1 XFG
    config.maxDepositAmount = 1000000000000;     // 1000 XFG
    config.minMaturityTime = 86400;              // 24 hours
    config.maxMaturityTime = 31536000;           // 1 year
    config.abelExchangeRate = 1.0;               // 1:1 exchange rate
    config.liquidationThreshold = 12000;         // 120% (12000 basis points)
    config.enableAutomaticLiquidation = true;
    config.enableGovernanceVoting = true;
    return config;
}
```

### Setting Configuration

```cpp
// Set custom configuration
FableDepositConfig config = FableDepositConfig::getDefault();
config.minDepositAmount = 500000000; // 0.5 XFG
config.abelExchangeRate = 2.0; // 2:1 exchange rate
config.enableAutomaticLiquidation = false;

depositManager->setConfig(config);
```

---

## Testing Strategy

### Unit Tests

The system includes comprehensive unit tests covering:

1. **Deposit Data Validation**
   - Valid deposit creation
   - Invalid deposit rejection
   - Maturity checking
   - Status validation

2. **Deposit Management**
   - Deposit creation, update, liquidation, withdrawal
   - Query operations by address, type, status
   - Deposit index management

3. **Configuration**
   - Default configuration validation
   - Custom configuration setting
   - Configuration parameter validation

4. **Transaction Extra Integration**
   - Create transaction with Fable commitment extra
   - Parse commitment from transaction extra
   - Validate transaction extra data

### Test Examples

```cpp
TEST_F(FableDepositSystemTest, DepositDataValidation) {
    auto deposit = createTestDeposit();
    
    EXPECT_TRUE(deposit.isValid());
    EXPECT_FALSE(deposit.isMature());
    EXPECT_TRUE(deposit.isActive());
}

TEST_F(FableDepositSystemTest, DepositManagerCreateDeposit) {
    auto deposit = createTestDeposit();
    
    bool result = depositManager->createDeposit(deposit);
    EXPECT_TRUE(result);
    
    auto retrievedDeposit = depositManager->getDeposit(deposit.depositId);
    EXPECT_TRUE(retrievedDeposit.has_value());
    EXPECT_EQ(retrievedDeposit->depositId, deposit.depositId);
}

TEST_F(FableDepositSystemTest, DepositValidatorLiquidationCheck) {
    auto deposit = createTestDeposit();
    deposit.xfgAmount = 1000000000; // 1 XFG
    deposit.abelAmount = 2000000000; // 2 ABEL (undercollateralized)
    deposit.maturityTimestamp = deposit.depositTimestamp - 1; // Make it mature
    
    bool isLiquidatable = depositValidator->isDepositLiquidatable(deposit);
    EXPECT_TRUE(isLiquidatable);
}
```

---

## Security Considerations

### 1. Deposit Security

- **Amount Validation**: Enforce minimum and maximum deposit amounts
- **Exchange Rate Validation**: Ensure ABEL amount matches expected exchange rate
- **Signature Validation**: Validate all deposit signatures
- **Address Validation**: Prevent duplicate or invalid addresses

### 2. Transaction Security

- **Extra Field Validation**: Validate all transaction extra fields
- **Signature Verification**: Verify all transaction signatures
- **Amount Validation**: Validate all transaction amounts
- **Timestamp Validation**: Prevent timestamp manipulation

### 3. Network Security

- **Sybil Attack Prevention**: Prevent single entity from controlling multiple deposits
- **DDoS Protection**: Implement rate limiting for deposit operations
- **Audit Trail**: Maintain comprehensive logs of all operations

---

## Usage Examples

### Example 1: Creating a Fable Stablecoin Deposit

```cpp
#include "FableDepositSystem.h"

// Create deposit manager
auto logger = Logging::createLogger("FableExample");
FableDepositManager depositManager(*logger);

// Create Fable stablecoin deposit
FableDepositData deposit;
deposit.depositId = Crypto::Hash::random();
deposit.depositType = FableDepositType::FABLE_STABLE;
deposit.xfgAmount = 1000000000; // 1 XFG
deposit.abelAmount = 1000000000; // 1 ABEL (1:1 exchange rate)
deposit.depositTimestamp = getCurrentTimestamp();
deposit.maturityTimestamp = deposit.depositTimestamp + 86400; // 24 hours
deposit.depositorAddress = "depositor_address_123";
deposit.status = DepositStatus::ACTIVE;
deposit.metadata = {0x01, 0x02, 0x03};
deposit.signature = {0x04, 0x05, 0x06};

// Create deposit
bool success = depositManager.createDeposit(deposit);
if (success) {
    logger(Logging::INFO) << "Created Fable deposit: " << deposit.toString();
} else {
    logger(Logging::ERROR) << "Failed to create deposit";
}
```

### Example 2: Transaction Extra Integration

```cpp
#include "CryptoNoteCore/TransactionExtra.h"

// Create transaction extra with Fable commitment
std::vector<uint8_t> tx_extra;
Crypto::Hash commitment = Crypto::Hash::random();

bool success = createTxExtraWithFableCommitment(
    commitment,
    1000000000, // 1 XFG
    {0x01, 0x02, 0x03}, // metadata
    tx_extra
);

if (success) {
    logger(Logging::INFO) << "Created transaction extra with Fable commitment";
    
    // Parse commitment from transaction extra
    TransactionExtraFableCommitment fableCommitment;
    if (getFableCommitmentFromExtra(tx_extra, fableCommitment)) {
        logger(Logging::INFO) << "Parsed commitment: " << fableCommitment.toString();
    }
} else {
    logger(Logging::ERROR) << "Failed to create transaction extra";
}
```

### Example 3: Monitoring Deposits

```cpp
// Get all active deposits
std::vector<FableDepositData> activeDeposits = depositManager.getActiveDeposits();
logger(Logging::INFO) << "Found " << activeDeposits.size() << " active deposits";

// Check for liquidatable deposits
std::vector<FableDepositData> liquidatableDeposits = depositManager.getLiquidatableDeposits();
if (!liquidatableDeposits.empty()) {
    logger(Logging::WARNING) << "Found " << liquidatableDeposits.size() << " liquidatable deposits";
    
    for (const auto& deposit : liquidatableDeposits) {
        logger(Logging::WARNING) << "Liquidatable deposit: " << Common::podToHex(deposit.depositId);
    }
}

// Get deposit index
FableDepositIndexEntry index = depositManager.getDepositIndex(0);
logger(Logging::INFO) << "Deposit index: " << index.toString();
```

---

## Conclusion

The Fable Deposit System provides a simple, secure, and efficient foundation for burn deposit operations on the Fuego blockchain. Key benefits include:

### Advantages:
- **Simple Architecture**: Follows the proven HEAT burn pattern
- **Multiple Deposit Types**: Support for various deposit types and use cases
- **Comprehensive Validation**: Extensive validation and security checks
- **Transaction Integration**: Seamless integration with Fuego transaction system
- **Configurable Parameters**: Highly configurable for different use cases
- **Comprehensive Testing**: Extensive test coverage for reliability

### Security Features:
- **Cryptographic Validation**: All deposits must be cryptographically signed
- **Amount Validation**: Enforce minimum and maximum deposit amounts
- **Exchange Rate Validation**: Ensure ABEL amount matches expected exchange rate
- **Audit Trail**: Comprehensive logging of all operations

This system enables the creation of sophisticated DeFi operations while maintaining the security and privacy features that make Fuego unique in the cryptocurrency space. The burn-to-mint mechanism provides a solid foundation for stable mechanisms and other advanced financial operations.