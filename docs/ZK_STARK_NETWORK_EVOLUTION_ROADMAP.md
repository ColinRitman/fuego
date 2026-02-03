# ZK-STARK Network Alias Evolution - Full Roadmap

## Vision

**Complete Evolution Path:** Hash-based aliases → STARK-based privacy → Network-wide ZK implementation

**Three Phases:**
1. **Phase 1 (MVP):** Simple 8-char aliases + hashed addresses on Fuego
2. **Phase 2 (Enhanced):** Optional zk-STARK proofs for elderfiers
3. **Phase 3+ (Full Network):** Fuego-wide ZK alias system replacing hashing

---

## Part 1: Phase 1 (Current - MVP)

### Simple Model (Week 1-2)

```
User Address: XFG1a2b3c4d5e6f...
    ↓
Alias: FIRENODE (public, 8 chars)
    ↓
Hashed: keccak256(address) = 0x1f3e5d7c (public)
    ↓
Real address: Private (optional publication)

Privacy Rating: 3/5 (moderate - hash not reversible but correlatable)
```

**Storage:** On-Fuego-chain transaction extra (0xEA tag)

**Cost:** ~2-3 days to implement

**Limitations:**
- Hash can be correlated with address
- If address ever published, hash reveals identity
- No cryptographic privacy guarantee

---

## Part 2: Phase 2 (Enhanced - Post-MVP)

### Optional ZK-STARK Proofs for Elderfiers

**Goal:** Add zk-STARK proof layer without removing hashing system

```
User Address: XFG1a2b3c4d5e6f...
    ↓
Alias: FIRENODE (public, 8 chars)
    ↓
Optional: Generate zk-STARK proof proving ownership
          without revealing address
    ↓
CommitmentIndex (Fuego): Verifies proof locally
    ↓
Result: Alias is cryptographically proven,
        address remains completely hidden

Privacy Rating: 5/5 (perfect - address completely private)
```

**NOTE:** Fuego is CryptoNote (no smart contracts). Proof verification happens in CommitmentIndex validation during block processing, not in a contract.

### Architecture

**Two-tier system:**

1. **Basic Tier (Free):** Hashed address model
   - Simple, fast
   - Privacy 3/5

2. **Premium Tier (Optional):** ZK-STARK proof model
   - Cryptographically private
   - Privacy 5/5
   - ~2-3 weeks to implement

**Backwards compatible:** Both systems work together

### Implementation Approach

```
User wants maximum privacy (Phase 2):
  1. Register alias FIRENODE (0xEA transaction with zk-STARK proof)
  2. Generate zk-STARK proof locally
  3. Include proof in tx_extra
  4. CommitmentIndex verifies proof during block validation
  5. Alias marked as "zk-verified" if proof valid
  6. Address hidden (only commitment hash on-chain)

User wants fast registration (Phase 1):
  1. Register alias FIRENODE (0xEA transaction, hashed model)
  2. Done
  3. Can upgrade later with zk-proof if Phase 2 available
```

### How to Extend xfg-stark for This

**Key insight:** We don't need general-purpose zk-STARKs.
We only need **specific** zk-STARK for alias proofs.

```
Current xfg-stark:
├── HEAT burn proof (proves burn amount)
├── COLD deposit proof (proves lock amount)
└── Merkle proofs (proves commitment in tree)

New xfg-stark (Phase 2):
├── (all above)
└── Alias proof (proves I own address without revealing it)
```

### Efficient STARK-Based Alias Proof

**Instead of extending Winterfell for ECDSA (expensive):**

Use a simpler model tailored to aliases:

```rust
// circuits/alias_proof.rs
// Proof: I know the secret that derives this alias
//        AND I own the address that corresponds to it

use winterfell::{Air, TraceTable, Trace};

struct AliasProof {
    // Secrets (never revealed)
    secret: [u64; 4],           // Random secret
    private_key: [u64; 32],     // ECDSA private key

    // Public inputs
    alias_hash: [u64; 4],       // Public alias
    address_hash: [u64; 4],     // Public address hash

    // Trace: Execution of hashing and signing
    trace: TraceTable,
}

impl Air for AliasProof {
    fn trace_columns(&self) -> usize {
        // Hash computation steps
        // ECDSA signature steps
        // Verification steps
        256  // Number of columns
    }

    fn constraints(&self, frame: &EvaluationFrame) -> Vec<Polynomial> {
        // Constraint 1: secret → alias_hash (via Poseidon)
        // Constraint 2: private_key → public_key (via ECDSA)
        // Constraint 3: public_key → address_hash (via Poseidon)
        // Constraint 4: Verify all hashes match
    }
}
```

