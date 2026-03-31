# Phase 5: Complete Elderfier Consensus System - Implementation Roadmap

## Status: IN PROGRESS ✅

### Completed in Phase 5 So Far:
1. ✅ CommitmentIndex fee tracking infrastructure
   - Added `ElderfierEpochRewards` struct
   - Implemented epoch-based fee distribution (7-day cycles)
   - Implemented 3-of-5 elderfier rotation schedule
   - All fee tracking methods stubbed and ready for integration

### Compilation Status:
- ✅ CryptoNoteCore compiles successfully with fee tracking

---

## Remaining Phase 5 Tasks (Priority Order)

### TASK 2: Fee Extraction in Blockchain.cpp
**Purpose**: Wire fee collection during block validation
**Status**: PENDING

**Work**:
1. Find HEAT transaction processing (~line 2470)
   - Extract 0.5% fee (50 basis points) from HEAT burns
   - Call `m_commitmentIndex.addElderfierFee(heatFee)`

2. Find COLD transaction processing (~line 2489)
   - Extract 1.0% fee (100 basis points) from COLD deposits
   - Call `m_commitmentIndex.addElderfierFee(coldFee)`

3. Add epoch finalization check
   - After each block added: `if (blockHeight % EPOCH_DURATION == 0) m_commitmentIndex.finalizeEpoch(blockHeight)`

**Files**:
- `src/CryptoNoteCore/Blockchain.cpp` - Main implementation
- `src/CryptoNoteCore/Blockchain.h` - Maybe add EPOCH constant

**Tests**:
- HEAT fee extraction: 1000 XFG → 5 XFG fee
- COLD fee extraction: 1000 XFG → 10 XFG fee
- Epoch boundary handling: Fees accumulated then distributed

---

### TASK 3: ElderfierDepositData Unstaking Model
**Purpose**: Implement user-initiated unstaking with 8-day window
**Status**: PENDING

**Work**:
1. Read `include/EldernodeIndexTypes.h`
   - Find `ElderfierDepositData` struct (line 181-223)
   - Verify fields: `unstakingRequested`, `unstakingRequestBlock`, `unstakeClaimableBlock`

2. Add methods to `ElderfierDepositData`:
   ```cpp
   void initiateUnstake(uint64_t blockHeight) {
     unstakingRequested = true;
     unstakingRequestBlock = blockHeight;
     unstakeClaimableBlock = blockHeight + 19200;  // ~8 days
   }
   
   bool canClaimUnstakedFunds(uint64_t currentBlock) const {
     return unstakingRequested && currentBlock >= unstakeClaimableBlock;
   }
   ```

3. Wire into transaction handling:
   - 0xEB tag: `initiateUnstake` handler
   - 0xEE tag: `claimUnstake` handler with validation

**Files**:
- `include/EldernodeIndexTypes.h` - Add methods
- `src/CryptoNoteCore/TransactionExtra.cpp` - Parse 0xEB/0xEE tags
- `src/CryptoNoteCore/Blockchain.cpp` - Process unstaking transactions

**Tests**:
- Create deposit: `unstakingRequested = false`
- User calls initiate-unstake: `unstakingRequested = true`, record block
- Wait 19,200 blocks
- User claims: Funds returned

---

### TASK 4: ElderfierSignatureBroadcaster Class
**Purpose**: Handle P2P signature gossip and consensus threshold
**Status**: PENDING

**Work**:
1. Create new files:
   - `src/CryptoNoteCore/ElderfierSignatureBroadcaster.h`
   - `src/CryptoNoteCore/ElderfierSignatureBroadcaster.cpp`

2. Implement core class:
   ```cpp
   class ElderfierSignatureBroadcaster {
   public:
     void handleSignatureMessage(const COMMAND_ELDERFIER_SIGNATURE& msg);
     void broadcastMerkleRoot(const Crypto::Hash& root);
     uint64_t getConsensusPercentage() const;
     bool hasReachedThreshold() const;  // >= 69%
     
   private:
     core& m_core;
     NodeServer& m_p2p;
   };
   ```

3. Wire into daemon initialization:
   - Create broadcaster instance
   - Subscribe to P2P messages
   - Start background consensus monitoring

