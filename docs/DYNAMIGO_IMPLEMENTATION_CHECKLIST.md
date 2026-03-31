# Dynamigo Phase Implementation Checklist

**Status:** Ready to begin Week 1
**Timeline:** 4 weeks to Dynamigo launch
**Objective:** Enable elderfier registration via 0xEF deposits before MVP completion

---

## Week 1: Foundational Infrastructure (4-5 days)

### Task 1.1: Add User-Initiated Unstaking Fields to ElderfierDepositData
**File:** `include/EldernodeIndexTypes.h` (lines 181-217)
**Current State:** `unlockRequested` and `unlockRequestTimestamp` exist (lines 201-202)
**Missing Fields:** Need to clarify the 8-day countdown after request

**Changes Required:**
```cpp
struct ElderfierDepositData {
  // ... existing fields ...

  // CORRECTED: User-Initiated Unstaking Model
  bool unstakingRequested;        // NEW: User initiated unstaking request
  uint64_t unstakingRequestBlock; // NEW: Block height when user initiated unstake
  uint64_t unstakeClaimableBlock; // NEW: Block height = unstakingRequestBlock + 19200

  // Note: These replace the old "unlockRequested" auto-countdown model
  // OLD MODEL (removed): Unlock available automatically after security window
  // NEW MODEL (in place): Indefinite staking until user initiates unstake request
};
```

**Validation:**
- [ ] Unstaking can only be initiated if not already in progress
- [ ] Countdown only starts after user explicitly calls initiate-unstake
- [ ] Claim only allowed after countdown expires
- [ ] No automatic unlocks (user must initiate)

**Effort:** 2-3 hours
**Owner:** [Primary implementation]
**PR:** Will be in Week 2

---

### Task 1.2: Implement Epoch-Based Fee Tracking in CommitmentIndex
**File:** `src/CryptoNoteCore/CommitmentIndex.h/.cpp`
**Current State:** Tracks commitments only, no fee tracking
**Required Additions:** Fee distribution system

**New Data Structures to Add:**

```cpp
// In CommitmentIndex.h (after CommitmentEntry struct, line 47)

struct ElderfierEpochRewards {
    uint64_t epochNumber;                      // Which epoch (0, 1, 2, ...)
    std::vector<std::string> activeElderfiers; // Addresses of active 3 elderfiers
    uint64_t totalFeesCollected;               // XFG collected during epoch
    std::map<std::string, uint64_t> distribution; // Each elder's share
    uint64_t epochStartBlock;                  // First block of epoch
    uint64_t epochEndBlock;                    // Last block of epoch
};

class CommitmentIndex {
  // ... existing code ...

  // NEW: Fee tracking methods
  private:
    std::vector<ElderfierEpochRewards> m_epochHistory;
    uint64_t m_currentEpochStartBlock;
    uint64_t m_currentEpochTotalFees;

    // Epoch configuration constants
    static const uint64_t EPOCH_DURATION_BLOCKS = 43200;  // 7 days
    static const uint8_t TOTAL_ELDERFIERS = 5;
    static const uint8_t ACTIVE_ELDERFIERS_PER_EPOCH = 3;

    // Deterministic rotation schedule (hard-coded for MVP)
    std::array<std::vector<uint8_t>, 5> ROTATION_SCHEDULE = {
        {1, 2, 3},    // Epoch 1: Elderfiers 1, 2, 3
        {2, 3, 4},    // Epoch 2: Elderfiers 2, 3, 4
        {3, 4, 5},    // Epoch 3: Elderfiers 3, 4, 5
        {4, 5, 1},    // Epoch 4: Elderfiers 4, 5, 1
        {5, 1, 2},    // Epoch 5: Elderfiers 5, 1, 2
    };

  public:
    // Fee extraction (called during HEAT/COLD processing in Blockchain.cpp)
    void addElderfierFee(uint64_t feeAmount);

    // Epoch finalization (called when block height % EPOCH_DURATION_BLOCKS == 0)
    void finalizeEpoch();

    // Query methods for RPC
    ElderfierEpochRewards getEpochRewards(uint64_t epochNumber) const;
    uint64_t getElderfierEarnings(const std::string& elderfierAddress, uint64_t epochNumber) const;
    uint64_t getCurrentEpoch() const;
    std::vector<std::string> getCurrentActiveElderfiers() const;
};
```

