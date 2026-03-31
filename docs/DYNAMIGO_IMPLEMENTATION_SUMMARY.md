# Dynamigo Implementation Summary

## Overview

**Dynamigo** is the Elderfier registration phase preceding MVP. It enables users to become Elderfiers by staking 1600-3200 XFG (split into 0xEF deposits), with privacy-preserving alias options and user-initiated unstaking windows.

**Status:** All architecture designed, existing infrastructure identified, ready for implementation.

---

## Part 1: What Already Exists (Review of EldernodeIndexManager)

The codebase already has comprehensive Elderfier infrastructure:

### EldernodeIndexTypes.h - Core Data Structures

✅ **Already defined:**
- `ElderfierServiceId` - Service IDs with privacy options (lines 36-48)
  - `ServiceIdType`: STANDARD_ADDRESS, CUSTOM_NAME (8-char), HASHED_ADDRESS
  - Already supports 8-character custom names!

- `ElderfierDepositData` - Deposit tracking (lines 181-217)
  - Deposit hash, amount, timestamps
  - **MISSING:** `unstakingRequested` flag (need to add for user-initiated unlock)
  - **MISSING:** `unstakingRequestBlock` (need to add)

- `ElderfierMonitoringConfig` - Configuration (lines 156-178)
  - Block-based monitoring, mempool buffer, Elder Council voting
  - Slashing config, security windows

- `MempoolSecurityWindow` - 8-hour security window (lines 67-85)
  - Already implements security window concept!

- `ElderCouncilVote` & `ElderCouncilVotingMessage` - Voting infrastructure (lines 103-137)
  - Voting system already designed
  - **Status:** Phase 2 (not for MVP/Dynamigo)

- `SlashingConfig` - Penalty system (lines 332-346)
  - **BURN only** - correct! (lines 323-330)
  - **Status:** Phase 2 (not for MVP/Dynamigo)

### EldernodeIndexManager.h - Manager Interface

✅ **Already defined methods:**
- `addElderfierDeposit()` - Add deposits
- `verifyElderfierDeposit()` - Verify deposits
- `getElderfierDeposit()` - Query deposits
- `addElderfierToENindex()` - Add to registry
- `removeElderfierFromENindex()` - Remove from registry
- `requestElderfierUnlock()` - Request unstaking (**need to update**)
- `processElderfierUnlock()` - Process unstaking (**need to update**)
- `getValidElderfierDeposits()` - Get all valid deposits
- `getTotalStakeAmount()` - Query total stake

**Status:** ~80% of Dynamigo infrastructure already in place!

---

## Part 2: What Needs to Be Added/Modified

### 1. User-Initiated Unlock Window (Small Addition)

**File:** `include/EldernodeIndexTypes.h`

```cpp
struct ElderfierDepositData {
    // ... existing fields ...

    // NEW for Dynamigo: User-initiated unstaking
    bool unstakingRequested;        // User initiated unstaking
    uint64_t unstakingRequestBlock; // When unstaking was requested (triggers 8-day countdown)
    uint64_t unstakeClaimableBlock; // Block when deposit becomes claimable (requestBlock + 19200)
};
```

**Changes:**
- Add 2 new uint64 fields (~16 bytes)
- Modify `requestElderfierUnlock()` to set these fields
- Modify `canElderfierUnlock()` to check if block >= unstakeClaimableBlock
- **Effort:** 1-2 hours

### 2. Privacy Registry Integration

**File:** Solidity contract (new)

```solidity
// ElderfiersPrivacyRegistry.sol
// Maps 8-character aliases to hashed addresses
// Already uses ElderfierServiceId concept from C++ code

mapping(string => bytes32) public aliasToHashedAddress;  // Public: Alias → Hash
mapping(bytes32 => address) private hashedToReal;        // Private: Hash → Address
```

**Integration with EldernodeIndexManager:**
- When adding elderfier to index, store alias mapping
- Query by alias: `EldernodeIndexManager::getEldernodeByServiceId()`
- Already supports this pattern! (line 48)

**Effort:** 2-3 hours (contract + testing)

### 3. Fee Extraction

**Files:**
- `src/CryptoNoteCore/TransactionValidator.cpp` - Validate fee percentage
- `src/CryptoNoteCore/Blockchain.cpp` - Extract fees during block processing
- `src/CryptoNoteCore/CommitmentIndex.cpp` - Accumulate fees

**Key Integration:**
- When 0xEF deposit processed, extract fee
- Store in `ElderfierFeePool` (new, lightweight)
- Expose via RPC: `get_elderfier_fees`

**Effort:** 4-5 hours (implementation + testing)

### 4. RPC Endpoints

**File:** `src/Rpc/CoreRpcServerCommandsDefinitions.h`

Extend with:
```cpp
struct GetElderfierCandidatesRequest  { };
struct GetElderfierCandidatesResponse {
    std::vector<ElderfierDepositData> candidates;
    uint64_t totalStaked;
};

struct GetElderfierStakeInfoRequest  { std::string accountAddress; };
struct GetElderfierStakeInfoResponse { /* deposit details */ };
```