**Files**:
- `src/CryptoNoteCore/ElderfierSignatureBroadcaster.h` (NEW)
- `src/CryptoNoteCore/ElderfierSignatureBroadcaster.cpp` (NEW)
- `src/Daemon/Daemon.cpp` - Wire into initialization
- `src/P2p/NetNode.cpp` - Register message handler

**Tests**:
- Receive signature messages
- Update consensus percentage
- Trigger at 69% threshold

---

### TASK 5: RPC Endpoints for Fee Queries
**Purpose**: Query elderfier earnings and epoch history via RPC
**Status**: PENDING

**Work**:
1. Add RPC command definitions:
   ```cpp
   struct COMMAND_RPC_GET_ELDERFIER_EPOCH_HISTORY {
     struct request { uint64_t startEpoch; uint64_t endEpoch; };
     struct response { 
       std::vector<ElderfierEpochRewards> epochs;
       std::string status;
     };
   };
   ```

2. Add RPC handlers in RpcServer:
   - `on_get_elderfier_earnings()` - Query earnings for EFiD in epoch
   - `on_get_elderfier_epoch_history()` - Get epoch rewards
   - `on_get_current_epoch()` - Get current epoch number
   - `on_get_active_elderfiers()` - Get 3 active elderfiers for epoch

3. Register handlers in s_handlers map

**Files**:
- `src/Rpc/CoreRpcServerCommandsDefinitions.h` - Add RPC struct
- `src/Rpc/RpcServer.h` - Add handler declarations
- `src/Rpc/RpcServer.cpp` - Add handler implementations + registration

**Tests**:
- RPC: get_elderfier_earnings {epoch: 1, elderfier_id: 5}
- RPC: get_elderfier_epoch_history {startEpoch: 0, endEpoch: 4}
- RPC: get_current_epoch() → {epoch: 5}

---

### TASK 6: Unit Tests
**Purpose**: Verify fee distribution, epoch calculation, rotation schedule
**Status**: PENDING

**Test Categories**:

1. **Fee Extraction Tests**:
   - Extract 0.5% from 1000 XFG HEAT → 5 XFG
   - Extract 1.0% from 1000 XFG COLD → 10 XFG
   - Rounding: 300 XFG → 100 per elderfier

2. **Epoch Calculation Tests**:
   - Block 0-43199 → Epoch 0
   - Block 43200-86399 → Epoch 1
   - Block 86400-129599 → Epoch 2

3. **Rotation Schedule Tests**:
   - Epoch 0 → [0, 1, 2]
   - Epoch 1 → [1, 2, 3]
   - Epoch 4 → [4, 0, 1]
   - Epoch 5 → [0, 1, 2] (repeats)

4. **Distribution Tests**:
   - 300 XFG fees → 100 per elderfier
   - 1000 XFG fees → 333 per elderfier (with 1 satoshi remainder handling)
   - 0 XFG fees → 0 distribution

5. **Consensus Tests**:
   - 3/5 signed → 60%
   - 4/5 signed → 80%
   - 5/5 signed → 100%

6. **Unstaking Tests**:
   - Create deposit: `unstakingRequested = false`
   - Initiate: Sets `unstakingRequested = true`
   - Can claim after 19200 blocks

**Files**:
- `tests/CommitmentIndexTests/FeeDistributionTests.cpp` (NEW)
- `tests/CommitmentIndexTests/EpochCalculationTests.cpp` (NEW)
- `tests/CommitmentIndexTests/UnstakingTests.cpp` (NEW)

---

### TASK 7: Integration Testing
**Purpose**: Full flow testing with multiple components
**Status**: PENDING

**Test Scenarios**:

1. **Multiple Deposits**:
   - Create 3 HEAT deposits of 100 XFG each → 1.5 XFG fees total
   - Create 2 COLD deposits of 200 XFG each → 4 XFG fees total
   - Total epoch fees: 5.5 XFG
   - Distribute: 1.83 XFG per elderfier (with rounding)

2. **Epoch Boundary**:
   - Add blocks up to epoch 1 boundary (43200)
   - Call finalizeEpoch
   - Verify epoch history recorded
   - Check next epoch starts fresh with 0 fees

3. **Rollback Handling**:
   - Create deposits
   - Finalize epoch 0
   - Simulate rollback to block 21600 (middle of epoch 0)
   - Verify epoch history rolled back

