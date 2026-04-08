# LP Swap Pool ZK Prover System — Design Spec
**Date:** 2026-04-08  
**Branch:** xfgCdswaps  
**Status:** Approved for implementation

---

## 1. Problem Statement

The Fuego LP swap pool (PoolAMM) is ~15% complete. The AMM math exists (`PoolAMM.cpp`), fee accounting exists, and the SP1 prover runs end-to-end for HEAT burns. What's missing:

- LP-specific SP1 Rust circuit (`fuego-lp-circuit`)
- `TransactionOutputLP` consensus type + blockchain validation
- Pedersen commitment privacy for LP pool state
- Proof verification on-chain
- Prover daemon (operator → permissionless upgrade path)
- Prover incentive mechanism tied to fee pool

---

## 2. Fee Flow (Finalized)

**1% atomic swap fee on XFG side** splits per epoch:

| Destination | Share | Purpose |
|-------------|-------|---------|
| CD interest pool | **80%** | Paid out to active CD holders |
| Prover reward pool | **10%** | Winner-takes-all per valid LP proof |
| Chain treasury | **10%** | Reserve, governance, future use |

**0.3% LP trade fee** goes directly to LP providers — separate system, does not touch CD/prover pools.

**Banking fee** (`amount * activeEfierCount / 1000`): currently dead (EFiers=0). Reserved for potential future use; not part of prover incentive.

---

## 3. Privacy Model — Pedersen Commitments

Operator must not see LP pool activity as plaintext. All LP positions and trade flows are committed using Pedersen commitments from day one.

**Commitment form:** `C = r·G + v·H` where `v` is the value (XFG amount), `r` is the blinding factor known only to the position holder.

**What this hides:**
- Individual LP deposit/withdrawal amounts
- Trade sizes passing through the pool
- LP provider share of fee earnings

**What the operator proves (without seeing plaintext):**
- AMM invariant holds after each trade: `x * y = k` (proven in-circuit over committed values)
- Fee was correctly computed: `fee = inputAmount * 30 / 10000` (proven in-circuit)
- LP share accounting is consistent (delta commitments balance)

**Blinding factor management:**
- Generated client-side by wallet
- Stored encrypted in wallet file
- Never transmitted to prover/operator

---

## 4. Architecture

```
┌─────────────────────────────────────────────────────┐
│                  WALLET / USER                       │
│  Generate blinding factors → build LP tx →          │
│  submit to daemon (committed amounts only)          │
└────────────────┬────────────────────────────────────┘
                 │ TransactionOutputLP (committed)
                 ▼
┌─────────────────────────────────────────────────────┐
│              FUEGOD (daemon)                         │
│  Validates commitment structure                      │
│  Queues LP tx → notifies prover daemon               │
│  Verifies proof when submitted                       │
│  Distributes prover_pool to winner each epoch        │
└────────┬──────────────────────────┬─────────────────┘
         │ LP state root            │ Verified proof
         ▼                          ▼
┌─────────────────┐      ┌──────────────────────────┐
│  PROVER DAEMON  │      │     ON-CHAIN VERIFIER     │
│  (Phase 1: op)  │      │  Checks SP1 proof against │
│  Fetches state  │      │  committed state root     │
│  Runs LP circuit│      │  If valid → reward prover │
│  Submits proof  │      └──────────────────────────┘
└─────────────────┘
         │ Phase 2: permissionless
         ▼
  Any node can run prover daemon,
  race for first valid proof, claim reward
```

---

## 5. SP1 Circuit — `fuego-lp-circuit`

New Rust crate under `fuego-prover/fuego-lp-circuit/` mirroring the existing `fuego-circuit` (HEAT burns).

**Inputs (private):**
- Pre-trade LP state: `(committed_x, committed_y, k)` with blinding factors
- Trade input: committed amount + blinding factor
- LP provider positions: vector of `(commitment, blinding, share_bps)`

**Inputs (public):**
- Pre-trade state root (Pedersen hash of state)
- Post-trade state root
- Fee amount commitment
- Prover address (for reward routing)

