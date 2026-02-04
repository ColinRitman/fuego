// Copyright (c) 2017-2025 Elderfire Privacy Council
// Simplified CommitmentIndex for Phase 3 Elderfier Consensus
// Minimal implementation to achieve compilation

#pragma once

#include <unordered_map>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>
#include "crypto/hash.h"

namespace CryptoNote {

class ISerializer;

// Simple commitment entry
struct CommitmentEntry {
  Crypto::Hash commitment;
  Crypto::Hash txHash;
  uint32_t blockHeight = 0;
  uint64_t amount = 0;
  uint32_t term = 0;

  // Internal commitment type (compatible with existing CommitmentType)
  enum class Type : uint8_t {
    HEAT = 0,              // Permanent burn (FOREVER deposits) - 0x08
    YIELD = 1,             // Interest-bearing deposits - 0xCD
    ELDERFIER_STAKING = 2  // Elderfier registration stakes - 0xEC (5x 800 XFG)
  };

  Type type = Type::HEAT;

  uint32_t targetChainId = 0;  // Chain ID: 0x08 (HEAT), 0xCD (COLD/YIELD), 0xEC (ELDERFIER_STAKING)

  void serialize(ISerializer& s);
};

// Cached elderfier signature from P2P gossip
struct CachedElderfierSignature {
  Crypto::Hash merkle_root;
  Crypto::Signature signature;
  uint8_t elderfier_id;
  uint64_t block_height;
  uint64_t timestamp;
  uint64_t received_block_height;
  bool is_valid = false;
};

// Elderfier epoch rewards (1000-block cycle)
struct ElderfierEpochRewards {
  uint64_t epochNumber = 0;
  std::vector<uint8_t> activeElderfiers;  // Elderfiers who signed during this epoch
  uint64_t totalFeesCollected = 0;
  std::map<uint8_t, uint64_t> distribution;  // EFiD -> fee amount (only for signers)
  uint64_t epochStartBlock = 0;
  uint64_t epochEndBlock = 0;
};

// Main CommitmentIndex class - simplified for compilation
class CommitmentIndex {
public:
  CommitmentIndex();
  ~CommitmentIndex();

  // Add a commitment entry
  void addCommitment(const CommitmentEntry& entry);

  // Add a signature to cache
  void addSignatureToCache(const CachedElderfierSignature& sig);

  // Check and flush signatures when threshold met
  void checkAndFlushThreshold(uint64_t current_block_height);

  // Update current merkle root
  void updateCurrentMerkleRoot(const Crypto::Hash& new_root);

  // Get consensus percentage
  uint64_t getConsensusPercentageForCurrentRoot() const;

  // Get signed elderfier IDs
  std::vector<uint8_t> getSignedElderfierIds() const;

  // Get pending elderfier IDs
  std::vector<uint8_t> getPendingElderfierIds() const;

  // Fee tracking and epoch management (Phase 5)
  void addElderfierFee(uint64_t feeAmount);
  void finalizeEpoch(uint64_t currentBlockHeight);
  uint64_t getCurrentEpoch(uint64_t blockHeight) const;
  std::vector<uint8_t> getActiveElderfiers(uint64_t epochNumber) const;
  uint64_t getElderfierEarnings(uint8_t elderfier_id, uint64_t epochNumber) const;
  void registerElderfierAddress(uint8_t elderfier_id, const std::string& address);
  ElderfierEpochRewards getEpochRewards(uint64_t epochNumber) const;
  std::vector<ElderfierEpochRewards> getEpochHistory(uint64_t startEpoch, uint64_t endEpoch) const;

  // Fee query methods
  uint64_t getTotalFeesInEscrow() const;
  uint64_t getTotalFeesDistributedAllTime() const;

  // Legacy commitment methods (for backward compatibility with Blockchain)
  typedef uint32_t Height;

  Crypto::Hash computeMerkleRoot() const;
  std::vector<Crypto::Hash> getMerkleProof(const Crypto::Hash& commitment) const;
  size_t getLeafIndex(const Crypto::Hash& commitment) const;
  Height highestBlock() const;
  size_t rollbackToHeight(Height h);

  // Additional legacy methods
  void clear();
  CommitmentEntry getByCommitment(const Crypto::Hash& commitment) const;
  bool hasCommitment(const Crypto::Hash& commitment) const;
  size_t size() const;
  size_t heatCount() const;
  size_t coldCount() const;

  // Serialization support
  void serialize(ISerializer& s) {}

private:
  mutable std::mutex m_mutex;

  // Pending elderfier stakes (0xEC deposits)
  struct PendingElderfierStake {
    int deposit_count = 0;
    uint64_t total_amount = 0;
    Crypto::PublicKey signing_pubkey;
  };

  std::map<std::string, PendingElderfierStake> m_pendingElderfierStakes;

  // Signature cache
  std::map<std::pair<uint8_t, std::string>, CachedElderfierSignature> m_signatures;
  std::map<std::string, uint64_t> m_root_first_seen_block;
  Crypto::Hash m_current_merkle_root;
  uint64_t m_current_block_height = 0;

  // List of registered elderfier IDs
  std::vector<uint8_t> m_elderfier_ids;

  // Fee tracking and epoch management (Phase 5)
  static const uint64_t EPOCH_DURATION_BLOCKS = 1000;  // Clean epoch boundary every 1000 blocks

  std::vector<ElderfierEpochRewards> m_epochHistory;
  uint64_t m_currentEpochStartBlock = 0;
  uint64_t m_currentEpochTotalFees = 0;
  std::map<uint8_t, std::string> m_elderfierAddresses;   // EFiD -> wallet address mapping

  // Helper methods
  bool isElderfierRegistrationDeposit(const CommitmentEntry& entry);
  std::string getWalletAddressFromTx(const Crypto::Hash& txHash);
  bool tryRegisterElderfier(const std::string& wallet, const Crypto::PublicKey& pubkey);
  std::vector<uint8_t> calculateActiveElderfiers(uint64_t epochNumber) const;
};

}  // namespace CryptoNote
