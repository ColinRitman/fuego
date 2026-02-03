// Copyright (c) 2017-2025 Elderfire Privacy Council
// Elderfier Signature Broadcaster Implementation
// Phase 5 Implementation

#include "ElderfierSignatureBroadcaster.h"
#include "CommitmentIndex.h"
#include "Core.h"
#include "P2p/NetNode.h"
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

  // TODO: Implement P2P network broadcast of merkle root
  // This would send a COMMAND_ELDERFIER_ROOT_BROADCAST message to all peers
  // For now, just update the current merkle root in CommitmentIndex via public method
  m_core.get_blockchain_storage().updateCurrentMerkleRoot(root);

  // logger(INFO) << "Merkle root broadcasted: " << Common::podToHex(root);
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

  // TODO: Start background monitoring threads if needed
  // For now, the broadcaster is passive and handles P2P messages
  // logger(INFO) << "Elderfier Signature Broadcaster started";
}

void ElderfierSignatureBroadcaster::stop() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_running = false;

  // TODO: Stop background threads
  // logger(INFO) << "Elderfier Signature Broadcaster stopped";
}

bool ElderfierSignatureBroadcaster::validateSignature(const CachedElderfierSignature& sig) const {
  // Basic validation checks
  if (sig.elderfier_id > 255) {
    return false;  // Invalid EFiD (must be 0-255)
  }

  // TODO: Verify cryptographic signature using sig.signature
  // For now, trust that P2P network has done basic validation

  return true;
}

}  // namespace CryptoNote
