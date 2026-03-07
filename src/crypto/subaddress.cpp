// Copyright (c) 2017-2026 Fuego Developers
//
// This file is part of Fuego.
//
// Fuego is free & open source software distributed in the hope
// it will be useful, but WITHOUT ANY WARRANTY; without even an
// implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. You may redistribute it and/or modify it under the terms
// of the GNU General Public License v3 or later versions as published
// by the Free Software Foundation. Fuego includes elements written
// by third parties. See file labeled LICENSE for more details.
// You should have received a copy of the GNU General Public License
// along with Fuego. If not, see <https://www.gnu.org/licenses/>.

#include "subaddress.h"

extern "C" {
#include "crypto-ops.h"
}

namespace Crypto {

SubAddressKeys deriveSubAddressKeys(
    const SecretKey&  a,      // masterViewSecretKey
    const PublicKey&  B,      // masterSpendPublicKey
    const SecretKey*  b,      // masterSpendSecretKey (nullable)
    uint32_t major,
    uint32_t minor)
{
  SubAddressKeys out{};
  out.hasSpendSecretKey = (b != nullptr);

  // m = H_s("SubAddr" || a || major_LE32 || minor_LE32)
  struct __attribute__((packed)) PreImage {
    char     domain[7];   // "SubAddr" — no null terminator intentional
    SecretKey a;
    uint32_t i;
    uint32_t j;
  } pre;

  memcpy(pre.domain, "SubAddr", 7);
  pre.a = a;
  pre.i = major;
  pre.j = minor;

  // Reduce to scalar mod l
  EllipticCurveScalar m;
  hash_to_scalar(&pre, sizeof(pre), m);

  // D = B + m*G  (point addition on ed25519)
  ge_p3 B_p3;
  if (ge_frombytes_vartime(&B_p3, reinterpret_cast<const unsigned char*>(&B)) != 0) {
    // Invalid master spend public key — return zeroed keys
    return out;
  }

  ge_p3 mG_p3;
  ge_scalarmult_base(&mG_p3, reinterpret_cast<const unsigned char*>(&m));

  ge_cached mG_cached;
  ge_p3_to_cached(&mG_cached, &mG_p3);

  ge_p1p1 D_p1p1;
  ge_add(&D_p1p1, &B_p3, &mG_cached);

  ge_p3 D_p3;
  ge_p1p1_to_p3(&D_p3, &D_p1p1);
  ge_p3_tobytes(reinterpret_cast<unsigned char*>(&out.spendPublicKey), &D_p3);

  // C = a * D  (scalar * point: compute ECDH-style)
  // generate_key_derivation already does: result = key2 * key1 * 8
  // We need plain a*D without the cofactor multiply, so we use ge_scalarmult directly.
  ge_p2 C_p2;
  ge_scalarmult(&C_p2, reinterpret_cast<const unsigned char*>(&a), &D_p3);
  ge_tobytes(reinterpret_cast<unsigned char*>(&out.viewPublicKey), &C_p2);

  // b_ij = b + m  (mod l)
  if (b != nullptr) {
    sc_add(reinterpret_cast<unsigned char*>(&out.spendSecretKey),
           reinterpret_cast<const unsigned char*>(b),
           reinterpret_cast<const unsigned char*>(&m));
  }

  return out;
}

} // namespace Crypto