**Proof outputs:**
- AMM invariant holds: `x' * y' >= k` (with rounding tolerance)
- Fee correctly computed: `committed_fee = committed_input * 30 / 10000`
- State root transition is valid
- No negative balances

**Circuit structure:**
```rust
// fuego-lp-circuit/src/main.rs
sp1_zkvm::entrypoint!(main);

fn main() {
    let input: LpProofInput = sp1_zkvm::io::read();
    
    // 1. Verify Pedersen commitment openings
    verify_pedersen_opening(&input.pre_state_x, &input.blinding_x);
    verify_pedersen_opening(&input.pre_state_y, &input.blinding_y);
    
    // 2. Verify AMM invariant post-trade
    let k_post = input.x_post * input.y_post;
    assert!(k_post >= input.k_pre, "AMM invariant violated");
    
    // 3. Verify fee computation
    let expected_fee = (input.trade_input * 30) / 10000;
    assert_eq!(input.fee_amount, expected_fee, "Fee mismatch");
    
    // 4. Commit outputs
    sp1_zkvm::io::commit(&input.post_state_root);
    sp1_zkvm::io::commit(&input.fee_commitment);
    sp1_zkvm::io::commit(&input.prover_address);
}
```

---

## 6. Consensus Type — `TransactionOutputLP`

New output type in `include/CryptoNote.h` alongside existing commitment types:

```cpp
struct TransactionOutputLP {
  uint64_t poolId;              // Which LP pool (future: multi-pool)
  Crypto::EllipticCurvePoint commitment;  // Pedersen commitment to LP amount
  Crypto::Hash stateRootBefore; // AMM state root before this tx
  Crypto::Hash stateRootAfter;  // AMM state root after this tx (claimed)
  Crypto::Hash proofHash;       // Hash of SP1 proof (proof stored off-chain)
  uint64_t feeCommitment;       // Committed fee amount (for prover pool)
};
```

Validation in `Blockchain.cpp`:
- `stateRootBefore` must match current pool state root
- `proofHash` must reference a valid submitted proof
- `feeCommitment` must equal 0.3% of trade volume (verified in proof)
- Sequential state roots (no forks in pool state)

---

## 7. On-Chain Proof Verification

New `ProofRegistry` in `CryptoNoteCore/`:

```cpp
class ProofRegistry {
public:
  bool submitProof(const Crypto::Hash& stateRoot, 
                   const std::vector<uint8_t>& sp1Proof,
                   const AccountPublicAddress& proverAddr);
  bool isProofValid(const Crypto::Hash& proofHash) const;
  AccountPublicAddress getProver(const Crypto::Hash& proofHash) const;
};
```

Verification uses SP1's on-chain verifier (Groth16 wrapper). First valid proof for a state root wins. Subsequent duplicate proofs for same root are rejected.

---

## 8. Prover Daemon — Phase 1 (Operator)

New binary `fuego-prover-daemon` (Rust, in `fuego-prover/`):

```
fuego-prover/
  fuego-circuit/          ← existing (HEAT burns)
  fuego-lp-circuit/       ← NEW (LP proofs)
  prover-daemon/          ← NEW (daemon binary)
    src/
      main.rs             ← watch daemon
      lp_prover.rs        ← fetch state, run circuit, submit proof
      reward_claimer.rs   ← claim epoch prover_pool reward
```

**Daemon loop:**
1. Poll `fuegod` for new LP transactions needing proofs
2. Fetch committed state from daemon
3. Run SP1 prover with `fuego-lp-circuit`
4. Submit proof to `fuegod`
5. At epoch boundary: submit reward claim tx to receive `prover_pool` share

**Phase 1:** Single operator runs this. Address is hardcoded in genesis or configurable via `fuegod.conf`.

---

## 9. Prover Incentive — Epoch-Based Reward

**Per-epoch prover_pool accumulation:**
```
prover_pool_epoch += swap_fee_total * 0.10
```

At epoch boundary (`height % EPOCH_DURATION_BLOCKS == 0`), `Blockchain.cpp` distributes:
- Count valid proofs submitted this epoch by each prover address
- Winner = prover who submitted the most valid proofs (or first, if tie)
- Transfer `prover_pool_epoch` to winner's address via `TransactionOutputProverReward`

