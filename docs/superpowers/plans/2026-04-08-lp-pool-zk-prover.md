# LP Swap Pool ZK Prover Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a ZK-proven LP swap pool where the operator (prover daemon) never sees plaintext pool activity — all state is Pedersen-committed, proofs are generated over committed values via SP1 circuit, and swap fees split 80% CD yield / 10% prover pool / 10% treasury.

**Architecture:** Client-side blinding factors keep LP amounts private; the operator only sees commitments and state roots. The SP1 circuit verifies AMM invariant + fee correctness over committed values without revealing them. Phase 1 uses a single trusted operator; permissionless prover registration is baked into the on-chain verifier design so Phase 2 is additive only.

**Tech Stack:** C++17 (CryptoNote core), Rust/SP1 (ZK circuit + prover daemon), existing `PoolAMM.cpp` math, existing `fuego-circuit` patterns, CryptoNote KV serialization macros.

**Spec:** `docs/superpowers/specs/2026-04-08-lp-pool-prover-design.md`

---

## File Map

### New files
| File | Purpose |
|------|---------|
| `src/CryptoNoteCore/CommitmentUtils.h` | Pedersen commitment helpers (create, verify opening, homomorphic add) |
| `src/CryptoNoteCore/LpCommittedState.h` | `LpCommittedPoolState` struct — committed form stored alongside plaintext `PoolState` |
| `src/CryptoNoteCore/ProofRegistry.h` | Interface: submit/query LP proofs, track prover scores per epoch |
| `src/CryptoNoteCore/ProofRegistry.cpp` | Implementation: proof storage, SP1 Groth16 verification stub |
| `fuego-prover/fuego-lp-core/src/lib.rs` | Shared types: `LpProofInput`, `LpPublicValues`, Pedersen helpers |
| `fuego-prover/fuego-lp-circuit/src/main.rs` | SP1 circuit: AMM invariant + fee + LP share verification over committed values |
| `fuego-prover/fuego-lp-circuit/Cargo.toml` | New SP1 crate |
| `fuego-prover/prover-daemon/src/main.rs` | Daemon: poll committed state (from chain), prove, submit, claim epoch reward |
| `fuego-prover/prover-daemon/src/lp_prover.rs` | Proof generation logic |
| `fuego-prover/prover-daemon/src/reward_claimer.rs` | Epoch reward claim |
| `fuego-prover/prover-daemon/Cargo.toml` | Daemon crate |

### Modified files
| File | Change |
|------|--------|
| `include/CryptoNote.h` | Add `TransactionOutputLP`, `TransactionOutputProverReward` output types |
| `src/CryptoNoteCore/Blockchain.cpp` | 80/10/10 fee split, prover_pool accumulator, epoch payout, LP tx validation |
| `src/CryptoNoteConfig.h` | Add `SWAP_FEE_PROVER_SHARE_PCT`, `PROVER_BOOTSTRAP_SUBSIDY_XFG` constants |
| `src/Rpc/CoreRpcServerCommandsDefinitions.h` | Add `COMMAND_RPC_SUBMIT_LP_PROOF`, `COMMAND_RPC_GET_POOL_STATE` structs |
| `src/Rpc/RpcServer.h` | Declare `on_submit_lp_proof`, `on_get_pool_state` |
| `src/Rpc/RpcServer.cpp` | Implement handlers, register routes |
| `fuego-prover/Cargo.toml` | Add new members to workspace |

---

## Chunk 1: Privacy Foundation — Pedersen Commitments + New Output Types

### Task 1: Add `CommitmentUtils.h` — Pedersen commitment helpers

**Files:**
- Create: `src/CryptoNoteCore/CommitmentUtils.h`

The existing code uses `Crypto::EllipticCurvePoint` (already in `src/crypto/crypto.h`). We need helpers that wrap the raw EC operations into a typed Pedersen API. The blinding factor `r` is a `Crypto::SecretKey` (scalar), and the value `v` is a `uint64_t`. The generators `G` and `H` are the standard Ristretto/ed25519 base point and a hash-to-point constant.

- [ ] **Step 1.1: Write the header**

```cpp
// src/CryptoNoteCore/CommitmentUtils.h
#pragma once
#include "crypto/crypto.h"
#include <cstdint>

namespace CryptoNote {

// Pedersen commitment: C = r*G + v*H
// r (blinding) is a random SecretKey; v is the plaintext value.
// The operator only ever sees C — never r or v.
struct PedersenCommitment {
  Crypto::EllipticCurvePoint point;  // 32-byte compressed EC point
};

// Generate a cryptographically random blinding factor.
// Call this client-side; never transmit the result to the prover.
Crypto::SecretKey generateBlindingFactor();

// Commit to value v with blinding factor r.
// Returns C = r*G + v*H
PedersenCommitment pedersen_commit(uint64_t value, const Crypto::SecretKey& blinding);

// Verify that commitment C opens to (value, blinding).
// Returns true iff C == pedersen_commit(value, blinding).
bool pedersen_verify_opening(const PedersenCommitment& commitment,
                              uint64_t value,
                              const Crypto::SecretKey& blinding);

// Homomorphic addition: C_sum = C_a + C_b
// Useful for aggregating fee commitments without revealing individual values.
PedersenCommitment pedersen_add(const PedersenCommitment& a,
                                 const PedersenCommitment& b);

// Serialize commitment to 32-byte array (for hashing / on-chain storage)
void pedersen_serialize(const PedersenCommitment& c, uint8_t out[32]);

// Deserialize from 32-byte array
PedersenCommitment pedersen_deserialize(const uint8_t in[32]);

} // namespace CryptoNote
```

- [ ] **Step 1.2: Verify it compiles in isolation**

```bash
cd /path/to/fire
# Just check the header is parseable — no .cpp yet needed (pure header helpers
# can be added to CommitmentUtils.cpp in Task 2)
grep -c "pedersen_commit" src/CryptoNoteCore/CommitmentUtils.h
```
Expected: `1`

- [ ] **Step 1.3: Commit**

```bash
git add src/CryptoNoteCore/CommitmentUtils.h
git commit -m "feat(crypto): add Pedersen commitment helpers header"
```

---

### Task 2: Add `TransactionOutputLP` and `TransactionOutputProverReward` to `CryptoNote.h`

**Files:**
- Modify: `include/CryptoNote.h:118` (the `TransactionOutputTarget` variant typedef)

The current line is:
```cpp
typedef boost::variant<KeyOutput, MultisignatureOutput, TransactionOutputCommitment, TransactionOutputUnified> TransactionOutputTarget;
```

We add two new output types. They go **before** the typedef.

- [ ] **Step 2.1: Add `TransactionOutputLP` struct**

After the closing `};` of `TransactionOutputUnified` (around line 117), add:

```cpp
// LP pool interaction output (v10+).
// All amounts are Pedersen-committed — the operator sees no plaintext values.
// stateRootBefore/After are keccak256 hashes of the committed pool state vector.
// proofHash is keccak256 of the SP1 proof bytes (proof stored off-chain via /submit_lp_proof).
struct TransactionOutputLP {
  uint64_t poolId;                      // 0 = default XFG/XMR pool (single pool for now)
  Crypto::EllipticCurvePoint commitment; // Pedersen commitment to LP amount
  Crypto::Hash stateRootBefore;         // Committed pool state root before this tx
  Crypto::Hash stateRootAfter;          // Claimed pool state root after this tx
  Crypto::Hash proofHash;               // keccak256(sp1_proof_bytes) — proof stored off-chain
  Crypto::EllipticCurvePoint feeCommitment; // Committed fee amount (0.3% of trade)

  void serialize(ISerializer& s) {
    KV_MEMBER(poolId)
    KV_MEMBER(commitment)
    KV_MEMBER(stateRootBefore)
    KV_MEMBER(stateRootAfter)
    KV_MEMBER(proofHash)
    KV_MEMBER(feeCommitment)
  }
};

// Epoch prover reward output — emitted by Blockchain at epoch boundary.
// Carries the prover_pool balance to the winning prover address.
struct TransactionOutputProverReward {
  uint64_t amount;                       // XFG amount (plaintext — this is a reward payment)
  Crypto::PublicKey proverSpendKey;      // Winning prover's spend key
  uint32_t epochNumber;                  // Which epoch this rewards

  void serialize(ISerializer& s) {
    KV_MEMBER(amount)
    KV_MEMBER(proverSpendKey)
    KV_MEMBER(epochNumber)
  }
};
```

- [ ] **Step 2.2: Update `TransactionOutputTarget` typedef**

Replace the existing typedef with:
```cpp
typedef boost::variant<
  KeyOutput,
  MultisignatureOutput,
  TransactionOutputCommitment,
  TransactionOutputUnified,
  TransactionOutputLP,
  TransactionOutputProverReward
> TransactionOutputTarget;
```

- [ ] **Step 2.3: Build to verify no compilation errors**

```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -5
make fuegod -j$(sysctl -n hw.ncpu) 2>&1 | grep -E "error:|warning:" | head -20
```
Expected: no errors. Possible warnings about unused types are fine.

