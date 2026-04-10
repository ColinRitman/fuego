use serde::{Deserialize, Serialize};
use tiny_keccak::{Hasher, Keccak};

// Mirrors C++ Crypto::Hash (32 bytes)
pub type Hash = [u8; 32];

// Mirrors C++ BlockHeader
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BlockHeader {
    pub major_version: u8,
    pub minor_version: u8,
    pub nonce: u32,
    pub timestamp: u64,
    pub previous_block_hash: Hash,
}

impl BlockHeader {
    /// Canonical serialization for PoW hashing (matches C++ serialization order)
    pub fn pow_bytes(&self) -> Vec<u8> {
        let mut out = Vec::with_capacity(43);
        out.push(self.major_version);
        out.push(self.minor_version);
        out.extend_from_slice(&self.timestamp.to_le_bytes());
        out.extend_from_slice(&self.previous_block_hash);
        out.extend_from_slice(&self.nonce.to_le_bytes());
        out
    }
}

// Mirrors C++ CommitmentEntry::Type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u8)]
pub enum CommitmentType {
    Heat = 0,
    Cold = 1,
    // ElderfierStaking = 2, // Removed - Elderfier operations being removed
    // LP Pool commitment types
    LpReserveA = 10,
    LpReserveB = 11,
    LpShare = 12,
    LpFee = 13,
    LpFeeAccumulator = 14,
    LpNonce = 15,
}

impl CommitmentType {
    /// Get the type name for hash-to-point derivation
    pub fn type_name(&self) -> &'static [u8] {
        match self {
            CommitmentType::Heat => b"HEAT",
            CommitmentType::Cold => b"COLD",
            CommitmentType::LpReserveA => b"LP_POOL_RESERVE_A",
            CommitmentType::LpReserveB => b"LP_POOL_RESERVE_B",
            CommitmentType::LpShare => b"LP_POOL_SHARE",
            CommitmentType::LpFee => b"LP_POOL_FEE",
            CommitmentType::LpFeeAccumulator => b"LP_POOL_FEE_ACCUMULATOR",
            CommitmentType::LpNonce => b"LP_POOL_NONCE",
        }
    }

    /// Check if this is an LP pool commitment type
    pub fn is_lp_pool(&self) -> bool {
        matches!(
            self,
            CommitmentType::LpReserveA
                | CommitmentType::LpReserveB
                | CommitmentType::LpShare
                | CommitmentType::LpFee
                | CommitmentType::LpFeeAccumulator
                | CommitmentType::LpNonce
        )
    }
}

// Mirrors C++ CommitmentEntry (fields relevant to ZK circuit)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CommitmentEntry {
    pub commitment: Hash,
    pub tx_hash: Hash,
    pub block_height: u32,
    pub amount: u64,
    pub term: u32,
    pub entry_type: CommitmentType,
    pub target_chain_id: u32,
}

// Public inputs committed to by the SP1 proof
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProofPublicValues {
    pub prev_checkpoint_hash: Hash,
    pub new_checkpoint_hash: Hash,
    pub new_merkle_root: Hash,
    pub height_start: u32,
    pub height_end: u32,
    pub difficulty_target: u32,
}

// Wire type for RPC block_range response
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RpcBlock {
    pub header: BlockHeader,
    /// Raw tx_extra bytes per transaction
    pub tx_extras: Vec<Vec<u8>>,
}

// Witness data fed to the circuit via SP1 stdin
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CircuitWitness {
    pub blocks: Vec<RpcBlock>,
    /// All commitment hashes already in the index before height_start (ordered)
    pub prev_leaves: Vec<Hash>,
    pub public: ProofPublicValues,
}

pub const TX_EXTRA_HEAT_TAG: u8 = 0x08;
pub const DEPOSIT_TERM_FOREVER: u32 = 0xFFFF_FFFF;

/// LP Pool constants
pub const LP_EPOCH_BLOCKS: u32 = 100; // ~13 hours
pub const LP_MIN_LIQUIDITY: u64 = 1000; // Minimum liquidity to prevent division by zero

// =============================================================================
// LP Pool Types - Pedersen Commitments with Type Separation
// =============================================================================

/// A Pedersen commitment with type-specific generator.
/// C = r*G + v*H_type where H_type is derived from CommitmentType.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LpCommitment {
    /// Compressed secp256k1 point (33 bytes) - stored as Vec for serde compatibility
    pub point: Vec<u8>,
    /// Commitment type (CommitmentType discriminant)
    pub commitment_type: u8,
    /// Epoch this commitment was created
    pub epoch: u32,
}

impl LpCommitment {
    /// Create a new LP commitment.
    /// Note: In practice, this requires secp256k1 point multiplication.
    /// The actual cryptographic implementation would be in a circuit or helper library.
    pub fn new(point: Vec<u8>, commitment_type: CommitmentType, epoch: u32) -> Self {
        Self {
            point,
            commitment_type: commitment_type as u8,
            epoch,
        }
    }

