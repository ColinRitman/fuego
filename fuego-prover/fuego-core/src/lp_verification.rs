//! LP Pool Proof Verification
//!
//! This module handles on-chain verification of LP pool ZK proofs.
//! In the Fuego context, verification happens via the node's RPC interface.

use crate::Hash;
use serde::{Deserialize, Serialize};

/// Public outputs from the LP circuit (matching SP1 journal)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LpProofPublicOutputs {
    pub prev_state_commitment: Hash,
    pub new_state_commitment: Hash,
    pub new_lp_merkle_root: Hash,
    pub new_fee_merkle_root: Hash,
    pub epoch_start: u32,
    pub epoch_end: u32,
    pub total_lp_shares_out: u64,
    pub fee_accum_a_out: u64,
    pub fee_accum_b_out: u64,
}

/// Verification request for LP pool proof
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LpProofVerificationRequest {
    pub pool_id: Hash,
    pub proof_bytes: Vec<u8>,
    pub public_outputs: LpProofPublicOutputs,
}

/// Verification result
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LpProofVerificationResult {
    pub valid: bool,
    pub pool_id: Hash,
    pub epoch_start: u32,
    pub epoch_end: u32,
    pub new_state_commitment: Hash,
    pub new_lp_merkle_root: Hash,
    pub new_fee_merkle_root: Hash,
    pub error: Option<String>,
}

/// Verify LP proof public outputs consistency
pub fn verify_public_outputs_consistency(
    prev_commitment: &Hash,
    outputs: &LpProofPublicOutputs,
) -> bool {
    // Verify epoch continuity
    if outputs.epoch_end <= outputs.epoch_start {
        return false;
    }

    // Verify state linkage
    if &outputs.prev_state_commitment != prev_commitment {
        return false;
    }

    // Verify Merkle roots are non-zero (sanity check)
    if outputs.new_lp_merkle_root == [0u8; 32] {
        return false;
    }
    if outputs.new_fee_merkle_root == [0u8; 32] {
        return false;
    }

    true
}

/// Compute expected state commitment from outputs
pub fn compute_expected_commitment(outputs: &LpProofPublicOutputs) -> Hash {
    use tiny_keccak::{Hasher, Keccak};

    let mut k = Keccak::v256();
    k.update(&outputs.new_state_commitment);
    k.update(&outputs.new_lp_merkle_root);
    k.update(&outputs.new_fee_merkle_root);
    k.update(&outputs.total_lp_shares_out.to_le_bytes());
    k.update(&outputs.fee_accum_a_out.to_le_bytes());
    k.update(&outputs.fee_accum_b_out.to_le_bytes());

    let mut result = [0u8; 32];
    k.finalize(&mut result);
    result
}

/// Parse SP1 proof journal to extract public outputs
/// In production, this would use the SP1 verifier library
pub fn parse_sp1_proof_outputs(proof_bytes: &[u8]) -> Result<LpProofPublicOutputs, String> {
    if proof_bytes.len() < 32 + 32 + 32 + 32 + 4 + 4 + 8 + 8 + 8 {
        return Err("Proof bytes too short".to_string());
    }

    let mut offset = 0;

    let mut prev_state_commitment = [0u8; 32];
    prev_state_commitment.copy_from_slice(&proof_bytes[offset..offset + 32]);
    offset += 32;

    let mut new_state_commitment = [0u8; 32];
    new_state_commitment.copy_from_slice(&proof_bytes[offset..offset + 32]);
    offset += 32;

    let mut new_lp_merkle_root = [0u8; 32];
    new_lp_merkle_root.copy_from_slice(&proof_bytes[offset..offset + 32]);
    offset += 32;

    let mut new_fee_merkle_root = [0u8; 32];
    new_fee_merkle_root.copy_from_slice(&proof_bytes[offset..offset + 32]);
    offset += 32;

    let epoch_start = u32::from_le_bytes(proof_bytes[offset..offset + 4].try_into().unwrap());
    offset += 4;

    let epoch_end = u32::from_le_bytes(proof_bytes[offset..offset + 4].try_into().unwrap());
    offset += 4;

    let total_lp_shares_out =
        u64::from_le_bytes(proof_bytes[offset..offset + 8].try_into().unwrap());
    offset += 8;

    let fee_accum_a_out = u64::from_le_bytes(proof_bytes[offset..offset + 8].try_into().unwrap());
    offset += 8;

    let fee_accum_b_out = u64::from_le_bytes(proof_bytes[offset..offset + 8].try_into().unwrap());

    Ok(LpProofPublicOutputs {
        prev_state_commitment,
        new_state_commitment,
        new_lp_merkle_root,
        new_fee_merkle_root,
        epoch_start,
        epoch_end,
        total_lp_shares_out,
        fee_accum_a_out,
        fee_accum_b_out,
    })
}