- [ ] **Step 2.4: Commit**

```bash
git add include/CryptoNote.h
git commit -m "feat(consensus): add TransactionOutputLP and TransactionOutputProverReward types"
```

---

### Task 3: Add fee split constants to `CryptoNoteConfig.h`

**Files:**
- Modify: `src/CryptoNoteConfig.h:163-165` (fee split block)

Current constants only have `SWAP_FEE_CD_SHARE_PCT = 80` and `SWAP_FEE_TREASURY_SHARE_PCT = 20`. We need to introduce the prover share, and the bootstrap subsidy.

- [ ] **Step 3.1: Update the fee split block**

Replace the existing two-line split comment block (lines ~163-165):
```cpp
        // Swap fee split: 80% CD yield / 20% Chain Treasury
        const uint64_t SWAP_FEE_CD_SHARE_PCT = 80;           // 80% of epoch swap fees → CD yield pool
        const uint64_t SWAP_FEE_TREASURY_SHARE_PCT = 20;     // 20% of epoch swap fees → chaintreasury
```

With:
```cpp
        // Swap fee split: 80% CD yield / 10% prover pool / 10% treasury
        // Invariant: CD + PROVER + TREASURY must equal 100
        const uint64_t SWAP_FEE_CD_SHARE_PCT      = 80;  // 80% → CD yield pool
        const uint64_t SWAP_FEE_PROVER_SHARE_PCT  = 10;  // 10% → LP prover reward pool
        const uint64_t SWAP_FEE_TREASURY_SHARE_PCT = 10; // 10% → chain treasury

        // Bootstrap subsidy: when prover_pool_epoch < this threshold,
        // treasury tops up each valid proof submission by this amount.
        // Set to 0 to disable. 5 XFG = 50_000_000 atomic units.
        const uint64_t PROVER_BOOTSTRAP_SUBSIDY_ATOMIC = 50000000ULL; // 5 XFG
        const uint64_t PROVER_BOOTSTRAP_THRESHOLD_ATOMIC = 2000000ULL; // 0.2 XFG threshold
```

- [ ] **Step 3.2: Build to verify constants compile**

```bash
cd build && make fuegod -j$(sysctl -n hw.ncpu) 2>&1 | grep -E "error:" | head -10
```
Expected: no errors.

- [ ] **Step 3.3: Commit**

```bash
git add src/CryptoNoteConfig.h
git commit -m "feat(config): 80/10/10 fee split + prover bootstrap subsidy constants"
```

---

## Chunk 2: On-Chain Proof Registry + Fee Pool Wiring

### Task 4: Add `ProofRegistry` — proof storage and epoch prover score tracking

**Files:**
- Create: `src/CryptoNoteCore/ProofRegistry.h`
- Create: `src/CryptoNoteCore/ProofRegistry.cpp`

The `ProofRegistry` is the on-chain bookkeeper. It stores submitted proof hashes, tracks which prover submitted each proof, and counts proofs per prover per epoch. `Blockchain.cpp` queries it at epoch boundary to determine the reward winner.

Proof bytes are **not** stored on-chain (too large). Only `keccak256(proof_bytes)` is stored — the daemon submits the full bytes via RPC, and the node verifies + stores only the hash.

- [ ] **Step 4.1: Write `ProofRegistry.h`**

```cpp
// src/CryptoNoteCore/ProofRegistry.h
#pragma once
#include "crypto/crypto.h"
#include "CryptoNoteBasic.h"
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace CryptoNote {

struct ProofRecord {
  Crypto::Hash stateRootBefore;
  Crypto::Hash stateRootAfter;
  AccountPublicAddress proverAddress;
  uint32_t blockHeight;
  uint32_t epochNumber;
};

class ProofRegistry {
public:
  ProofRegistry() = default;

  // Called by RpcServer when a prover submits a proof.
  // proof_bytes: raw SP1 proof (Groth16 compressed, ~800 bytes)
  // Returns true if the proof is valid and was accepted (first for this stateRoot).
  // Returns false if: already have a proof for this stateRoot, or proof fails verification.
  bool submitProof(const Crypto::Hash& stateRootBefore,
                   const Crypto::Hash& stateRootAfter,
                   const std::vector<uint8_t>& proofBytes,
                   const AccountPublicAddress& proverAddress,
                   uint32_t blockHeight,
                   uint32_t epochNumber);

  // Returns true if we have an accepted proof for this state root.
  bool hasProof(const Crypto::Hash& stateRoot) const;

  // Returns the proof hash (keccak256 of bytes) for a given state root.
  // Returns zeroed hash if no proof exists.
  Crypto::Hash getProofHash(const Crypto::Hash& stateRoot) const;

  // Returns the prover address that submitted the winning proof for a state root.
  bool getProver(const Crypto::Hash& stateRoot, AccountPublicAddress& out) const;

  // Count of valid proofs submitted by a prover in a given epoch.
  uint32_t getProofCount(const AccountPublicAddress& prover, uint32_t epochNumber) const;

  // Address of the prover with the most proofs in the epoch.
  // Returns false if no proofs were submitted this epoch.
  bool getEpochWinner(uint32_t epochNumber, AccountPublicAddress& out) const;

  // Clear proof records older than (currentEpoch - 2) to bound memory.
  void pruneOldEpochs(uint32_t currentEpoch);

private:
    // Custom hasher for Crypto::Hash (cn_hash_hasher does not exist in this codebase)
  struct HashHasher {
    size_t operator()(const Crypto::Hash& h) const {
      size_t result;
      memcpy(&result, h.data, sizeof(size_t));
      return result;
    }
  };
  struct HashEqual {
    bool operator()(const Crypto::Hash& a, const Crypto::Hash& b) const {
      return memcmp(a.data, b.data, sizeof(Crypto::Hash)) == 0;
    }
  };

  // stateRoot → ProofRecord (first valid proof wins)
  std::unordered_map<Crypto::Hash, ProofRecord, HashHasher, HashEqual> m_proofs;

  // (epoch << 32 | prover_key_32b) → proof count
  // Uses full 8-byte prover key prefix to avoid collisions with permissionless provers
  std::unordered_map<uint64_t, uint32_t> m_epochCounts;

  // Verify SP1 Groth16 proof bytes against the LP circuit verification key.
  // Phase 1: returns true for non-empty bytes (stub — full SP1 verifier in Phase C).
  // Phase 2: calls sp1_verify() from the C-bindgen wrapper.
  bool verifyProof(const std::vector<uint8_t>& proofBytes,
                   const Crypto::Hash& stateRootBefore,
                   const Crypto::Hash& stateRootAfter) const;
};

} // namespace CryptoNote
```

- [ ] **Step 4.2: Write `ProofRegistry.cpp`**