**Key advantage:** Smaller proof than general ECDSA
- Specific to our use case
- Optimized for Poseidon hash (native in STARKs)
- Can use ECDSA signature (already computed locally)

**Estimated proof size:**
- Generic Winterfell ECDSA: 500 KB - 2 MB (too large)
- **Optimized alias proof: 50-100 KB** (reasonable)

**Estimated gas cost:**
- Generic: 5-10M gas (too expensive)
- **Optimized: 500k-1M gas** (expensive but acceptable for premium tier)

### Timeline for Phase 2

**Weeks 1-2:** Research optimal STARK circuit for aliases
**Weeks 3-4:** Implement alias-specific STARK proof
**Week 5:** Smart contract verifier
**Week 6:** Integration with xfg-stark-cli
**Week 7:** Testing & optimization

**Total: 7 weeks post-MVP**

---

## Part 3: Phase 3 (Network-Wide ZK Evolution)

### Vision: Fuego-Wide Privacy Alias System

**Goal:** Eventually replace hashing model with zk-STARKs for entire network

### Long-term Architecture

```
Phase 1 (Now):
  Address → Alias (plain)
  Address → Hash (keccak256)
  Privacy: Moderate

Phase 3 (Future):
  Address → Alias (plain)
  Address → Zk-STARK proof of ownership
  Privacy: Perfect

Bonus: ALL users can use aliases (not just elderfiers)
```

### System-Wide Implementation Strategy

**Step 1: Make alias registration universal** (Phase 2 end)
- All Fuego users, not just elderfiers
- Optional zk-proof layer for privacy
- Backwards compatible with hashing

**Step 2: Optimize STARK proofs** (Phase 3)
- Reduce proof size (current: 50-100 KB → target: 1-10 KB)
- Reduce proof generation time
- Batched proof verification

**Step 3: Smart contract optimization** (Phase 3)
- Aggregate multiple alias proofs in one L1 transaction
- Reduce per-alias gas cost
- Implement proof caching

**Step 4: Network incentives** (Phase 3)
- Reward users for using zk-proofs
- Zk-proof fee rebate (e.g., 50% refund)
- Better UI for zk-proof generation

### How to Optimize Proof Size Over Time

**Current approach (Winterfell):**
```
Trace: 256 columns × 100k rows = 25M field elements
Proof: ~100 KB per proof
```

**Optimization 1: Specialized circuit** (Week 1-2)
```
Trace: 32 columns × 10k rows = 320k field elements
Proof: ~10 KB per proof
Improvement: 10x smaller
```

**Optimization 2: PLONK-style aggregation** (Phase 3)
```
Multiple proofs in one:
Proof size: ~10 KB + 1 KB per additional proof
Improvement: Amortized cost → 100 byte per extra proof
```

**Optimization 3: Recursive STARKs** (Phase 3)
```
Compress proof of proof:
Final proof: ~1-5 KB (independent of number of users)
Improvement: Constant size, scales indefinitely
```

### Network Timeline

```
Phase 1 (Now):        Hashing model for elderfiers
                      Effort: 2-3 days
                      Privacy: 3/5

Phase 2 (MVP+6wk):    Optional zk-STARKs for elderfiers
                      Effort: 7 weeks
                      Privacy: 5/5 (opt-in)

Phase 3 (MVP+6mo):    Network-wide aliases (all users)
                      Optional zk-STARKs for everyone
                      Effort: 4-8 weeks
                      Privacy: 5/5 for anyone who uses it

Phase 4+ (Future):    Fully zk-based network
                      Default privacy for everyone
                      Effort: Ongoing optimization
                      Privacy: Perfect (default)
```

---

## Part 4: Proof Efficiency Roadmap

### Current State (Phase 1)

```
Hashing model:
  - Proof: None (just use hash)
  - Size: 0 (hash verification only)
  - Cost: Free (RPC query)
  - Privacy: 3/5
```

### Phase 2 Goal

```
Zk-STARK proof (eldernode only):
  - Proof size: 50-100 KB
  - Proof generation time: 5-30 seconds
  - Verification: On-chain (~500k-1M gas)
  - Privacy: 5/5 (optional)
  - Adoption: ~5% of users (privacy-conscious elderfiers)
```

### Phase 3 Goal