/// Verify an LP proof (full verification)
///
/// In production, this would:
/// 1. Verify the SP1 proof using the verifier
/// 2. Extract public outputs
/// 3. Check state linkage
/// 4. Update pool state
pub fn verify_lp_proof(
    request: &LpProofVerificationRequest,
    expected_prev_commitment: &Hash,
) -> LpProofVerificationResult {
    // Step 1: Parse and verify SP1 proof
    let outputs = match parse_sp1_proof_outputs(&request.proof_bytes) {
        Ok(o) => o,
        Err(e) => {
            return LpProofVerificationResult {
                valid: false,
                pool_id: request.pool_id,
                epoch_start: 0,
                epoch_end: 0,
                new_state_commitment: [0u8; 32],
                new_lp_merkle_root: [0u8; 32],
                new_fee_merkle_root: [0u8; 32],
                error: Some(e),
            };
        }
    };

    // Step 2: Verify state linkage
    if !verify_public_outputs_consistency(expected_prev_commitment, &outputs) {
        return LpProofVerificationResult {
            valid: false,
            pool_id: request.pool_id,
            epoch_start: outputs.epoch_start,
            epoch_end: outputs.epoch_end,
            new_state_commitment: outputs.new_state_commitment,
            new_lp_merkle_root: outputs.new_lp_merkle_root,
            new_fee_merkle_root: outputs.new_fee_merkle_root,
            error: Some("State linkage verification failed".to_string()),
        };
    }

    // Step 3: Verify pool matches
    // (In production, would verify the pool_id matches)

    LpProofVerificationResult {
        valid: true,
        pool_id: request.pool_id,
        epoch_start: outputs.epoch_start,
        epoch_end: outputs.epoch_end,
        new_state_commitment: outputs.new_state_commitment,
        new_lp_merkle_root: outputs.new_lp_merkle_root,
        new_fee_merkle_root: outputs.new_fee_merkle_root,
        error: None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_public_outputs_consistency() {
        let prev = [1u8; 32];
        let outputs = LpProofPublicOutputs {
            prev_state_commitment: prev,
            new_state_commitment: [2u8; 32],
            new_lp_merkle_root: [3u8; 32],
            new_fee_merkle_root: [4u8; 32],
            epoch_start: 100,
            epoch_end: 200,
            total_lp_shares_out: 1000,
            fee_accum_a_out: 100,
            fee_accum_b_out: 50,
        };

        assert!(verify_public_outputs_consistency(&prev, &outputs));
    }

    #[test]
    fn test_invalid_epoch_order() {
        let prev = [1u8; 32];
        let outputs = LpProofPublicOutputs {
            prev_state_commitment: prev,
            new_state_commitment: [2u8; 32],
            new_lp_merkle_root: [3u8; 32],
            new_fee_merkle_root: [4u8; 32],
            epoch_start: 200,
            epoch_end: 100, // Invalid!
            total_lp_shares_out: 1000,
            fee_accum_a_out: 100,
            fee_accum_b_out: 50,
        };

        assert!(!verify_public_outputs_consistency(&prev, &outputs));
    }
}
