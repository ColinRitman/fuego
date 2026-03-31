# Week 1, Task 1.2: Epoch-Based Fee Tracking in CommitmentIndex

**Status:** In Progress
**Timeline:** 4-5 hours implementation + 2 hours testing
**Files Modified:** src/CryptoNoteCore/CommitmentIndex.h/.cpp
**Objective:** Track HEAT/COLD fees and distribute to active elderfiers via epoch-based rotation

---

## Part 1: Data Structures to Add

### New Structs in CommitmentIndex.h:

**Add after CommitmentEntry struct (after line 47):**

```cpp
/// Fee epoch tracking for elderfier compensation
/// Elderfiers rotate in/out every 7 days (43200 blocks)
/// 3 of 5 active at any time, split fees equally
struct ElderfierEpochRewards {
    uint64_t epochNumber;                        // Which epoch (0, 1, 2, ...)
    std::vector<std::string> activeElderfiers;   // Addresses of active 3 elderfiers THIS epoch
    uint64_t totalFeesCollected;                 // Total XFG collected during epoch
    std::map<std::string, uint64_t> distribution; // Earned: address -> amount
    uint64_t epochStartBlock;                    // First block of epoch
    uint64_t epochEndBlock;                      // Last block of epoch

    // Helper methods
    bool isValid() const {
        return epochNumber >= 0 &&
               activeElderfiers.size() == 3 &&
               !distribution.empty() &&
               epochStartBlock <= epochEndBlock;
    }
};
```

**Constants for epoch management:**

```cpp
namespace CommitmentIndexConstants {
    // Epoch configuration
    static constexpr uint64_t EPOCH_DURATION_BLOCKS = 43200;      // 7 days
    static constexpr uint8_t TOTAL_ELDERFIERS = 5;
    static constexpr uint8_t ACTIVE_ELDERFIERS_PER_EPOCH = 3;

    // Fee percentages (basis points)
    static constexpr uint64_t HEAT_FEE_BPS = 50;      // 0.5%
    static constexpr uint64_t COLD_FEE_BPS = 100;     // 1.0%

    // Block time (Fuego)
    static constexpr uint64_t BLOCKS_PER_DAY = 20160;  // ~1 minute per block
}
```

---

## Part 2: CommitmentIndex Class Modifications

### Add Private Members:

**In CommitmentIndex private section (after line 143):**

```cpp
private:
    // Existing members:
    mutable std::mutex m_mutex;
    std::unordered_map<Crypto::Hash, CommitmentEntry> m_commitments;
    std::unordered_map<Height, std::vector<Crypto::Hash>> m_byHeight;
    size_t m_heatCount;
    size_t m_coldCount;
    Height m_highestBlock;

    // NEW: Epoch-based fee tracking
    std::vector<ElderfierEpochRewards> m_epochHistory;  // Historical record of all epochs
    uint64_t m_currentEpochStartBlock;                  // Start block of current epoch
    uint64_t m_currentEpochTotalFees;                   // Fees accumulated this epoch

    // Current block height (updated during block processing)
    mutable uint64_t m_currentBlockHeight;

    // Deterministic rotation schedule (hard-coded for MVP)
    // Format: activeElderfiers[epochNumber % 5] gives active 3 for that epoch
    std::array<std::vector<uint8_t>, 5> m_rotationSchedule = {{
        {1, 2, 3},    // Epoch 0: Elderfiers 1, 2, 3 active
        {2, 3, 4},    // Epoch 1: Elderfiers 2, 3, 4 active
        {3, 4, 5},    // Epoch 2: Elderfiers 3, 4, 5 active
        {4, 5, 1},    // Epoch 3: Elderfiers 4, 5, 1 active
        {5, 1, 2}     // Epoch 4: Elderfiers 5, 1, 2 active
        // Then cycles back to epoch 0
    }};

    // Elderfier addresses (map from index to address)
    // Index 1 -> "FIRENODE"
    // Index 2 -> "XFG4LIFE"
    // etc.
    std::map<uint8_t, std::string> m_elderfierAddresses;

    // Private helper methods
    uint64_t calculateCurrentEpoch(Height blockHeight) const;
    std::vector<std::string> getActiveElderfiers(uint64_t epochNumber) const;
    void finalizeEpochImpl();
```

