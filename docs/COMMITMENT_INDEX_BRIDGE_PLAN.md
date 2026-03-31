# Fuego Commitment Index & EVM Bridge Plan

## Overview

This document describes the implementation of trustless Fuego→EVM bridge verification using a CommitmentIndex with Merkle proofs, allowing HEAT/COLD deposits on Fuego to be claimed on Ethereum/Arbitrum without exposing user secrets.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              FUEGO CHAIN                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│  User creates HEAT burn (0x08) or COLD deposit (0xCD)                       │
│       ↓                                                                      │
│  CommitmentIndex indexes: commitment, txHash, blockHeight, amount, term     │
│       ↓                                                                      │
│  RPC endpoints expose: /get_commitment, /get_commitment_merkle_root,        │
│                        /get_commitment_merkle_proof                          │
└─────────────────────────────────────────────────────────────────────────────┘
                                    ↓
                         Elderfiers bridge data
                                    ↓
┌─────────────────────────────────────────────────────────────────────────────┐
│                              EVM CHAIN                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│  FuegoCommitmentMerkleVerifier.sol                                          │
│    - Elderfiers submit Merkle root (threshold signatures)                   │
│    - Verifies Merkle proofs for commitments                                 │
│    - Tracks nullifiers to prevent double-claims                             │
│                                                                              │
│  FuegoBlockHeaderRelay.sol                                                  │
│    - Relays Fuego block headers for SPV-style verification                  │
│    - Transaction Merkle proof verification                                  │
│    - Chain continuity validation                                            │
│                                                                              │
│  HEATBurnProofVerifier.sol / COLDDepositProofVerifier.sol                   │
│    - Verifies STARK proof (knowledge of secret)                             │
│    - Verifies Merkle proof (commitment exists on Fuego)                     │
│    - Mints tokens to msg.sender                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Unified Commitment Format

Both HEAT and COLD use the same 88-byte preimage:

```
keccak256(
    secret              || // 32 bytes
    le64(amount)        || // 8 bytes (atomic units)
    tx_prefix_hash      || // 32 bytes
    network_id          || // 4 bytes
    target_chain_id     || // 4 bytes
    version             || // 4 bytes
    le32(term)             // 4 bytes
)
```

- **HEAT burns**: term = `0xFFFFFFFF` (DEPOSIT_TERM_FOREVER)
- **COLD deposits**: term = actual lock period in blocks

## Privacy Model

1. **No ETH address on Fuego**: Recipient not in commitment preimage
2. **Commitment is public**: Stored in tx_extra, safe to index
3. **Secret stays local**: User computes commitment hash locally to query
4. **Nullifier prevents replay**: `keccak256(secret || "nullifier")`
5. **Contract mints to msg.sender**: Recipient determined at claim time

## Components Implemented

### Fuego C++ (src/CryptoNoteCore/)

| File | Purpose |
|------|---------|
| `CommitmentIndex.h` | Header for commitment index class |
| `CommitmentIndex.cpp` | Implementation with Merkle tree support |
| `Blockchain.cpp` | Hooks for indexing on block processing |
| `Core.cpp` | Accessor methods for RPC layer |

### RPC Endpoints (src/Rpc/)

| Endpoint | Purpose |
|----------|---------|
| `/get_commitment` | Query deposit by commitment hash |
| `/get_commitment_stats` | Get index statistics (counts, highest block) |
| `/get_commitment_merkle_root` | Get current Merkle root for verification |
| `/get_commitment_merkle_proof` | Get proof for specific commitment |

### Solidity Contracts (xfg-stark/)

| Contract | Purpose |
|----------|---------|
| `FuegoCommitmentMerkleVerifier.sol` | Merkle proof verification, root management |
| `FuegoBlockHeaderRelay.sol` | Block header relay for SPV verification |

## Verification Flow

```
1. USER: Creates HEAT/COLD deposit on Fuego
   - Generates random 32-byte secret
   - Computes commitment = keccak256(preimage)
   - Broadcasts tx with commitment in tx_extra

2. FUEGO: Indexes commitment
   - CommitmentIndex.addCommitment() called during block processing
   - Commitment stored with metadata (txHash, height, amount, term)

3. ELDERFIERS: Bridge Merkle root to EVM
   - Query /get_commitment_merkle_root from Fuego RPC
   - Submit root to FuegoCommitmentMerkleVerifier with threshold signatures

4. USER: Claims on EVM
   - Query /get_commitment_merkle_proof from Fuego RPC
   - Generate STARK proof locally (proves knowledge of secret)
   - Submit to HEATBurnProofVerifier:
     * STARK proof
     * Merkle proof + leaf index
     * Nullifier

5. CONTRACT: Verifies and mints
   - Verify STARK proof (cryptographic)
   - Verify Merkle proof against published root
   - Check nullifier not used
   - Mint tokens to msg.sender
   - Mark nullifier as used
```

## Security Considerations

### Why Merkle Root Bridge (not full SPV)?

- Fuego uses CN-UPX2 PoW which is memory-hard
- Verifying PoW on-chain would cost prohibitive gas
- Eldernode consensus provides economic security
- Merkle root bridge is pragmatic middle ground

### Trust Assumptions

1. **Threshold of Eldernodes are honest** (e.g., 3/5)
2. **STARK proof is cryptographically sound**
3. **Keccak256 preimage resistance** (standard assumption)

### Attack Vectors Mitigated

| Attack | Mitigation |
|--------|------------|
| Double-claim | Nullifier tracking on-chain |
| Forge commitment | STARK proof requires secret knowledge |
| Fake Merkle root | Threshold eldernode signatures required |
| Front-running | Claim goes to msg.sender, not attacker |
| Secret exposure | Secret never leaves user's device |

## 4-Tier Validation System

| Tier | XFG Amount | HEAT/COLD Amount | Atomic Units |
|------|------------|------------------|--------------|
| Micro | 0.8 XFG | 8M tokens | 8,000,000 |
| Small | 8 XFG | 80M tokens | 80,000,000 |
| Medium | 80 XFG | 800M tokens | 800,000,000 |
| Large | 800 XFG | 8B tokens | 8,000,000,000 |

## Future Enhancements

1. **Incremental Merkle tree**: Avoid recomputing full tree on each query
2. **Batch proofs**: Verify multiple commitments in single tx
3. **Cross-chain messaging**: Direct L1↔L2 communication
4. **Zero-knowledge Merkle proofs**: Hide leaf index for privacy
5. **Optimistic verification**: Challenge period instead of threshold sigs

## File Locations

```
src/CryptoNoteCore/
├── CommitmentIndex.h        # Index class header
├── CommitmentIndex.cpp      # Implementation
├── Blockchain.h             # Added index member
├── Blockchain.cpp           # Block processing hooks
├── Core.h                   # Public accessors
└── Core.cpp                 # Accessor implementations

src/Rpc/
├── CoreRpcServerCommandsDefinitions.h  # RPC command structs
├── RpcServer.h              # Handler declarations
└── RpcServer.cpp            # Handler implementations

xfg-stark/
├── FuegoCommitmentMerkleVerifier.sol  # Merkle verification
├── FuegoBlockHeaderRelay.sol          # Block header relay
├── burn_mint_air.rs         # STARK prover (unified commitment)
└── proof_data_schema.rs     # Proof data structures
```
