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

#include "EscrowTxBuilder.h"
#include "Common/StringTools.h"

#include <algorithm>
#include <cstring>
#include <random>

namespace {

// ── BlobWriter: minimal binary serializer for the CryptoNote wire format ──

struct BlobWriter {
  std::vector<uint8_t> buf;

  void writeByte(uint8_t b) {
    buf.push_back(b);
  }

  void writeBytes(const void* data, size_t n) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    buf.insert(buf.end(), p, p + n);
  }

  // 7-bit varint encoding (high bit = "more bytes follow").
  void writeVarint(uint64_t v) {
    while (v >= 0x80) {
      buf.push_back(static_cast<uint8_t>(v & 0x7F) | 0x80);
      v >>= 7;
    }
    buf.push_back(static_cast<uint8_t>(v));
  }
};

// CryptoNote transaction version used by current Fuego consensus (v10).
static constexpr uint64_t TX_VERSION = 10;

// Wire tags matching CryptoNoteSerialization.cpp BinaryVariantTagGetter.
static constexpr uint8_t TAG_KEY_INPUT  = 0x02;
static constexpr uint8_t TAG_KEY_OUTPUT = 0x02;

} // anonymous namespace

namespace XfgSwap {

bool EscrowTxBuilder::build(
    const Crypto::SecretKey& escrowKey,
    const Crypto::PublicKey& escrowPubKey,
    uint64_t escrowAmount,
    uint64_t fee,
    const Crypto::PublicKey& destPubKey,
    const std::vector<RingMember>& ringMembers,
    uint64_t escrowGlobalIndex,
    EscrowSpendResult& result)
{
  // ── Validation ─────────────────────────────────────────────────────
  if (ringMembers.empty()) {
    return false; // need at least 1 decoy + 1 real = ring size 2
  }
  if (fee >= escrowAmount) {
    return false; // fee must be less than escrow amount
  }

  const size_t ringSize = ringMembers.size() + 1; // +1 for the real input

  // ── Insert the real escrow output at a random position ─────────────
  // Build the full ring with global indices and public keys.
  struct IndexedKey {
    uint64_t globalIndex;
    Crypto::PublicKey key;
  };

  std::vector<IndexedKey> ring;
  ring.reserve(ringSize);
  for (const auto& m : ringMembers) {
    ring.push_back({m.globalIndex, m.key});
  }

  // Choose a random insertion position for the real output.
  Crypto::random_engine<uint32_t> rng;
  std::uniform_int_distribution<size_t> dist(0, ring.size());
  size_t realInsertPos = dist(rng);

  ring.insert(ring.begin() + static_cast<ptrdiff_t>(realInsertPos),
              {escrowGlobalIndex, escrowPubKey});

  // ── Sort by global index (CryptoNote requires sorted for delta encoding) ──
  std::sort(ring.begin(), ring.end(),
    [](const IndexedKey& a, const IndexedKey& b) {
      return a.globalIndex < b.globalIndex;
    });

  // Find where the real output ended up after sorting.
  size_t realIndex = 0;
  for (size_t i = 0; i < ring.size(); ++i) {
    if (ring[i].globalIndex == escrowGlobalIndex &&
        std::memcmp(ring[i].key.data, escrowPubKey.data, 32) == 0) {
      realIndex = i;
      break;
    }
  }

  // ── Compute delta-encoded output indexes ───────────────────────────
  std::vector<uint64_t> deltas(ringSize);
  deltas[0] = ring[0].globalIndex;
  for (size_t i = 1; i < ringSize; ++i) {
    deltas[i] = ring[i].globalIndex - ring[i - 1].globalIndex;
  }

  // ── Compute key image ─────────────────────────────────────────────
  Crypto::KeyImage keyImage;
  Crypto::generate_key_image(escrowPubKey, escrowKey, keyImage);

  // ── Generate random tx public key ─────────────────────────────────
  // Must be a valid Ed25519 point (not random bytes) for CryptoNote
  // deserialization and key derivation to work correctly.
  Crypto::PublicKey txPubKey;
  Crypto::SecretKey txSecKey;
  Crypto::generate_keys(txPubKey, txSecKey);

  // ── Build the transaction prefix blob ─────────────────────────────
  BlobWriter prefix;

  // version
  prefix.writeVarint(TX_VERSION);
  // unlockTime
  prefix.writeVarint(0);

  // ── inputs ──
  prefix.writeVarint(1); // input count

  // KeyInput
  prefix.writeByte(TAG_KEY_INPUT);
  prefix.writeVarint(escrowAmount);         // amount
  prefix.writeVarint(ringSize);             // outputIndexes count
  for (size_t i = 0; i < ringSize; ++i) {
    prefix.writeVarint(deltas[i]);          // delta-encoded index
  }
  prefix.writeBytes(keyImage.data, 32);     // keyImage

  // ── outputs ──
  prefix.writeVarint(1); // output count

  uint64_t outAmount = escrowAmount - fee;
  prefix.writeVarint(outAmount);            // amount
  prefix.writeByte(TAG_KEY_OUTPUT);         // KeyOutput tag
  prefix.writeBytes(destPubKey.data, 32);   // target key

  // ── extra ──
  // extra = [0x01 + 32-byte txPubKey] => 33 bytes
  prefix.writeVarint(33);                   // extra length
  prefix.writeByte(0x01);                   // TX_EXTRA_TAG_PUBKEY
  prefix.writeBytes(txPubKey.data, 32);

  // ── Hash the prefix → prefixHash ──────────────────────────────────
  Crypto::cn_fast_hash(prefix.buf.data(), prefix.buf.size(), result.prefixHash);

  // ── Generate ring signature ───────────────────────────────────────
  // Build array of pointers to public keys (required by the API).
  std::vector<const Crypto::PublicKey*> pubPtrs(ringSize);
  for (size_t i = 0; i < ringSize; ++i) {
    pubPtrs[i] = &ring[i].key;
  }

  std::vector<Crypto::Signature> sigs(ringSize);
  Crypto::generate_ring_signature(
    result.prefixHash,
    keyImage,
    pubPtrs.data(),
    ringSize,
    escrowKey,
    realIndex,
    sigs.data());

  // ── Assemble full transaction blob ────────────────────────────────
  // Full blob = prefix + signatures (ring_size * 64 bytes, raw).
  BlobWriter full;
  full.writeBytes(prefix.buf.data(), prefix.buf.size());

  for (size_t i = 0; i < ringSize; ++i) {
    full.writeBytes(sigs[i].data, 64);
  }

  // ── Hash the full blob → txHash ───────────────────────────────────
  Crypto::cn_fast_hash(full.buf.data(), full.buf.size(), result.txHash);

  // ── Convert to hex string ─────────────────────────────────────────
  result.txHex = Common::toHex(full.buf);

  return true;
}

} // namespace XfgSwap