**Implementation Details:**

1. **addElderfierFee(uint64_t feeAmount):**
   - Add to `m_currentEpochTotalFees`
   - Called from Blockchain.cpp during commitment processing

2. **finalizeEpoch():**
   ```cpp
   void CommitmentIndex::finalizeEpoch() {
       uint64_t epoch = getCurrentEpoch();
       auto activeElderfiers = getCurrentActiveElderfiers();
       uint64_t perElderfier = m_currentEpochTotalFees / activeElderfiers.size();

       ElderfierEpochRewards rewards;
       rewards.epochNumber = epoch;
       rewards.activeElderfiers = activeElderfiers;
       rewards.totalFeesCollected = m_currentEpochTotalFees;
       rewards.epochStartBlock = m_currentEpochStartBlock;
       rewards.epochEndBlock = getCurrentBlockHeight();

       for (const auto& elder : activeElderfiers) {
           rewards.distribution[elder] = perElderfier;
       }

       m_epochHistory.push_back(rewards);
       m_currentEpochTotalFees = 0;
       m_currentEpochStartBlock = getCurrentBlockHeight() + 1;
   }
   ```

3. **getCurrentEpoch():**
   ```cpp
   uint64_t CommitmentIndex::getCurrentEpoch() const {
       return getCurrentBlockHeight() / EPOCH_DURATION_BLOCKS;
   }
   ```

**Validation:**
- [ ] Fees accumulate correctly during epoch
- [ ] Finalization splits 3 ways only
- [ ] Epoch rotation follows ROTATION_SCHEDULE
- [ ] All 5 elderfiers get equal earnings over 5 epochs
- [ ] No rounding errors (use integer division)

**Effort:** 4-5 hours
**Owner:** [Primary implementation]
**Dependencies:** Blockchain.cpp integration (Week 2)

---

### Task 1.3: Add 3 RPC Endpoints for Elderfier Queries
**File:** `src/Rpc/CoreRpcServerCommandsDefinitions.h`
**Current State:** HEAT/COLD RPC endpoints exist, no elderfier-specific endpoints

**Required RPC Endpoints:**

#### 1. `get_elderfier_candidates`
Returns list of registered elderfiers who have completed 0xEF deposits.

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": "0",
  "method": "get_elderfier_candidates",
  "params": {}
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": "0",
  "result": {
    "candidates": [
      {
        "address": "XFG...",
        "deposit_hash": "0x...",
        "deposit_amount": 800000000000,
        "deposited_block": 1000000,
        "status": "active",
        "total_deposits": 1600000000000,
        "security_window_blocks": 28800,
        "security_window_end_block": 1028800
      }
    ],
    "total_candidates": 12,
    "total_staked_xfg": 19200000000000
  }
}
```

**Implementation:**
```cpp
// In RpcServer.cpp
bool RpcServer::on_get_elderfier_candidates(
    const COMMAND_RPC_GET_ELDERFIER_CANDIDATES::request& req,
    COMMAND_RPC_GET_ELDERFIER_CANDIDATES::response& res) {

  // Query EldernodeIndexManager for all deposits
  auto candidates = m_eldernodeIndexManager->getAllElderfierDeposits();

  for (const auto& deposit : candidates) {
    res.candidates.push_back({
      .address = deposit.elderfierAddress,
      .deposit_hash = Common::podToHex(deposit.depositHash),
      .deposit_amount = deposit.depositAmount,
      .deposited_block = deposit.depositTimestamp,  // or use block height
      .status = deposit.isActive ? "active" : "inactive",
      .total_deposits = deposit.depositAmount,
      .security_window_blocks = deposit.securityWindowDuration,
      .security_window_end_block = deposit.securityWindowEnd
    });
  }

  res.total_candidates = res.candidates.size();
  // sum all amounts
  res.total_staked_xfg = ..;

  return true;
}
```

**Validation:**
- [ ] Returns all registered elderfiers
- [ ] Shows correct security window countdown
- [ ] Status accurate (active/inactive)
- [ ] Performance acceptable (< 1 second for 100 candidates)

---

#### 2. `get_elderfier_stake_info`
Returns per-account elderfier stake information.

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": "0",
  "method": "get_elderfier_stake_info",
  "params": {
    "address": "XFG7..."
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": "0",
  "result": {
    "address": "XFG7...",
    "is_elderfier": true,
    "deposits": [
      {
        "index": 0,
        "amount": 800000000000,
        "created_block": 1000000,
        "status": "staking",
        "unstaking_requested": false,
        "unlock_block": null,
        "days_until_unlock": null
      },
      {
        "index": 1,
        "amount": 800000000000,
        "created_block": 1000001,
        "status": "unstaking",
        "unstaking_requested": true,
        "unstaking_requested_block": 1050000,
        "unlock_block": 1069200,
        "days_until_unlock": 1.30
      }
    ],
    "total_staked": 1600000000000,
    "total_unstaking": 800000000000,
    "last_signature_block": 1050100
  }
}
```

