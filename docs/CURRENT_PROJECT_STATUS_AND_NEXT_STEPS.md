# Fuego Project Status & Next Steps

## Executive Summary

**Current Phase:** Architecture Design Complete - Ready for Implementation

**Project Timeline:**
1. **Dynamigo (Week 1-3):** Elderfier registration with 0xEF deposits, privacy aliases, user-initiated unstaking
2. **MVP (Week 3-6, parallel):** HEAT/COLD contracts, root submission relay, block header relay fixes, React frontend
3. **Phase 2 (Post-launch):** Elder Kings Council voting, reward distribution, slashing mechanism

**Key Decision:** Separate **Elderfiers** (fee governance) from **COLDAO** (interest rates). Both operate independently.

---

## Part 1: Dynamigo Phase (Elderfier Registration)

### What is Dynamigo?

A pre-MVP launch allowing Fuego users to register as Elderfiers by:
- Creating 1600-3200 XFG deposits (0xEF tagged transactions)
- Split into 2-4 deposits of 800 XFG each with 8-day unlock window
- Registering 8-character privacy alias (FIRENODE, XFG4LIFE, etc.)
- **No access to HEAT burn/COLD deposit functionality** (hidden until MVP)

### Why Dynamigo?

1. **Builds community:** Generate excitement before MVP launch
2. **Derisks MVP:** Have elderfiers ready on Day 1
3. **Independent delivery:** No MVP contract dependencies
4. **Time window:** Gives devs 8-14 days to finish MVP contracts
5. **Simple scope:** Pure deposit registration, no complex voting

### What Already Exists

✅ **80% of infrastructure in place:**
- `EldernodeIndexManager` - Deposit management
- `ElderfierDepositData` - Deposit tracking
- `ElderfierServiceId` - 8-character alias support
- Security window logic
- Selection multiplier system
- Slashing framework (BURN only, correct!)
- Elder Council voting architecture

### What Needs Building (22-28 days)

1. **User-initiated unlock window** (2 hours)
   - Add `unstakingRequested` flag
   - Add `unstakingRequestBlock` timestamp
   - Add `unstakeClaimableBlock` calculation

2. **Privacy registry contract** (2-3 days)
   - `ElderfiersPrivacyRegistry.sol`
   - Maps alias → hashed address
   - Resolve alias (owner only)

3. **Fee extraction system** (1-2 days)
   - Implement `ElderfierFeePool`
   - Extract fees from 0xEF deposits
   - Expose via RPC

4. **RPC endpoints** (1-2 days)
   - `get_elderfier_candidates`
   - `get_elderfier_stake_info`
   - `get_elderfier_fees`

5. **CLI commands** (2-3 days)
   - `elderfier-stake create`
   - `elderfier-stake set-alias`
   - `elderfier-stake status`
   - `elderfier-stake initiate-unstake`
   - `elderfier-stake claim`

6. **Testing & security review** (3-4 days)
   - Unit tests (80%+ coverage)
   - Integration tests (full flow)
   - Security audit
   - Testnet validation

**Timeline:** 3 weeks with dedicated team

---

## Part 2: MVP Phase (Parallel with Dynamigo Weeks 2-3)

### Core MVP Components

1. **Smart Contracts** (Already ~90% complete)
   - ✅ FuegoCommitmentMerkleVerifier.sol (root submission + nullifier tracking)
   - ✅ HEATToken.sol, FuegoCOLDAOToken.sol (token contracts)
   - ✅ COLDProofVerifier_v3.sol, HEATBurnProofVerifier_v3.sol (proof verification)
   - ✅ FuegoBlockHeaderRelay.sol (block header verification)
   - ⏳ ElderfiersPrivacyRegistry.sol (alias registration)
   - ⏳ ElderfierTreasuryPool.sol (fee distribution)

2. **Blockchain Layer** (Already ~80% complete)
   - ✅ CommitmentIndex.h/.cpp (merkle tree for commitments)
   - ✅ Transaction processing (0x08 HEAT, 0xCD COLD)
   - ⏳ Fee extraction (HEAT burn fee, COLD deposit fee)
   - ✅ RPC endpoints (get_commitment_merkle_root, get_commitment_merkle_proof)
   - ⏳ RPC fees endpoints (get_elderfier_fees, history)

