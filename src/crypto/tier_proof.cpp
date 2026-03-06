// Copyright (c) 2017-2026 Fuego Developers
// Copyright (c) 2020-2026 Elderfire Privacy Group
//
// 1-of-N OR proof implementation.
//
// Protocol (Schnorr OR, Fiat-Shamir):
//   Given commitment C = T[j]*H + mask*G for secret index j,
//   define P[i] = C - T[i]*H for each tier i.
//   Note: P[j] = mask*G (prover knows discrete log).
//         P[i] = (T[j]-T[i])*H + mask*G for i!=j (unknown DL).
//
//   Prover:
//     For i != j: pick random e[i], s[i]; R[i] = s[i]*G + e[i]*P[i]
//     For i == j: pick random k;          R[j] = k*G
//     e_total = H_s("FuegoTierProof" || C || R[0] || ... || R[N-1])
//     e[j] = e_total - sum(e[i], i!=j)
//     s[j] = k - e[j] * mask             (sc_mulsub)
//
//   Verifier:
//     For each i: P[i] = C - T[i]*H; R[i] = s[i]*G + e[i]*P[i]
//     e' = H_s("FuegoTierProof" || C || R[0] || ... || R[N-1])
//     Check: sum(e[i]) == e'

#include "tier_proof.h"
#include "pedersen.h"
#include <cstring>
#include <mutex>

extern "C" {
#include "crypto-ops.h"
#include "hash-ops.h"
#include "random.h"
}