**Implementation:**
```cpp
bool RpcServer::on_get_elderfier_stake_info(
    const COMMAND_RPC_GET_ELDERFIER_STAKE_INFO::request& req,
    COMMAND_RPC_GET_ELDERFIER_STAKE_INFO::response& res) {

  auto deposits = m_eldernodeIndexManager->getElderfierDeposits(req.address);

  res.address = req.address;
  res.is_elderfier = deposits.size() > 0;

  for (size_t i = 0; i < deposits.size(); i++) {
    const auto& deposit = deposits[i];
    auto& depositInfo = res.deposits.emplace_back();

    depositInfo.index = i;
    depositInfo.amount = deposit.depositAmount;
    depositInfo.created_block = deposit.depositTimestamp;
    depositInfo.status = deposit.unstakingRequested ? "unstaking" : "staking";
    depositInfo.unstaking_requested = deposit.unstakingRequested;

    if (deposit.unstakingRequested) {
      depositInfo.unstaking_requested_block = deposit.unstakingRequestBlock;
      depositInfo.unlock_block = deposit.unstakeClaimableBlock;
      double blocksRemaining = static_cast<double>(depositInfo.unlock_block - getCurrentBlockHeight());
      depositInfo.days_until_unlock = blocksRemaining / (24.0 * 60.0);  // ~1 min per block
    }
  }

  res.total_staked = ..;  // sum non-unstaking deposits
  res.total_unstaking = ..; // sum unstaking deposits
  res.last_signature_block = deposit.lastSignatureTimestamp;

  return true;
}
```

---