    /// Get the commitment type
    pub fn get_type(&self) -> Option<CommitmentType> {
        match self.commitment_type {
            10 => Some(CommitmentType::LpReserveA),
            11 => Some(CommitmentType::LpReserveB),
            12 => Some(CommitmentType::LpShare),
            13 => Some(CommitmentType::LpFee),
            14 => Some(CommitmentType::LpFeeAccumulator),
            15 => Some(CommitmentType::LpNonce),
            _ => None,
        }
    }

    /// Get the point as a fixed-size array (panics if not 33 bytes)
    #[allow(dead_code)]
    pub fn point_array(&self) -> [u8; 33] {
        let mut arr = [0u8; 33];
        arr.copy_from_slice(&self.point);
        arr
    }
}

/// Represents the on-chain state of an LP pool (commitments only, values hidden)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LpPoolState {
    /// Unique pool identifier
    pub pool_id: Hash,
    /// Token A identifier (e.g., "XFG")
    pub token_a: String,
    /// Token B identifier (e.g., "ETH")
    pub token_b: String,
    /// Fee in basis points (e.g., 30 = 0.3%)
    pub fee_bps: u16,
    /// Minimum liquidity to prevent division by zero
    pub min_liquidity: u64,

    /// Commitment to reserve A
    pub c_reserve_a: LpCommitment,
    /// Commitment to reserve B
    pub c_reserve_b: LpCommitment,
    /// Commitment to total LP shares
    pub c_lp_shares: LpCommitment,
    /// Commitment to fee accumulator A
    pub c_fee_accum_a: LpCommitment,
    /// Commitment to fee accumulator B
    pub c_fee_accum_b: LpCommitment,

    /// Merkle root of LP share balances (public, not a commitment)
    pub lp_merkle_root: Hash,
    /// Merkle root of fee claims (public, not a commitment)
    pub fee_merkle_root: Hash,

    /// Epoch tracking
    pub epoch_start: u32,
    pub epoch_end: u32,
    /// Hash of previous state (for state chain verification)
    pub prev_state_commitment: Hash,
}

/// LP pool event types
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum LpEvent {
    /// LP deposits both tokens and receives LP shares
    Deposit {
        provider: Hash,
        amount_a: u64,
        amount_b: u64,
        lp_minted: u64,
    },
    /// LP burns shares and receives tokens back
    Withdrawal {
        provider: Hash,
        lp_burned: u64,
        amount_a: u64,
        amount_b: u64,
    },
    /// User swaps between tokens
    Swap {
        trader: Hash,
        input_amount: u64,
        output_amount: u64,
        fee: u64,
        a_for_b: bool, // true = swap A for B, false = swap B for A
    },
    /// LP claims accumulated fees
    FeeClaim {
        provider: Hash,
        claimed_a: u64,
        claimed_b: u64,
    },
}

/// Encrypted LP event (for submission to pool)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EncryptedLpEvent {
    /// Encrypted commitment data (serialized LpEvent with commitments)
    pub ciphertext: Vec<u8>,
    /// Nonce used for encryption
    pub nonce: [u8; 12],
}

/// LP Proof structure (public outputs from SP1 circuit)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LpProof {
    /// Previous state commitment (links to previous epoch)
    pub prev_state_commitment: Hash,
    /// New state commitment (hash of new state)
    pub new_state_commitment: Hash,
    /// New LP share Merkle root
    pub lp_merkle_root: Hash,
    /// New fee record Merkle root
    pub fee_merkle_root: Hash,
    /// Epoch start block height
    pub epoch_start: u32,
    /// Epoch end block height
    pub epoch_end: u32,
    /// SP1 proof bytes
    pub sp1_proof: Vec<u8>,
    /// Verification key hash
    pub vk_hash: Hash,
}

/// LP Pool witness data for SP1 circuit
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LpCircuitWitness {
    /// Previous pool state
    pub prev_state: LpPoolState,
    /// Decrypted events for this epoch
    pub events: Vec<LpEvent>,
    /// Epoch key (revealed at epoch boundary)
    pub epoch_key: [u8; 32],
    /// Previous LP Merkle tree leaves
    pub prev_lp_leaves: Vec<Hash>,
    /// Previous fee Merkle tree leaves
    pub prev_fee_leaves: Vec<Hash>,
}

/// Derive epoch key from pool ID and epoch number.
/// This key is revealed at epoch boundary so any prover can decrypt events.
pub fn derive_epoch_key(pool_id: &Hash, epoch: u32) -> [u8; 32] {
    let mut k = Keccak::v256();
    k.update(pool_id);
    k.update(&epoch.to_le_bytes());
    let mut out = [0u8; 32];
    k.finalize(&mut out);
    out
}

