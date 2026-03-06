// Copyright (c) 2017-2026 Fuego Developers
// Copyright (c) 2020-2026 Elderfire Privacy Group
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

// For XFG-STARKs + Elderfier Consensus


#pragma once

#include <unordered_map>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <cstdint>
#include <string>
#include <optional>
#include "../crypto/hash.h"
#include "AliasIndex.h"
#include "Currency.h"

namespace CryptoNote {

class ISerializer;

// Simple commitment entry
struct CommitmentEntry {
  Crypto::Hash commitment;
  Crypto::Hash txHash;
  uint32_t blockHeight = 0;
  uint64_t amount = 0;
  uint32_t term = 0;

  // Internal commitment type (how this deposit is used)
  //   HEAT = permanent burn via tx extra tag 0x08
  //   COLD = interest-bearing term deposit via tx extra tag 0xCD
  //   ELDERFIER_STAKING = service node stake via tx extra tag 0xEF (5x 800 XFG)
  enum class Type : uint8_t {
    HEAT = 0,              // Permanent burn (FOREVER deposits)
    COLD = 1,              // Interest-bearing term deposits
    ELDERFIER_STAKING = 2  // Elderfier registration stakes (5x 800 XFG)
  };

  Type type = Type::HEAT;

  uint32_t targetChainId = 0;  // Claim chain code: 1=ETH, 2=ARB, 3=SOL, etc. (0 = no cross-chain claim)

  std::string senderAddress;   // Wallet address that created this commitment (populated for 0xEF deposits)
  std::string ceremonyAlias;   // 8-char alias from 0xEF deposit metadata (0xEA tag), auto-registered with EFiD

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

  // Post-quantum hybrid extension fields (backward compatible)
  uint8_t sig_algorithm = 0;              // 0=Ed25519, 1=ML-DSA-65
  std::vector<uint8_t> pq_signature;      // Empty for Ed25519
  std::vector<uint8_t> pq_public_key;     // Empty for Ed25519
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

// Elderfier registration status tracking (public for method signatures)
// 0 = unregistered/dead state, higher values = more active
enum class ElderfierStatus : uint8_t {
  VOID = 0,             // Address permanently locked (unstaking completed or slashed) or not registered
  ACTIVE = 1,           // Actively registered and participating
  UNSTAKING = 2         // Unstaking initiated (review window active)
};

struct ElderfierRegistration {
  std::string address;
  uint8_t elderfier_id;
  ElderfierStatus status = ElderfierStatus::ACTIVE;
  uint32_t unstaking_start_block = 0;  // Block where unstaking was initiated
  uint32_t unstaking_review_window = 0; // Review window duration in blocks (1000 mainnet, 10 testnet)

  bool isInReviewWindow(uint32_t currentBlock) const {
    if (status != ElderfierStatus::UNSTAKING) return false;
    uint32_t review_end = unstaking_start_block + unstaking_review_window;
    return currentBlock < review_end;
  }

  bool canCompleteUnstaking(uint32_t currentBlock) const {
    if (status != ElderfierStatus::UNSTAKING) return false;
    uint32_t review_end = unstaking_start_block + unstaking_review_window;
    return currentBlock >= review_end;
  }
};



// Main CommitmentIndex class
class CommitmentIndex {
public:
  CommitmentIndex(const CryptoNote::Currency& currency);
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

  // Per-block banking fee tracking (for coinbase split)
  void addBlockBankingFee(uint64_t height, uint64_t fee);
  uint64_t getBlockBankingFee(uint64_t height) const;

  // Finalize epoch at boundary. Returns EFier reward distribution for coinbase outputs.
  // If no signers, fees carry over to next epoch (only signing EFiers receive rewards).
  std::vector<std::pair<AccountPublicAddress, uint64_t>> finalizeEpoch(uint64_t currentBlockHeight);

  // Check if height is an epoch boundary
  bool isEpochBoundary(uint64_t height) const;

  uint64_t getCurrentEpoch(uint64_t blockHeight) const;
  std::vector<uint8_t> getActiveElderfiers(uint64_t epochNumber) const;
  uint64_t getElderfierEarnings(uint8_t elderfier_id, uint64_t epochNumber) const;
  void registerElderfierAddress(uint8_t elderfier_id, const std::string& address);

