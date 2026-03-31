// Copyright (c) 2017-2026 Fuego Developers
//
// This file is part of Fuego.
//
// Fuego is free software distributed in the hope that it
// will be useful, but WITHOUT ANY WARRANTY; without even the
// implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. You can redistribute it and/or modify it under the terms
// of the GNU General Public License v3 or later versions as published
// by the Free Software Foundation. Fuego includes elements written
// by third parties. See file labeled LICENSE for more details.
// You should have received a copy of the GNU General Public License
// along with Fuego. If not, see <https://www.gnu.org/licenses/>.
//
// Minimal CryptoNote transaction builder for spending a Musig2 escrow
// output.  The escrow UTXO is a normal KeyOutput to a 2-of-2 aggregate
// key; the caller has reconstructed the full private key via scalar
// addition of both parties' key shares.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "crypto/crypto.h"
#include "crypto/hash.h"

namespace XfgSwap {

struct RingMember {
  uint64_t globalIndex;   // global output index for this amount
  Crypto::PublicKey key;  // one-time public key of the output
};

struct EscrowSpendResult {
  std::string txHex;       // hex-encoded raw transaction blob
  Crypto::Hash txHash;     // transaction hash (hash of full blob)
  Crypto::Hash prefixHash; // tx prefix hash (what was signed)
};

class EscrowTxBuilder {
public:
  // Build and sign a transaction spending the escrow output.
  //
  // escrowKey: full private key of the escrow (reconstructed from key shares)
  // escrowPubKey: the escrow aggregate public key
  // escrowAmount: amount in the escrow output (atomic XFG)
  // fee: transaction fee (atomic XFG), typically 10000
  // destPubKey: one-time public key for the destination output
  //   (caller should derive this from the recipient's address)
  // ringMembers: decoy outputs for the ring signature (must be exactly ring_size - 1)
  //   The escrow output is automatically inserted at a random position.
  // escrowGlobalIndex: global output index of the escrow output
  //
  // Returns true on success, fills result.
  static bool build(
    const Crypto::SecretKey& escrowKey,
    const Crypto::PublicKey& escrowPubKey,
    uint64_t escrowAmount,
    uint64_t fee,
    const Crypto::PublicKey& destPubKey,
    const std::vector<RingMember>& ringMembers,
    uint64_t escrowGlobalIndex,
    EscrowSpendResult& result);
};

} // namespace XfgSwap
