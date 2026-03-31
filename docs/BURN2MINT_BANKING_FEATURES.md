# Burn2Mint (HEAT) & COLD Banking Features — v3 Unified EF Sigs

## Overview

The Fuego Electron Wallet includes two DeFi features that bridge XFG to Ethereum L1 via Arbitrum L2:

1. **Burn2Mint (HEAT)** — Permanently burn XFG on Fuego; mint HEAT ERC-20 on C0DL3 rollup
2. **COLD Banking** — Lock XFG for 3 or 12 months; mint CD interest tokens (ERC-1155) on Ethereum L1

Both use **v3 unified STARK commitment format** with **Elderfier (EF) signature consensus** for
decentralized cross-chain verification. The API backend (Option B MVP) handles proof validation
until full Eldernode relay (Option A) is deployed.

---

## STARK Commitment Format (v3 Unified)

All commitments — HEAT and COLD — use the same preimage structure:

```
preimage (56 bytes) = secret[32] || amount[8-LE] || networkId[4-LE] || chainId[4-LE] || version[4-LE] || term[4-LE]
commitment = keccak256(preimage)
nullifier  = keccak256(secret || "nullifier" || amount)
```

- **version = 3** for all v3 deposits
- **term = 0xFFFFFFFF** for HEAT (permanent burn); term = block-height-based for COLD

---

## Elderfier (EF) Signature Consensus

Active Elderfiers (registered via `elderking_ceremony`, 5 × 800 XFG staked) sign the
CommitmentIndex merkle root after each block containing deposits:

- Signatures broadcast via P2P gossip (`ElderfierSignatureBroadcaster`)
- Consensus threshold: **≥ 69%** of active Elderfiers must sign
- Max active Elderfiers: **8** (for meaningful reward splits)
- Epoch rotation: **1,234 blocks** (~7 days)

The xfg-stark-cli bundles merkle proof + EF signatures into a `CompleteProofPackage` for
submission to the L2 contract.

---

## 🔥 Burn2Mint (HEAT)

### What It Does

Burns XFG permanently on Fuego (`0x08` tag in tx_extra). Mints HEAT ERC-20 tokens
on C0DL3 rollup (Arbitrum L2 → Ethereum L1 via ARB_SYS).

**Conversion rate: 1 XFG = 10,000,000 HEAT**

### HEAT Tier Structure (4 Tiers, Amount-Based)

| Tier | XFG Burned | HEAT Minted | Atomic XFG |
|------|-----------|-------------|------------|
| 0 | 0.8 XFG | 8,000,000 HEAT | 8,000,000 |
| 1 | 8 XFG | 80,000,000 HEAT | 80,000,000 |
| 2 | 80 XFG | 800,000,000 HEAT | 800,000,000 |
| 3 | 800 XFG | 8,000,000,000 HEAT | 8,000,000,000 |

- HEAT has **18 decimals** (standard ERC-20)
- **Mainnet only** (no testnet HEAT)

### User Flow (v3)

1. **Create burn deposit** — Wallet sends XFG with `0x08` tag; secret + commitment generated locally
2. **Wait for EF consensus** — ≥69% of Elderfiers sign the merkle root containing the commitment
3. **Bundle proof** — `xfg-stark bundle` fetches merkle proof + EF signatures from any synced node
4. **Submit to L2** — User calls `claimHEAT()` on `HEATBurnProofVerifier` on Arbitrum

### IPC Handlers (main.js)

```javascript
ipcMain.handle('create-heat-burn', async (event, { tier }) => {
  // tier 0-3; creates burn_deposit tx with 0x08 tag
  // returns { txHash, commitment, nullifier }
});

ipcMain.handle('get-ef-consensus-status', async (event, { commitment }) => {
  // queries /get_commitment_stats via fuegod RPC
  // returns { consensusPercent, thresholdMet, signedEFIds }
});

ipcMain.handle('bundle-heat-proof', async (event, { commitment, recipient }) => {
  // calls xfg-stark bundle --commitment <hash> --recipient <addr>
  // returns CompleteProofPackage JSON
});

ipcMain.handle('get-heat-burn-history', async (event, { limit }) => {
  // filters transactions for 0x08 tag
  // returns list of burns with EF consensus status
});
```

### Security

- Nullifier prevents double-spend (on-chain in `HEATBurnProofVerifier.nullifiersUsed`)
- Merkle proof proves burn commitment exists in CommitmentIndex tree
- EF signatures prove ≥69% consensus on that merkle root
- Minimum burn: 0.8 XFG (Tier 0)

---

## ❄️ COLD Banking

### What It Does

Locks XFG on Fuego for 3 or 12 months (`0xCD` tag in tx_extra). Mints CD **interest** tokens
(ERC-1155, `FuegoCOLDAOToken`) on Ethereum L1. Principal unlocks after the lock period.

**Only interest is minted — the principal (XFG) stays locked and returns at maturity.**

### COLD Tier Structure (8 Tiers = 4 Amounts × 2 Terms)

Tier encoding: `tier = (amountIndex × 2) + termIndex`

| Tier | XFG Amount | Lock | APY | CD Interest (atomic, 12 dec) |
|------|-----------|------|-----|------------------------------|
| 0 | 0.8 XFG | 3 months | 8% | 640,000 |
| 1 | 0.8 XFG | 12 months | 27% | 2,160,000 |
| 2 | 8 XFG | 3 months | 18% | 14,400,000 |
| 3 | 8 XFG | 12 months | 33% | 26,400,000 |
| 4 | 80 XFG | 3 months | 27% | 216,000,000 |
| 5 | 80 XFG | 12 months | 42% | 336,000,000 |
| 6 | 800 XFG | 3 months | 33% | 2,640,000,000 |
| 7 | 800 XFG | 12 months | 69% | 5,520,000,000 |