  // Elderfier registration lifecycle management
  bool isAddressRegisteredAsElderfier(const std::string& address, uint8_t efid) const;
  bool canAddressRegisterNewElderfier(const std::string& address) const;
  bool initiateElderfierUnstaking(const std::string& address, uint8_t efid, uint32_t currentBlock, uint32_t reviewWindow);
  bool completeElderfierUnstaking(const std::string& address, uint8_t efid, uint32_t currentBlock);
  ElderfierStatus getElderfierStatus(const std::string& address, uint8_t efid) const;
  bool isElderfierInReviewWindow(const std::string& address, uint8_t efid, uint32_t currentBlock) const;
  bool isAddressBlacklisted(const std::string& address, uint8_t efid) const;
  std::vector<ElderfierRegistration> getElderfierRegistrationsByAddress(const std::string& address) const;

  ElderfierEpochRewards getEpochRewards(uint64_t epochNumber) const;
  std::vector<ElderfierEpochRewards> getEpochHistory(uint64_t startEpoch, uint64_t endEpoch) const;

  // Fee query methods
  uint64_t getTotalFeesInEscrow() const;
  uint64_t getTotalFeesDistributedAllTime() const;

  // Set the AliasIndex reference (called by Blockchain after construction)
  void setAliasIndex(AliasIndex* aliasIndex) { m_aliasIndex = aliasIndex; }

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

  // Pending elderfier stakes (0xEF deposits)
  struct PendingElderfierStake {
    int deposit_count = 0;
    uint64_t total_amount = 0;
    Crypto::PublicKey signing_pubkey;
    std::string alias;  // Ceremony alias extracted from 0xEF metadata (0xEA tag)
  };

  std::map<std::string, PendingElderfierStake> m_pendingElderfierStakes;

  // Signature cache
  std::map<std::pair<uint8_t, std::string>, CachedElderfierSignature> m_signatures;
  std::map<std::string, uint64_t> m_root_first_seen_block;
  Crypto::Hash m_current_merkle_root;
  uint64_t m_current_block_height = 0;

  // List of registered elderfier IDs
  std::vector<uint8_t> m_elderfier_ids;

  // Banking fee tracking and epoch management
  uint64_t getEpochDuration() const {
    return m_currency.isTestnet() ? CryptoNote::parameters::TESTNET_EPOCH_DURATION_BLOCKS
                                  : CryptoNote::parameters::EPOCH_DURATION_BLOCKS;
  }

  std::vector<ElderfierEpochRewards> m_epochHistory;
  uint64_t m_currentEpochStartBlock = 0;
  uint64_t m_currentEpochTotalFees = 0;
  std::map<uint8_t, std::string> m_elderfierAddresses;   // EFiD -> wallet address mapping
  std::map<uint64_t, uint64_t> m_blockBankingFees;       // height -> banking fee sum for that block

  // Elderfier registration and unstaking status tracking
  std::map<std::string, ElderfierRegistration> m_elderfierRegistrations;  // address -> registration
  std::set<std::pair<std::string, uint8_t>> m_voidRegistrations;   // (address, EFiD) -> permanently locked

  // AliasIndex reference (owned by Blockchain, not by CommitmentIndex)
  AliasIndex* m_aliasIndex = nullptr;

  // Commitment storage (indexed by commitment hash hex)
  std::map<std::string, CommitmentEntry> m_commitments;        // commitHash hex -> entry
  std::vector<Crypto::Hash> m_merkle_leaves;                   // ordered leaf hashes for merkle tree
  std::map<uint32_t, std::vector<std::string>> m_heightIndex;  // blockHeight -> list of commitHash hex
  size_t m_heat_count = 0;
  size_t m_cold_count = 0;
  size_t m_elderfier_stake_count = 0;

  // Currency reference for network detection
  const CryptoNote::Currency& m_currency;

  // Helper methods
  bool isElderfierRegistrationDeposit(const CommitmentEntry& entry);
  std::string getWalletAddressFromTx(const Crypto::Hash& txHash);
  bool tryRegisterElderfier(const std::string& wallet, const Crypto::PublicKey& pubkey, const std::string& alias);
  std::vector<uint8_t> calculateActiveElderfiers(uint64_t epochNumber) const;
  Crypto::Hash computeMerkleRootInternal() const;  // Recompute root from m_merkle_leaves (caller holds lock)
};

}  // namespace CryptoNote