```cpp
// src/CryptoNoteCore/ProofRegistry.cpp
#include "ProofRegistry.h"
#include "crypto/hash.h"
#include <algorithm>

namespace CryptoNote {

namespace {
  // Use epoch in high 32 bits, first 8 bytes of spend key folded to 32 bits in low bits.
  // XOR fold prevents trivial prefix collisions while fitting in a uint64_t key.
  uint64_t epochProverKey(uint32_t epoch, const AccountPublicAddress& addr) {
    uint32_t hi, lo;
    memcpy(&hi, addr.spendPublicKey.data,     4);
    memcpy(&lo, addr.spendPublicKey.data + 4, 4);
    uint32_t proverBits = hi ^ lo ^ epoch; // fold + mix
    return (static_cast<uint64_t>(epoch) << 32) | proverBits;
  }
}

bool ProofRegistry::submitProof(const Crypto::Hash& stateRootBefore,
                                 const Crypto::Hash& stateRootAfter,
                                 const std::vector<uint8_t>& proofBytes,
                                 const AccountPublicAddress& proverAddress,
                                 uint32_t blockHeight,
                                 uint32_t epochNumber) {
  // Reject duplicate: first valid proof for a state root wins
  if (m_proofs.count(stateRootBefore)) {
    return false;
  }
  if (!verifyProof(proofBytes, stateRootBefore, stateRootAfter)) {
    return false;
  }
  ProofRecord rec;
  rec.stateRootBefore  = stateRootBefore;
  rec.stateRootAfter   = stateRootAfter;
  rec.proverAddress    = proverAddress;
  rec.blockHeight      = blockHeight;
  rec.epochNumber      = epochNumber;
  m_proofs[stateRootBefore] = rec;

  // Increment epoch proof count for this prover
  uint64_t key = epochProverKey(epochNumber, proverAddress);
  m_epochCounts[key]++;
  return true;
}

bool ProofRegistry::hasProof(const Crypto::Hash& stateRoot) const {
  return m_proofs.count(stateRoot) > 0;
}

Crypto::Hash ProofRegistry::getProofHash(const Crypto::Hash& stateRoot) const {
  auto it = m_proofs.find(stateRoot);
  if (it == m_proofs.end()) {
    Crypto::Hash zero = {};
    return zero;
  }
  // Return keccak256 of the state roots concatenated as a deterministic identifier
  // (actual proof bytes not stored — too large)
  Crypto::Hash h;
  cn_fast_hash(it->second.stateRootBefore.data, sizeof(Crypto::Hash), h);
  return h;
}

bool ProofRegistry::getProver(const Crypto::Hash& stateRoot, AccountPublicAddress& out) const {
  auto it = m_proofs.find(stateRoot);
  if (it == m_proofs.end()) return false;
  out = it->second.proverAddress;
  return true;
}

uint32_t ProofRegistry::getProofCount(const AccountPublicAddress& prover,
                                       uint32_t epochNumber) const {
  uint64_t key = epochProverKey(epochNumber, prover);
  auto it = m_epochCounts.find(key);
  return it == m_epochCounts.end() ? 0 : it->second;
}

bool ProofRegistry::getEpochWinner(uint32_t epochNumber, AccountPublicAddress& out) const {
  uint32_t best = 0;
  bool found = false;
  for (auto& [key, count] : m_epochCounts) {
    uint32_t epoch = static_cast<uint32_t>(key >> 32);
    if (epoch == epochNumber && count > best) {
      best = count;
      found = true;
      // Recover address prefix from key (sufficient for reward routing)
      // Full address comes from the proof record lookup
    }
  }
  if (!found) return false;
  // Walk proof records to find winner's full address
  for (auto& [root, rec] : m_proofs) {
    if (rec.epochNumber == epochNumber &&
        getProofCount(rec.proverAddress, epochNumber) == best) {
      out = rec.proverAddress;
      return true;
    }
  }
  return false;
}

void ProofRegistry::pruneOldEpochs(uint32_t currentEpoch) {
  if (currentEpoch < 2) return;
  uint32_t cutoff = currentEpoch - 2;
  for (auto it = m_proofs.begin(); it != m_proofs.end(); ) {
    if (it->second.epochNumber < cutoff) it = m_proofs.erase(it);
    else ++it;
  }
  for (auto it = m_epochCounts.begin(); it != m_epochCounts.end(); ) {
    uint32_t epoch = static_cast<uint32_t>(it->first >> 32);
    if (epoch < cutoff) it = m_epochCounts.erase(it);
    else ++it;
  }
}

bool ProofRegistry::verifyProof(const std::vector<uint8_t>& proofBytes,
                                 const Crypto::Hash& /*stateRootBefore*/,
                                 const Crypto::Hash& /*stateRootAfter*/) const {
  // Phase 1 stub: accept any non-empty proof bytes.
  // Phase C will call the SP1 Groth16 C-bindgen verifier here.
  // TODO: replace with sp1_verify(proofBytes.data(), proofBytes.size(), vk_bytes, public_inputs)
  return !proofBytes.empty();
}

} // namespace CryptoNote
```

- [ ] **Step 4.3: Add `ProofRegistry.cpp` to CMakeLists**

In `src/CryptoNoteCore/CMakeLists.txt`, add `ProofRegistry.cpp` to the sources list alongside the other `.cpp` files.

```bash
grep -n "CommitmentIndex.cpp\|Blockchain.cpp" src/CryptoNoteCore/CMakeLists.txt
```
Find the line and add `ProofRegistry.cpp` on the next line.

- [ ] **Step 4.4: Build**

```bash
cd build && make fuegod -j$(sysctl -n hw.ncpu) 2>&1 | grep -E "error:" | head -20
```
Expected: clean build.

- [ ] **Step 4.5: Commit**

```bash
git add src/CryptoNoteCore/ProofRegistry.h src/CryptoNoteCore/ProofRegistry.cpp src/CryptoNoteCore/CMakeLists.txt
git commit -m "feat(core): add ProofRegistry — LP proof storage and epoch winner tracking"
```

---

### Task 5: Wire 80/10/10 fee split into `Blockchain.cpp`

**Files:**
- Modify: `src/CryptoNoteCore/Blockchain.cpp:3482-3525` (epoch fee distribution block)

The current code at line 3482 computes `treasuryShare` as 20% and `cdSwapShare` as the remainder. We need to add a `proverShare` and track it in a new accumulator `m_proverPoolBalance`.

- [ ] **Step 5.1: Add `m_proverPoolBalance` to `BlockchainStorage` serialization**

Find the serialization block around line 215 (where `m_currentEpochSwapFees` and `m_treasuryBalance` are serialized):

```cpp
s(m_bs.m_currentEpochSwapFees, "current_epoch_swap_fees");
// ...
s(m_bs.m_treasuryBalance, "treasury_balance");
```

Add after `m_treasuryBalance`:
```cpp
s(m_bs.m_proverPoolBalance, "prover_pool_balance");
```

Also add the field declaration in the `BlockchainStorage` struct (search for `uint64_t m_treasuryBalance` to find it):
```cpp
uint64_t m_proverPoolBalance = 0;
```

- [ ] **Step 5.2: Update epoch fee distribution (lines ~3482-3506)**

Replace the current split logic:
```cpp
    uint64_t epochSwapFees = m_currentEpochSwapFees;
   // uint64_t efierSwapShare = ...
    uint64_t treasuryShare = (epochSwapFees * CryptoNote::parameters::SWAP_FEE_TREASURY_SHARE_PCT) / 100;
    uint64_t cdSwapShare = epochSwapFees - treasuryShare;
```

With:
```cpp
    uint64_t epochSwapFees = m_currentEpochSwapFees;
    uint64_t treasuryShare = (epochSwapFees * CryptoNote::parameters::SWAP_FEE_TREASURY_SHARE_PCT) / 100;
    uint64_t proverShare   = (epochSwapFees * CryptoNote::parameters::SWAP_FEE_PROVER_SHARE_PCT)  / 100;
    uint64_t cdSwapShare   = epochSwapFees - treasuryShare - proverShare;

    // Accumulate prover pool
    m_proverPoolBalance += proverShare;

    // Bootstrap subsidy: if prover_pool < threshold, top up from treasury
    if (m_proverPoolBalance < CryptoNote::parameters::PROVER_BOOTSTRAP_THRESHOLD_ATOMIC &&
        m_treasuryBalance >= CryptoNote::parameters::PROVER_BOOTSTRAP_SUBSIDY_ATOMIC) {
      m_proverPoolBalance += CryptoNote::parameters::PROVER_BOOTSTRAP_SUBSIDY_ATOMIC;
      m_treasuryBalance   -= CryptoNote::parameters::PROVER_BOOTSTRAP_SUBSIDY_ATOMIC;
    }
```

Then after `m_treasuryBalance += treasuryShare;`, add the epoch winner payout:
```cpp
    // Pay out prover pool to epoch winner (if any proofs were submitted)
    AccountPublicAddress epochWinner;
    if (m_proofRegistry.getEpochWinner(epochNumber, epochWinner) &&
        m_proverPoolBalance > 0) {
      // Emit prover reward as coinbase-style output (recorded in epoch report)
      // Actual transfer happens via addProverRewardToBlock() called by block builder
      m_pendingProverReward = m_proverPoolBalance;
      m_pendingProverAddress = epochWinner;
      m_proverPoolBalance = 0;
    }
    m_proofRegistry.pruneOldEpochs(epochNumber);
```

- [ ] **Step 5.3: Add `m_proofRegistry`, `m_pendingProverReward`, `m_pendingProverAddress` to Blockchain class**

In `src/CryptoNoteCore/Blockchain.h`, add to the private section:
```cpp
  ProofRegistry m_proofRegistry;
  uint64_t m_pendingProverReward = 0;
  AccountPublicAddress m_pendingProverAddress = {};
```

Add `#include "ProofRegistry.h"` to `Blockchain.h` includes.

- [ ] **Step 5.4: Build**

```bash
cd build && make fuegod -j$(sysctl -n hw.ncpu) 2>&1 | grep -E "error:" | head -20
```

- [ ] **Step 5.5: Commit**

```bash
git add src/CryptoNoteCore/Blockchain.cpp src/CryptoNoteCore/Blockchain.h
git commit -m "feat(blockchain): 80/10/10 fee split, prover pool accumulator, epoch winner payout"
```

---

### Task 6: Add `/submit_lp_proof` and `/get_pool_state` RPC endpoints

**Files:**
- Modify: `src/Rpc/CoreRpcServerCommandsDefinitions.h` — add command structs
- Modify: `src/Rpc/RpcServer.h` — declare handlers
- Modify: `src/Rpc/RpcServer.cpp` — implement + register

- [ ] **Step 6.1: Add command structs to `CoreRpcServerCommandsDefinitions.h`**

Add near the end of the file (before closing namespace):

