# On-Chain Mixer Using Deposits

## Overview

The Fuego On-Chain Mixer leverages the existing deposit infrastructure to provide enhanced privacy through **privacy pools** where users can deposit funds, wait for a mixing period, and withdraw to new addresses with improved anonymity.

## Core Concept

### How It Works

1. **Deposit Phase**: Users deposit XFG into a mixer pool with a commitment
2. **Mixing Phase**: Funds are locked for a specified period (1-12 months)
3. **Withdrawal Phase**: Users can withdraw to new addresses with enhanced privacy

### Privacy Features

- **Ring Signatures**: Uses existing CryptoNote ring signature system (8+ mixins)
- **Zero-Knowledge Commitments**: Pedersen commitments for deposit amounts
- **Nullifier System**: Prevents double-spending and ensures one-time withdrawals
- **Time Delays**: Mandatory mixing periods with random withdrawal delays
- **Batch Processing**: Multiple withdrawals processed together for enhanced privacy

## Technical Implementation

### 1. Mixer Deposit Structure

```cpp
struct TransactionExtraMixerDeposit {
  Crypto::Hash depositHash;           // Unique deposit identifier
  uint64_t depositAmount;             // XFG amount (1-1000 XFG)
  Crypto::PublicKey recipientKey;     // Recipient's public key (encrypted)
  uint32_t mixingTerm;                // Mixing period in blocks (1-12 months)
  std::vector<uint8_t> nullifier;     // Nullifier to prevent double-spending
  std::vector<uint8_t> commitment;    // ZK commitment for privacy
  std::vector<uint8_t> metadata;      // Additional metadata
  std::vector<uint8_t> signature;     // Deposit signature
};
```

### 2. Mixer Pool Management

```cpp
struct MixerPool {
  uint64_t poolId;                    // Unique pool identifier
  uint64_t totalLiquidity;            // Total XFG in pool
  uint32_t minMixingTerm;             // Minimum mixing period (1 month)
  uint32_t maxMixingTerm;             // Maximum mixing period (12 months)
  uint64_t minDepositAmount;          // Minimum deposit (1 XFG)
  uint64_t maxDepositAmount;          // Maximum deposit (1000 XFG)
  std::vector<MixerDeposit> deposits; // Active deposits
  std::vector<MixerWithdrawal> withdrawals; // Pending withdrawals
};
```

### 3. Privacy Mechanisms

#### Ring Signatures
- **Minimum Mixins**: 8 (enhanced privacy)
- **Dynamic Ring Size**: Based on pool size and activity
- **Decoy Selection**: Random selection from pool participants

#### Zero-Knowledge Proofs
- **Amount Commitments**: Pedersen commitments hide deposit amounts
- **Range Proofs**: Prove amounts are within valid ranges
- **Nullifier Proofs**: Prove knowledge of nullifier without revealing it

#### Time-Based Privacy
- **Mixing Periods**: 1-12 months mandatory lock-up
- **Random Delays**: Additional random delays for withdrawals
- **Batch Processing**: Multiple withdrawals processed together

## Configuration

### Constants

```cpp
// Mixer Deposit Constants
const uint64_t MIXER_DEPOSIT_MIN_AMOUNT = 1000000;  // 1 XFG
const uint64_t MIXER_DEPOSIT_MAX_AMOUNT = 100000000000; // 1000 XFG
const uint32_t MIXER_DEPOSIT_MIN_TERM = 5480;       // 1 month
const uint32_t MIXER_DEPOSIT_MAX_TERM = 65760;      // 12 months
```

### Transaction Extra Tag

- **Identifier**: `0x13` (`TX_EXTRA_MIXER_DEPOSIT`)
- **Purpose**: Identifies mixer deposit transactions
- **Validation**: Amount, term, and commitment validation

## Usage Examples

### 1. Creating a Mixer Deposit

