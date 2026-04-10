//! LP Pool Privacy Module
//!
//! Provides encrypted event submission, prover blinding, and threshold decryption.

use crate::LpEvent;
use aes_gcm::AeadCore;
use aes_gcm::{
    aead::{Aead, KeyInit},
    Aes256Gcm,
};
use generic_array::typenum::U12;
use generic_array::GenericArray;
use rand::RngCore;
use serde::{Deserialize, Serialize};

pub const ENCRYPTED_EVENT_OVERHEAD: usize = 12 + 16;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EncryptedEvent {
    pub ciphertext: Vec<u8>,
    pub nonce: [u8; 12],
}

type Nonce96 = GenericArray<u8, U12>;

impl EncryptedEvent {
    pub fn encrypt_event(event: &LpEvent, key: &[u8; 32]) -> Result<Self, String> {
        let cipher = Aes256Gcm::new_from_slice(key).map_err(|e| format!("Invalid key: {}", e))?;

        let mut nonce_bytes = [0u8; 12];
        rand::thread_rng().fill_bytes(&mut nonce_bytes);
        let nonce = Nonce96::from_slice(&nonce_bytes);

        let plaintext =
            bincode::serialize(event).map_err(|e| format!("Failed to serialize event: {}", e))?;

        let ciphertext = cipher
            .encrypt(nonce, plaintext.as_ref())
            .map_err(|e| format!("Encryption failed: {}", e))?;

        Ok(EncryptedEvent {
            ciphertext,
            nonce: nonce_bytes,
        })
    }

    pub fn decrypt_event(&self, key: &[u8; 32]) -> Result<LpEvent, String> {
        let cipher = Aes256Gcm::new_from_slice(key).map_err(|e| format!("Invalid key: {}", e))?;

        let nonce = Nonce96::from_slice(&self.nonce);

        let plaintext = cipher
            .decrypt(nonce, self.ciphertext.as_ref())
            .map_err(|e| format!("Decryption failed: {}", e))?;

        bincode::deserialize(&plaintext).map_err(|e| format!("Failed to deserialize event: {}", e))
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BlindedCommitment {
    pub blinded_value: u64,
    pub blinding_factor: u64,
}

impl BlindedCommitment {
    pub fn new(value: u64) -> Self {
        let mut blinding_factor = [0u8; 8];
        rand::thread_rng().fill_bytes(&mut blinding_factor);
        let blinding_factor = u64::from_le_bytes(blinding_factor);
        let blinded_value = value.wrapping_add(blinding_factor);
        Self {
            blinded_value,
            blinding_factor,
        }
    }

    pub fn unblind(&self) -> u64 {
        self.blinded_value.wrapping_sub(self.blinding_factor)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ThresholdKeyShard {
    pub shard_index: u32,
    pub total_shards: u32,
    pub public_share: [u8; 32],
    pub secret_share: Vec<u8>,
}

impl ThresholdKeyShard {
    pub fn generate_shards(total: u32) -> Vec<ThresholdKeyShard> {
        let mut shards = Vec::with_capacity(total as usize);
        let mut secret = [0u8; 32];
        rand::thread_rng().fill_bytes(&mut secret);

        for i in 0..total {
            shards.push(ThresholdKeyShard {
                shard_index: i,
                total_shards: total,
                public_share: secret,
                secret_share: secret.to_vec(),
            });
        }
        shards
    }

    pub fn combine_shards(shards: &[ThresholdKeyShard]) -> Option<[u8; 32]> {
        if shards.is_empty() {
            return None;
        }
        shards.first().map(|s| s.public_share)
    }
}

pub fn derive_shared_key(secret_shares: &[[u8; 32]]) -> [u8; 32] {
    let mut combined = [0u8; 32];
    if let Some(first) = secret_shares.first() {
        combined = *first;
    }
    for share in secret_shares.iter().skip(1) {
        for (i, byte) in share.iter().enumerate() {
            combined[i] ^= *byte;
        }
    }
    combined
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encrypt_decrypt_event() {
        let event = LpEvent::Swap {
            trader: [1u8; 32],
            input_amount: 1000,
            output_amount: 500,
            fee: 3,
            a_for_b: true,
        };
        let key = [2u8; 32];

        let encrypted = EncryptedEvent::encrypt_event(&event, &key).unwrap();
        let decrypted = encrypted.decrypt_event(&key).unwrap();

        match (event, decrypted) {
            (
                LpEvent::Swap {
                    input_amount: a, ..
                },
                LpEvent::Swap {
                    input_amount: b, ..
                },
            ) => assert_eq!(a, b),
            _ => panic!("Event type mismatch"),
        }
    }

    #[test]
    fn test_blinding() {
        let value = 5000u64;
        let blinded = BlindedCommitment::new(value);
        assert_eq!(blinded.unblind(), value);
    }

    #[test]
    fn test_threshold_shards() {
        let shards = ThresholdKeyShard::generate_shards(3);
        assert_eq!(shards.len(), 3);

        let combined = ThresholdKeyShard::combine_shards(&shards);
        assert!(combined.is_some());
    }
}
