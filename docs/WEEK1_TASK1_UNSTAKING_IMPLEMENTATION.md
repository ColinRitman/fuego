# Week 1, Task 1.1: User-Initiated Unstaking Implementation

**Status:** In Progress
**Timeline:** 2-3 hours implementation + 1 hour testing
**Files Modified:** include/EldernodeIndexTypes.h
**Files Created/Modified:** src/EldernodeIndexManager/ElderfierDepositData.cpp (NEW)

---

## Part 1: Header Changes (COMPLETED ✓)

### Changes Made to `include/EldernodeIndexTypes.h`:

**1. Updated Field Names and Added Fields (lines 196-205):**

BEFORE:
```cpp
// Security window fields
uint64_t lastSignatureTimestamp; // Last signature timestamp
uint64_t securityWindowEnd;      // When security window ends
uint64_t securityWindowDuration; // Duration of security window
bool isInSecurityWindow;         // Currently in security window
bool unlockRequested;            // Elderfier requested to unlock
uint64_t unlockRequestTimestamp; // When unlock was requested
```

AFTER:
```cpp
// Security window fields
uint64_t lastSignatureTimestamp; // Last signature timestamp
uint64_t securityWindowEnd;      // When security window ends
uint64_t securityWindowDuration; // Duration of security window
bool isInSecurityWindow;         // Currently in security window

// User-Initiated Unstaking Model (Dynamigo Phase)
// DESIGN: Stakes held indefinitely until user explicitly requests unstaking
// Then 8-day (19200 blocks) countdown begins before claiming is allowed
bool unstakingRequested;         // true = user initiated unstaking, false = still staking
uint64_t unstakingRequestBlock;  // Block height when user called initiate-unstake (0 if not requested)
uint64_t unstakeClaimableBlock;  // Block height = unstakingRequestBlock + 19200 when claim becomes possible
```

**2. Updated Method Signatures (lines 210-215):**

BEFORE:
```cpp
void requestUnlock(uint64_t timestamp);  // Request to unlock deposit
```

AFTER:
```cpp
void initiateUnstake(uint64_t blockHeight);   // User requests unstaking (starts 8-day countdown)
bool canClaimUnstakedFunds(uint64_t currentBlock) const; // Check if 8-day window has passed
```

---

## Part 2: Implementation in ElderfierDepositData.cpp (TO DO)

**File to Create:** `src/EldernodeIndexManager/ElderfierDepositData.cpp`

**Purpose:** Implement the behavior of unstaking model

### Constructor & Initialization:

```cpp
// In ElderfierDepositData.cpp (or as inline in header)

// Constructor - initialize with default values
ElderfierDepositData::ElderfierDepositData()
    : depositAmount(0),
      depositTimestamp(0),
      lastSeenTimestamp(0),
      totalUptimeSeconds(0),
      selectionMultiplier(0),
      isActive(false),
      isSlashable(true),
      isUnlocked(false),
      isSpent(false),
      lastSignatureTimestamp(0),
      securityWindowEnd(0),
      securityWindowDuration(28800),  // 8 hours default
      isInSecurityWindow(false),
      unstakingRequested(false),
      unstakingRequestBlock(0),
      unstakeClaimableBlock(0) {
}

// Copy constructor / assignment operator (compiler generated is fine)
```

### Method Implementations:

```cpp
// initiateUnstake: Called when user requests unstaking
void ElderfierDepositData::initiateUnstake(uint64_t blockHeight) {
    if (unstakingRequested) {
        // Already requested - should not call twice
        return;  // Or throw exception for safety
    }

    if (isSpent) {
        // Cannot unstake already spent deposit
        return;
    }

    // Mark as unstaking with 8-day (19200 blocks) countdown
    unstakingRequested = true;
    unstakingRequestBlock = blockHeight;
    unstakeClaimableBlock = blockHeight + 19200;  // 7 days later

    // Status changes to "unstaking"
    isActive = false;  // No longer actively signing
}

// canClaimUnstakedFunds: Check if enough time has passed
bool ElderfierDepositData::canClaimUnstakedFunds(uint64_t currentBlock) const {
    if (!unstakingRequested) {
        return false;  // Never requested unstaking
    }

    if (isSpent) {
        return false;  // Already claimed/spent
    }

    // Can claim once we reach unstakeClaimableBlock
    return currentBlock >= unstakeClaimableBlock;
}

// isValid: Existing validation - add unstaking checks
bool ElderfierDepositData::isValid() const {
    // Basic checks
    if (depositHash == Crypto::Hash()) {
        return false;  // No deposit hash
    }

    if (depositAmount != 800000000000) {
        return false;  // Must be exactly 800 XFG
    }

    if (elderfierAddress.empty()) {
        return false;  // Must have address
    }

    // Unstaking state checks
    if (unstakingRequested) {
        // If unstaking, must have valid block heights
        if (unstakingRequestBlock == 0) {
            return false;  // Request block not set
        }
        if (unstakeClaimableBlock != unstakingRequestBlock + 19200) {
            return false;  // Claimable block not correct
        }
    }

    return true;
}

// toString: For logging/debugging
std::string ElderfierDepositData::toString() const {
    std::stringstream ss;
    ss << "ElderfierDeposit {"
       << "address=" << elderfierAddress
       << ", amount=" << depositAmount
       << ", deposited_block=" << depositTimestamp
       << ", status=" << (unstakingRequested ? "UNSTAKING" : "STAKING");

    if (unstakingRequested) {
        ss << ", request_block=" << unstakingRequestBlock
           << ", claimable_block=" << unstakeClaimableBlock;
    }

    ss << "}";
    return ss.str();
}
```

### Additional Helper Methods (Optional, for convenience):

```cpp
// Get countdown information for RPC/CLI
uint64_t ElderfierDepositData::getBlocksUntilClaimable(uint64_t currentBlock) const {
    if (!unstakingRequested || currentBlock >= unstakeClaimableBlock) {
        return 0;  // Either not unstaking or already claimable
    }
    return unstakeClaimableBlock - currentBlock;
}

// Convert blocks to days/hours/minutes
std::string ElderfierDepositData::getCountdownDisplay(uint64_t currentBlock) const {
    uint64_t blocksRemaining = getBlocksUntilClaimable(currentBlock);
    if (blocksRemaining == 0) {
        return "Ready to claim!";
    }

    // Fuego mainnet: ~1 minute per block (60 blocks per hour)
    uint64_t minutesRemaining = blocksRemaining;  // 1 block ≈ 1 minute
    uint64_t hoursRemaining = minutesRemaining / 60;
    uint64_t daysRemaining = hoursRemaining / 24;

    std::stringstream ss;
    if (daysRemaining > 0) {
        ss << daysRemaining << "d ";
        hoursRemaining %= 24;
    }
    if (hoursRemaining > 0) {
        ss << hoursRemaining << "h ";
    }
    ss << (minutesRemaining % 60) << "m";

    return ss.str();
}

// Claim the unstaked funds
bool ElderfierDepositData::claimUnstakedFunds(uint64_t currentBlock) {
    if (!canClaimUnstakedFunds(currentBlock)) {
        return false;  // Not yet eligible
    }

    // Mark as spent (funds transferred)
    isSpent = true;

    // Could emit event here for logging
    // LOG_INFO("Elderfier " << elderfierAddress << " claimed " << depositAmount << " XFG");

    return true;
}
```

---

## Part 3: Integration Points

### Where These Methods Are Called:

**1. CLI Command: `elderfier-stake create`**
- Calls: Constructor to create new ElderfierDepositData
- Location: xfg-stark-cli or Blockchain.cpp

**2. CLI Command: `elderfier-stake initiate-unstake`**
- Calls: `initiateUnstake(blockHeight)`
- Creates special transaction marking the request
- RPC call to Fuego node

**3. RPC: `get_elderfier_stake_info`**
- Reads: `unstakingRequested`, `unstakingRequestBlock`, `unstakeClaimableBlock`
- Calls: `getCountdownDisplay(currentBlock)` for user display
- Returns countdown: "7d 23h 45m"