**Phase 2 (permissionless):** Any node running `fuego-prover-daemon` races to submit first valid proof per state root. Winner-takes-all per state root transition. Epoch payout distributes based on proof count.

---

## 10. Economic Viability (Monte Carlo — 5,000 iterations)

| Scenario | XFG Price | Provers | CD APY | Prover $/proof | Viable |
|----------|-----------|---------|--------|----------------|--------|
| Bootstrapping | $0.05 | 1 | 0.60% | $0.03 | ✗ needs subsidy |
| Early Growth | $0.20 | 5 | 0.60% | $0.20 | ✓ breakeven |
| Mature | $1.00 | 15 | 1.49% | $3.33 | ✓ profitable |
| Peak | $5.00 | 40 | 2.37% | $25.12 | ✓ strong |

**Bootstrapping subsidy:** Treasury pays fixed 5 XFG/proof while prover_pool < breakeven. Condition checked in `Blockchain.cpp` at epoch boundary.

Prover cost assumption: $0.20/proof (SP1 cloud compute). CD APY and prover rewards do not compete — they scale together with swap volume.

---

## 11. Implementation Phases

### Phase A — Privacy Foundation
- [ ] Add Pedersen commitment helpers to `CryptoNoteCore/CommitmentUtils.h`
- [ ] Add `TransactionOutputLP` to `include/CryptoNote.h`
- [ ] Add serialization tag (next available after existing types)
- [ ] Blockchain validation stub (accepts but doesn't verify proof yet)

### Phase B — SP1 Circuit
- [ ] Create `fuego-prover/fuego-lp-circuit/` crate
- [ ] Implement AMM invariant check in-circuit (over Pedersen-committed values)
- [ ] Implement fee verification in-circuit
- [ ] Unit tests with known good/bad state transitions

### Phase C — Proof Verification On-Chain
- [ ] Add `ProofRegistry` to `CryptoNoteCore/`
- [ ] Wire SP1 Groth16 verifier into `ProofRegistry::submitProof()`
- [ ] Update `TransactionOutputLP` validation to require valid proof

### Phase D — Prover Daemon
- [ ] Create `fuego-prover/prover-daemon/` Rust binary
- [ ] Implement state polling from `fuegod` RPC
- [ ] Implement proof generation + submission loop
- [ ] Implement epoch reward claim

### Phase E — Fee Pool Wiring
- [ ] Update `Blockchain::computeEpochFeeDistribution()` with 80/10/10 split
- [ ] Add `prover_pool` accumulator to epoch state
- [ ] Add epoch boundary payout logic
- [ ] Add bootstrapping subsidy condition

### Phase F — Permissionless Upgrade
- [ ] Remove hardcoded operator address
- [ ] Add prover registration (submit address + bond)
- [ ] Update winner-selection to count-based across all registered provers
- [ ] Add RPC endpoints: `register_prover`, `list_provers`, `get_prover_stats`

---

## 12. Files Changed / Created

| File | Action | Purpose |
|------|--------|---------|
| `include/CryptoNote.h` | modify | Add `TransactionOutputLP`, `TransactionOutputProverReward` |
| `src/CryptoNoteCore/CommitmentUtils.h` | create | Pedersen commitment helpers |
| `src/CryptoNoteCore/ProofRegistry.h/.cpp` | create | Proof submission + verification |
| `src/CryptoNoteCore/Blockchain.cpp` | modify | 80/10/10 split, prover payout, LP tx validation |
| `src/Rpc/CoreRpcServerCommandsDefinitions.h` | modify | Add LP proof submission RPC structs |
| `src/Rpc/RpcServer.cpp` | modify | Add `/submit_lp_proof`, `/get_pool_state` routes |
| `fuego-prover/fuego-lp-circuit/` | create | SP1 Rust crate for LP proofs |
| `fuego-prover/prover-daemon/` | create | Rust prover daemon binary |

---

## 13. Out of Scope

- Multi-pool support (poolId field reserved but single pool for now)
- LP position NFT / transferability (separate feature)  
- Governance voting on treasury spend
- Cross-chain LP (future)