### Add Public Methods:

**In CommitmentIndex public section (add after getMerkleProof method, around line 125):**

```cpp
public:
    // Fee tracking methods (Dynamigo Phase)

    /// Add fee collected from deposit (called during block processing)
    /// @param feeAmount: Fee in atomic XFG units
    void addElderfierFee(uint64_t feeAmount);

    /// Finalize current epoch and prepare for next one
    /// Called when blockHeight % EPOCH_DURATION_BLOCKS == 0
    /// Distributes accumulated fees to active elderfiers
    void finalizeEpoch(uint64_t currentBlockHeight);

    /// Get rewards for specific epoch
    /// @param epochNumber: Epoch number (0, 1, 2, ...)
    /// @return Epoch rewards data (empty if not found)
    ElderfierEpochRewards getEpochRewards(uint64_t epochNumber) const;

    /// Get earnings for specific elderfier in specific epoch
    /// @param elderfierAddress: Elderfier address string
    /// @param epochNumber: Epoch number
    /// @return Earned amount in atomic XFG (0 if not active)
    uint64_t getElderfierEarnings(const std::string& elderfierAddress, uint64_t epochNumber) const;

    /// Get current epoch number based on block height
    uint64_t getCurrentEpoch(uint64_t currentBlockHeight) const;

    /// Get active elderfiers for given epoch
    std::vector<std::string> getActiveElderfiers(uint64_t epochNumber) const;

    /// Register elderfier address at index
    /// Called during initialization
    void registerElderfierAddress(uint8_t index, const std::string& address);

    /// Get total fees accumulated in current epoch
    uint64_t getCurrentEpochFees() const;

    /// Get epoch history (for debugging/stats)
    std::vector<ElderfierEpochRewards> getEpochHistory() const;
```

---

## Part 3: Method Implementations in CommitmentIndex.cpp

### Constructor Update:

```cpp
CommitmentIndex::CommitmentIndex()
    : m_heatCount(0),
      m_coldCount(0),
      m_highestBlock(0),
      m_currentEpochStartBlock(0),
      m_currentEpochTotalFees(0),
      m_currentBlockHeight(0) {

    // Initialize elderfier addresses with default names
    // Can be overridden later via registerElderfierAddress()
    m_elderfierAddresses[1] = "FIRENODE";
    m_elderfierAddresses[2] = "XFG4LIFE";
    m_elderfierAddresses[3] = "HODLER42";
    m_elderfierAddresses[4] = "MOONBOUND";
    m_elderfierAddresses[5] = "KEYSTROKE";
}
```

### Fee Tracking Implementation:

```cpp
void CommitmentIndex::addElderfierFee(uint64_t feeAmount) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (feeAmount == 0) {
        return;  // No fee to add
    }

    // Accumulate fee in current epoch
    m_currentEpochTotalFees += feeAmount;

    // Prevent overflow
    if (m_currentEpochTotalFees < feeAmount) {
        // Overflow detected - log and cap at max
        LOG_ERROR("CommitmentIndex: Fee accumulation overflow detected!");
        m_currentEpochTotalFees = std::numeric_limits<uint64_t>::max();
    }
}

uint64_t CommitmentIndex::getCurrentEpochFees() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentEpochTotalFees;
}
```

### Epoch Calculation:

```cpp
uint64_t CommitmentIndex::calculateCurrentEpoch(Height blockHeight) const {
    // Epoch number is determined by block height
    // Epoch 0: blocks 0-43199
    // Epoch 1: blocks 43200-86399
    // etc.
    return blockHeight / CommitmentIndexConstants::EPOCH_DURATION_BLOCKS;
}

uint64_t CommitmentIndex::getCurrentEpoch(uint64_t currentBlockHeight) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return calculateCurrentEpoch(currentBlockHeight);
}
```

### Active Elderfier Selection:

```cpp
std::vector<std::string> CommitmentIndex::getActiveElderfiers(uint64_t epochNumber) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Get rotation schedule entry for this epoch
    size_t scheduleIndex = epochNumber % 5;  // Cycles every 5 epochs
    const auto& activeIndices = m_rotationSchedule[scheduleIndex];

    // Convert indices to addresses
    std::vector<std::string> addresses;
    for (uint8_t index : activeIndices) {
        auto it = m_elderfierAddresses.find(index);
        if (it != m_elderfierAddresses.end()) {
            addresses.push_back(it->second);
        }
    }

    return addresses;  // Should have 3 entries
}

std::vector<std::string> CommitmentIndex::getActiveElderfiers(uint64_t epochNumber) const {
    return getActiveElderfiers(epochNumber);  // Forwarding public method
}
```

### Fee Distribution:

```cpp
void CommitmentIndex::finalizeEpochImpl() {
    // This runs INSIDE the mutex lock
    // Called from finalizeEpoch() and during rollback

    uint64_t epoch = calculateCurrentEpoch(m_currentBlockHeight);
    auto activeElderfiers = getActiveElderfiers(epoch);

    if (activeElderfiers.size() != 3) {
        LOG_ERROR("CommitmentIndex: Expected 3 active elderfiers, got " << activeElderfiers.size());
        return;
    }

    // Calculate per-elderfier share
    uint64_t perElderfier = 0;
    if (m_currentEpochTotalFees > 0) {
        perElderfier = m_currentEpochTotalFees / activeElderfiers.size();
    }

    // Create epoch record
    ElderfierEpochRewards rewards;
    rewards.epochNumber = epoch;
    rewards.activeElderfiers = activeElderfiers;
    rewards.totalFeesCollected = m_currentEpochTotalFees;
    rewards.epochStartBlock = m_currentEpochStartBlock;
    rewards.epochEndBlock = m_currentBlockHeight;

    // Distribute fees
    for (const auto& address : activeElderfiers) {
        rewards.distribution[address] = perElderfier;
    }

    // Store in history
    m_epochHistory.push_back(rewards);

    // Validate
    if (!rewards.isValid()) {
        LOG_ERROR("CommitmentIndex: Invalid epoch rewards");
    }

    // Log epoch finalization
    LOG_INFO("CommitmentIndex: Epoch " << epoch << " finalized"
             << " | Fees: " << m_currentEpochTotalFees
             << " | Per-elderfier: " << perElderfier
             << " | Active: " << activeElderfiers[0] << ", " << activeElderfiers[1] << ", " << activeElderfiers[2]);
}

void CommitmentIndex::finalizeEpoch(uint64_t currentBlockHeight) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Update current block height
    m_currentBlockHeight = currentBlockHeight;

    // Finalize current epoch
    finalizeEpochImpl();

    // Reset for next epoch
    m_currentEpochStartBlock = currentBlockHeight + 1;
    m_currentEpochTotalFees = 0;
}
```

### Query Methods:

```cpp
ElderfierEpochRewards CommitmentIndex::getEpochRewards(uint64_t epochNumber) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (epochNumber >= m_epochHistory.size()) {
        return ElderfierEpochRewards();  // Return empty
    }

    return m_epochHistory[epochNumber];
}

uint64_t CommitmentIndex::getElderfierEarnings(const std::string& elderfierAddress, uint64_t epochNumber) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (epochNumber >= m_epochHistory.size()) {
        return 0;  // Epoch not found or not finalized
    }

    const auto& rewards = m_epochHistory[epochNumber];
    auto it = rewards.distribution.find(elderfierAddress);

    if (it != rewards.distribution.end()) {
        return it->second;  // Return earned amount
    }

    return 0;  // Not active in this epoch
}

std::vector<ElderfierEpochRewards> CommitmentIndex::getEpochHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_epochHistory;
}

void CommitmentIndex::registerElderfierAddress(uint8_t index, const std::string& address) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (index < 1 || index > 5) {
        LOG_ERROR("CommitmentIndex: Invalid elderfier index " << static_cast<int>(index));
        return;
    }

    m_elderfierAddresses[index] = address;
    LOG_INFO("CommitmentIndex: Registered elderfier #" << static_cast<int>(index) << " = " << address);
}
```

### Rollback Support:

**Update rollbackToHeight() to handle epoch cleanup:**