**Integration:**
- Use existing `EldernodeIndexManager::getValidElderfierDeposits()`
- Format response with privacy alias if registered

**Effort:** 3-4 hours (endpoints + testing)

### 5. CLI Commands

**File:** `xfg-stark-cli` (elderfier-stake subcommands)

```bash
elderfier-stake create --amount 800
elderfier-stake set-alias --alias FIRENODE --salt 0x...
elderfier-stake status
elderfier-stake initiate-unstake --index 0
elderfier-stake unstake-status
elderfier-stake claim --index 0
```

**Integration:**
- Use RPC endpoints to query status
- Call EldernodeIndexManager via RPC for operations
- Generate secrets locally (CSPRNG)

**Effort:** 5-6 hours (implementation + testing)

---

## Part 3: Implementation Checklist

### Phase 1: Core Infrastructure (2-3 days)

- [ ] **Add unstaking fields to ElderfierDepositData**
  - [ ] `unstakingRequested: bool`
  - [ ] `unstakingRequestBlock: uint64_t`
  - [ ] `unstakeClaimableBlock: uint64_t`
  - [ ] Update methods in EldernodeIndexManager

- [ ] **Fee tracking system**
  - [ ] Create `ElderfierFeePool` class
  - [ ] Track HEAT burn fees
  - [ ] Track COLD deposit fees
  - [ ] Expose via RPC

- [ ] **RPC Endpoints (3 minimum)**
  - [ ] `get_elderfier_candidates` - List all registered elderfiers
  - [ ] `get_elderfier_stake_info` - Get details by account
  - [ ] `get_elderfier_fees` - Get accumulated fees

### Phase 2: Privacy & Privacy Registry (1-2 days)

- [ ] **ElderfiersPrivacyRegistry.sol contract**
  - [ ] `registerAlias(alias, hashSalt)`
  - [ ] `resolveAlias(alias)` - Owner only
  - [ ] `getPublicAlias(hashedAddress)`
  - [ ] Alias validation (8 chars, [A-Z0-9])

- [ ] **Integration with EldernodeIndexManager**
  - [ ] Store alias when elderfier registered
  - [ ] Query alias when displaying elderfier
  - [ ] Hashed address support

### Phase 3: CLI Commands (1-2 days)

- [ ] `elderfier-stake create --amount 800`
- [ ] `elderfier-stake set-alias --alias FIRENODE`
- [ ] `elderfier-stake status`
- [ ] `elderfier-stake initiate-unstake --index 0`
- [ ] `elderfier-stake unstake-status`
- [ ] `elderfier-stake claim --index 0`

### Phase 4: Testing & Security (2-3 days)

- [ ] Unit tests for unstaking logic
- [ ] Unit tests for fee calculation
- [ ] Integration tests (create deposit → query → unstake → claim)
- [ ] Security audit: Private key handling, alias collision
- [ ] Testnet validation

**Total Effort: 8-12 days**

---

## Part 4: Existing Infrastructure Checklist

### ✅ Already Implemented (Don't Duplicate)

| Feature | Status | Location |
|---------|--------|----------|
| Elderfier deposit data structure | ✅ Complete | EldernodeIndexTypes.h:181-217 |
| Deposit validation | ✅ Complete | IEldernodeIndexManager::verifyElderfierDeposit() |
| ENindex (registry) | ✅ Complete | EldernodeIndexManager::addElderfierToENindex() |
| Service ID system | ✅ Complete | ElderfierServiceId::createCustomName() |
| 8-character names | ✅ Complete | ElderfierServiceConfig::customNameLength = 8 |
| Security window tracking | ✅ Complete | ElderfierMonitoringConfig |
| Uptime monitoring | ✅ Complete | ElderfierDepositData::totalUptimeSeconds |
| Selection multipliers | ✅ Complete | SelectionMultipliers (1x-16x based on uptime) |
| Slashing (BURN only) | ✅ Complete | SlashingDestination::BURN |
| Elder Council voting system | ✅ Complete | ElderCouncilVote, ElderCouncilVotingMessage |

### ⏳ Needs Extension/Integration

| Feature | Work Needed | Priority |
|---------|------------|----------|
| User-initiated unlock | Add unstakingRequested, unstakingRequestBlock fields | HIGH |
| Fee tracking | Create ElderfierFeePool, integrate with transaction processing | HIGH |
| Privacy registry | Create ElderfiersPrivacyRegistry.sol contract | MEDIUM |
| CLI commands | Implement elderfier-stake subcommands | HIGH |
| RPC endpoints | Extend with get_elderfier_* methods | HIGH |

### ❌ Explicitly NOT for Dynamigo MVP

| Feature | Status | Reason |
|---------|--------|--------|
| Elder Kings Council voting | Phase 2 | Complex governance, not needed for registration |
| Slashing mechanism | Phase 2 | Needs Elder Council voting first |
| Misbehavior detection | Phase 2 | Requires running elderfiers first |
| Mempool security window | Phase 2 | For transactions after MVP launch |
| Network-wide aliases | Phase 2 | Too much work, can add later |