**Legacy bonus (deposits before 2026-01-01 00:00:00 UTC, tiers 6 & 7 only):**

| Tier | XFG Amount | Lock | APY | CD Interest (atomic) |
|------|-----------|------|-----|----------------------|
| 6 | 800 XFG | 3 months | **80%** | 6,400,000,000 |
| 7 | 800 XFG | 12 months | **80%** | 6,400,000,000 |

### User Flow (v3, Option B MVP)

1. **Create COLD deposit** — Wallet sends XFG with `0xCD` tag; tier, secret, commitment generated
2. **Wait for EF consensus** — ≥69% Elderfiers sign the merkle root
3. **Get domain signature** — Frontend submits nullifier to `POST /api/cold/claim`; API verifies
   commitment on Fuego RPC (`check_commitment_exists`) and returns Ed25519 domain signature
4. **Submit to L2** — User calls `claimCD()` on `COLDProofVerifier_v3` with domain signature
5. **L1 minting** — L2 contract sends ARB_SYS message; `FuegoCOLDAOToken.mintFromL2` mints CD
6. **Unlock principal** — After lock period expires, user withdraws XFG principal on Fuego

### IPC Handlers (main.js)

```javascript
ipcMain.handle('create-cold-deposit', async (event, { tier }) => {
  // tier 0-7; creates cold_deposit tx with 0xCD tag
  // returns { txHash, commitment, nullifier, tier, lockMonths }
});

ipcMain.handle('get-cd-deposits', async () => {
  // calls getDeposits RPC
  // returns list with tier info, maturity status, CD interest amounts
});

ipcMain.handle('withdraw-cd-deposit', async (event, { depositId }) => {
  // calls withdrawDeposit RPC (after lock period expires)
  // returns principal XFG back to wallet
});

ipcMain.handle('claim-cd-interest', async (event, { nullifier, walletAddress }) => {
  // submits to /api/cold/claim for domain signature
  // then user calls claimCD() on Arbitrum L2
  // returns { domainSignature, contractAddress }
});
```

### CD Token Details

- **CD decimals:** 12 (1 CD = 10^12 atomic units)
- **Supply ratio:** 1 CD : 100,000 XFG (base ratio before APY)
- **Token standard:** ERC-1155 (`FuegoCOLDAOToken`)
- **Network:** Ethereum L1 (via Arbitrum L2 bridge)

---

## Integration Points

### Burn2Mint Ecosystem
```
Fuego Blockchain (burn tx with 0x08 tag)
    -> CommitmentIndex merkle tree updated
    -> Elderfiers sign merkle root via P2P (≥69% threshold)
    -> xfg-stark-cli bundles proof + EF signatures
    -> Arbitrum L2: HEATBurnProofVerifier.claimHEAT()
    -> ARB_SYS L2->L1 message
    -> Ethereum L1: EmbersTokenHEAT.mintFromL2()
    -> User's ETH wallet receives HEAT
```

### COLD Banking Ecosystem
```
Fuego Blockchain (deposit tx with 0xCD tag)
    -> CommitmentIndex merkle tree updated
    -> Elderfiers sign merkle root via P2P (≥69% threshold)
    -> User submits nullifier to /api/cold/claim (Option B MVP)
    -> API checks check_commitment_exists RPC -> returns domain sig
    -> Arbitrum L2: COLDProofVerifier_v3.claimCD() with domain sig
    -> ARB_SYS L2->L1 message
    -> Ethereum L1: FuegoCOLDAOToken.mintFromL2()
    -> User's ETH wallet receives CD interest tokens
    -> After lock period: withdrawDeposit returns XFG principal
```

---

## RPC Endpoints Reference

### Fuego Daemon (fuegod)

| Method | Purpose | Used By |
|--------|---------|---------|
| `check_commitment_exists` | Check if commitment is on-chain | API, CLI |
| `get_commitment` | Full commitment entry with block/tier/tx data | CLI |
| `get_commitment_stats` | Merkle root, consensus %, EF IDs | CLI, wallet |
| `get_commitment_merkle_proof` | Merkle path for bundle | CLI |
| `get_height` | Chain height | All |

### API Backend (usexfg.org)

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/cold/claim` | POST | Domain signature for COLD claim |
| `/api/cold/health` | GET | Daemon + Arbitrum connectivity |

### L2 Contracts (Arbitrum)

| Contract | Function | Purpose |
|----------|----------|---------|
| `HEATBurnProofVerifier` | `claimHEAT(tier, nullifier, commitment, domainSig)` | Mint HEAT |
| `COLDProofVerifier_v3` | `claimCD(recipient, tier 0-7, claimKey, commitment, domainSig)` | Mint CD interest |
| Both | `isNullifierUsed(nullifier)` | Check double-spend |
| Both | `estimateL1GasFee(recipient, tier)` | Gas estimate |

---

## Known Issues / TODO

- Ed25519 verification in L2 contract is placeholder (needs precompile for production)
- Full Option A (Eldernode relay, no API) contract not yet deployed to testnet
- STARK proof generation (`xfg-stark bundle`) requires secret key from wallet (pending wallet integration)
