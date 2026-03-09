// Copyright (c) 2017-2025 Elderfire Privacy Council
// Elderfier Signature Broadcaster for P2P Consensus
// Phase 5 Implementation

#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "CommitmentIndex.h"

namespace CryptoNote {

class core;
class NodeServer;
class IP2pEndpoint;
struct COMMAND_ELDERFIER_SIGNATURE;

// Handles P2P signature gossip, consensus threshold tracking, and active signing
class ElderfierSignatureBroadcaster {
public:
  ElderfierSignatureBroadcaster(core& ccore, NodeServer& p2psrv, IP2pEndpoint* p2pEndpoint = nullptr);
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

  // Configure signing keys (enables active signing mode)
  void setSigningKeys(const Crypto::PublicKey& pub, const Crypto::SecretKey& sec);

  // Start broadcaster (and signing thread if keys are set)
  void start();

  // Stop broadcaster and signing thread
  void stop();

private:
  core& m_core;
  NodeServer& m_p2p;
  IP2pEndpoint* m_p2pEndpoint;
  mutable std::mutex m_mutex;
  bool m_running = false;

  // Signing keys (set via --elderfier-key)
  Crypto::PublicKey m_signingPubKey;
  Crypto::SecretKey m_signingSecKey;
  bool m_hasSigningKeys = false;
  uint8_t m_myEfid = 0;        // resolved from registration by pubkey
  bool m_efidResolved = false;  // true once we've looked up EFiD

  // Signing thread
  std::thread m_signingThread;
  std::atomic<bool> m_signingRunning{false};
  std::atomic<uint32_t> m_lastSignedHeight{0};
  void signingThread();

  // Helper method to validate signatures
  bool validateSignature(const CachedElderfierSignature& sig) const;
};

}  // namespace CryptoNote
