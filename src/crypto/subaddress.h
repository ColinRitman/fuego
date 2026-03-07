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

#pragma once

#include <cstdint>
#include <cstring>
#include "crypto.h"
#include "hash.h"

// Sub-address key derivation
//
//   Master keys: a = viewSecretKey,  b = spendSecretKey
//                A = a*G (viewPublicKey), B = b*G (spendPublicKey)
//
//   Sub-address at index (major i, minor j):
//     m    = H_s("SubAddr" || a || i_LE32 || j_LE32)   [scalar, reduced mod l]
//     D_ij = B + m*G                                    [sub spend public key]
//     C_ij = a * D_ij                                   [sub view public key]
//     b_ij = b + m  (mod l)                             [sub spend secret key]
//
//   Sender encodes: tx_pub_key R = r*D_ij, output_key = H(r*C_ij, idx)*G + D_ij
//   Receiver scans: derivation = a*R = a*r*D_ij = r*C_ij (same formula as main address)
//   Spend:          key image using b_ij instead of b
//
// The index (0, 0) is reserved for the master address — do not create sub-addresses at (0,0).

namespace Crypto {

struct SubAddressKeys {
  PublicKey spendPublicKey;    // D_ij = B + m*G
  PublicKey viewPublicKey;     // C_ij = a*D_ij
  SecretKey spendSecretKey;    // b_ij = b + m  (zeroed if master spend key not provided)
  bool hasSpendSecretKey;
};

// Derive sub-address keys at index (major, minor).
// masterSpendSecretKey: pass nullptr for view-only wallets (spendSecretKey will not be set).
SubAddressKeys deriveSubAddressKeys(
    const SecretKey&  masterViewSecretKey,
    const PublicKey&  masterSpendPublicKey,
    const SecretKey*  masterSpendSecretKey,
    uint32_t major,
    uint32_t minor);

} // namespace Crypto
