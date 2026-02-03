# COLD-STARKs Branch Summary

**Branch:** `cold-starks`
**Status:** ✅ Ready for testnet deployment
**Date:** 2026-01-17

---

## ✅ **What's Ready**

### **1. Smart Contracts (Solidity)**

#### **COLDDepositProofVerifier.sol** ✅
- Amount×time-based tiers (combines deposit size with lock period)
- 8 tiers: 4 XFG amounts × 2 lock periods
- API verification (usexfg.org backend validates basic existence of transaction containing deposit on Fuego blockchain by RPC to Eldernode's http API server)
- Submit api verification with user generated STARK proof to contract
- L2→L1 message via Arbitrum
- Supports both mainnet and testnet network IDs
- Legacy deposit support: 800 XFG before 2026 @ 80% APY
- Simple: tier (0-7) + timestamp → CD amount out

**Fixed CD amounts (amount×time-based):**
```solidity
// Standard tiers (12 decimals)
TIER0_CD_INTEREST = 640_000;          // 0.8 XFG × 3mo @ 8%
TIER1_CD_INTEREST = 2_160_000;        // 0.8 XFG × 12mo @ 27%
TIER2_CD_INTEREST = 14_400_000;       // 8 XFG × 3mo @ 18%
TIER3_CD_INTEREST = 26_400_000;       // 8 XFG × 12mo @ 33%
TIER4_CD_INTEREST = 216_000_000;      // 80 XFG × 3mo @ 27%
TIER5_CD_INTEREST = 336_000_000;      // 80 XFG × 12mo @ 42%
TIER6_CD_INTEREST = 2_640_000_000;    // 800 XFG × 3mo @ 33%
TIER7_CD_INTEREST = 5_520_000_000;    // 800 XFG × 12mo @ 69%

// Legacy tiers (only 800 XFG before 2026)
LEGACY_TIER6_CD = 6_400_000_000;      // 800 XFG × 3mo @ 80%
LEGACY_TIER7_CD = 6_400_000_000;      // 800 XFG × 12mo @ 80%
```

**Network IDs:**
```solidity
FUEGO_MAINNET_NETWORK_ID = 93385046440755750514194170694064996624;
FUEGO_TESTNET_NETWORK_ID = 112015110234323138517908755257434054688; // "TEST FUEGO NET  "
```

#### **FuegoCOLDAOToken.sol** ✅
- Simplified `mintFromL2(commitment, recipient, editionId, cdAmount, version)`
- No xfgPrincipal tracking (excessive/already tracked on Fuego)
- Multi-minter support (for LP rewards)
- ERC-1155 with editions

#### **COLDAOGovernor.sol** ✅
- DAO governance for CD holders
- APY voting (not used on-chain, just governance)
- Edition management
- Already existed, no changes needed

---

## 📋 **What's NOT in This Branch**

### **LP Rewards** ❌
- LPRewardsManager is on main branch
- Not needed for COLD deposits MVP
- Will integrate later when merging branches

### **Eldernode Verification** ❌
- Removed for MVP
- API verification instead
- Simpler, faster, cheaper

### **On-Chain APY Calculation** ❌
- CD amount tiers with pre-calculated APY tiers
- Hardcoded in contract
- No gas-expensive calculations

---

## 🎯 **How It Works**

```
1. User deposits XFG on Fuego testnet
   ↓
2. Generate STARK proof locally (xfg-stark-cli)
   ↓
1. Submit proof thru usexfg.org after API verification 
   ↓
3. API validates Fuego deposit txn exists on-chain
   ↓
5. Submits api txn verification with STARK proof to contract  COLDDepositProofVerifier.claimCD() on Arbitrum
   ↓
6. Verifier sends L2→L1 message via ARB_SYS
   ↓
7. COLDAOToken.mintFromL2() called by Arbitrum Outbox
   ↓
8. CD tokens minted to user on Ethereum L1
```

---

## 🚀 **Ready to Deploy**

### **Networks:**
1. **Fuego Testnet** - XFG deposits happen here
2. **Arbitrum Sepolia** - COLDDepositProofVerifier lives here
3. **Ethereum Sepolia** - FuegoCOLDAOToken lives here

### **Deployment Order:**
1. Deploy FuegoCOLDAOToken on Ethereum Sepolia
2. Deploy COLDAOGovernor on Ethereum Sepolia
3. Deploy COLDDepositProofVerifier on Arbitrum Sepolia
4. Configure: authorize minter, set API verifier
5. Test end-to-end flow

**See:** `COLD_TESTNET_DEPLOYMENT.md` for step-by-step guide

---

## 🧪 **Testing Checklist**

- [ ] Deploy all contracts to testnets
- [ ] Verify on block explorers
- [ ] Configure minter authorization
- [ ] Set API verifier address
- [ ] Test tier 0 (0.8 XFG × 3mo @ 8%)
- [ ] Test tier 1 (0.8 XFG × 12mo @ 27%)
- [ ] Test tier 2 (8 XFG × 3mo @ 18%)
- [ ] Test tier 3 (8 XFG × 12mo @ 33%)
- [ ] Test tier 4 (80 XFG × 3mo @ 27%)
- [ ] Test tier 5 (80 XFG × 12mo @ 42%)
- [ ] Test tier 6 (800 XFG × 3mo @ 33%)
- [ ] Test tier 7 (800 XFG × 12mo @ 69%)
- [ ] Test legacy tier 6 (800 XFG × 3mo @ 80% pre-2026)
- [ ] Test legacy tier 7 (800 XFG × 12mo @ 80% pre-2026)
- [ ] Verify CD balances on L1
- [ ] Test nullifier replay protection
- [ ] Test commitment replay protection
- [ ] Test timestamp validation for legacy detection
- [ ] Measure gas costs
- [ ] Document any issues

---

## 📁 **Files in This Branch**

**New Contracts:**
- `COLDDepositProofVerifier.sol` - L2 verifier (Arbitrum)
- `COLD_TESTNET_DEPLOYMENT.md` - Deployment guide
- `COLD_BRANCH_SUMMARY.md` - This file

**Modified Contracts:**
- `FuegoCOLDAOToken.sol` - Simplified mintFromL2

**Unchanged (Already Existed):**
- `COLDAOGovernor.sol` - DAO governance
- `interfaces/ICOLDAOGovernor.sol` - Interface

**Not Included (On Main Branch):**
- `LPRewardsManager.sol` - LP rewards (separate contract)
- `HEATBurnProofVerifier.sol` - HEAT system (separate)
- `HEATToken.sol` - HEAT system (separate)

---

## 🔜 **Next Steps**

### **Immediate:**
1. ✅ Review contracts
2. ⏳ Deploy to testnets
3. ⏳ Test end-to-end flow
4. ⏳ Fix any issues

### **Short-term:**
1. Build usexfg.org API backend
2. Create STARK proof generator for COLD
3. Integrate with frontend
4. Documentation

### **Long-term:**
1. Security audit
2. Mainnet deployment
3. Merge with main branch (HEAT + COLD unified)
4. Add LP rewards back
5. On-chain Elderfier verification 

---

## 🎨 **Design Decisions**

### **Why Fixed CD Amounts?**
- ✅ Simpler contract logic
- ✅ Lower gas costs
- ✅ Easier to test
- ✅ Mirrors HEAT exactly
- ❌ Cannot change APY without redeployment

**Solution:** DAO governance can deploy new editions with different rates

### **Why API Verification?**
- ✅ MVP approach - ship faster
- ✅ Much cheaper (no on-chain STARK verification)
- ✅ Can upgrade later to on-chain
- ❌ Requires trusting usexfg.org (API just does RPC to Eldernode's http /getinfo /gettransactionbyhash or whatever to look up deposit txn)

**Solution:** Phase 2 may? add pre-verification ?

### **Why Separate from HEAT?**
- ✅ Prevents breaking HEAT system
- ✅ Easier to test independently
- ✅ Can merge later when stable
- ❌ Code duplication

**Solution:** Will unify COLD/HEAT in same version (all-in-one xfg-stark-cli)

---

## 📞 **Questions?**

- Check `COLD_TESTNET_DEPLOYMENT.md` for deployment
- Check contract natspec comments for details
- Open GitHub issue for bugs

---

**Status:** ✅ **Ready to deploy to testnet!**

**Winter is coming. ❄️**