#### 3. `get_elderfier_earnings`
Returns earned fees for elderfier in specific epoch.

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": "0",
  "method": "get_elderfier_earnings",
  "params": {
    "elderfier_address": "XFG7...",
    "epoch": 1
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": "0",
  "result": {
    "elderfier_address": "XFG7...",
    "epoch": 1,
    "earned_xfg": 233500000000,
    "active": true,
    "epoch_start_block": 43200,
    "epoch_end_block": 86400,
    "epoch_duration_days": 7
  }
}
```

**Implementation:**
```cpp
bool RpcServer::on_get_elderfier_earnings(
    const COMMAND_RPC_GET_ELDERFIER_EARNINGS::request& req,
    COMMAND_RPC_GET_ELDERFIER_EARNINGS::response& res) {

  uint64_t earnings = m_commitmentIndex.getElderfierEarnings(
      req.elderfier_address,
      req.epoch
  );

  auto epochRewards = m_commitmentIndex.getEpochRewards(req.epoch);

  res.elderfier_address = req.elderfier_address;
  res.epoch = req.epoch;
  res.earned_xfg = earnings;
  res.active = (earnings > 0);
  res.epoch_start_block = epochRewards.epochStartBlock;
  res.epoch_end_block = epochRewards.epochEndBlock;
  res.epoch_duration_days = (epochRewards.epochEndBlock - epochRewards.epochStartBlock) / 20160;  // blocks per day

  return true;
}
```

**Validation:**
- [ ] Returns correct earnings for active elderfiers
- [ ] Returns 0 for inactive elderfiers
- [ ] Blocks per day calculation correct
- [ ] Works for past epochs

**Effort:** 3-4 hours
**Owner:** [Primary implementation]
**Testing:** Unit tests for each RPC

---

## Week 1 Summary & Deliverables

### What Gets Built:
1. ✅ User-initiated unstaking model (field updates)
2. ✅ Epoch-based fee tracking infrastructure
3. ✅ Three RPC endpoints for elderfier queries
4. ✅ Test coverage (unit tests, RPC validation)

### What Does NOT Get Built Yet (Week 2+):
- ❌ 0xEF deposit validation (Week 2)
- ❌ CLI commands (Week 2)
- ❌ Smart contract integration (MVP phase)
- ❌ Relay daemon (Week 4)
- ❌ On-chain aliases (Week 3)

### Code Quality Checklist (Week 1):
- [ ] All field additions use correct types (uint64_t, std::string, etc.)
- [ ] Thread-safe (mutex protection in CommitmentIndex)
- [ ] No memory leaks (proper cleanup in destructors)
- [ ] Error handling complete (return codes, logging)
- [ ] Const-correctness maintained
- [ ] Code follows project style guide
- [ ] All methods documented with comments

### Testing Checklist (Week 1):
- [ ] Unit tests for epoch calculation
- [ ] Unit tests for fee distribution logic
- [ ] RPC endpoint tests (mock elderfier data)
- [ ] Performance tests (100+ elderfiers)
- [ ] Rollback/reorg handling tests

---

## Week 2: Transaction Processing (4-5 days)

### Task 2.1: Implement 0xEF Stake Deposit Validation
**File:** `src/CryptoNoteCore/TransactionValidator.cpp` (new section)
**Current State:** Validation exists for other deposit types
**Required Additions:** 0xEF-specific validation

**Validation Rules:**
```cpp
bool TransactionValidator::validateElderfierDeposit(
    const TransactionExtraElderfierDeposit& deposit) {

  // 1. Amount must be exactly 800 XFG (or one of allowed amounts)
  if (deposit.depositAmount != 800000000000) {  // 800 XFG in atomic units
    return false;  // Error: "Elderfier deposit must be exactly 800 XFG"
  }

  // 2. Security window must be exactly 28800 seconds (8 hours)
  if (deposit.securityWindow != 28800) {
    return false;  // Error: "Security window must be 8 hours"
  }

  // 3. Elderfier address must be valid
  if (deposit.elderfierAddress.empty()) {
    return false;  // Error: "Elderfier address required"
  }

  // 4. No integer overflow in metadata
  if (deposit.metadata.size() > 1000) {
    return false;  // Error: "Metadata too large"
  }

  // 5. Signature validation (if signature is included)
  if (deposit.signature.size() > 0) {
    // Validate signature format
  }

  // 6. Deposit hash must be unique (not already registered)
  if (m_eldernodeIndexManager->hasDeposit(deposit.depositHash)) {
    return false;  // Error: "Deposit already registered"
  }

  return true;
}
```

**Effort:** 3-4 hours
**Testing:** Valid/invalid test cases, edge cases

---

### Task 2.2: Integrate Fee Extraction into Blockchain.cpp
**File:** `src/CryptoNoteCore/Blockchain.cpp` (modify lines 2455-2503)
**Current State:** HEAT/COLD commitments tracked, no fee extraction

**Changes Required:**
```cpp
// Around line 2470 (in HEAT processing):
if (field.type() == typeid(TransactionExtraHeatCommitment)) {
    const auto& heatCommit = boost::get<TransactionExtraHeatCommitment>(field);
    permanentBurns += heatCommit.amount;

    // ... existing commitment indexing ...

    // NEW: Extract fee for elderfiers
    uint64_t feeAmount = (heatCommit.amount * parameters::HEAT_ELDERFIER_FEE_PERCENTAGE) / 10000;
    m_commitmentIndex.addElderfierFee(feeAmount);
}