/// Compute LP pool state commitment (hash of state for linkage)
pub fn compute_lp_state_commitment(state: &LpPoolState) -> Hash {
    let mut k = Keccak::v256();
    k.update(&state.c_reserve_a.point);
    k.update(&state.c_reserve_b.point);
    k.update(&state.c_lp_shares.point);
    k.update(&state.lp_merkle_root);
    k.update(&state.fee_merkle_root);
    let mut out = [0u8; 32];
    k.finalize(&mut out);
    out
}

/// Parse HEAT commitment hashes out of raw tx_extra bytes.
/// Tag format: 0x08 || commitment[32]
pub fn parse_heat_commitments(tx_extra: &[u8]) -> Vec<Hash> {
    let mut out = Vec::new();
    let mut i = 0;
    while i < tx_extra.len() {
        let tag = tx_extra[i];
        i += 1;
        match tag {
            TX_EXTRA_HEAT_TAG => {
                if i + 32 <= tx_extra.len() {
                    let mut commitment = [0u8; 32];
                    commitment.copy_from_slice(&tx_extra[i..i + 32]);
                    out.push(commitment);
                    i += 32;
                }
            }
            // Skip other known tags; for unknown tags we stop (conservative)
            _ => break,
        }
    }
    out
}

/// Compute merkle root from an ordered list of leaves using keccak256.
/// Matches C++ CommitmentIndex::computeMerkleRoot():
///   - internal nodes: keccak256(left || right)
///   - odd leaf: keccak256(leaf || leaf)
pub fn compute_merkle_root(leaves: &[Hash]) -> Hash {
    if leaves.is_empty() {
        return [0u8; 32];
    }
    if leaves.len() == 1 {
        return leaves[0];
    }
    let mut level: Vec<Hash> = leaves.to_vec();
    while level.len() > 1 {
        let mut next = Vec::with_capacity((level.len() + 1) / 2);
        let mut i = 0;
        while i < level.len() {
            let left = level[i];
            let right = if i + 1 < level.len() {
                level[i + 1]
            } else {
                left
            };
            next.push(keccak256_pair(&left, &right));
            i += 2;
        }
        level = next;
    }
    level[0]
}

fn keccak256_pair(left: &Hash, right: &Hash) -> Hash {
    let mut buf = [0u8; 64];
    buf[..32].copy_from_slice(left);
    buf[32..].copy_from_slice(right);
    let mut k = Keccak::v256();
    k.update(&buf);
    let mut out = [0u8; 32];
    k.finalize(&mut out);
    out
}

/// Compute checkpoint hash:
///   keccak256(merkle_root || height_end_le32 || keccak256(leaves_concat))
pub fn compute_checkpoint_hash(merkle_root: &Hash, height_end: u32, leaves: &[Hash]) -> Hash {
    // Hash all leaves concatenated
    let mut k = Keccak::v256();
    for leaf in leaves {
        k.update(leaf);
    }
    let mut leaves_hash = [0u8; 32];
    k.finalize(&mut leaves_hash);

    // Final hash
    let mut k2 = Keccak::v256();
    k2.update(merkle_root);
    k2.update(&height_end.to_le_bytes());
    k2.update(&leaves_hash);
    let mut out = [0u8; 32];
    k2.finalize(&mut out);
    out
}

/// Verify a merkle proof for a leaf at leaf_index against a known root.
/// proof is the ordered list of sibling hashes from leaf to root.
pub fn verify_merkle_proof(leaf: &Hash, proof: &[Hash], leaf_index: usize, root: &Hash) -> bool {
    let mut current = *leaf;
    let mut index = leaf_index;
    for sibling in proof {
        current = if index % 2 == 0 {
            keccak256_pair(&current, sibling)
        } else {
            keccak256_pair(sibling, &current)
        };
        index /= 2;
    }
    &current == root
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn merkle_single_leaf() {
        let leaf = [1u8; 32];
        assert_eq!(compute_merkle_root(&[leaf]), leaf);
    }

    #[test]
    fn merkle_two_leaves_roundtrip() {
        let a = [1u8; 32];
        let b = [2u8; 32];
        let root = compute_merkle_root(&[a, b]);
        assert!(verify_merkle_proof(&a, &[b], 0, &root));
        assert!(verify_merkle_proof(&b, &[a], 1, &root));
    }

    #[test]
    fn parse_heat_commitments_basic() {
        let commitment = [0xABu8; 32];
        let mut extra = vec![TX_EXTRA_HEAT_TAG];
        extra.extend_from_slice(&commitment);
        let parsed = parse_heat_commitments(&extra);
        assert_eq!(parsed.len(), 1);
        assert_eq!(parsed[0], commitment);
    }
}

pub mod config;
pub mod lp_verification;
pub mod privacy;