---

## Part 5: Code Quality Standards for Dynamigo

### Security Requirements

- [ ] CSPRNG for secret generation (use `crypto_rand_*` from libsodium)
- [ ] No hardcoded values (all configurable)
- [ ] Input validation on all RPC calls
- [ ] Overflow checks on amount/fee calculations
- [ ] Mutex protection for concurrent access
- [ ] No private key logging

### Code Style

- [ ] Match existing Fuego codebase style
- [ ] Maximum line length: 120 characters
- [ ] Clear variable names (no single-letter except loop counters)
- [ ] Comments explain WHY, not WHAT
- [ ] Functions < 50 lines where possible

### Testing Coverage

- [ ] Unit tests: > 80% coverage
- [ ] Edge cases: block 0, max uint64, etc.
- [ ] Integration: Create → query → unstake → claim
- [ ] Stress: 10k deposits, query performance < 100ms
- [ ] Regression: HEAT/COLD unaffected

### Documentation

- [ ] Function headers with purpose/params/return
- [ ] Complex logic explained (why 19200 blocks = 8 days)
- [ ] Assumptions listed (e.g., "assumes no block reorg > 100")
- [ ] Known limitations documented

---

## Part 6: Timeline for Dynamigo Launch

### Week 1: Core Infrastructure
- Days 1-2: Add unstaking fields, extend EldernodeIndexManager
- Days 3-4: Implement fee tracking system
- Days 5: Complete RPC endpoints

### Week 2: Privacy & CLI
- Days 1-2: ElderfiersPrivacyRegistry.sol contract
- Days 3-4: Implement CLI commands
- Days 5: Testing & debugging

### Week 3: Security & Testnet
- Days 1-2: Security audit
- Days 3-4: Testnet validation
- Days 5: Documentation, launch prep

**Total:** ~15 days (3 weeks)

### Parallel Work (Weeks 1-3)

While Dynamigo implementation proceeds:
- Implement relay daemon for root submission
- Fix block header relay reorg handling
- Deploy FuegoCommitmentMerkleVerifier to testnet
- Build React frontend (parallel team)

**MVP Launch:** ~4 weeks after Dynamigo launch

---

## Part 7: What to Reuse vs. Build

### Reuse (Don't Reinvent)

```cpp
// EldernodeIndexManager already has:
- addElderfierDeposit(const ElderfierDepositData& deposit)
- verifyElderfierDeposit(const ElderfierDepositData& deposit)
- getValidElderfierDeposits() const
- requestElderfierUnlock(const Crypto::PublicKey& pk, uint64_t timestamp)
- processElderfierUnlock(const Crypto::PublicKey& pk)
- getTotalStakeAmount() const

// Just call these methods!
```

### Build New

```cpp
// Add to existing classes:
- unstakingRequested: bool
- unstakingRequestBlock: uint64_t
- unstakeClaimableBlock: uint64_t

// New contract (Solidity):
- ElderfiersPrivacyRegistry.sol (maps alias → hashed address)

// New RPC endpoints:
- get_elderfier_candidates()
- get_elderfier_stake_info()
- get_elderfier_fees()

// New CLI commands:
- elderfier-stake create
- elderfier-stake set-alias
- elderfier-stake status
- elderfier-stake initiate-unstake
- elderfier-stake unstake-status
- elderfier-stake claim
```

---

## Part 8: Risk Mitigation

### Potential Issues & Mitigations

| Risk | Mitigation |
|------|-----------|
| **Alias collisions** | First-come-first-served, reserv common names |
| **Double-unstaking** | Set `unstakingRequested` flag, check before processing |
| **Fee calculation errors** | Use SafeMath, extensive unit tests |
| **Privacy leaks** | Never log hashed address with Fuego tx hash |
| **Reorg during unstaking** | Verify `unstakingRequestBlock` still valid after reorg |

### Testing Critical Paths

1. **Happy path:** Create → status → initiate unstake → 8-day wait → claim
2. **Error path:** Create → try to claim immediately (should fail)
3. **Alias path:** Create → set alias → query by alias → verify hashed address
4. **Fee path:** Create deposit → verify fee extracted → query fees
5. **Reorg path:** Create deposit → reorg happens → verify deposit still valid

---

## Summary

**Status:** All architecture complete, existing infrastructure reviewed, ready to build.

**Key Insight:** ~80% of Dynamigo infrastructure already exists in EldernodeIndexManager!

**Work Remaining:**
1. Add user-initiated unlock fields (~2 hours)
2. Implement fee tracking (~4-5 hours)
3. Create privacy registry contract (~2-3 hours)
4. Build CLI commands (~5-6 hours)
5. Create RPC endpoints (~3-4 hours)
6. Testing & security review (~5-7 hours)

**Total: ~22-28 days** (single developer, conservative estimate with testing)

**Recommended:** 3-week timeline with dedicated team of 2 (implementation + testing in parallel)

**Launch Target:** Dynamigo opens 8-14 days before MVP, allows community participation while you finalize contracts.