// Around line 2489 (in COLD processing):
else if (field.type() == typeid(TransactionExtraColdCommitment)) {
    const auto& coldCommit = boost::get<TransactionExtraColdCommitment>(field);

    // ... existing commitment indexing ...

    // NEW: Extract fee for elderfiers
    uint64_t feeAmount = (coldCommit.amount * parameters::COLD_ELDERFIER_FEE_PERCENTAGE) / 10000;
    m_commitmentIndex.addElderfierFee(feeAmount);
}
```

**Fee Configuration (CryptoNoteConfig.h):**
```cpp
namespace parameters {
    static const uint64_t HEAT_ELDERFIER_FEE_PERCENTAGE = 50;    // 0.5%
    static const uint64_t COLD_ELDERFIER_FEE_PERCENTAGE = 100;   // 1.0%
}
```

**Effort:** 2-3 hours
**Testing:** Verify fees accumulate correctly per block

---

### Task 2.3: Integrate with EldernodeIndexManager
**File:** `include/EldernodeIndexManager.h` (lines 34-120)
**Current State:** ~80% of methods exist
**Required Additions:** Deposit tracking, status updates

**Missing Methods to Implement:**
```cpp
class IEldernodeIndexManager {
    // Deposit management
    virtual bool addElderfierDeposit(const ElderfierDepositData& deposit) = 0;
    virtual ElderfierDepositData getElderfierDeposit(const Crypto::Hash& depositHash) = 0;
    virtual std::vector<ElderfierDepositData> getAllElderfierDeposits() const = 0;
    virtual std::vector<ElderfierDepositData> getElderfierDeposits(const std::string& address) const = 0;

    // Unstaking
    virtual bool requestUnstake(const Crypto::Hash& depositHash, uint64_t blockHeight) = 0;
    virtual bool canClaim(const Crypto::Hash& depositHash, uint64_t currentBlock) = 0;

    // Status
    virtual bool isElderfier(const std::string& address) const = 0;
    virtual uint64_t getTotalStaked(const std::string& address) const = 0;
};
```

**Effort:** 2-3 hours (mostly glue code)
**Dependencies:** ElderfierDepositData fields (Task 1.1)

---

### Task 2.4: Implement CLI Commands
**File:** `xfg-stark-cli` (new elderfier-stake subcommand)
**Current State:** CLI exists for other features
**Required Commands:**

```bash
# 1. Create deposit
$ xfg-stark-cli elderfier-stake create --amount 800
# Output: "Deposit created! Hash: 0x... | Unlock in: 8 days (after request)"

# 2. Check status
$ xfg-stark-cli elderfier-stake status
# Output: Lists all deposits, shows "STAKING" status

# 3. Request unstaking
$ xfg-stark-cli elderfier-stake initiate-unstake --index 0
# Output: "Unstaking initiated! Countdown: 8 days"

# 4. Check unlock countdown
$ xfg-stark-cli elderfier-stake unstake-status
# Output: Shows deposits ready to claim, countdown remaining