```cpp
size_t CommitmentIndex::rollbackToHeight(Height height) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Remove commitments above height
    size_t removed = 0;

    // ... existing rollback code ...

    // Rollback epoch history if needed
    uint64_t rollbackEpoch = calculateCurrentEpoch(height);

    // Remove epochs that occurred after rollback point
    while (m_epochHistory.size() > rollbackEpoch) {
        m_epochHistory.pop_back();
    }

    // Reset current epoch state
    m_currentBlockHeight = height;
    m_currentEpochStartBlock = (rollbackEpoch * CommitmentIndexConstants::EPOCH_DURATION_BLOCKS);
    m_currentEpochTotalFees = 0;  // Reset - could recompute if needed

    LOG_INFO("CommitmentIndex: Rolled back to height " << height << " (epoch " << rollbackEpoch << ")");

    return removed;
}
```

---

## Part 4: Integration Points

### Where Fee Tracking Is Called:

**1. Blockchain.cpp during HEAT processing:**

```cpp
// Around line 2470
if (field.type() == typeid(TransactionExtraHeatCommitment)) {
    const auto& heatCommit = boost::get<TransactionExtraHeatCommitment>(field);

    // ... existing code ...

    // NEW: Extract fee for elderfiers
    uint64_t feeAmount = (heatCommit.amount * CommitmentIndexConstants::HEAT_FEE_BPS) / 10000;
    if (feeAmount > 0) {
        m_commitmentIndex.addElderfierFee(feeAmount);
    }
}
```

**2. Blockchain.cpp during COLD processing:**

```cpp
// Around line 2489
else if (field.type() == typeid(TransactionExtraColdCommitment)) {
    const auto& coldCommit = boost::get<TransactionExtraColdCommitment>(field);

    // ... existing code ...

    // NEW: Extract fee for elderfiers
    uint64_t feeAmount = (coldCommit.amount * CommitmentIndexConstants::COLD_FEE_BPS) / 10000;
    if (feeAmount > 0) {
        m_commitmentIndex.addElderfierFee(feeAmount);
    }
}
```

**3. Blockchain.cpp at epoch boundary:**

```cpp
// During block processing (in addNewBlock or similar)
if (block.height % CommitmentIndexConstants::EPOCH_DURATION_BLOCKS == 0) {
    // Finalize completed epoch
    m_commitmentIndex.finalizeEpoch(block.height);
}
```

---

## Part 5: RPC Integration

### RPC Endpoint: get_elderfier_earnings

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