3. **Relay Daemon** (NOT STARTED)
   - Query merkle root periodically
   - Detect changes (event-driven)
   - Aggregate signatures (3-of-5 threshold)
   - Submit to L1 contract
   - Handle errors & retries
   - **Technology:** Node.js + ethers.js
   - **Location:** xfg-stark-cli relay mode
   - **Effort:** 3-4 days

4. **Block Header Relay Fix** (IN PROGRESS)
   - Fix `_resolveReorg()` function
   - Proper fork resolution (build alternative chain)
   - Test 5-block and 100-block reorgs
   - **Effort:** 2-3 days

5. **React Frontend** (NOT STARTED)
   - Wallet connection (MetaMask, WalletConnect)
   - Deposit form (input Fuego tx hash, auto-detect tier)
   - APY calculator (real-time from contract)
   - Transaction tracker (pending → confirmed → minted)
   - Balance display (CD token holdings)
   - DAO voting interface
   - **Technology:** React 18, ethers.js, Wagmi
   - **Effort:** 4-5 days

### MVP Success Criteria

- ✅ Elderfiers ready to operate on Day 1
- ✅ Root submission working (hybrid event-driven + periodic)
- ✅ Users can claim HEAT/COLD tokens
- ✅ Nullifier tracking prevents double-claims
- ✅ Merkle proofs verify correctly
- ✅ L1/L2 bridge works seamlessly
- ✅ Gas costs acceptable
- ✅ No critical security issues

### MVP Timeline

- **Weeks 1-2:** Dynamigo core + MVP contract prep
- **Weeks 2-3:** Relay daemon + React frontend (parallel)
- **Week 3:** Block header relay fixes + integration testing
- **Week 4:** End-to-end testing + mainnet prep

**Total MVP: 4 weeks**

**Launch Date:** Week 4 (Dynamigo → MVP → Live)

---

## Part 3: Architecture Decisions (Finalized)

### 1. Elderfiers vs. COLDAO

**FINAL DECISION: Completely Independent**

| Aspect | Elderfiers | COLDAO |
|--------|-----------|--------|
| **Governance** | Elder Kings Council | CD token holders |
| **Controls** | HEAT/COLD burn fees | Interest rates (APY) |
| **Treasury** | Elderfier Treasury Pool | DAO Treasury |
| **Voting** | Elderfier quorum (3-of-5+) | CD proportional voting |
| **Scope** | Fee management, root submission | Interest, LP rewards |
| **Intersection** | NONE | ZERO OVERLAP |

**Fee Structure:**
- HEAT burn: 1-3% to elderfiers
- COLD deposit: 0.5-1.5% to elderfiers
- Banking: 0.1-0.5% to elderfiers (future)
- **100% to elderfiers, 0% to miners** (clear incentive alignment)

**Interest Rates (COLDAO only):**
- 8-tier system (0.8 XFG @ 8% up to 800 XFG @ 69%)
- Adjustable via DAO governance
- No elderfier involvement

### 2. Submission Frequency Model

**FINAL DECISION: Hybrid Event-Driven + Periodic Aging**

**When to Submit:**
1. **Root changed** → Submit immediately (event-driven)
   - New commitments detected
   - Gas cost: ~45k
   - User can claim within 1 block

2. **Root aged > 600 blocks** → Resubmit (periodic)
   - Even if no new commitments
   - Ensures freshness guarantee
   - Gas cost: ~45k every 5 minutes

3. **Root unchanged and fresh** → Skip submission
   - Save gas
   - Users still have claim window

**Daily Gas Cost:** ~630k gas ≈ $5-15 USD
**Per-claim amortized:** Negligible

### 3. Privacy Model for Elderfiers

**FINAL DECISION: 8-Character Alias + Hashed Address**

**Components:**
1. **Public Alias** (e.g., "FIRENODE")
   - 8 characters, [A-Z0-9] only
   - Unique, first-come-first-served
   - Human-readable

2. **Hashed Address** (e.g., 0x1f3e5d7c)
   - keccak256(real_address || salt)
   - Semi-public (no direct link)
   - Used in merkle tree