```
Optimized zk-STARK proof (network-wide):
  - Proof size: 1-10 KB (10-100x smaller)
  - Proof generation time: 100-500 ms (faster)
  - Verification: On-chain (~50k-100k gas, 10x cheaper)
  - Privacy: 5/5 (adopted by 30-50% of users)
  - Batch verification: Multiple proofs → 1 on-chain tx
```

### Phase 4+ Goal

```
Recursive zk-STARKs (theoretical maximum):
  - Proof size: 1-5 KB (constant, regardless of users)
  - Proof generation: Off-chain, delegated
  - Verification: On-chain (~10k-20k gas, constant)
  - Privacy: 5/5 (adopted by 90%+)
  - Scalability: Unlimited users, fixed L1 cost
```

### How to Get There

**Key insight:** Use existing xfg-stark infrastructure as foundation

```
1. Current xfg-stark:
   - Winterfell framework (good for algebraic proofs)
   - SHA-256, Poseidon hashing (efficient)
   - Merkle tree verification (working)

2. Extend for alias proofs (Phase 2):
   - Add alias-specific circuit (smaller, simpler)
   - Reuse existing hash constraints
   - Optimize for size

3. Implement recursive proofs (Phase 3):
   - Prove a proof is valid (STARK of STARK)
   - Reduces final proof to constant size
   - Uses existing verifier as building block

4. Deploy network-wide (Phase 4):
   - All users can register zk-STARK aliases
   - Batch multiple proofs into single L1 call
   - Incentivize adoption with fee rebates
```

---

## Part 5: Comparison: Hash vs. ZK-STARK

### MVP (Phase 1): Hashing

```
User: XFG1a2b3c4d5e6f...
Alias: FIRENODE
Hash: keccak256(address) = 0x1f3e5d7c

Pros:
  ✅ Simple (2-3 days to implement)
  ✅ Fast (instant verification)
  ✅ No trusted setup needed
  ✅ Immutable record on-chain

Cons:
  ❌ Hash could be reversed (if address leaked)
  ❌ Correlatable (same address → same hash)
  ❌ No cryptographic privacy guarantee

Use case: MVP, acceptable for launch
Privacy: 3/5 (moderate)
Cost: Free
```

### Phase 2: Optional ZK-STARK Proofs

```
User: XFG1a2b3c4d5e6f...
Alias: FIRENODE
Zk-Proof: I own this address (without revealing it)

Pros:
  ✅ Perfect cryptographic privacy
  ✅ Address completely hidden
  ✅ Can be verified on-chain
  ✅ Compatible with hashing (optional layer)

Cons:
  ❌ Proof generation: 5-30 seconds
  ❌ Proof size: 50-100 KB
  ❌ Verification cost: 500k-1M gas
  ❌ Complex circuit design (1-2 weeks)

Use case: Privacy-conscious users, optional
Privacy: 5/5 (perfect)
Cost: ~$5-20 per registration (for gas)
```

### Phase 3+: Network-Wide Optimized ZK

```
User: XFG1a2b3c4d5e6f...
Alias: FIRENODE
Zk-Proof: Optimized (1-10 KB, fast generation)

Pros:
  ✅ Perfect cryptographic privacy
  ✅ 10-100x smaller proofs
  ✅ 10x cheaper verification (50k-100k gas)
  ✅ Practical for network-wide adoption

Cons:
  ❌ More complex circuit optimization
  ❌ Longer development time (weeks)
  ❌ Still need trusted setup? (depends on approach)

Use case: Full network adoption
Privacy: 5/5 (perfect)
Cost: ~$0.50-5 per registration (if optimized well)
```

---

## Part 6: Implementation Roadmap with xfg-stark

### Can We Use Current xfg-stark?

**Yes, but needs optimization:**

```
Current xfg-stark:
  - Designed for HEAT/COLD proofs
  - Uses Winterfell for algebraic constraints
  - Good for: Merkle trees, hashing, field arithmetic
  - Not optimized for: ECDSA (expensive), general-purpose circuits

For alias proofs:
  1. We need: Prove "I own this address" without revealing it
  2. We don't need: Full ECDSA verification (user has signature offline)
  3. Optimization: Use Poseidon hash (native in STARKs) instead of SHA-256

Result: Can extend xfg-stark, but needs specialized circuit
```

### Phased xfg-stark Evolution

**Phase 1 (Current):**
```
xfg-stark/
├── HEAT burn proofs ✅
├── COLD deposit proofs ✅
└── Merkle proofs ✅
```

**Phase 2 (Add alias proofs):**
```
xfg-stark/
├── (all current)
└── Alias ownership proofs ← NEW
    ├── Poseidon hash constraints
    ├── ECDSA signature constraints
    └── Alias derivation proof
```