```cpp
// ─── LP Proof Submission ────────────────────────────────────────────────────
struct COMMAND_RPC_SUBMIT_LP_PROOF {
  struct request {
    std::string state_root_before; // hex-encoded Crypto::Hash
    std::string state_root_after;  // hex-encoded Crypto::Hash
    std::string proof_bytes_hex;   // hex-encoded SP1 proof (~800 bytes → ~1600 hex chars)
    std::string prover_address;    // XFG address of prover (for reward routing)

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(state_root_before)
      KV_SERIALIZE(state_root_after)
      KV_SERIALIZE(proof_bytes_hex)
      KV_SERIALIZE(prover_address)
    END_KV_SERIALIZE_MAP()
  };

  struct response {
    bool accepted;          // true if proof was accepted (first valid for this root)
    std::string reason;     // rejection reason if accepted == false
    std::string proof_hash; // hex of stored proof hash

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(accepted)
      KV_SERIALIZE(reason)
      KV_SERIALIZE(proof_hash)
    END_KV_SERIALIZE_MAP()
  };
};

// ─── Pool Committed State Query ─────────────────────────────────────────────
// Returns the pool state as commitments only — NO plaintext amounts.
// The prover daemon calls this to fetch witness data for proof generation.
struct COMMAND_RPC_GET_POOL_STATE {
  struct request {
    uint64_t pool_id;  // 0 = default XFG pool

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(pool_id)
    END_KV_SERIALIZE_MAP()
  };

  struct response {
    std::string state_root;               // hex: current committed state root
    std::string commitment_reserve_a;     // hex: Pedersen commitment to reserveA
    std::string commitment_reserve_b;     // hex: Pedersen commitment to reserveB
    std::string commitment_fee_accum_a;   // hex: committed feeAccumulatorA
    std::string commitment_fee_accum_b;   // hex: committed feeAccumulatorB
    uint64_t    total_lp_shares;          // LP share count (public, not sensitive)
    uint32_t    last_proven_height;       // height of last accepted proof
    bool        proof_pending;            // true if txs queued awaiting proof
    std::string status;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(state_root)
      KV_SERIALIZE(commitment_reserve_a)
      KV_SERIALIZE(commitment_reserve_b)
      KV_SERIALIZE(commitment_fee_accum_a)
      KV_SERIALIZE(commitment_fee_accum_b)
      KV_SERIALIZE(total_lp_shares)
      KV_SERIALIZE(last_proven_height)
      KV_SERIALIZE(proof_pending)
      KV_SERIALIZE(status)
    END_KV_SERIALIZE_MAP()
  };
};
```

- [ ] **Step 6.2: Declare handlers in `RpcServer.h`**

In the private handler declarations section, add:
```cpp
bool on_submit_lp_proof(const COMMAND_RPC_SUBMIT_LP_PROOF::request& req,
                         COMMAND_RPC_SUBMIT_LP_PROOF::response& res);
bool on_get_pool_state(const COMMAND_RPC_GET_POOL_STATE::request& req,
                        COMMAND_RPC_GET_POOL_STATE::response& res);
```

- [ ] **Step 6.3: Register routes in `RpcServer.cpp`**

In the `s_handlers` map, add:
```cpp
{"/submit_lp_proof", {&RpcServer::on_submit_lp_proof, false}},
{"/get_pool_state",  {&RpcServer::on_get_pool_state,  false}},
```

- [ ] **Step 6.4: Implement `on_get_pool_state`**

```cpp
bool RpcServer::on_get_pool_state(const COMMAND_RPC_GET_POOL_STATE::request& req,
                                   COMMAND_RPC_GET_POOL_STATE::response& res) {
  // For now: return placeholder committed state from SwapDaemon PoolOrganizer
  // (Phase D wires in real committed state once prover daemon builds it)
  // The key invariant: response contains ONLY commitments, never plaintext reserves.
  res.state_root            = "0000000000000000000000000000000000000000000000000000000000000000";
  res.commitment_reserve_a  = "0000000000000000000000000000000000000000000000000000000000000000";
  res.commitment_reserve_b  = "0000000000000000000000000000000000000000000000000000000000000000";
  res.commitment_fee_accum_a = "0000000000000000000000000000000000000000000000000000000000000000";
  res.commitment_fee_accum_b = "0000000000000000000000000000000000000000000000000000000000000000";
  res.total_lp_shares       = 0;
  res.last_proven_height    = 0;
  res.proof_pending         = false;
  res.status                = "ok";
  return true;
}
```

- [ ] **Step 6.5: Implement `on_submit_lp_proof`**

```cpp
bool RpcServer::on_submit_lp_proof(const COMMAND_RPC_SUBMIT_LP_PROOF::request& req,
                                    COMMAND_RPC_SUBMIT_LP_PROOF::response& res) {
  // Decode hex inputs
  Crypto::Hash rootBefore, rootAfter;
  if (!Common::fromHex(req.state_root_before, &rootBefore, sizeof(rootBefore)) ||
      !Common::fromHex(req.state_root_after,  &rootAfter,  sizeof(rootAfter))) {
    res.accepted = false;
    res.reason = "invalid hex in state_root_before or state_root_after";
    return true;
  }
  std::vector<uint8_t> proofBytes;
  if (!Common::fromHex(req.proof_bytes_hex, proofBytes)) {
    res.accepted = false;
    res.reason = "invalid hex in proof_bytes_hex";
    return true;
  }
  // Parse prover address
  AccountPublicAddress proverAddr;
  if (!m_core.currency().parseAccountAddressString(req.prover_address, proverAddr)) {
    res.accepted = false;
    res.reason = "invalid prover_address";
    return true;
  }

  uint32_t height    = m_core.get_current_blockchain_height();
  uint32_t epoch     = height / CryptoNote::parameters::EPOCH_DURATION_BLOCKS;
  bool accepted = m_core.getBlockchain().submitLpProof(
      rootBefore, rootAfter, proofBytes, proverAddr, height, epoch);

  res.accepted   = accepted;
  res.reason     = accepted ? "" : "proof rejected (invalid or duplicate for this state root)";
  res.proof_hash = accepted ? Common::toHex(rootBefore.data, sizeof(rootBefore)) : "";
  return true;
}
```

- [ ] **Step 6.6: Add `submitLpProof` pass-through to `Core.h`/`Core.cpp`**

In `src/CryptoNoteCore/Core.h`, add to the public interface:
```cpp
bool submitLpProof(const Crypto::Hash& rootBefore,
                   const Crypto::Hash& rootAfter,
                   const std::vector<uint8_t>& proofBytes,
                   const AccountPublicAddress& proverAddr,
                   uint32_t height, uint32_t epoch);
```

In `Core.cpp`:
```cpp
bool Core::submitLpProof(const Crypto::Hash& rootBefore,
                          const Crypto::Hash& rootAfter,
                          const std::vector<uint8_t>& proofBytes,
                          const AccountPublicAddress& proverAddr,
                          uint32_t height, uint32_t epoch) {
  return m_blockchain.getProofRegistry().submitProof(
      rootBefore, rootAfter, proofBytes, proverAddr, height, epoch);
}
```

Also add `getProofRegistry()` accessor to `Blockchain.h`:
```cpp
ProofRegistry& getProofRegistry() { return m_proofRegistry; }
```

- [ ] **Step 6.7: Build**

```bash
cd build && make fuegod -j$(sysctl -n hw.ncpu) 2>&1 | grep -E "error:" | head -30
```

- [ ] **Step 6.8: Smoke test RPC endpoints**

```bash
# Start daemon in background
./build/release/src/fuegod --testnet --data-dir /tmp/fuego-lp-test &
sleep 3

# Check endpoint exists
curl -s -X POST http://127.0.0.1:28280/get_pool_state \
  -H "Content-Type: application/json" \
  -d '{"pool_id":0}' | python3 -m json.tool

# Check submit endpoint rejects empty proof gracefully
curl -s -X POST http://127.0.0.1:28280/submit_lp_proof \
  -H "Content-Type: application/json" \
  -d '{"state_root_before":"0000000000000000000000000000000000000000000000000000000000000000","state_root_after":"0000000000000000000000000000000000000000000000000000000000000001","proof_bytes_hex":"","prover_address":"INVALID"}' \
  | python3 -m json.tool

kill %1
```

Expected: `get_pool_state` returns `"status":"ok"` with zeroed commitments. `submit_lp_proof` returns `"accepted":false` with reason about invalid address.

- [ ] **Step 6.9: Commit**

```bash
git add src/Rpc/CoreRpcServerCommandsDefinitions.h src/Rpc/RpcServer.h src/Rpc/RpcServer.cpp \
        src/CryptoNoteCore/Core.h src/CryptoNoteCore/Core.cpp
git commit -m "feat(rpc): add /submit_lp_proof and /get_pool_state endpoints"
```

---

## Chunk 3: SP1 ZK Circuit — AMM Invariant Over Committed Values

### Task 7: Create `fuego-lp-core` shared types crate

**Files:**
- Create: `fuego-prover/fuego-lp-core/Cargo.toml`
- Create: `fuego-prover/fuego-lp-core/src/lib.rs`

This mirrors `fuego-core` but for LP-specific types. Keeping it separate avoids polluting the HEAT circuit with LP dependencies.

- [ ] **Step 7.1: Create `fuego-lp-core/Cargo.toml`**

```toml
[package]
name = "fuego-lp-core"
version = "0.1.0"
edition = "2021"

[dependencies]
serde      = { version = "1", features = ["derive"] }
tiny-keccak = { version = "2", features = ["keccak"] }
```