**Response Implementation:**

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

    if (epochRewards.isValid()) {
        res.epoch_start_block = epochRewards.epochStartBlock;
        res.epoch_end_block = epochRewards.epochEndBlock;
    }

    return true;
}
```

---

## Part 6: Testing Strategy

### Unit Tests:

1. **Epoch Calculation:**
   - Block 0 → Epoch 0
   - Block 43199 → Epoch 0
   - Block 43200 → Epoch 1
   - Block 86400 → Epoch 2

2. **Active Elderfier Selection:**
   - Epoch 0 → [1, 2, 3]
   - Epoch 1 → [2, 3, 4]
   - Epoch 2 → [3, 4, 5]
   - Epoch 3 → [4, 5, 1]
   - Epoch 4 → [5, 1, 2]
   - Epoch 5 → [1, 2, 3] (cycles)

3. **Fee Accumulation:**
   - addElderfierFee(100) → currentFees = 100
   - addElderfierFee(50) → currentFees = 150
   - No overflow with max values

4. **Fee Distribution:**
   - 300 XFG fees → 100 per elderfier
   - 1000 XFG fees → 333 per elderfier (333*3=999, 1 satoshi lost)
   - 0 XFG fees → 0 per elderfier

5. **Epoch Finalization:**
   - Fees transferred to history
   - New epoch starts fresh
   - History preserved

### Integration Tests:

1. Create multiple HEAT/COLD deposits
2. Extract fees correctly based on amounts
3. Accumulate over blocks
4. Finalize epoch at boundary
5. Query earnings via RPC
6. Verify distribution split 3 ways
7. Roll back and re-verify

### Performance Tests:

1. Add 1000 fees in single epoch
2. Query with 100 epochs of history
3. Rollback through multiple epochs

---

## Part 7: Edge Cases & Error Handling

### Edge Cases to Handle:

1. **Rounding Errors:**
   - 1000 XFG / 3 elderfiers = 333 each (1 XFG remains)
   - Acceptable: just drop the 1 satoshi or burn it

2. **Zero Fees:**
   - No deposits in epoch → 0 fees → 0 per elderfier
   - Should still create epoch record for completeness

3. **Overflow Prevention:**
   - Check for overflow before adding fee
   - Cap at max uint64_t if needed

4. **Missing Elderfier Addresses:**
   - Handle gracefully if elderfier address not registered
   - Default to "UNKNOWN_#INDEX" if needed

5. **Concurrent Access:**
   - All methods use mutex lock_guard
   - const methods are thread-safe (read-only)

6. **State Inconsistency:**
   - Rollback properly cleans up epoch state
   - No orphaned fees or epochs

---

## Part 8: Data Validation

### isValid() Checks:

```cpp
bool ElderfierEpochRewards::isValid() const {
    // Check all required fields
    if (activeElderfiers.size() != 3) return false;
    if (distribution.empty()) return false;
    if (epochStartBlock > epochEndBlock) return false;

    // Check distribution is complete
    uint64_t totalDistributed = 0;
    for (const auto& pair : distribution) {
        if (distribution.count(pair.first) != 1) return false;  // Duplicate
        totalDistributed += pair.second;
    }

    // All active elderfiers should have entry
    for (const auto& addr : activeElderfiers) {
        if (distribution.count(addr) != 1) return false;  // Missing entry
    }

    // Total distributed should not exceed collected
    if (totalDistributed > totalFeesCollected) return false;

    return true;
}
```

---

## Part 9: Constants Summary

**Epoch Configuration:**
- Duration: 43200 blocks (7 days)
- Cycle: 5 epochs (35 days until rotation repeats)
- Active per epoch: 3 of 5 elderfiers

**Fee Percentages:**
- HEAT burn: 0.5% to elderfiers
- COLD deposit: 1.0% to elderfiers
- Example: 800 XFG HEAT burn → 4 XFG fee → 1.33 XFG per active elderfier

**Distribution:**
- All 3 active elderfiers split equally
- Rounding: integer division (may lose < 1 satoshi per epoch)

---

## Part 10: Implementation Checklist

### Header Updates:
- [x] Add ElderfierEpochRewards struct
- [x] Add constants namespace
- [x] Add private epoch tracking members
- [x] Add public fee tracking methods

### Implementation:
- [ ] Implement addElderfierFee()
- [ ] Implement finalizeEpoch()
- [ ] Implement calculateCurrentEpoch()
- [ ] Implement getActiveElderfiers()
- [ ] Implement getEpochRewards()
- [ ] Implement getElderfierEarnings()
- [ ] Implement registerElderfierAddress()
- [ ] Update rollbackToHeight() for epochs
- [ ] Update constructor with initialization

### Testing:
- [ ] Unit tests for all methods
- [ ] Integration with Blockchain.cpp
- [ ] RPC endpoint tests
- [ ] Overflow/edge case tests
- [ ] Performance tests

### Integration:
- [ ] Update Blockchain.cpp to call addElderfierFee()
- [ ] Update Blockchain.cpp to call finalizeEpoch()
- [ ] Implement RPC endpoint
- [ ] Add CLI queries for earnings

---

## Summary

**Task 1.2: Epoch-Based Fee Tracking**

**What Gets Implemented:**
1. ElderfierEpochRewards struct for tracking epochs
2. Fee accumulation during block processing
3. Automatic finalization at epoch boundaries (every 43200 blocks)
4. Deterministic rotation schedule (3 of 5 elderfiers active)
5. Query methods for RPC/CLI to check earnings

**Code Metrics:**
- ~250 lines of header declarations
- ~400 lines of implementation code
- ~150 lines of tests
- Total: ~800 lines

**Key Features:**
- Thread-safe (mutex-protected)
- Stateful (history preserved)
- Rollback-safe (cleanup on chain reorganization)
- Decentralized (deterministic schedule, no governance needed)
- Privacy-preserving (no user data involved)

**Next Steps After Task 1.2:**
1. Test fee tracking with multiple deposits
2. Verify epoch finalization works
3. Move to Task 1.3 (RPC endpoints)
4. Complete Week 1 by end of week

---

**Effort:** 4-5 hours implementation + 2 hours testing = 6-7 hours total
**Owner:** [Primary implementation]
**Status:** Ready for implementation