# 5. Claim after unlocking
$ xfg-stark-cli elderfier-stake claim --index 0
# Output: "Claim successful! 800 XFG returned"
```

**Implementation Details:**

1. **create:** Generate 0xEF transaction, submit to network
2. **status:** Query RPC `get_elderfier_stake_info`
3. **initiate-unstake:** Create special transaction marking unlock request
4. **unstake-status:** Query RPC to show countdown
5. **claim:** Create 0xEE withdrawal transaction

**Effort:** 5-6 hours
**Testing:** End-to-end CLI tests

---

## Week 2 Summary

### Deliverables:
- ✅ 0xEF transaction validation complete
- ✅ Fee extraction integrated into block processing
- ✅ EldernodeIndexManager integration complete
- ✅ CLI commands fully functional

### Code Quality (Week 2):
- [ ] Input validation on all CLI inputs
- [ ] Proper error messages for user guidance
- [ ] CSPRNG for any random data generation
- [ ] No exposure of private keys in logs
- [ ] Transaction signing correct

### Testing (Week 2):
- [ ] Create deposit on testnet ✓
- [ ] Query candidates list ✓
- [ ] Verify security window ✓
- [ ] Initiate unstaking ✓
- [ ] Claim after countdown ✓
- [ ] Fee calculations correct ✓

---

## Week 3: Privacy & Aliases (3-4 days)

### Task 3.1: On-Fuego-Chain Alias System (0xEA Tag)
**File:** `src/CryptoNoteCore/TransactionExtra.h` + CommitmentIndex
**Status:** Requires new transaction tag
**Effort:** 3-4 days (design + implementation + testing)

(See FUEGO_CHAIN_ALIAS_SYSTEM.md for full details)

---

## Week 4: Testing & Launch Prep (3-4 days)

### Task 4.1: Security Review
- Code review by 2+ developers
- No critical/high-severity issues
- Medium issues documented

### Task 4.2: Testnet Validation
- Create deposits ✓
- Verify unlock windows ✓
- Claim successfully ✓
- No data inconsistencies ✓

### Task 4.3: Documentation
- "How to Become an Elderfier" guide
- FAQ section
- Example commands
- Community briefing

---

## Pre-Launch Checklist (48 hours before Dynamigo)

### Code Readiness:
- [ ] All 0xEF deposit code committed
- [ ] All RPC endpoints implemented
- [ ] CLI commands working
- [ ] Tests passing (80%+ coverage)
- [ ] No compiler warnings

### Security:
- [ ] Code reviewed by team
- [ ] No critical issues
- [ ] No high-severity issues
- [ ] Memory safety validated

### Testnet:
- [ ] Deposits work on testnet
- [ ] RPC queries work
- [ ] CLI commands work
- [ ] Unlock countdown accurate
- [ ] Claiming works after unlock

### Documentation:
- [ ] User guide published
- [ ] FAQ document complete
- [ ] Example commands documented
- [ ] API documentation updated

### Support:
- [ ] Discord channel created
- [ ] Support team trained
- [ ] Monitoring alerts set up
- [ ] Backup plan documented

---

## Key Success Metrics

### Functionality:
- ✅ Elderfiers can create 0xEF deposits
- ✅ User-initiated unstaking works
- ✅ 8-day unlock window enforced
- ✅ Multiple deposits per elderfier supported
- ✅ Fee accumulation visible in RPC

### Privacy:
- ✅ No correlation between deposits
- ✅ Hashed address format optional (Phase 2+)
- ✅ On-Fuego aliases planned (Phase 3)

### Performance:
- ✅ RPC queries < 1 second for 100+ elderfiers
- ✅ Block processing not slowed by fee tracking
- ✅ Epoch finalization efficient

### Security:
- ✅ Amount validation (exactly 800 XFG)
- ✅ Security window enforced (8 hours)
- ✅ No double-claiming
- ✅ Thread-safe access

---

## File Dependencies

| File | Dependency | Purpose |
|------|-----------|---------|
| CommitmentIndex.h/cpp | TransactionExtra.h | Fee tracking |
| Blockchain.cpp | CommitmentIndex | Call addElderfierFee() |
| CoreRpcServerCommandsDefinitions.h | EldernodeIndexManager | Query deposits |
| TransactionValidator.cpp | TransactionExtra.h | Validate 0xEF |
| EldernodeIndexManager.h/cpp | ElderfierDepositData | Store deposits |
| xfg-stark-cli | RPC endpoints | Query chain state |

---

## Risk Assessment

### High Risk:
- Thread safety in CommitmentIndex (MITIGATED: mutex protection)
- Fee loss due to rounding (MITIGATED: use integer division)
- Epoch mismatch across nodes (MITIGATED: deterministic schedule)

### Medium Risk:
- RPC endpoint performance (MITIGATION: test with 100+ deposits)
- Unlock countdown off-by-one errors (MITIGATION: unit tests)
- CLI usability issues (MITIGATION: user testing)

### Low Risk:
- Backward compatibility (0xEF is new tag, no conflicts)
- Data migration (fresh start, no legacy support)

---

## Success Criteria for Dynamigo Launch

✅ Elderfiers can create 0xEF deposits
✅ Deposits show in RPC queries
✅ CLI commands fully functional
✅ Unlock countdown accurate (8 days)
✅ Fee accumulation verified
✅ No critical issues in testnet
✅ Documentation complete
✅ Community ready to register

**Target Launch Date:** End of Week 4 (4 weeks from start of Week 1)
**Parallel Work:** MVP continues in parallel, targets completion 4 weeks after Dynamigo launch