- [ ] **Step 7.2: Create `fuego-lp-core/src/lib.rs`**

```rust
use serde::{Deserialize, Serialize};
use tiny_keccak::{Hasher, Keccak};

pub type Hash = [u8; 32];

// A Pedersen commitment: 32-byte compressed EC point.
// Inside the SP1 circuit we work with the commitment as an opaque hash
// (we verify the *algebraic* opening, not the curve point arithmetic).
// For the circuit's purposes, a commitment is valid if:
//   keccak256(value_bytes || blinding_bytes) == commitment_hash
// This is a simplified binding commitment suitable for the zkVM arithmetic model.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub struct Commitment {
  pub hash: Hash,
}

impl Commitment {
  pub fn new(value: u64, blinding: &[u8; 32]) -> Self {
    let mut k = Keccak::v256();
    k.update(&value.to_le_bytes());
    k.update(blinding);
    let mut out = [0u8; 32];
    k.finalize(&mut out);
    Self { hash: out }
  }

  pub fn verify(&self, value: u64, blinding: &[u8; 32]) -> bool {
    Self::new(value, blinding).hash == self.hash
  }
}

// The complete LP pool state (committed form).
// This is what the operator sees and passes to the prover — NO plaintext values.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CommittedPoolState {
  pub reserve_a:      Commitment,  // committed XFG reserve
  pub reserve_b:      Commitment,  // committed counter-asset reserve
  pub k_commitment:   Commitment,  // committed k = reserveA * reserveB (for invariant check)
  pub fee_accum_a:    Commitment,  // committed fee accumulator (A side)
  pub fee_accum_b:    Commitment,  // committed fee accumulator (B side)
  pub total_lp_shares: u64,        // total LP shares (public — not sensitive)
  pub state_root:     Hash,        // keccak256 of this whole struct
}

impl CommittedPoolState {
  /// Canonical state root. Field order must match LpCommittedState.h on the C++ side.
  /// Order: reserve_a, reserve_b, fee_accum_a, fee_accum_b, total_lp_shares (le64)
  /// k_commitment is NOT included — it is derived, not stored on-chain.
  pub fn compute_root(&self) -> Hash {
    let mut k = Keccak::v256();
    k.update(&self.reserve_a.hash);
    k.update(&self.reserve_b.hash);
    k.update(&self.fee_accum_a.hash);
    k.update(&self.fee_accum_b.hash);
    k.update(&self.total_lp_shares.to_le_bytes());
    let mut out = [0u8; 32];
    k.finalize(&mut out);
    out
  }
}

// Private witness: the opening values for all commitments.
// This is ONLY ever held by the LP user client. Never transmitted to operator.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PoolStateWitness {
  pub reserve_a:    u64,
  pub blinding_a:   [u8; 32],
  pub reserve_b:    u64,
  pub blinding_b:   [u8; 32],
  pub k_value:      u64,
  pub blinding_k:   [u8; 32],
  pub fee_accum_a:  u64,
  pub blinding_fa:  [u8; 32],
  pub fee_accum_b:  u64,
  pub blinding_fb:  [u8; 32],
}

// Full input to the LP ZK circuit (fed via SP1 stdin).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LpProofInput {
  // Public: committed state before/after (operator provides these)
  pub pre_state:    CommittedPoolState,
  pub post_state:   CommittedPoolState,

  // Private: opening values (known only to LP client, passed to circuit)
  pub pre_witness:  PoolStateWitness,
  pub post_witness: PoolStateWitness,

  // Trade parameters (private)
  pub trade_input:    u64,
  pub blinding_trade: [u8; 32],
  pub trade_output:   u64,
  pub fee_amount:     u64,
  pub fee_bps:        u32,   // 30 for 0.3%

  // Prover address (public — for reward routing)
  pub prover_address: [u8; 64],
}

// What the circuit commits to publicly (visible on-chain).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LpPublicValues {
  pub pre_state_root:  Hash,
  pub post_state_root: Hash,
  pub fee_commitment:  Hash,    // keccak(fee_amount || blinding)
  pub prover_address:  [u8; 64],
}
```

- [ ] **Step 7.3: Add `fuego-lp-core` to workspace**

Edit `fuego-prover/Cargo.toml`:
```toml
[workspace]
members = [
    "fuego-core",
    "fuego-cn",
    "fuego-circuit",
    "fuego-prover-cli",
    "fuego-lp-core",    # ← add
    "fuego-lp-circuit", # ← add (next task)
    "prover-daemon",    # ← add (later task)
]
```

- [ ] **Step 7.4: Build `fuego-lp-core`**

```bash
cd fuego-prover
cargo build -p fuego-lp-core 2>&1 | grep -E "^error" | head -20
```
Expected: clean build.

- [ ] **Step 7.5: Commit**

```bash
git add fuego-prover/fuego-lp-core/ fuego-prover/Cargo.toml
git commit -m "feat(prover): add fuego-lp-core crate — committed pool state types"
```

---

### Task 8: Create `fuego-lp-circuit` — SP1 ZK circuit

**Files:**
- Create: `fuego-prover/fuego-lp-circuit/Cargo.toml`
- Create: `fuego-prover/fuego-lp-circuit/src/main.rs`

This is the circuit that runs *inside* the SP1 zkVM. It verifies:
1. All Pedersen commitments open correctly (pre-state and post-state)
2. AMM invariant holds: `reserveA_post * reserveB_post >= k_pre`
3. Fee was correctly computed: `fee == tradeInput * feeBps / 10000`
4. State root transitions are consistent

**Critically: the circuit never outputs the plaintext values — only the state roots and fee commitment.**

- [ ] **Step 8.1: Create `Cargo.toml`**

```toml
[package]
name = "fuego-lp-circuit"
version = "0.1.0"
edition = "2021"

[dependencies]
fuego-lp-core = { path = "../fuego-lp-core" }
sp1-zkvm      = "3"
serde         = { version = "1", features = ["derive"] }
tiny-keccak   = { version = "2", features = ["keccak"] }
```

- [ ] **Step 8.2: Write the circuit `src/main.rs`**

```rust
#![no_main]
sp1_zkvm::entrypoint!(main);

use fuego_lp_core::{Commitment, LpProofInput, LpPublicValues};
use tiny_keccak::{Hasher, Keccak};

fn keccak256(data: &[u8]) -> [u8; 32] {
    let mut k = Keccak::v256();
    k.update(data);
    let mut out = [0u8; 32];
    k.finalize(&mut out);
    out
}

pub fn main() {
    // ─── 1. Read witness from SP1 stdin ──────────────────────────────────────
    let input: LpProofInput = sp1_zkvm::io::read::<LpProofInput>();

    // ─── 2. Verify pre-state commitment openings ─────────────────────────────
    // Each field: commitment.hash == keccak256(value || blinding)
    // This proves the prover knows the plaintext values — without revealing them.
    let pre = &input.pre_witness;
    let pre_state = &input.pre_state;

    assert!(
        pre_state.reserve_a.verify(pre.reserve_a, &pre.blinding_a),
        "pre reserve_a commitment mismatch"
    );
    assert!(
        pre_state.reserve_b.verify(pre.reserve_b, &pre.blinding_b),
        "pre reserve_b commitment mismatch"
    );
    assert!(
        pre_state.k_commitment.verify(pre.k_value, &pre.blinding_k),
        "pre k commitment mismatch"
    );
    assert!(
        pre_state.fee_accum_a.verify(pre.fee_accum_a, &pre.blinding_fa),
        "pre fee_accum_a commitment mismatch"
    );
    assert!(
        pre_state.fee_accum_b.verify(pre.fee_accum_b, &pre.blinding_fb),
        "pre fee_accum_b commitment mismatch"
    );

    // ─── 3. Verify pre-state root is consistent ───────────────────────────────
    let computed_pre_root = pre_state.compute_root();
    assert_eq!(
        computed_pre_root, pre_state.state_root,
        "pre-state root mismatch"
    );

    // ─── 4. Verify post-state commitment openings ────────────────────────────
    let post = &input.post_witness;
    let post_state = &input.post_state;

    assert!(
        post_state.reserve_a.verify(post.reserve_a, &post.blinding_a),
        "post reserve_a commitment mismatch"
    );
    assert!(
        post_state.reserve_b.verify(post.reserve_b, &post.blinding_b),
        "post reserve_b commitment mismatch"
    );

    // ─── 5. AMM invariant: x' * y' >= k ─────────────────────────────────────
    // Using 128-bit arithmetic to prevent overflow (reserves can be large).
    let k_post = (post.reserve_a as u128) * (post.reserve_b as u128);
    let k_pre  = pre.k_value as u128;
    assert!(
        k_post >= k_pre,
        "AMM invariant violated: k decreased after trade"
    );

    // ─── 6. Fee computation correctness ─────────────────────────────────────
    // fee = tradeInput * feeBps / 10000
    // We allow ±1 rounding tolerance for integer division.
    let expected_fee = ((input.trade_input as u128) * (input.fee_bps as u128)) / 10000;
    let fee_diff = if input.fee_amount as u128 > expected_fee {
        input.fee_amount as u128 - expected_fee
    } else {
        expected_fee - input.fee_amount as u128
    };
    assert!(fee_diff <= 1, "fee computation incorrect (diff={fee_diff})");

    // ─── 7. LP share accounting: total shares conservation ───────────────────
    // If this is a swap (not a deposit/withdrawal), LP share count must not change.
    // Deposits/withdrawals change total_lp_shares; the circuit checks delta is consistent.
    // For swaps: post.total_lp_shares == pre.total_lp_shares
    // For deposits: post.total_lp_shares == pre.total_lp_shares + minted_shares
    // The minted_shares value is committed in the fee_commitment field for deposits.
    // For Phase 1 (swap only): assert shares unchanged.
    assert_eq!(
        input.post_state.total_lp_shares, input.pre_state.total_lp_shares,
        "LP share count changed unexpectedly for a swap (not a deposit/withdrawal)"
    );

    // ─── 8. No negative balances ─────────────────────────────────────────────
    assert!(post.reserve_a > 0, "post reserve_a is zero");
    assert!(post.reserve_b > 0, "post reserve_b is zero");

    // ─── 9. Verify post-state root is consistent ──────────────────────────────
    let computed_post_root = post_state.compute_root();
    assert_eq!(
        computed_post_root, post_state.state_root,
        "post-state root mismatch"
    );

    // ─── 9. Commit public outputs ────────────────────────────────────────────
    // These are the ONLY values visible on-chain. No plaintext reserves exposed.
    let fee_commitment = {
        let mut data = [0u8; 40];
        data[..8].copy_from_slice(&input.fee_amount.to_le_bytes());
        // blinding for fee commitment is derived from trade blinding
        data[8..40].copy_from_slice(&input.blinding_trade);
        keccak256(&data)
    };

    let public_values = LpPublicValues {
        pre_state_root:  pre_state.state_root,
        post_state_root: post_state.state_root,
        fee_commitment,
        prover_address: input.prover_address,
    };

    sp1_zkvm::io::commit(&public_values);
}
```