```cpp
// Generate deposit hash
Crypto::Hash depositHash = Crypto::cn_fast_hash(recipientAddress + std::to_string(amount));

// Create mixer deposit
TransactionExtraMixerDeposit deposit;
deposit.depositHash = depositHash;
deposit.depositAmount = amount;
deposit.recipientKey = recipientPublicKey;
deposit.mixingTerm = 16440; // 3 months
deposit.nullifier = generateNullifier(secretKey);
deposit.commitment = generateCommitment(amount, secretKey);

// Add to transaction extra
std::vector<uint8_t> extra;
createTxExtraWithMixerDeposit(depositHash, amount, recipientPublicKey, 
                             mixingTerm, nullifier, commitment, metadata, extra);
```

### 2. Withdrawing from Mixer

```cpp
// Generate withdrawal proof
WithdrawalProof proof = generateWithdrawalProof(deposit, secretKey);

// Create withdrawal transaction
Transaction withdrawalTx;
withdrawalTx.extra = createWithdrawalExtra(proof, newAddress);
```

## Security Considerations

### 1. Privacy Guarantees

- **Anonymity Set**: Size of the mixing pool
- **Time Delays**: Prevents timing analysis
- **Ring Signatures**: Cryptographic privacy
- **ZK Proofs**: Zero-knowledge of amounts and identities

### 2. Attack Resistance

- **Nullifier System**: Prevents double-spending
- **Commitment Scheme**: Hides deposit amounts
- **Time Locks**: Prevents immediate withdrawal
- **Batch Processing**: Reduces correlation attacks

### 3. Economic Security

- **Minimum Deposits**: Prevents dust attacks
- **Maximum Deposits**: Prevents whale dominance
- **Mixing Fees**: Incentivizes participation
- **Slashing Conditions**: Penalizes malicious behavior

## Integration Points

### 1. Wallet Integration

- **Deposit Creation**: Easy mixer deposit creation
- **Withdrawal Management**: Automated withdrawal processing
- **Privacy Monitoring**: Track anonymity set size
- **Fee Management**: Handle mixing fees

### 2. API Endpoints

```cpp
// Create mixer deposit
POST /createMixerDeposit
{
  "amount": 1000000000,  // 100 XFG
  "mixingTerm": 16440,   // 3 months
  "recipientAddress": "fire1...",
  "metadata": {}
}

// Withdraw from mixer
POST /withdrawFromMixer
{
  "depositHash": "0x...",
  "newAddress": "fire1...",
  "proof": "0x..."
}
```

### 3. Governance Integration

- **Pool Parameters**: Adjustable via governance
- **Fee Structure**: Community-controlled fees
- **Upgrade Path**: Protocol improvements
- **Emergency Controls**: Circuit breakers

## Future Enhancements

### 1. Advanced Privacy

- **Tornado Cash-style**: Fixed denomination pools
- **Semaphore**: Anonymous voting and signaling
- **Bulletproofs**: More efficient range proofs
- **Multi-asset**: Support for different tokens

### 2. Cross-Chain Integration

- **Bridge Mixing**: Cross-chain privacy
- **Multi-chain Pools**: Unified privacy across chains
- **Atomic Swaps**: Private cross-chain exchanges

### 3. DeFi Integration

- **Liquidity Mining**: Earn rewards while mixing
- **Yield Farming**: Mix with yield generation
- **Lending**: Use mixed funds as collateral

## Implementation Status

### Phase 1: Core Infrastructure ✅
- [x] Mixer deposit structure (`0x13`)
- [x] Basic validation logic
- [x] Transaction extra parsing
- [x] Configuration constants

### Phase 2: Privacy Features (In Progress)
- [ ] ZK proof integration
- [ ] Enhanced ring signatures
- [ ] Nullifier system
- [ ] Batch processing

### Phase 3: Advanced Features (Planned)
- [ ] Multiple pool sizes
- [ ] Dynamic fees
- [ ] Governance integration
- [ ] Cross-chain support

## Conclusion

The Fuego On-Chain Mixer provides a powerful privacy solution that leverages the existing deposit infrastructure while adding sophisticated privacy features. By combining time delays, ring signatures, and zero-knowledge proofs, it offers strong privacy guarantees while maintaining the security and efficiency of the Fuego blockchain.

The modular design allows for incremental implementation and future enhancements, making it a robust foundation for privacy-preserving transactions on the Fuego network.