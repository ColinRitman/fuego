// Copyright (c) 2017-2026 Fuego Developers
// Copyright (c) 2020-2026 Elderfire Privacy Group
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
// Solana Ed25519 keypair management for XFG/SOL atomic swap HTLCs.
//
// Generates, stores, and loads Solana-compatible keypairs using Fuego's
// crypto RNG.  Keys are persisted in JSON files under <dataDir>/sol_keys/
// with 0600 permissions (they are secrets).

#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace XfgSwap {

// Solana keypair: 64 bytes = [32-byte Ed25519 seed][32-byte public key]
// Stored in a JSON file in the swap data directory.
struct SolKeypair {
  std::vector<uint8_t> seed;     // 32 bytes
  std::vector<uint8_t> pubkey;   // 32 bytes

  // The full 64-byte keypair (seed || pubkey), base58 encoded.
  // This is what Solana CLI tools and RPC calls expect.
  std::string toBase58() const;

  // The 32-byte pubkey, base58 encoded.
  std::string pubkeyBase58() const;

  // Check validity (both fields are 32 bytes)
  bool isValid() const;
};

class SolKeypairStore {
public:
  // dataDir: path to ~/.xfg-swap or equivalent
  explicit SolKeypairStore(const std::string& dataDir);

  // Generate a new random Solana keypair using Fuego's crypto RNG.
  // The keypair is generated using the standard NaCl derivation:
  //   seed = 32 random bytes
  //   SHA-512(seed)[0..32] clamped -> scalar a
  //   pubkey = a * B (Ed25519 basepoint)
  static SolKeypair generate();

  // Save keypair to disk: <dataDir>/sol_keys/<swapId>.json
  bool save(const std::string& swapId, const SolKeypair& kp);

  // Load keypair from disk
  bool load(const std::string& swapId, SolKeypair& kp);

  // Check if a keypair exists for this swap
  bool exists(const std::string& swapId) const;

  // Delete keypair (after swap completion for security)
  bool remove(const std::string& swapId);

private:
  std::string keyPath(const std::string& swapId) const;
  std::string m_dataDir;
};

} // namespace XfgSwap