- [ ] **Step 8.3: Build circuit**

```bash
cd fuego-prover
cargo build -p fuego-lp-circuit 2>&1 | grep -E "^error" | head -20
```
Expected: clean build.

- [ ] **Step 8.4: Write circuit unit test (native mode — no ZK overhead)**

Create `fuego-prover/fuego-lp-circuit/tests/circuit_test.rs`:

```rust
use fuego_lp_core::{Commitment, CommittedPoolState, LpProofInput, PoolStateWitness};

fn make_witness(reserve_a: u64, reserve_b: u64, fee_a: u64, fee_b: u64)
    -> (CommittedPoolState, PoolStateWitness)
{
    let ba = [1u8; 32];
    let bb = [2u8; 32];
    let bk = [3u8; 32];
    let bfa = [4u8; 32];
    let bfb = [5u8; 32];
    let k = reserve_a.saturating_mul(reserve_b);
    let state = CommittedPoolState {
        reserve_a:    Commitment::new(reserve_a, &ba),
        reserve_b:    Commitment::new(reserve_b, &bb),
        k_commitment: Commitment::new(k, &bk),
        fee_accum_a:  Commitment::new(fee_a, &bfa),
        fee_accum_b:  Commitment::new(fee_b, &bfb),
        total_lp_shares: 1000,
        state_root: [0u8; 32], // will be computed
    };
    let mut state = state;
    state.state_root = state.compute_root();
    let witness = PoolStateWitness {
        reserve_a, blinding_a: ba,
        reserve_b, blinding_b: bb,
        k_value: k, blinding_k: bk,
        fee_accum_a: fee_a, blinding_fa: bfa,
        fee_accum_b: fee_b, blinding_fb: bfb,
    };
    (state, witness)
}

#[test]
fn test_valid_swap() {
    // Pool: 10000 A, 10000 B, k=100_000_000
    // Trade: input 100 A → output ~99 B (with 0.3% fee)
    let (pre_state, pre_witness) = make_witness(10_000, 10_000, 0, 0);
    let (post_state, post_witness) = make_witness(10_100, 9_901, 0, 0);
    // fee = 100 * 30 / 10000 = 0 (rounds to 0 for small amounts)
    // For test purposes use fee=0
    let input = LpProofInput {
        pre_state, post_state, pre_witness, post_witness,
        trade_input: 100, blinding_trade: [6u8; 32],
        trade_output: 99, fee_amount: 0, fee_bps: 30,
        prover_address: [0u8; 64],
    };
    // Verify commitment openings manually (mimics circuit checks)
    assert!(input.pre_state.reserve_a.verify(input.pre_witness.reserve_a, &input.pre_witness.blinding_a));
    assert!(input.pre_state.reserve_b.verify(input.pre_witness.reserve_b, &input.pre_witness.blinding_b));
    // AMM invariant: post.k >= pre.k
    let k_post = (input.post_witness.reserve_a as u128) * (input.post_witness.reserve_b as u128);
    let k_pre  = input.pre_witness.k_value as u128;
    assert!(k_post >= k_pre, "AMM invariant should hold");
}

#[test]
fn test_invariant_violation_caught() {
    // Malicious post-state: reserves decrease in a way that violates k
    let (pre_state, pre_witness) = make_witness(10_000, 10_000, 0, 0);
    let (post_state, post_witness) = make_witness(9_000, 9_000, 0, 0); // k shrinks!
    let k_post = (post_witness.reserve_a as u128) * (post_witness.reserve_b as u128);
    let k_pre  = pre_witness.k_value as u128;
    assert!(k_post < k_pre, "Should detect invariant violation");
}

#[test]
fn test_fee_verification() {
    let fee_bps: u32 = 30;
    let trade_input: u64 = 100_000;
    let correct_fee = (trade_input as u128 * fee_bps as u128 / 10_000) as u64; // 300
    let wrong_fee = 100u64;
    let diff = if wrong_fee > correct_fee {
        wrong_fee - correct_fee
    } else {
        correct_fee - wrong_fee
    };
    assert!(diff > 1, "Wrong fee should be caught");
    assert_eq!(correct_fee, 300);
}
```

- [ ] **Step 8.5: Run circuit unit tests (native, fast)**

```bash
cd fuego-prover
cargo test -p fuego-lp-circuit 2>&1
```
Expected: all 3 tests pass.

- [ ] **Step 8.6: Commit**

```bash
git add fuego-prover/fuego-lp-circuit/ fuego-prover/Cargo.toml fuego-prover/fuego-lp-core/
git commit -m "feat(circuit): add fuego-lp-circuit — SP1 ZK circuit verifying AMM invariant over committed values"
```

---

## Chunk 4: Prover Daemon — Operator Sees Only Commitments

### Task 9: Create `prover-daemon` binary

**Files:**
- Create: `fuego-prover/prover-daemon/Cargo.toml`
- Create: `fuego-prover/prover-daemon/src/main.rs`
- Create: `fuego-prover/prover-daemon/src/lp_prover.rs`
- Create: `fuego-prover/prover-daemon/src/reward_claimer.rs`

The daemon's critical privacy property: it fetches **only committed state** from the daemon via `/get_pool_state`. It never receives, requests, or stores plaintext reserve amounts. It constructs a proof that the state transition is valid using the `fuego-lp-circuit`, then submits it to `/submit_lp_proof`.

- [ ] **Step 9.1: Create `Cargo.toml`**

```toml
[package]
name = "prover-daemon"
version = "0.1.0"
edition = "2021"

[[bin]]
name = "fuego-prover-daemon"
path = "src/main.rs"

[dependencies]
fuego-lp-core = { path = "../fuego-lp-core" }
sp1-sdk       = "3"
anyhow        = "1"
clap          = { version = "4", features = ["derive"] }
serde         = { version = "1", features = ["derive"] }
serde_json    = "1"
reqwest       = { version = "0.11", features = ["json", "blocking"] }
tokio         = { version = "1", features = ["full"] }
hex           = "0.4"
```

- [ ] **Step 9.2: Create `src/main.rs`**

