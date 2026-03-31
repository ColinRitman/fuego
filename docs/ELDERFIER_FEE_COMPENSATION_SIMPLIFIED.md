# Elderfier Fee Compensation - Simplified Design

## Clarification: Fuego-Native Compensation (No Treasury Contracts)

**Key Insight:** Elderfiers are Fuego blockchain nodes that extract compensation **entirely in XFG** from fees on the Fuego chain itself.

**No smart contracts needed.** Just direct fee extraction.

---

## Part 1: The Challenge & Solution

### The Problem

**Scenario:**
- 5 elderfiers total (FIRENODE, XFG4LIFE, HODLER42, MOONBOUND, KEYSTROKE)
- Only 3 needed for merkle root attestation
- 100 XFG in deposit fees accumulated per day
- How to fairly pay elderfiers if only 3 are "active" at a time?

**Naive approach:** Pay only the 3 who signed the root (unfair to inactive ones)

**Better approach:** Rotate attestation duty, all 5 earn rewards based on uptime/participation

### The Solution: Uptime-Based Fair Compensation

**Key principle:**
```
Each elderfier earns compensation proportional to:
1. How often they participated in merkle attestations
2. How long they've been running/online
3. Total fees accumulated during their "duty window"
```

**Not all 5 sign every root.**
**But all 5 earn fees based on their contribution.**

---

## Part 2: Elderfier Fee Distribution Model

### Model: Epoch-Based Distribution

**Concept:** Divide time into epochs, rotate elderfiers through duty cycles

```
Epoch Duration: 7 days (configurable)

Epoch 1:
  Active elderfiers: [FIRENODE, XFG4LIFE, HODLER42]
  Accumulated fees: 700 XFG
  Distribution: 233 XFG each
  Inactive: [MOONBOUND, KEYSTROKE] earn 0

Epoch 2:
  Active elderfiers: [FIRENODE, MOONBOUND, KEYSTROKE]
  Accumulated fees: 650 XFG (less activity)
  Distribution: 217 XFG each
  Inactive: [XFG4LIFE, HODLER42] earn 0

Epoch 3:
  Active elderfiers: [XFG4LIFE, HODLER42, KEYSTROKE]
  Accumulated fees: 800 XFG
  Distribution: 267 XFG each
  Inactive: [FIRENODE, MOONBOUND] earn 0
```

**Over time, all 5 elderfiers get paid equally** (if they all contribute equally)

### Implementation: Track on Fuego Chain

```cpp
// In CommitmentIndex or ElderfierManager

struct ElderfierEpochRewards {
    uint64_t epoch;                    // Epoch number
    std::vector<bytes32> activeElders; // Who was active this epoch
    uint64_t totalFeesCaptured;        // XFG collected
    std::map<bytes32, uint64_t> earned; // Each elder's share
    uint64_t epochStartBlock;
    uint64_t epochEndBlock;
};

std::vector<ElderfierEpochRewards> m_epochHistory;

// Method: Calculate which elderfiers are "active" for current epoch
std::vector<bytes32> getCurrentActiveElderfiers();

// Method: Distribute fees to active elderfiers only
void distributeFeesToActiveElderfiers(uint64_t totalFees);

// Method: Query earned fees for specific elderfier
uint64_t getElderfierEarnings(bytes32 elderfierHash, uint64_t epoch);
```

---

## Part 3: Rotation Strategy

### Option A: Time-Based Rotation (Simplest)

**Every 7 days, rotate who's "active"**

```
Weeks 1-2 (Epoch 1):
  Active: Elderfiers 1, 2, 3
  Inactive: 4, 5

Weeks 3-4 (Epoch 2):
  Active: Elderfiers 2, 3, 4
  Inactive: 1, 5

Weeks 5-6 (Epoch 3):
  Active: Elderfiers 3, 4, 5
  Inactive: 1, 2

Weeks 7-8 (Epoch 4):
  Active: Elderfiers 4, 5, 1
  Inactive: 2, 3

Weeks 9-10 (Epoch 5):
  Active: Elderfiers 5, 1, 2
  Inactive: 3, 4

Weeks 11-12: Cycle repeats (back to Epoch 1)
```

