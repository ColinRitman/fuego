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


#include "ElderfierSignatureBroadcaster.h"
#include "CommitmentIndex.h"
#include "Core.h"
#include "P2p/NetNode.h"
#include "P2p/P2pProtocolDefinitions.h"
#include "crypto/crypto.h"
#include <Logging/LoggerRef.h>
#include <Common/StringTools.h>

namespace CryptoNote {

ElderfierSignatureBroadcaster::ElderfierSignatureBroadcaster(core& ccore, NodeServer& p2psrv)
  : m_core(ccore), m_p2p(p2psrv), m_running(false) {
}

ElderfierSignatureBroadcaster::~ElderfierSignatureBroadcaster() {
  stop();
}

void ElderfierSignatureBroadcaster::handleSignatureMessage(const CachedElderfierSignature& sig) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Validate signature before adding to cache
  if (!validateSignature(sig)) {
    return;  // Invalid signature, discard
  }

  // Add to commitment index signature cache via public method
  m_core.get_blockchain_storage().addSignatureToCache(sig);

  // Log receipt
  // logger(INFO) << "Elderfier " << (int)sig.elderfier_id
  //             << " signature cached for root " << Common::podToHex(sig.merkle_root);

  // Check threshold and trigger finalization if needed
  if (hasReachedThreshold()) {
    // At 69% or more consensus, signatures are flush (handled by CommitmentIndex)
    // logger(INFO) << "✓ Consensus threshold reached at " << getConsensusPercentage() << "%";
  }
}

void ElderfierSignatureBroadcaster::broadcastMerkleRoot(const Crypto::Hash& root) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Update the current merkle root in CommitmentIndex
  m_core.get_blockchain_storage().updateCurrentMerkleRoot(root);

  // Broadcast via P2P: relay merkle root update to all peers
  // (individual elderfier signatures are broadcast separately by the SignatureDaemon)
}

uint64_t ElderfierSignatureBroadcaster::getConsensusPercentage() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_core.get_blockchain_storage().getConsensusPercentageForCurrentRoot();
}

bool ElderfierSignatureBroadcaster::hasReachedThreshold() const {
  return getConsensusPercentage() >= 69;
}

std::vector<uint8_t> ElderfierSignatureBroadcaster::getSignedElderfierIds() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_core.get_blockchain_storage().getSignedElderfierIds();
}

std::vector<uint8_t> ElderfierSignatureBroadcaster::getPendingElderfierIds() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_core.get_blockchain_storage().getPendingElderfierIds();
}

void ElderfierSignatureBroadcaster::start() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_running = true;
  // Broadcaster is event-driven via P2P message handler — no background threads needed
}

void ElderfierSignatureBroadcaster::stop() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_running = false;
}

bool ElderfierSignatureBroadcaster::validateSignature(const CachedElderfierSignature& sig) const {
  // Basic validation checks
  if (sig.elderfier_id > 255) {
    return false;  // Invalid EFiD (must be 0-255)
  }

  // Post-quantum hybrid signature validation
  // Per ELDERFIER_HYBRID_CRYPTO_GUIDE.md: detect algorithm by signature length
  // - Ed25519: 64 bytes (sig_algorithm == 0)
  // - ML-DSA-65: 3293 bytes (sig_algorithm == 1) — future, behind #ifdef FUEGO_PQ_ENABLED

  if (sig.sig_algorithm == 0) {
    // Ed25519 signature: validate merkle root is non-zero and signature is non-zero
    // Full Ed25519 verification against registered pubkey is done at block validation time
    // in Blockchain::pushBlock() — the broadcaster validates format only
    if (sig.merkle_root == Crypto::Hash()) {
      return false;  // Empty merkle root
    }
    if (sig.signature == Crypto::Signature()) {
      return false;  // Empty signature
    }
    return true;
  }

#ifdef FUEGO_PQ_ENABLED
  if (sig.sig_algorithm == 1) {
    // ML-DSA-65 post-quantum signature: 3293 bytes
    if (sig.pq_signature.size() != 3293) {
      return false;
    }
    if (sig.pq_pubkey.size() != 1952) {
      return false;
    }
    // ML-DSA-65 verification deferred to block validation (requires liboqs)
    // Format validation: signature and pubkey sizes are correct
    return true;
  }
#endif

  return false;  // Unknown signature algorithm
}

}  // namespace CryptoNote