**4. CLI Command: `elderfier-stake claim`**
- Checks: `canClaimUnstakedFunds(currentBlock)` before allowing claim
- Calls: `claimUnstakedFunds(currentBlock)` to mark as spent
- Returns: "Claim successful! 800 XFG returned"

**5. Block Processing: Blockchain.cpp**
- Detects 0xEF deposits when parsing transactions
- Creates/updates ElderfierDepositData
- Tracks deposit in EldernodeIndexManager
- Monitors for unstaking requests and claim transactions

---

## Part 4: Data Flow Diagrams

### Deposit Lifecycle:

```
User creates 0xEF deposit
    ↓
Blockchain.cpp processes transaction
    ↓
ElderfierDepositData created:
  - unstakingRequested = false
  - unstakingRequestBlock = 0
  - unstakeClaimableBlock = 0
  ↓ (displayed as "STAKING" status)
  ↓ (can remain indefinitely)
  ↓
User calls: initiate-unstake
    ↓
ElderfierDepositData.initiateUnstake(blockHeight)
  - unstakingRequested = true
  - unstakingRequestBlock = current block height
  - unstakeClaimableBlock = currentBlock + 19200
  ↓ (displayed as "UNSTAKING" with countdown)
  ↓ (waits 8 days / 19200 blocks)
  ↓
Block height >= unstakeClaimableBlock
    ↓
User calls: claim
    ↓
ElderfierDepositData.canClaimUnstakedFunds(currentBlock) returns true
    ↓
ElderfierDepositData.claimUnstakedFunds(currentBlock)
  - isSpent = true
    ↓
Transaction submitted, 800 XFG returned to user address
    ↓
Deposit marked as CLAIMED (read-only historical record)
```

### Field State Transitions:

```
State 1: STAKING (indefinite)
  unstakingRequested = false
  unstakingRequestBlock = 0
  unstakeClaimableBlock = 0
  isActive = true

  ↓ [User calls initiate-unstake]

State 2: UNSTAKING (8-day countdown)
  unstakingRequested = true
  unstakingRequestBlock = 1050000
  unstakeClaimableBlock = 1069200
  isActive = false

  ↓ [8 days pass, block >= 1069200]

State 3: CLAIMABLE (ready to withdraw)
  unstakingRequested = true
  unstakingRequestBlock = 1050000
  unstakeClaimableBlock = 1069200
  canClaimUnstakedFunds() = true
  isActive = false

  ↓ [User calls claim]

State 4: CLAIMED (historical record)
  isSpent = true
  [Deposit is read-only, no further actions possible]
```

---

## Part 5: Validation Rules

### Validation in `isValid()`:

✓ Must have valid depositHash (not empty)
✓ Amount must be exactly 800 XFG
✓ Must have elderfierAddress
✓ If unstakingRequested:
  - unstakingRequestBlock must be > 0
  - unstakeClaimableBlock must equal unstakingRequestBlock + 19200
  - Cannot claim before unstakeClaimableBlock

### Validation in `initiateUnstake()`:

✓ Cannot call if already unstakingRequested = true (prevent double-requests)
✓ Cannot call if isSpent = true (already claimed)
✓ blockHeight must be current block height (passed from Blockchain.cpp)

### Validation in `canClaimUnstakedFunds()`:

✓ Return false if unstakingRequested = false (never requested)
✓ Return false if isSpent = true (already claimed)
✓ Return true only if currentBlock >= unstakeClaimableBlock

---

## Part 6: Testing Strategy

### Unit Tests to Write:

1. **Test Constructor:**
   - Verify all fields initialized to defaults
   - unstakingRequested = false
   - unstakingRequestBlock = 0
   - unstakeClaimableBlock = 0

2. **Test initiateUnstake():**
   - Call with valid blockHeight
   - Verify unstakingRequested = true
   - Verify unstakingRequestBlock == blockHeight
   - Verify unstakeClaimableBlock == blockHeight + 19200
   - Try calling twice - second should be no-op

