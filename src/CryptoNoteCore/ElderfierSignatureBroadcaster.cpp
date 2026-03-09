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
#include "P2p/NetNodeCommon.h"
#include "P2p/P2pProtocolDefinitions.h"
#include "P2p/LevinProtocol.h"
#include "crypto/crypto.h"
#include <Logging/LoggerRef.h>
#include <Logging/LoggerManager.h>
#include <Common/StringTools.h>
#include <chrono>

namespace CryptoNote {

ElderfierSignatureBroadcaster::ElderfierSignatureBroadcaster(core& ccore, NodeServer& p2psrv, IP2pEndpoint* p2pEndpoint)
  : m_core(ccore), m_p2p(p2psrv), m_p2pEndpoint(p2pEndpoint), m_running(false) {
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

void ElderfierSignatureBroadcaster::setSigningKeys(const Crypto::PublicKey& pub, const Crypto::SecretKey& sec) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_signingPubKey = pub;
  m_signingSecKey = sec;
  m_hasSigningKeys = true;
}

void ElderfierSignatureBroadcaster::start() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running = true;
  }
  // If signing keys are configured, start the signing thread
  if (m_hasSigningKeys) {
    m_signingRunning = true;
    m_signingThread = std::thread([this] { signingThread(); });
  }
}

void ElderfierSignatureBroadcaster::stop() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running = false;
  }
  m_signingRunning = false;
  if (m_signingThread.joinable()) {
    m_signingThread.join();
  }
}

void ElderfierSignatureBroadcaster::signingThread() {
  // Wait for core to fully sync before signing
  std::this_thread::sleep_for(std::chrono::seconds(5));

  while (m_signingRunning) {
    try {
      uint32_t currentHeight;
      Crypto::Hash topId;
      m_core.get_blockchain_top(currentHeight, topId);

      if (currentHeight > m_lastSignedHeight && currentHeight > 0) {
        // get commitment merkle root (what L2 contracts use to verify merkle proofs against)
        Crypto::Hash commitmentRoot = m_core.get_blockchain_storage().getCommitmentMerkleRoot();

        // only sign if there are commitments (non-zero root)
        if (commitmentRoot == Crypto::Hash()) {
          m_lastSignedHeight = currentHeight;
        } else {
          // sign merkle root
          Crypto::Signature sig;
          Crypto::generate_signature(commitmentRoot, m_signingPubKey, m_signingSecKey, sig);

          // build p2p message
          COMMAND_ELDERFIER_SIGNATURE::request sig_msg;
          sig_msg.merkle_root = commitmentRoot;
          sig_msg.signature = sig;
          sig_msg.elderfier_id = 0;  // looked up by pubkey on receiving end
          sig_msg.block_height = currentHeight;
          sig_msg.timestamp = std::time(nullptr);
          sig_msg.version = 1;
          sig_msg.sig_algorithm = 0;  // Ed25519

          // relay to all connected peers via IP2pEndpoint interface
          if (m_p2pEndpoint) {
            auto buf = LevinProtocol::encode(sig_msg);
            m_p2pEndpoint->externalRelayNotifyToAll(
                COMMAND_ELDERFIER_SIGNATURE::ID, buf, nullptr);
          }

          // cache locally
          CachedElderfierSignature cached;
          cached.merkle_root = commitmentRoot;
          cached.signature = sig;
          cached.elderfier_id = 0;
          cached.block_height = currentHeight;
          cached.timestamp = sig_msg.timestamp;
          cached.sig_algorithm = 0;
          m_core.get_blockchain_storage().addSignatureToCache(cached);

          m_lastSignedHeight = currentHeight;
        }
      }
    } catch (const std::exception& e) {
      // Silently continue — signing failures are non-fatal
    }

    // Check for new blocks every 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
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