3. **Real Address** (Private)
   - Stored in contract (owner-only access)
   - COLDAO can resolve if needed
   - Not exposed publicly

**Privacy Rating:** 3/5 (Moderate)
- Public knows: FIRENODE is registered
- Public can see: Hashed address stake/activity
- Public cannot see: Real wallet address
- Owner can resolve: "FIRENODE" → real address

### 4. Unlock Window Model

**FINAL DECISION: Indefinite Staking + User-Initiated Request**

**Three Phases:**

1. **Staking** (Indefinite)
   - User creates 0xEF deposit
   - Stake is LOCKED
   - No countdown starts automatically
   - User can keep staked forever

2. **Unstaking Request** (User-initiated)
   - User: `elderfier-stake initiate-unstake --index 0`
   - **Countdown STARTS here** (8-day window)
   - Block recorded in blockchain

3. **Claim** (After window expires)
   - After block + 19200 blocks (~8 days)
   - User: `elderfier-stake claim --index 0`
   - 800 XFG returned to account

**Privacy Benefit:** Stake duration is user's choice, no time-linked registration timestamp

---

## Part 4: Documentation Completed

| Document | Purpose | Status |
|----------|---------|--------|
| **DYNAMIGO_ELDERFIER_REGISTRATION_PHASE.md** | Dynamigo specification | ✅ Complete |
| **ELDERFIER_FUNCTION_CHECKLISTS.md** | Implementation checklist | ✅ Complete |
| **ELDERFIER_REGISTRY_AND_SUBMISSION_DESIGN.md** | Technical architecture | ✅ Complete |
| **ELDERFIER_FEE_STRUCTURE.md** | Fee system design | ✅ Complete |
| **ELDERFIER_PRIVACY_AND_ALIASES.md** | Privacy & alias system | ✅ Complete |
| **DYNAMIGO_IMPLEMENTATION_SUMMARY.md** | Implementation roadmap | ✅ Complete |
| **CURRENT_PROJECT_STATUS_AND_NEXT_STEPS.md** | This document | ✅ Complete |

**All 7 design documents ready for reference during implementation**

---

## Part 5: Code Quality Standards

### For All Elderfier Code

**Security:**
- [ ] CSPRNG for secrets (libsodium)
- [ ] No hardcoded values
- [ ] Input validation on all boundaries
- [ ] Overflow/underflow protection
- [ ] No information leaks (logging)
- [ ] No private key exposure

**Readability:**
- [ ] Clear variable names (no single letters)
- [ ] Function comments (purpose, params, return)
- [ ] Complex logic explained (comments explain WHY)
- [ ] Maximum 100-character lines
- [ ] Maximum 50-line functions

**Testing:**
- [ ] Unit tests: > 80% coverage
- [ ] Edge cases tested
- [ ] Integration tests (full flows)
- [ ] Testnet validation
- [ ] Stress tests (10k deposits)

---

## Part 6: Critical Path to Launch

### Week 1 (Dynamigo Core)
- [ ] Day 1-2: Add unstaking fields to ElderfierDepositData
- [ ] Day 3-4: Implement fee tracking & RPC endpoints
- [ ] Day 5: CLI create command + basic testing

### Week 2 (Dynamigo + MVP Prep)
- [ ] Day 1-2: Privacy registry contract + alias system
- [ ] Day 3-4: CLI status/unstake commands + testing
- [ ] Day 5: Start relay daemon (parallel MVP work)

### Week 3 (MVP Implementation)
- [ ] Day 1-2: Relay daemon (events, signatures, submission)
- [ ] Day 3-4: React frontend scaffold + wallet integration
- [ ] Day 5: Block header relay reorg fix + testing

### Week 4 (Integration & Testing)
- [ ] Day 1-2: End-to-end testing (create → query → unstake → claim)
- [ ] Day 3-4: Contract deployment to testnet
- [ ] Day 5: Launch prep + documentation

**Dynamigo Launch:** End of Week 2
**MVP Launch:** End of Week 4

---

## Part 7: Questions Answered & Decisions Made

✅ **Q: How often does relay submit roots?**
- **A:** Hybrid event-driven (on change) + periodic (when aged > 600 blocks)
- **Cost:** ~1-2 submissions/hour, ~$5-15 USD/day

