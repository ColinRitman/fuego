// DLEQ proof: 64 bytes (challenge + response).
// The proof allows verifying that log_G(P1) == log_G(P2) without revealing
// the secret scalar that relates P1 and P2. This is crucial for adaptor signatures
// where the secret is only revealed through a specific mechanism (e.g., HTLC preimage).
#pragma once

#include <vector>
#include "crypto.h" // For PublicKey, Scalar

namespace Crypto {

// Represents a Discrete Logarithm Equality (DLEQ) proof.
// It consists of a challenge and a response, typically using Schnorr-like protocol.
struct DleqProof {
    Scalar challenge;
    Scalar response;

    // TODO: Add serialization/deserialization methods
};

// Generates a DLEQ proof for two points P1 and P2, given a generator G and a secret scalar s,
// such that P1 = s*G and P2 = s*G.
// This function is used to prove that an adaptor point T is derived from a secret scalar t
// without revealing t.
bool generateDleqProof(const Scalar& secretScalar, const PublicKey& point, const PublicKey& basePoint, DleqProof& proof);

// Verifies a DLEQ proof.
// Checks if log_G(P1) == log_G(P2) using the provided proof.
// This is used to verify that the adaptor point is well-formed or that a secret
// was correctly derived.
bool verifyDleqProof(const PublicKey& point1, const PublicKey& point2, const PublicKey& basePoint, const DleqProof& proof);

} // namespace Crypto