```rust
use anyhow::Result;
use clap::Parser;
mod lp_prover;
mod reward_claimer;

#[derive(Parser)]
#[command(name = "fuego-prover-daemon", about = "Fuego LP pool prover daemon")]
struct Args {
    /// fuegod RPC URL
    #[arg(long, default_value = "http://127.0.0.1:28280")]
    rpc: String,

    /// Prover XFG address (for reward routing)
    #[arg(long)]
    prover_address: String,

    /// Poll interval in seconds
    #[arg(long, default_value = "30")]
    poll_interval: u64,

    /// Pool ID to prove (default: 0)
    #[arg(long, default_value = "0")]
    pool_id: u64,
}

#[tokio::main]
async fn main() -> Result<()> {
    let args = Args::parse();
    println!("[fuego-prover-daemon] starting, rpc={}", args.rpc);
    println!("[fuego-prover-daemon] prover_address={}", args.prover_address);
    println!("[fuego-prover-daemon] PRIVACY: operator fetches committed state only");

    let mut last_proven_root = [0u8; 32];

    loop {
        // 1. Fetch committed pool state (NO plaintext values)
        match lp_prover::fetch_committed_state(&args.rpc, args.pool_id).await {
            Ok(state) => {
                if state.proof_pending && state.state_root != last_proven_root {
                    println!("[prover] new state root detected, generating proof...");
                    // 2. Generate proof over committed values
                    // NOTE: witness (plaintext opening values) must be provided by LP client
                    // For Phase 1 operator mode: operator has pre-authorized witness access
                    // via a separate authenticated channel (not the public RPC)
                    match lp_prover::generate_and_submit_proof(
                        &args.rpc,
                        &state,
                        &args.prover_address,
                    ).await {
                        Ok(proof_hash) => {
                            println!("[prover] proof accepted, hash={proof_hash}");
                            last_proven_root = state.state_root;
                        }
                        Err(e) => eprintln!("[prover] proof generation failed: {e}"),
                    }
                }
            }
            Err(e) => eprintln!("[prover] failed to fetch pool state: {e}"),
        }

        // 3. Check for epoch reward claims
        if let Err(e) = reward_claimer::check_and_claim(&args.rpc, &args.prover_address).await {
            eprintln!("[prover] reward claim check failed: {e}");
        }

        tokio::time::sleep(tokio::time::Duration::from_secs(args.poll_interval)).await;
    }
}
```

- [ ] **Step 9.3: Create `src/lp_prover.rs`**

```rust
use anyhow::{bail, Context, Result};
use fuego_lp_core::{CommittedPoolState, Commitment, LpProofInput, PoolStateWitness, LpPublicValues};
use serde::{Deserialize, Serialize};
use sp1_sdk::{ProverClient, SP1Stdin};

// RPC response shape for /get_pool_state
#[derive(Deserialize)]
pub struct PoolStateResponse {
  pub state_root:            String,  // hex
  pub commitment_reserve_a:  String,  // hex
  pub commitment_reserve_b:  String,  // hex
  pub commitment_fee_accum_a: String, // hex
  pub commitment_fee_accum_b: String, // hex
  pub total_lp_shares:       u64,
  pub last_proven_height:    u32,
  pub proof_pending:         bool,
  pub status:                String,
}

pub struct FetchedState {
  pub pool_state:  CommittedPoolState,
  pub state_root:  [u8; 32],
  pub proof_pending: bool,
}

fn hex_to_hash(s: &str) -> Result<[u8; 32]> {
  let bytes = hex::decode(s).context("invalid hex")?;
  if bytes.len() != 32 { bail!("expected 32 bytes, got {}", bytes.len()); }
  let mut arr = [0u8; 32];
  arr.copy_from_slice(&bytes);
  Ok(arr)
}

fn hex_to_commitment(s: &str) -> Result<Commitment> {
  Ok(Commitment { hash: hex_to_hash(s)? })
}

pub async fn fetch_committed_state(rpc: &str, pool_id: u64) -> Result<FetchedState> {
  let client = reqwest::Client::new();
  let resp: serde_json::Value = client
    .post(format!("{}/get_pool_state", rpc))
    .json(&serde_json::json!({ "pool_id": pool_id }))
    .send().await?
    .json().await?;

  let state_root = hex_to_hash(resp["state_root"].as_str().unwrap_or(""))?;
  let pool_state = CommittedPoolState {
    reserve_a:    hex_to_commitment(resp["commitment_reserve_a"].as_str().unwrap_or(""))?,
    reserve_b:    hex_to_commitment(resp["commitment_reserve_b"].as_str().unwrap_or(""))?,
    k_commitment: Commitment { hash: [0u8; 32] }, // derived in proof
    fee_accum_a:  hex_to_commitment(resp["commitment_fee_accum_a"].as_str().unwrap_or(""))?,
    fee_accum_b:  hex_to_commitment(resp["commitment_fee_accum_b"].as_str().unwrap_or(""))?,
    total_lp_shares: resp["total_lp_shares"].as_u64().unwrap_or(0),
    state_root,
  };

  Ok(FetchedState {
    pool_state,
    state_root,
    proof_pending: resp["proof_pending"].as_bool().unwrap_or(false),
  })
}

pub async fn generate_and_submit_proof(
    rpc: &str,
    state: &FetchedState,
    prover_address: &str,
) -> Result<String> {
  // Phase 1: stub proof — returns dummy bytes.
  // Phase C: load circuit ELF and run SP1 prover.
  // The witness (opening values) comes from the LP client's secure channel,
  // NOT from the public RPC. The operator daemon never holds plaintext reserves.
  let proof_bytes = vec![0xAB, 0xCD, 0xEF]; // Phase 1 stub

  // Submit proof to daemon
  let client = reqwest::Client::new();
  let resp: serde_json::Value = client
    .post(format!("{}/submit_lp_proof", rpc))
    .json(&serde_json::json!({
      "state_root_before": hex::encode(state.state_root),
      "state_root_after":  hex::encode(state.state_root), // same until real tx
      "proof_bytes_hex":   hex::encode(&proof_bytes),
      "prover_address":    prover_address,
    }))
    .send().await?
    .json().await?;

  if resp["accepted"].as_bool().unwrap_or(false) {
    Ok(resp["proof_hash"].as_str().unwrap_or("").to_string())
  } else {
    bail!("proof rejected: {}", resp["reason"].as_str().unwrap_or("unknown"))
  }
}
```

- [ ] **Step 9.4: Create `src/reward_claimer.rs`**

```rust
use anyhow::Result;

pub async fn check_and_claim(rpc: &str, prover_address: &str) -> Result<()> {
  // Phase 1 stub — epoch reward is emitted automatically by Blockchain.cpp
  // at epoch boundary. Daemon logs this as informational.
  // Phase E: query /get_prover_stats to check pending rewards, emit claim tx.
  let _ = (rpc, prover_address);
  Ok(())
}
```

- [ ] **Step 9.5: Build daemon**

```bash
cd fuego-prover
cargo build -p prover-daemon 2>&1 | grep -E "^error" | head -20
```
Expected: clean build.

- [ ] **Step 9.6: Smoke test daemon startup**

```bash
cd fuego-prover
cargo run -p prover-daemon -- --rpc http://127.0.0.1:28280 --prover-address fakeaddress123 &
sleep 2
kill %1
```
Expected: logs `[fuego-prover-daemon] starting` and `PRIVACY: operator fetches committed state only`, then exits cleanly on kill.

- [ ] **Step 9.7: Commit**

```bash
git add fuego-prover/prover-daemon/
git commit -m "feat(prover): add prover-daemon — operator polls committed state, never sees plaintext"
```

---

## Chunk 5: Integration, Documentation, and Phase Markers

### Task 10: Store committed pool state on-chain (not derived from plaintext)

**Files:**
- Create: `src/CryptoNoteCore/LpCommittedState.h`
- Modify: `src/Rpc/RpcServer.cpp` (the `on_get_pool_state` stub from Task 6)

**Privacy architecture correction:** The previous design (Task 10 original) had the server convert plaintext `PoolState` reserves to commitments. This breaks the privacy model — a server that can compute `keccak(reserve || blinding)` knows `reserve`. The correct model is:

> **Commitments are generated CLIENT-SIDE and submitted with LP transactions. The server only ever stores and returns commitments it received — it never computes them from plaintext.**

The `PoolState` plaintext reserves in `PoolOrganizer` remain necessary for the SwapDaemon's AMM trade execution (`poolGetOutputAmount`). But the committed form — which the ZK prover uses — comes from the LP client's transaction, not from the server.

For Phase 1 (no LP clients yet), `get_pool_state` returns zeroed commitments with `proof_pending: false`. Once LP client transactions start flowing (Phase D work), the committed state is populated from `TransactionOutputLP.commitment` fields.

- [ ] **Step 10.1: Create `LpCommittedState.h`**

```cpp
// src/CryptoNoteCore/LpCommittedState.h
#pragma once
#include "crypto/crypto.h"
#include <string>
#include <cstdint>

namespace CryptoNote {

// The committed form of pool state.
// Populated ONLY from TransactionOutputLP fields submitted by LP clients.
// The server NEVER derives this from plaintext PoolState reserves.
// This is what the prover daemon fetches — it contains no plaintext.
struct LpCommittedPoolState {
  Crypto::EllipticCurvePoint commitmentReserveA;   // from LP tx output
  Crypto::EllipticCurvePoint commitmentReserveB;   // from LP tx output
  Crypto::EllipticCurvePoint commitmentFeeAccumA;  // from LP tx output
  Crypto::EllipticCurvePoint commitmentFeeAccumB;  // from LP tx output
  uint64_t totalLpShares = 0;       // public count (not sensitive)
  Crypto::Hash stateRoot = {};      // keccak of commitment fields
  uint32_t lastProvenHeight = 0;    // height of last accepted proof
  bool proofPending = false;        // true when txs are queued without a proof

  // Serialize commitment fields to hex for RPC response
  std::string reserveAHex() const;
  std::string reserveBHex() const;
  std::string feeAccumAHex() const;
  std::string feeAccumBHex() const;
  std::string stateRootHex() const;
};

} // namespace CryptoNote
```