**Benefits:**
- Simple round-robin
- All 5 get equal opportunities
- Predictable (everyone knows when they're "active")
- Penalizes downtime (if you're not online, you don't earn)

### Option B: Uptime-Based Rotation (Fair but Complex)

**Active elderfiers selected based on continuous uptime**

```
Elderfier uptime tracking:
  FIRENODE:    99.9% uptime (1 day offline in 100 days) ← Very reliable
  XFG4LIFE:    98.5% uptime (1.5 days offline in 100 days)
  HODLER42:    95.0% uptime (5 days offline in 100 days)
  MOONBOUND:   92.0% uptime (8 days offline in 100 days)
  KEYSTROKE:   88.0% uptime (12 days offline in 100 days)

Selection for next epoch:
  Top 3 by uptime get selected
  Results: FIRENODE, XFG4LIFE, HODLER42 ← Most reliable

  MOONBOUND and KEYSTROKE not selected this epoch
  Incentive: If they increase uptime, they get selected next time
```

**Benefits:**
- Rewards reliability (incentivizes running reliable nodes)
- Punishes downtime (don't earn if offline)
- Self-adjusting (no governance needed)

**Drawbacks:**
- More complex to implement
- Requires continuous uptime tracking
- Unfair to new elderfiers with initial downtime

### Recommendation: Option A (Time-Based)

**For MVP:** Use simple time-based rotation
- Easy to implement
- Fair (everyone gets equal turns)
- Predictable schedule
- Can upgrade to uptime-based later if desired

---

## Part 4: Example Fee Flow

### Daily Fee Collection

```
User creates 0xEF deposit: 800 XFG
Fee: 0.5% = 4 XFG

Day 1: 25 deposits → 100 XFG in fees collected on-chain
Day 2: 18 deposits → 72 XFG in fees
Day 3: 30 deposits → 120 XFG in fees
...
Week 1 Total: 700 XFG in fees

Active elderfiers (Week 1): [FIRENODE, XFG4LIFE, HODLER42]
Distribution (end of Week 1):
  FIRENODE:   700 ÷ 3 = 233 XFG
  XFG4LIFE:   700 ÷ 3 = 233 XFG
  HODLER42:   700 ÷ 3 = 233 XFG
  (Remaining 1 XFG rounding loss → burn or save for next epoch)

MOONBOUND, KEYSTROKE: 0 XFG this week (inactive)
```

### Over Full Cycle (5 Epochs × 7 days = 35 days)

```
If total fees: 5000 XFG over 35 days
And all 5 elderfiers have equal uptime:

Each elderfier gets active for 3 epochs (21 days)
Each epoch gets: 5000 ÷ 5 epochs = 1000 XFG per epoch
Per elderfier: 1000 × 3 ÷ 3 elderfiers per epoch = 1000 XFG total

Result: All 5 earn exactly 1000 XFG (fair!)
```

---

## Part 5: On-Chain Fee Extraction (Simplified)

### No Treasury Contract Needed

**Just track fees in CommitmentIndex:**

```cpp
// In CommitmentIndex.h

struct FeeEpoch {
    uint64_t epochNumber;
    std::vector<bytes32> activeElderfiers; // 3 addresses
    uint64_t totalFeesCollected;          // In atomic units
    std::map<bytes32, uint64_t> distribution; // Each elder's share
    uint64_t blockHeight;
};

class CommitmentIndex {
    std::vector<FeeEpoch> m_feeHistory;

    // Called when processing 0xEF deposits
    void addFee(uint64_t feeAmount) {
        m_currentEpoch.totalFeesCollected += feeAmount;
    }

    // Called at end of epoch
    void finalizeEpoch() {
        uint64_t perElderfier = m_currentEpoch.totalFeesCollected / 3;
        for (auto& elder : m_currentEpoch.activeElderfiers) {
            m_currentEpoch.distribution[elder] = perElderfier;

            // EMIT EVENT: Elderfier earned fees
            // Elderfier's Fuego node sees this event, claims reward
        }
        m_feeHistory.push_back(m_currentEpoch);
    }

    // Elderfiers call this RPC to check their earnings
    uint64_t getElderfierEarnings(bytes32 elder, uint64_t epoch) {
        return m_feeHistory[epoch].distribution[elder];
    }
};
```

### RPC Endpoint to Check Earnings

```bash
POST http://localhost:18180/json_rpc

Request:
{
  "jsonrpc": "2.0",
  "id": "0",
  "method": "get_elderfier_earnings",
  "params": {
    "elderfier_hash": "0x1f3e5d7c",
    "epoch": 1
  }
}

Response:
{
  "jsonrpc": "2.0",
  "id": "0",
  "result": {
    "epoch": 1,
    "elderfier": "0x1f3e5d7c",
    "earned_xfg": 233.5,
    "active": true,
    "epoch_start_block": 1000000,
    "epoch_end_block": 1043200
  }
}
```

### Claiming Earnings (On-Chain)

```cpp
// Elderfier runs this to claim their XFG earnings

// Call RPC to get fees:
uint64_t earnings = getRpcFees("0x1f3e5d7c", epoch);

// Create withdrawal transaction:
Transaction withdraw;
withdraw.fee = 0.001 XFG;
withdraw.amount = earnings; // e.g., 233.5 XFG
withdraw.destination = elderfier_address;
withdraw.tag = 0xEE; // Special "elderfier earnings" tag

// Submit to network
broadcastTransaction(withdraw);
```

---

## Part 6: Epoch Rotation Logic

### Simple Implementation

```cpp
// In CommitmentIndex

const uint64_t EPOCH_DURATION_BLOCKS = 43200; // 7 days on Fuego
const uint8_t TOTAL_ELDERFIERS = 5;
const uint8_t ACTIVE_ELDERFIERS_PER_EPOCH = 3;

// Rotation schedule (deterministic)
std::array<std::vector<uint8_t>, 5> ROTATION_SCHEDULE = {
    {1, 2, 3},    // Epoch 1: Elderfiers 1, 2, 3
    {2, 3, 4},    // Epoch 2: Elderfiers 2, 3, 4
    {3, 4, 5},    // Epoch 3: Elderfiers 3, 4, 5
    {4, 5, 1},    // Epoch 4: Elderfiers 4, 5, 1
    {5, 1, 2},    // Epoch 5: Elderfiers 5, 1, 2
};

uint64_t currentEpoch() const {
    return getCurrentBlockHeight() / EPOCH_DURATION_BLOCKS;
}

std::vector<uint8_t> getCurrentActiveElderfiers() const {
    return ROTATION_SCHEDULE[currentEpoch() % 5];
}

bool isElderfierActive(uint8_t elderfierIndex) const {
    auto active = getCurrentActiveElderfiers();
    return std::find(active.begin(), active.end(), elderfierIndex) != active.end();
}
```

---

## Part 7: Fee Distribution at Epoch Boundary

```cpp
// Called at end of each epoch (block height % EPOCH_DURATION_BLOCKS == 0)

void CommitmentIndex::finalizeEpoch() {
    uint64_t epoch = currentEpoch();
    uint64_t totalFees = m_currentEpoch.totalFeesCollected;

    auto activeElderfiers = ROTATION_SCHEDULE[epoch % 5];
    uint64_t perElderfier = totalFees / activeElderfiers.size();

    // Record distribution
    for (auto elderfierIndex : activeElderfiers) {
        bytes32 elderfierHash = m_elderfierHashes[elderfierIndex];
        m_feeHistory[epoch].distribution[elderfierHash] = perElderfier;

        // Emit event: Elderfier can now claim their earnings
        logEpochFeeDistribution(epoch, elderfierHash, perElderfier);
    }

    // Reset for next epoch
    m_currentEpoch.totalFeesCollected = 0;
}
```

---

## Part 8: Elderfier Node Claiming Process

### Automated Claiming (Elderfier Node)

```bash
# Elderfier node background process (e.g., runs every 12 hours)

1. Query: Get current epoch
   RPC: get_current_epoch()
   Response: epoch = 5

2. Query: Was I active in epoch 5?
   RPC: is_elderfier_active(my_hash, epoch=5)
   Response: active = true, earned = 233.5 XFG

3. If not yet claimed:
   Create withdrawal transaction
   destination = my_wallet_address
   amount = 233.5 XFG
   tag = 0xEE (earnings claim)

4. Submit transaction
   Broadcast to Fuego network

5. Wait for confirmation
   Once block confirmed, earnings transferred to wallet

6. Log claim
   track_claimed_earnings(epoch=5, amount=233.5)
```

---

## Part 9: No Smart Contracts Needed

### What You DON'T Need

```
❌ ElderfierTreasuryPool.sol (no L1 contract)
❌ L1/L2 bridge for fees (no cross-chain needed)
❌ DAO voting for distribution (no governance overhead)
❌ Token minting (just use existing XFG)
```

### What You DO Need

```
✅ Fee extraction on Fuego (already designed)
✅ Fee tracking in CommitmentIndex (add ~50 lines of code)
✅ Epoch rotation logic (add ~100 lines of code)
✅ RPC endpoint to check earnings (~30 lines)
✅ Withdrawal transaction processing (~20 lines)
```

**Total effort: ~200 lines of C++ code (1-2 days)**

---

## Part 10: Complete Flow

```
User creates 0xEF deposit
    ↓
Fee extracted (0.5% of deposit)
    ↓
Added to current epoch's fee pool
    ↓
Epoch ends (7 days / 43200 blocks)
    ↓
Fees distributed equally to active 3 elderfiers
    ↓
Each elderfier can claim their XFG
    ↓
Elderfier submits withdrawal transaction
    ↓
XFG transferred to elderfier's Fuego wallet
    ↓
Next epoch begins, rotation changes
    ↓
Different 3 elderfiers are now "active"
```

---

## Part 11: Summary

### Simplified Elderfier Compensation

**No treasury contracts.** Just fees on Fuego chain.

**How it works:**
1. 5 elderfiers total
2. 3 are "active" each week (rotating schedule)
3. All deposit fees that week split 3 ways
4. Active elderfiers claim their XFG
5. Next week, different 3 are active
6. Over 5 weeks, everyone earns equally (if uptime equal)

**Effort: ~200 lines of code (1-2 days)**

**Result:**
- Simple ✅
- Fair ✅
- Transparent ✅
- No smart contracts ✅
- Entirely on Fuego ✅
- Direct XFG compensation ✅

This is much cleaner than treasury contracts!