4. **RPC Queries**:
   - Query epoch history via RPC
   - Query elderfier earnings
   - Query current epoch
   - Verify all values match internal state

**Files**:
- `tests/CommitmentIndexTests/IntegrationTests.cpp` (NEW)

---

## Implementation Order (Recommended)

1. ✅ **Task 1**: CommitmentIndex fee tracking (DONE)
2. ⏳ **Task 2**: Fee extraction in Blockchain.cpp (2 hours)
3. ⏳ **Task 3**: Unstaking model implementation (2 hours)
4. ⏳ **Task 4**: ElderfierSignatureBroadcaster class (3 hours)
5. ⏳ **Task 5**: RPC endpoints (1.5 hours)
6. ⏳ **Task 6**: Unit tests (4 hours)
7. ⏳ **Task 7**: Integration testing (2 hours)

**Total Estimated Phase 5 Time**: ~14.5 hours

---

## Key Integration Points

### Blockchain.cpp Integration:
```cpp
// In transaction processing loop:
if (HEAT_FEE_FLAG_SET) {
  uint64_t heat_fee = (transaction_amount * 5) / 1000;  // 0.5%
  m_commitmentIndex.addElderfierFee(heat_fee);
}

if (COLD_FEE_FLAG_SET) {
  uint64_t cold_fee = (deposit_amount * 10) / 1000;  // 1.0%
  m_commitmentIndex.addElderfierFee(cold_fee);
}

// After adding each block:
m_commitmentIndex.finalizeEpoch(blockHeight);
```

### RPC Integration:
```cpp
// In RpcServer::on_get_elderfier_earnings():
auto earnings = m_core.getBlockchain().getElderfierEarnings(elderfier_id, epoch);
res.earned_xfg = earnings;

// In RpcServer::on_get_epoch_history():
auto history = m_core.getBlockchain().getEpochHistory(start, end);
for (const auto& epoch : history) {
  res.epochs.push_back(epoch);
}
```

### Daemon Integration:
```cpp
// In Daemon.cpp main():
auto broadcaster = std::make_unique<ElderfierSignatureBroadcaster>(ccore, p2psrv);
broadcaster->start();
// Store as daemon member to keep alive
```

---

## Testing Verification Checklist

- [ ] Fee extraction calculates correctly
- [ ] Epoch boundaries trigger finalization
- [ ] Rotation schedule produces correct active set
- [ ] Distribution splits fees evenly with remainder handling
- [ ] Consensus percentage calculates accurately
- [ ] Unstaking windows work correctly
- [ ] RPC queries return expected values
- [ ] Rollback cleans up epoch state
- [ ] Concurrent access is thread-safe
- [ ] Edge cases handled (0 fees, < 3 elderfiers, etc.)

---

## Notes for Next Developer

1. **Mutex Usage**: All CommitmentIndex methods use `std::lock_guard<std::mutex>` for thread safety. Maintain this pattern.

2. **Epoch Boundaries**: Always check for epoch boundaries after block processing. The finalization must happen exactly at multiples of 43200 blocks.

3. **Rounding**: When distributing fees to 3 elderfiers, distribute remainder of 1-2 satoshi to first elderfiers in list. This is acceptable as total loss is <3 satoshi per epoch.

4. **Privacy**: EFiD (0-255) is used throughout, never wallet addresses in RPC responses.

5. **Constants**: 
   - EPOCH_DURATION_BLOCKS = 43200 (7 days)
   - ROTATION_CYCLE_EPOCHS = 5 (35-day full cycle)
   - ACTIVE_ELDERFIERS_PER_EPOCH = 3

---

## Success Criteria

Phase 5 is complete when:
- ✅ All 7 tasks implemented and passing tests
- ✅ CryptoNoteCore/Rpc/P2P/Daemon all compile without error
- ✅ Fee tracking works end-to-end with multiple deposits
- ✅ Epoch boundaries trigger proper finalization and distribution
- ✅ RPC endpoints return accurate historical data
- ✅ Elderfier rotation schedule is deterministic and correct
- ✅ Unstaking model allows 8-day windows
- ✅ Unit test coverage > 80% of fee logic
- ✅ No regressions from Phase 4 implementation
- ✅ Testnet-ready for multi-node testing

