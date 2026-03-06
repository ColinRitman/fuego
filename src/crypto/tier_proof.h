// Copyright (c) 2017-2026 Fuego Developers
// Copyright (c) 2020-2026 Elderfire Privacy Group
//
// 1-of-N OR proof (Cramer-Damgard-Schoenmakers) proving a Pedersen
// commitment hides one of N known tier amounts without revealing which.
// Replaces Bulletproofs range proofs for Fuego's fixed-tier deposit model.
// Proof size: N * 64 bytes (256 bytes for 4 tiers).

#pragma once

#include <cstddef>
#include <cstdint>
#include "../../include/CryptoTypes.h"

namespace Crypto {

// Generate a 1-of-N OR proof.
//   proof:       output proof struct
//   commitment:  the Pedersen commitment C = real_amount*H + mask*G
//   real_amount: the actual committed amount (must be in tiers[])
//   mask:        the blinding factor used to create the commitment
//   tiers:       array of valid tier amounts (public constants)
//   tier_count:  number of tiers (must equal FUEGO_TIER_COUNT)
// Returns false if real_amount is not in tiers[] or inputs are invalid.
bool generate_tier_proof(TierProof &proof,
                         const EllipticCurvePoint &commitment,
                         uint64_t real_amount,
                         const EllipticCurveScalar &mask,
                         const uint64_t *tiers,
                         size_t tier_count);

// Verify a tier proof.
//   proof:      the proof to verify
//   commitment: the Pedersen commitment from the output
//   tiers:      array of valid tier amounts (same as used during generation)
//   tier_count: number of tiers (must equal FUEGO_TIER_COUNT)
// Returns true if the proof is valid.
bool check_tier_proof(const TierProof &proof,
                      const EllipticCurvePoint &commitment,
                      const uint64_t *tiers,
                      size_t tier_count);

} // namespace Crypto
