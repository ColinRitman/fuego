// Ed25519 adaptor signatures for atomic swaps.
#pragma once

#include <vector>
#include <cstdint>

#include "crypto.h"
#include "dleq.h"

namespace Crypto {

// Represents an adaptor signature, which is a Schnorr signature encrypted under a public key.
// The secret key is revealed only when the public key is used in a specific way (e.g., in an HTLC).
// Ed25519Signature is the same as Crypto::Signature (64-byte Ed25519 signature)
using Ed25519Signature = Signature;

struct AdaptorSignature {
    Ed25519Signature signature;
    PublicKey adaptorPoint; // The public key under which the signature is encrypted

    // TODO: Add serialization/deserialization methods
};

// Generates an adaptor signature for a given message hash, secret key, and adaptor point.
// This is typically done by the maker (Alice) who wants to lock funds.
// The secret key `t` is not revealed, only the adaptor point `T = t*G`.
AdaptorSignature generateAdaptorSignature(const SecretKey& sk, const Hash& messageHash, const PublicKey& adaptorPoint);

// Verifies an adaptor signature.
// This is done by the taker (Bob) to check if the signature is valid for the message
// and if the adaptor point is well-formed (via DLEQ proof).
bool verifyAdaptorSignature(const Hash& messageHash, const AdaptorSignature& adaptorSig);

} // namespace Crypto
