// Copyright (c) 2017-2025 Elderfire Privacy Council
// Elderfier Signature Broadcaster for P2P Consensus
// Phase 5 Implementation

#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include "crypto/hash.h"
#include "CommitmentIndex.h"

namespace CryptoNote {

class core;
class NodeServer;
struct COMMAND_ELDERFIER_SIGNATURE;

// Handles P2P signature gossip and consensus threshold tracking
class ElderfierSignatureBroadcaster {
public:
  ElderfierSignatureBroadcaster(core& ccore, NodeServer& p2psrv);
  ~ElderfierSignatureBroadcaster();

  // Handle incoming signature messages from P2P network
  void handleSignatureMessage(const CachedElderfierSignature& sig);

  // Broadcast merkle root to network for signatures
  void broadcastMerkleRoot(const Crypto::Hash& root);

  // Get current consensus percentage
  uint64_t getConsensusPercentage() const;

  // Check if 69% consensus threshold has been reached
  bool hasReachedThreshold() const;

  // Get signed elderfier IDs for current root
  std::vector<uint8_t> getSignedElderfierIds() const;

  // Get pending elderfier IDs (haven't signed yet)
  std::vector<uint8_t> getPendingElderfierIds() const;

  // Start background monitoring (if needed for future async operations)
  void start();

  // Stop background monitoring
  void stop();

private:
  core& m_core;
  NodeServer& m_p2p;
  mutable std::mutex m_mutex;
  bool m_running = false;

  // Helper method to validate signatures
  bool validateSignature(const CachedElderfierSignature& sig) const;
};

}  // namespace CryptoNote