namespace Crypto {

extern std::mutex random_lock;

static const char TIER_PROOF_DOMAIN[] = "FuegoTierProof";
static const size_t TIER_PROOF_DOMAIN_LEN = 14;

// Hash data layout for Fiat-Shamir challenge.
// domain(14) + C(32) + R[0..3](4*32) = 174 bytes
struct TierProofHashData {
  char domain[14];
  unsigned char C[32];
  unsigned char R[FUEGO_TIER_COUNT][32];
};

static void amount_to_scalar(unsigned char out[32], uint64_t amount) {
  memset(out, 0, 32);
  out[0] = (unsigned char)(amount);
  out[1] = (unsigned char)(amount >> 8);
  out[2] = (unsigned char)(amount >> 16);
  out[3] = (unsigned char)(amount >> 24);
  out[4] = (unsigned char)(amount >> 32);
  out[5] = (unsigned char)(amount >> 40);
  out[6] = (unsigned char)(amount >> 48);
  out[7] = (unsigned char)(amount >> 56);
}

// Compute Fiat-Shamir challenge from commitment and reconstructed R points.
static void compute_challenge(EllipticCurveScalar &result,
                              const unsigned char *C,
                              const unsigned char R[][32]) {
  TierProofHashData buf;
  memcpy(buf.domain, TIER_PROOF_DOMAIN, TIER_PROOF_DOMAIN_LEN);
  memcpy(buf.C, C, 32);
  memcpy(buf.R, R, FUEGO_TIER_COUNT * 32);

  unsigned char hash[32];
  cn_fast_hash(&buf, sizeof(buf), reinterpret_cast<char*>(hash));
  sc_reduce32(hash);
  memcpy(&result, hash, 32);
}

// Compute P = C - amount*H as ge_p3.
// P[j] = mask*G when amount matches the committed value.
static bool compute_difference_point(ge_p3 &P, const ge_p3 &C_p3, uint64_t tier_amount) {
  unsigned char scalar[32];
  amount_to_scalar(scalar, tier_amount);

  // tier_amount * H
  ge_p2 aH_p2;
  ge_scalarmult(&aH_p2, scalar, &pedersen_H());

  // Round-trip to ge_p3 (ge_scalarmult outputs ge_p2)
  unsigned char aH_bytes[32];
  ge_tobytes(aH_bytes, &aH_p2);
  ge_p3 aH_p3;
  if (ge_frombytes_vartime(&aH_p3, aH_bytes) != 0)
    return false;

  // P = C - aH
  ge_cached aH_cached;
  ge_p3_to_cached(&aH_cached, &aH_p3);
  ge_p1p1 P_p1p1;
  ge_sub(&P_p1p1, &C_p3, &aH_cached);
  ge_p1p1_to_p3(&P, &P_p1p1);

  return true;
}

// Generate a random scalar (reduced mod l). Caller must hold random_lock.
static void random_scalar_unlocked(EllipticCurveScalar &res) {
  unsigned char tmp[64];
  generate_random_bytes(64, tmp);
  sc_reduce(tmp);
  memcpy(&res, tmp, 32);
}

bool generate_tier_proof(TierProof &proof,
                         const EllipticCurvePoint &commitment,
                         uint64_t real_amount,
                         const EllipticCurveScalar &mask,
                         const uint64_t *tiers,
                         size_t tier_count) {
  if (tier_count != FUEGO_TIER_COUNT) return false;

  pedersen_init();

  // Find which tier index holds the real amount
  int real_idx = -1;
  for (size_t i = 0; i < tier_count; i++) {
    if (tiers[i] == real_amount) { real_idx = static_cast<int>(i); break; }
  }
  if (real_idx < 0) return false;

  // Parse commitment to ge_p3
  ge_p3 C_p3;
  if (ge_frombytes_vartime(&C_p3, reinterpret_cast<const unsigned char*>(&commitment)) != 0)
    return false;

  // Precompute P[i] = C - T[i]*H for each tier
  ge_p3 P[FUEGO_TIER_COUNT];
  for (size_t i = 0; i < tier_count; i++) {
    if (!compute_difference_point(P[i], C_p3, tiers[i]))
      return false;
  }

  memset(&proof, 0, sizeof(proof));

  unsigned char R_bytes[FUEGO_TIER_COUNT][32];

  // Lock RNG for random scalar generation
  EllipticCurveScalar k;
  {
    std::lock_guard<std::mutex> lock(random_lock);

    // Random nonce for real index
    random_scalar_unlocked(k);

    // Random e[i], s[i] for simulated indices
    for (size_t i = 0; i < tier_count; i++) {
      if (static_cast<int>(i) != real_idx) {
        random_scalar_unlocked(proof.e[i]);
        random_scalar_unlocked(proof.s[i]);
      }
    }
  }

  // Compute R[i] for each tier
  for (size_t i = 0; i < tier_count; i++) {
    if (static_cast<int>(i) == real_idx) {
      // Real: R[j] = k * G
      ge_p3 kG_p3;
      ge_scalarmult_base(&kG_p3, reinterpret_cast<const unsigned char*>(&k));
      ge_p3_tobytes(R_bytes[i], &kG_p3);
    } else {
      // Simulated: R[i] = s[i]*G + e[i]*P[i]
      ge_p2 R_p2;
      ge_double_scalarmult_base_vartime(&R_p2,
        reinterpret_cast<const unsigned char*>(&proof.e[i]),
        &P[i],
        reinterpret_cast<const unsigned char*>(&proof.s[i]));
      ge_tobytes(R_bytes[i], &R_p2);
    }
  }

  // Fiat-Shamir challenge
  EllipticCurveScalar e_total;
  compute_challenge(e_total,
    reinterpret_cast<const unsigned char*>(&commitment),
    R_bytes);

  // e[j] = e_total - sum(e[i] for i != j)
  unsigned char e_sum[32];
  sc_0(e_sum);
  for (size_t i = 0; i < tier_count; i++) {
    if (static_cast<int>(i) != real_idx) {
      sc_add(e_sum, e_sum, reinterpret_cast<const unsigned char*>(&proof.e[i]));
    }
  }
  sc_sub(reinterpret_cast<unsigned char*>(&proof.e[real_idx]),
         reinterpret_cast<unsigned char*>(&e_total),
         e_sum);

  // s[j] = k - e[j] * mask
  // sc_mulsub(s, a, b, c) computes s = c - a*b
  sc_mulsub(reinterpret_cast<unsigned char*>(&proof.s[real_idx]),
            reinterpret_cast<const unsigned char*>(&proof.e[real_idx]),
            reinterpret_cast<const unsigned char*>(&mask),
            reinterpret_cast<const unsigned char*>(&k));

  return true;
}

bool check_tier_proof(const TierProof &proof,
                      const EllipticCurvePoint &commitment,
                      const uint64_t *tiers,
                      size_t tier_count) {
  if (tier_count != FUEGO_TIER_COUNT) return false;

  pedersen_init();

  // Parse commitment
  ge_p3 C_p3;
  if (ge_frombytes_vartime(&C_p3, reinterpret_cast<const unsigned char*>(&commitment)) != 0)
    return false;

  // Validate all scalars
  for (size_t i = 0; i < tier_count; i++) {
    if (sc_check(reinterpret_cast<const unsigned char*>(&proof.e[i])) != 0) return false;
    if (sc_check(reinterpret_cast<const unsigned char*>(&proof.s[i])) != 0) return false;
  }

  // Reconstruct R[i] = s[i]*G + e[i]*P[i]
  unsigned char R_bytes[FUEGO_TIER_COUNT][32];
  for (size_t i = 0; i < tier_count; i++) {
    ge_p3 P;
    if (!compute_difference_point(P, C_p3, tiers[i]))
      return false;

    ge_p2 R_p2;
    ge_double_scalarmult_base_vartime(&R_p2,
      reinterpret_cast<const unsigned char*>(&proof.e[i]),
      &P,
      reinterpret_cast<const unsigned char*>(&proof.s[i]));
    ge_tobytes(R_bytes[i], &R_p2);
  }

  // Recompute expected challenge
  EllipticCurveScalar e_expected;
  compute_challenge(e_expected,
    reinterpret_cast<const unsigned char*>(&commitment),
    R_bytes);

  // Verify: sum(e[i]) == e_expected
  unsigned char e_sum[32];
  sc_0(e_sum);
  for (size_t i = 0; i < tier_count; i++) {
    sc_add(e_sum, e_sum, reinterpret_cast<const unsigned char*>(&proof.e[i]));
  }
  sc_sub(e_sum, e_sum, reinterpret_cast<unsigned char*>(&e_expected));

  return sc_isnonzero(e_sum) == 0;
}

} // namespace Crypto