**Phase 3 (Optimize for network):**
```
xfg-stark/
├── (all current)
├── Alias ownership proofs (optimized)
├── Batch proof verification
└── Recursive STARK proofs ← NEW (compress proofs)
```

### Estimated Development

| Phase | Task | Effort | Timeline |
|-------|------|--------|----------|
| 1 | Current xfg-stark | ✅ Complete | Complete |
| 2 | Add alias circuits | 3-4 weeks | After MVP |
| 2 | Smart contract verifier | 1 week | After MVP |
| 3 | Proof optimization | 2-3 weeks | Phase 3 |
| 3 | Recursive STARKs | 3-4 weeks | Phase 3 |
| 3 | Network-wide deployment | 2-3 weeks | Phase 3 |

---

## Part 7: Recommended Path Forward

### MVP (Weeks 1-4)

```
✅ Simple hashing model (FIRENODE → 0x1f3e5d7c)
✅ Stored on Fuego chain (0xEA transactions)
✅ Full elderfier registration working
✅ 2-3 days to implement
✅ Privacy 3/5 (acceptable for launch)
```

### Phase 2 (Weeks 5-10, After MVP)

```
✅ Optional zk-STARK alias proofs for elderfiers
✅ Extends xfg-stark with alias circuit
✅ Smart contract verifier on L1
✅ ~7 weeks development
✅ Privacy 5/5 (for opt-in users)
✅ Optional fee tier ($5-20 per registration)
```

### Phase 3 (Months 4-6)

```
✅ Expand aliases to all Fuego users
✅ Optimize proof size (50-100 KB → 1-10 KB)
✅ Reduce gas cost (500k-1M → 50k-100k)
✅ Batch proof verification
✅ ~6-8 weeks development
✅ ~30-50% user adoption
```

### Phase 4+ (Future)

```
✅ Recursive STARKs (constant proof size)
✅ Network-wide zk-STARK default
✅ Ongoing optimization
✅ Perfect privacy for everyone
```

---

## Part 8: Decision: Hash Now, ZK Later

### Why Start with Hashing

**Pro:**
- ✅ Fast to implement (2-3 days)
- ✅ Doesn't delay MVP launch
- ✅ Immediately usable
- ✅ Can upgrade later
- ✅ Low risk (simple code)

**Con:**
- ❌ Moderate privacy (3/5)
- ❌ Hash-based approach has known weaknesses

### Why Defer ZK-STARKs

**Pro:**
- ✅ Gives team time to optimize circuit design
- ✅ Can implement after MVP stabilizes
- ✅ Allows community feedback on hashing model
- ✅ Reduces MVP scope (critical for launch)

**Con:**
- ❌ Delayed privacy enhancement (5-10 weeks)
- ❌ Users must wait for better privacy

### Recommendation: Hybrid Approach

```
MVP: Deploy hashing model
     ↓
Week 1: Announce Phase 2 zk-STARK plan
        Community knows better privacy is coming
        ↓
Weeks 5-10: Implement optional zk-STARK layer
            Users can upgrade aliases to zk-proofs
            ↓
Phase 3: Full network-wide optimization
         Eventually replace hashing entirely
```

---

## Summary

**Phase 1 (MVP):** Simple aliases with hashing (2-3 days)
- Privacy: 3/5
- Cost: Free
- Effort: Minimal

**Phase 2 (Enhanced):** Optional zk-STARK proofs (7 weeks post-MVP)
- Privacy: 5/5 (optional)
- Cost: $5-20 per registration
- Effort: Moderate

**Phase 3+ (Full Network):** Optimized zk-STARKs for everyone (future)
- Privacy: 5/5 (default)
- Cost: <$5 per registration (optimized)
- Effort: Ongoing

**This enables gradual evolution from hashing → perfect privacy via zk-STARKs**

---

## Action Items

### For MVP
- [ ] Implement hashing model (0xEA transactions)
- [ ] Create RPC endpoints for aliases
- [ ] Build CLI commands
- [ ] Launch with hashing

### For Phase 2 Planning
- [ ] Research optimal STARK circuit for aliases
- [ ] Study Winterfell constraints for ECDSA
- [ ] Plan smart contract verifier
- [ ] Prototype zk-STARK generation

### For Phase 3+ Planning
- [ ] Design proof optimization strategy
- [ ] Plan recursive STARK implementation
- [ ] Design network-wide rollout
- [ ] Plan incentive structure

This creates a clear evolutionary path from simple hashing → sophisticated zk-STARKs!