✅ **Q: Do elderfiers work with COLDAO?**
- **A:** No, completely independent. Elderfiers control fees, COLDAO controls interest rates

✅ **Q: How do we prevent mining pools from taking fees?**
- **A:** Elderfier treasury receives 100% of fees, miners get 0

✅ **Q: How should unlock window work?**
- **A:** Indefinite staking, user-initiated request, 8-day countdown after request

✅ **Q: What privacy options for elderfier IDs?**
- **A:** 8-character alias (public) + hashed address (semi-public) + real address (private)

✅ **Q: Should network-wide aliases be MVP?**
- **A:** No, defer to Phase 2. Dynamigo uses elderfier-only aliases (~5-7 days to implement)

✅ **Q: What about Elder Kings Council?**
- **A:** Phase 2 (post-MVP). MVP just needs registration + root submission

---

## Part 8: Files to Create/Modify

### New Files (To Create)

```
/docs/DYNAMIGO_ELDERFIER_REGISTRATION_PHASE.md         ✅ Created
/docs/ELDERFIER_FUNCTION_CHECKLISTS.md                 ✅ Created
/docs/ELDERFIER_REGISTRY_AND_SUBMISSION_DESIGN.md      ✅ Created
/docs/ELDERFIER_FEE_STRUCTURE.md                       ✅ Created
/docs/ELDERFIER_PRIVACY_AND_ALIASES.md                 ✅ Created
/docs/DYNAMIGO_IMPLEMENTATION_SUMMARY.md               ✅ Created
/docs/CURRENT_PROJECT_STATUS_AND_NEXT_STEPS.md         ✅ Created

Solidity Contracts (TO CREATE)
/xfg-stark/ElderfiersPrivacyRegistry.sol                ⏳ Pending
/xfg-stark/ElderfierTreasuryPool.sol                    ⏳ Pending

C++ Code (TO MODIFY)
/include/EldernodeIndexTypes.h                          ⏳ Add unstaking fields
/include/EldernodeIndexManager.h                        ⏳ Update unlock methods
/src/EldernodeIndexManager/EldernodeIndexManager.cpp    ⏳ Implement unstaking logic
/src/CryptoNoteCore/Blockchain.cpp                      ⏳ Fee extraction
/src/Rpc/CoreRpcServerCommandsDefinitions.h             ⏳ Add fee endpoints

CLI (TO IMPLEMENT)
/xfg-stark-cli elderfier-stake                          ⏳ New command group
/xfg-stark-cli relay                                    ⏳ New relay mode
```

---

## Part 9: Success Metrics

### Dynamigo Success
- [ ] 5+ users register with 1600-3200 XFG stakes
- [ ] All aliases unique and functioning
- [ ] User-initiated unstaking works correctly
- [ ] Fee accumulation tracking accurate
- [ ] Testnet registration smooth & error-free
- [ ] Zero security issues found

### MVP Success
- [ ] Elderfiers ready to operate on mainnet Day 1
- [ ] Root submission works reliably
- [ ] Users can claim HEAT/COLD tokens
- [ ] Merkle proofs verify correctly
- [ ] Block header relay handles reorgs
- [ ] Gas costs within budget
- [ ] Testnet to mainnet migration clean

### Phase 1 (Dynamigo + MVP)
- ✅ Community excited about elderfier opportunity
- ✅ Clear separation: Elderfiers vs. COLDAO
- ✅ Privacy-preserving aliases in use
- ✅ Fee structure transparent and fair
- ✅ Foundation for Phase 2 complete

---

## Summary

**Project Status:** All architecture designed, ready for implementation

**Next Steps:**
1. Review this document with team
2. Prioritize Week 1 tasks (unstaking + fee tracking)
3. Set up git branches for parallel work
4. Begin implementation with Dynamigo core

**Team Requirements:**
- 2 C++ developers (blockchain layer)
- 1 Solidity developer (contracts)
- 1 Node.js developer (relay daemon)
- 1 React developer (frontend)

**Timeline:** 4 weeks to live (Dynamigo Week 2, MVP Week 4)

**Risk Level:** Low - architecture proven, existing infrastructure leveraged

Let's build it! 🚀