3. **Test canClaimUnstakedFunds():**
   - Before initiateUnstake: return false
   - After initiateUnstake, before countdown: return false
   - After countdown reaches threshold: return true
   - After claiming: return false

4. **Test getCountdownDisplay():**
   - 19200 blocks remaining → "7d 22h 0m"
   - 3600 blocks remaining → "0d 1h 0m"
   - 100 blocks remaining → "0d 0h 100m"
   - 0 blocks remaining → "Ready to claim!"

5. **Test isValid():**
   - Valid deposit → true
   - Wrong amount → false
   - Empty address → false
   - Bad unstaking block values → false

### Integration Tests:

1. Create 0xEF deposit on testnet
2. Query status via RPC - should be "STAKING"
3. Call initiate-unstake
4. Query status - should be "UNSTAKING" with countdown
5. Wait for countdown to expire
6. Claim funds
7. Verify status is "CLAIMED"

---

## Part 7: Constants & Configuration

**Hardcoded in ElderfierDepositData:**

```cpp
static const uint64_t UNSTAKING_COUNTDOWN_BLOCKS = 19200;  // 7 days (~1 min per block)
static const uint64_t ELDERFIER_DEPOSIT_AMOUNT = 800000000000;  // 800 XFG
static const uint32_t DEFAULT_SECURITY_WINDOW = 28800;  // 8 hours
```

**Derived from constants:**
- Blocks per day: 20160 (on Fuego mainnet)
- Days to claim: 7
- Hours to claim: 168
- Minutes to claim: 10080

---

## Part 8: Code Quality Checklist

- [ ] All method signatures match header file
- [ ] Const-correctness maintained (const methods for reads)
- [ ] Thread-safe access (caller handles locking)
- [ ] No memory leaks (RAII, automatic cleanup)
- [ ] Proper error handling (return false on error)
- [ ] Clear comments explaining logic
- [ ] Edge cases handled:
  - [ ] Zero block heights handled
  - [ ] Integer overflow prevented (validate block heights)
  - [ ] State consistency maintained

---

## Part 9: Implementation Checklist

### Header Changes (COMPLETED):
- [x] Update field names (unstakingRequested, unstakingRequestBlock, unstakeClaimableBlock)
- [x] Update method signatures (initiateUnstake, canClaimUnstakedFunds)
- [x] Add comments explaining new model

### Implementation File (TO DO):
- [ ] Create ElderfierDepositData.cpp
- [ ] Implement initiateUnstake()
- [ ] Implement canClaimUnstakedFunds()
- [ ] Implement claimUnstakedFunds()
- [ ] Implement getCountdownDisplay()
- [ ] Implement getBlocksUntilClaimable()
- [ ] Update isValid() with unstaking checks
- [ ] Update toString() with unstaking state

### Testing (TO DO):
- [ ] Unit tests for all methods
- [ ] Integration tests with Blockchain.cpp
- [ ] RPC endpoint tests
- [ ] CLI command tests

### Integration (TO DO):
- [ ] Update Blockchain.cpp to call initiateUnstake()
- [ ] Update RPC endpoints to use canClaimUnstakedFunds()
- [ ] Update CLI to display countdown

---

## Summary

**Task 1.1 Header Changes: COMPLETE ✓**
- Fields added: unstakingRequested, unstakingRequestBlock, unstakeClaimableBlock
- Methods updated: initiateUnstake(), canClaimUnstakedFunds()
- Comments explaining new user-initiated model

**Task 1.1 Implementation: READY TO BUILD**
- ElderfierDepositData.cpp to be created
- ~200 lines of C++ code
- 3-4 hours implementation + 1 hour testing

**Next Steps:**
1. Create ElderfierDepositData.cpp with method implementations
2. Add unit tests
3. Update Blockchain.cpp integration
4. Test with CLI commands
5. Move to Task 1.2 (fee tracking)

---

**Effort:** 2-3 hours implementation + 1 hour testing = 3-4 hours total
**Owner:** [Primary implementation]
**Status:** Ready for implementation