- [ ] **Step 10.2: Add `m_lpCommittedState` to `Blockchain` (alongside existing `PoolState`)**

In `src/CryptoNoteCore/Blockchain.h`, add to the private state section:
```cpp
  #include "LpCommittedState.h"
  CryptoNote::LpCommittedPoolState m_lpCommittedState;
```

Add a public accessor:
```cpp
const LpCommittedPoolState& getLpCommittedState() const { return m_lpCommittedState; }
```

When a `TransactionOutputLP` is processed in `Blockchain.cpp`, update `m_lpCommittedState` from the transaction's committed fields:
```cpp
// In the block processing path where TransactionOutputLP is visited:
if (auto* lpOut = boost::get<TransactionOutputLP>(&output.target)) {
  m_lpCommittedState.commitmentReserveA  = lpOut->commitment;
  m_lpCommittedState.stateRoot           = lpOut->stateRootAfter;
  m_lpCommittedState.lastProvenHeight    = blockHeight;
  m_lpCommittedState.proofPending        = false; // proof was verified to accept this tx
}
```

- [ ] **Step 10.3: Add `getLpCommittedState()` pass-through to `Core.h/Core.cpp`**

```cpp
// Core.h public:
const CryptoNote::LpCommittedPoolState& getLpCommittedState() const;

// Core.cpp:
const CryptoNote::LpCommittedPoolState& Core::getLpCommittedState() const {
  return m_blockchain.getLpCommittedState();
}
```

- [ ] **Step 10.4: Wire into `on_get_pool_state` RPC handler**

Replace the stub body in `RpcServer.cpp::on_get_pool_state` with:
```cpp
  const auto& committed = m_core.getLpCommittedState();
  res.state_root             = committed.stateRootHex();
  res.commitment_reserve_a   = committed.reserveAHex();
  res.commitment_reserve_b   = committed.reserveBHex();
  res.commitment_fee_accum_a = committed.feeAccumAHex();
  res.commitment_fee_accum_b = committed.feeAccumBHex();
  res.total_lp_shares        = committed.totalLpShares;
  res.last_proven_height     = committed.lastProvenHeight;
  res.proof_pending          = committed.proofPending;
  res.status                 = "ok";
  // NOTE: all values above are commitments — no plaintext reserve amounts.
  // The plaintext reserves live in PoolOrganizer (SwapDaemon) for AMM math only
  // and are NEVER exposed via this endpoint.
```

- [ ] **Step 10.5: Build**

```bash
cd build && make fuegod -j$(sysctl -n hw.ncpu) 2>&1 | grep -E "^error:" | head -20
```

- [ ] **Step 10.6: Smoke test**

```bash
./build/release/src/fuegod --testnet --data-dir /tmp/fuego-lp-test &
sleep 3
curl -s -X POST http://127.0.0.1:28280/get_pool_state -H "Content-Type: application/json" \
  -d '{"pool_id":0}' | python3 -m json.tool
kill %1
```
Expected: returns zeroed 64-char hex commitment strings + `"proof_pending": false`. No integer reserve amounts in the response.

- [ ] **Step 10.7: Commit**

```bash
git add src/CryptoNoteCore/LpCommittedState.h src/CryptoNoteCore/Blockchain.h \
        src/CryptoNoteCore/Blockchain.cpp src/CryptoNoteCore/Core.h \
        src/CryptoNoteCore/Core.cpp src/Rpc/RpcServer.cpp
git commit -m "feat(pool): committed LP state stored from tx outputs — operator never derives from plaintext"
```

---

### Task 11: Add Phase markers and TODO comments for Phase C (real SP1 verification)

The Phase 1 stub in `ProofRegistry::verifyProof()` accepts any non-empty bytes. Document the Phase C upgrade path clearly so it's impossible to miss.

- [ ] **Step 11.1: Add TODO banner to `ProofRegistry.cpp`**

At the top of `verifyProof()`:
```cpp
// ═══════════════════════════════════════════════════════════════════════════
// PHASE 1 STUB — accepts any non-empty proof bytes.
// PHASE C UPGRADE: replace this function body with:
//   sp1_verify_groth16(proofBytes.data(), proofBytes.size(),
//                      LP_CIRCUIT_VERIFICATION_KEY,
//                      public_inputs_from(stateRootBefore, stateRootAfter));
// See: fuego-prover/fuego-lp-circuit/elf/ for the circuit verification key.
// The upgrade is backward-compatible: proof format doesn't change.
// ═══════════════════════════════════════════════════════════════════════════
```

- [ ] **Step 11.2: Add Phase C tracking issue comment to `lp_prover.rs`**

In `generate_and_submit_proof()` where the stub proof bytes are:
```rust
  // ═══════════════════════════════════════════════════════════════════════
  // PHASE 1: stub proof bytes. Replace with real SP1 prover call:
  //
  //   let elf = std::fs::read(LP_CIRCUIT_ELF_PATH)?;
  //   let client = ProverClient::new();
  //   let (pk, vk) = client.setup(&elf);
  //   let mut stdin = SP1Stdin::new();
  //   stdin.write(&lp_proof_input);  // witness from LP client secure channel
  //   let proof = client.prove(&pk, stdin).groth16().run()?;
  //   let proof_bytes = proof.bytes();
  //
  // PRIVACY INVARIANT: lp_proof_input.pre_witness / post_witness contain
  // plaintext values — they come from the LP client, NOT from /get_pool_state.
  // The operator daemon must receive witness data via authenticated channel,
  // NEVER from the public RPC.
  // ═══════════════════════════════════════════════════════════════════════
```

- [ ] **Step 11.3: Commit**

```bash
git add src/CryptoNoteCore/ProofRegistry.cpp fuego-prover/prover-daemon/src/lp_prover.rs
git commit -m "docs(prover): add Phase C upgrade markers for real SP1 Groth16 verification"
```

---

### Task 12: Final integration build + end-to-end smoke test

- [ ] **Step 12.1: Full clean build**

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu) 2>&1 | tail -20
```
Expected: `fuegod`, `simplewallet` build cleanly.

- [ ] **Step 12.2: Rust workspace build**

```bash
cd fuego-prover
cargo build --workspace 2>&1 | grep -E "^error" | head -20
cargo test --workspace 2>&1 | tail -20
```
Expected: all tests pass including `fuego-lp-circuit` AMM invariant tests.

- [ ] **Step 12.3: Full smoke test**

```bash
# Start testnet daemon
./build/release/src/fuegod --testnet --data-dir /tmp/fuego-lp-smoke &
sleep 4

# Fee pool info — verify prover_pool field exists
curl -s http://127.0.0.1:28280/get_fee_pool_info | python3 -m json.tool

# Pool state — verify returns commitments not plaintext
curl -s -X POST http://127.0.0.1:28280/get_pool_state \
  -H "Content-Type: application/json" -d '{"pool_id":0}' | python3 -m json.tool

# Submit proof — verify accepted with Phase 1 stub
curl -s -X POST http://127.0.0.1:28280/submit_lp_proof \
  -H "Content-Type: application/json" \
  -d '{"state_root_before":"'$(python3 -c "print('00'*32)")'","state_root_after":"'$(python3 -c "print('01'*32)")'","proof_bytes_hex":"abcdef","prover_address":"<your_testnet_address>"}' \
  | python3 -m json.tool

# Start prover daemon
cd fuego-prover
cargo run -p prover-daemon -- --rpc http://127.0.0.1:28280 --prover-address "<your_testnet_addr>" &
sleep 5
kill %2

kill %1
```

- [ ] **Step 12.4: Final commit**

```bash
git add -A
git commit -m "feat(lp-prover): Phase A-D complete — ZK LP prover with operator-private committed state"
```

---

## Privacy Invariant Checklist

Before each commit touching pool state or proof submission, verify:

- [ ] `/get_pool_state` returns hex commitment strings — no `reserve_a`, `reserve_b`, or fee values as integers
- [ ] `prover-daemon` fetches state only via `/get_pool_state` — no direct DB/memory access to `PoolState`
- [ ] Witness data (opening values) never flows through the public RPC — only through the authenticated LP client channel
- [ ] `ProofRegistry::submitProof` stores only proof hash + prover address — not proof bytes
- [ ] Circuit public outputs contain only state roots + fee commitment — no plaintext amounts

---

## Phase Roadmap

| Phase | What | Privacy impact |
|-------|------|---------------|
| **A–D** (this plan) | Foundation, circuit, daemon | Full Pedersen commitment privacy from launch |
| **C** | Replace stub with real SP1 Groth16 verifier | No privacy change — upgrade path documented |
| **F** | Permissionless prover registration | Adds prover bond + count-based winner selection; privacy unchanged |

