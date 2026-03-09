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

#include <set>
#include <algorithm>
#include <cstring>

#include "CommitmentIndex.h"
#include "TransactionExtra.h"
#include "../Serialization/ISerializer.h"
#include "../Common/StringTools.h"
#include "../crypto/hash.h"
#include "../CryptoNoteConfig.h"

namespace CryptoNote {

void CommitmentEntry::serialize(ISerializer& s) {
  // Crypto::Hash is a fixed-size array, serialize as binary
  s.binary(&commitment, sizeof(commitment), "commitment");
  s.binary(&txHash, sizeof(txHash), "tx_hash");
  s(blockHeight, "block_height");
  s(amount, "amount");
  s(term, "term");
  s(targetChainId, "target_chain_id");
}

CommitmentIndex::CommitmentIndex(const CryptoNote::Currency& currency) : m_currency(currency) {
}

CommitmentIndex::~CommitmentIndex() {
}

void CommitmentIndex::addCommitment(const CommitmentEntry& entry) {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::string commitHex = Common::podToHex(entry.commitment);

  // Skip duplicate commitments
  if (m_commitments.find(commitHex) != m_commitments.end()) {
    return;
  }

  // Store the commitment
  m_commitments[commitHex] = entry;
  m_merkle_leaves.push_back(entry.commitment);
  m_heightIndex[entry.blockHeight].push_back(commitHex);

  // Update type counters
  switch (entry.type) {
    case CommitmentEntry::Type::HEAT:
      m_heat_count++;
      break;
    case CommitmentEntry::Type::COLD:
      m_cold_count++;
      break;
    case CommitmentEntry::Type::ELDERFIER_STAKING:
      m_elderfier_stake_count++;
      break;
  }

  // Update highest block height
  if (entry.blockHeight > m_current_block_height) {
    m_current_block_height = entry.blockHeight;
  }

  // Recompute merkle root after adding new leaf
  m_current_merkle_root = computeMerkleRootInternal();

  // Track 0xEF deposits for elderfier registration
  if (isElderfierRegistrationDeposit(entry)) {
    // Use senderAddress from the CommitmentEntry (populated from TransactionExtraElderfierDeposit)
    const std::string& wallet = entry.senderAddress;
    if (!wallet.empty()) {
      m_pendingElderfierStakes[wallet].deposit_count++;
      m_pendingElderfierStakes[wallet].total_amount += entry.amount;

      // Extract ceremony alias and signing pubkey from CommitmentEntry if present
      if (!entry.ceremonyAlias.empty() && m_pendingElderfierStakes[wallet].alias.empty()) {
        m_pendingElderfierStakes[wallet].alias = entry.ceremonyAlias;
      }
      if (entry.signingPubKey != Crypto::PublicKey()) {
        m_pendingElderfierStakes[wallet].signing_pubkey = entry.signingPubKey;
      }

      // Auto-register when five 800 XFG deposits for 4000 XFG total are confirmed
      const uint64_t REGISTRATION_AMOUNT = CryptoNote::parameters::ELDERKING_CEREMONY_AMOUNT;  // 4000 XFG in atomic units
      if (m_pendingElderfierStakes[wallet].deposit_count == 5 &&
          m_pendingElderfierStakes[wallet].total_amount >= REGISTRATION_AMOUNT) {
        tryRegisterElderfier(wallet, m_pendingElderfierStakes[wallet].signing_pubkey, m_pendingElderfierStakes[wallet].alias);
        m_pendingElderfierStakes.erase(wallet);
      }
    }
  }
}

void CommitmentIndex::addSignatureToCache(const CachedElderfierSignature& sig) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Verify Ed25519 signature against registered signing pubkey
  CachedElderfierSignature verified_sig = sig;
  verified_sig.is_valid = false;

  if (sig.sig_algorithm == 0) {
    // Look up registered pubkey for this EFiD
    Crypto::PublicKey registered_pubkey = {};
    bool found = false;
    for (const auto& pair : m_elderfierRegistrations) {
      if (pair.second.elderfier_id == sig.elderfier_id &&
          pair.second.status == ElderfierStatus::ACTIVE) {
        registered_pubkey = pair.second.signing_pubkey;
        found = true;
        break;
      }
    }

    if (found && registered_pubkey != Crypto::PublicKey()) {
      // Cryptographic verification: is this signature from the registered EFier?
      verified_sig.is_valid = Crypto::check_signature(
          sig.merkle_root, registered_pubkey, sig.signature);
    }
  }

  std::string merkle_root_hex = Common::podToHex(sig.merkle_root);
  auto key = std::make_pair(sig.elderfier_id, merkle_root_hex);
  m_signatures[key] = verified_sig;

  // Track when root was first seen
  if (m_root_first_seen_block.find(merkle_root_hex) == m_root_first_seen_block.end()) {
    m_root_first_seen_block[merkle_root_hex] = sig.received_block_height;
  }

  // Update current merkle root if newer (only from verified signatures)
  if (verified_sig.is_valid && sig.received_block_height >= m_current_block_height) {
    m_current_merkle_root = sig.merkle_root;
    m_current_block_height = sig.received_block_height;
  }
}

void CommitmentIndex::checkAndFlushThreshold(uint64_t current_block_height) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Calculate consensus percentage
  std::string current_root_hex = Common::podToHex(m_current_merkle_root);
  size_t valid_signatures = 0;

  for (auto it = m_signatures.begin(); it != m_signatures.end(); ++it) {
    if (it->first.second == current_root_hex && it->second.is_valid) {
      valid_signatures++;
    }
  }

  if (m_elderfier_ids.empty()) {
    return;
  }

  uint64_t total_elderfiers = m_elderfier_ids.size();
  uint64_t consensus_pct = (valid_signatures * 100) / total_elderfiers;

  // At 69% threshold: flush stale signatures (for non-current roots)
  // Fee distribution happens in finalizeEpoch() at epoch boundaries
  if (consensus_pct >= 69) {
    // Flush signatures for current root
    std::vector<std::pair<uint8_t, std::string>> to_remove;
    for (auto it = m_signatures.begin(); it != m_signatures.end(); ++it) {
      if (it->first.second != current_root_hex) {
        to_remove.push_back(it->first);
      }
    }
    for (size_t i = 0; i < to_remove.size(); ++i) {
      m_signatures.erase(to_remove[i]);
    }
  }
}

void CommitmentIndex::updateCurrentMerkleRoot(const Crypto::Hash& new_root) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_current_merkle_root = new_root;
  m_current_block_height = 0;

  std::string new_root_hex = Common::podToHex(new_root);
  if (m_root_first_seen_block.find(new_root_hex) == m_root_first_seen_block.end()) {
    m_root_first_seen_block[new_root_hex] = 0;
  }
}

uint64_t CommitmentIndex::getConsensusPercentageForCurrentRoot() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::string current_root_hex = Common::podToHex(m_current_merkle_root);
  size_t valid_signatures = 0;

  for (auto it = m_signatures.begin(); it != m_signatures.end(); ++it) {
    if (it->first.second == current_root_hex && it->second.is_valid) {
      valid_signatures++;
    }
  }

  if (m_elderfier_ids.empty()) {
    return 0;
  }

  return (valid_signatures * 100) / m_elderfier_ids.size();
}

std::vector<uint8_t> CommitmentIndex::getSignedElderfierIds() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::string current_root_hex = Common::podToHex(m_current_merkle_root);
  std::vector<uint8_t> signed_ids;
  std::set<uint8_t> seen;

  for (auto it = m_signatures.begin(); it != m_signatures.end(); ++it) {
    if (it->first.second == current_root_hex && it->second.is_valid && seen.find(it->first.first) == seen.end()) {
      signed_ids.push_back(it->first.first);
      seen.insert(it->first.first);
    }
  }

  return signed_ids;
}

std::vector<uint8_t> CommitmentIndex::getPendingElderfierIds() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::set<uint8_t> signed_set;
  std::string current_root_hex = Common::podToHex(m_current_merkle_root);

  for (auto it = m_signatures.begin(); it != m_signatures.end(); ++it) {
    if (it->first.second == current_root_hex && it->second.is_valid) {
      signed_set.insert(it->first.first);
    }
  }

  std::vector<uint8_t> pending;
  for (size_t i = 0; i < m_elderfier_ids.size(); ++i) {
    uint8_t efid = m_elderfier_ids[i];
    if (signed_set.find(efid) == signed_set.end()) {
      pending.push_back(efid);
    }
  }

  return pending;
}

bool CommitmentIndex::isElderfierRegistrationDeposit(const CommitmentEntry& entry) {
  // Check for 0xEF deposits (used for elderfier registration)
  return entry.type == CommitmentEntry::Type::ELDERFIER_STAKING;
}

std::string CommitmentIndex::getWalletAddressFromTx(const Crypto::Hash& txHash) {
  // Look up the commitment entry by txHash to retrieve the senderAddress
  // that was populated from Common::podToHex(TransactionExtraElderfierDeposit::elderfierCommitment)
  // during addCommitment() from Blockchain::pushBlock()
  std::string txHex = Common::podToHex(txHash);
  for (const auto& pair : m_commitments) {
    if (Common::podToHex(pair.second.txHash) == txHex && !pair.second.senderAddress.empty()) {
      return pair.second.senderAddress;
    }
  }
  return "";
}

bool CommitmentIndex::tryRegisterElderfier(const std::string& wallet, const Crypto::PublicKey& pubkey, const std::string& alias) {
  // Check if this address can register (not already registered, not void)
  // Note: caller already holds m_mutex

  // Check active registrations
  if (m_elderfierRegistrations.find(wallet) != m_elderfierRegistrations.end()) {
    return false;  // Already registered
  }

  // Check void registrations
  for (const auto& vr : m_voidRegistrations) {
    if (vr.first == wallet) {
      return false;  // Address permanently VOID
    }
  }

  // Assign next available EFiD (0-255)
  if (m_elderfier_ids.size() >= 256) {
    return false;  // Maximum 256 Elderfiers reached
  }

  // Find next unused EFiD
  std::set<uint8_t> used_ids(m_elderfier_ids.begin(), m_elderfier_ids.end());
  uint8_t efid = 0;
  while (used_ids.count(efid) > 0 && efid < 255) {
    efid++;
  }
  if (used_ids.count(efid) > 0) {
    return false;  // All EFiDs exhausted
  }

  // Register the Elderfier
  ElderfierRegistration reg;
  reg.address = wallet;
  reg.elderfier_id = efid;
  reg.signing_pubkey = pubkey;
  reg.status = ElderfierStatus::ACTIVE;
  reg.unstaking_start_block = 0;
  reg.unstaking_review_window = 0;

  m_elderfierRegistrations[wallet] = reg;
  m_elderfier_ids.push_back(efid);
  m_elderfierAddresses[efid] = wallet;

  // Auto-register ceremony alias via AliasIndex (tied to EFiD — voids on unstake)
  if (m_aliasIndex && !alias.empty() && (alias.length() == 8 || alias == "GALAPAGOS" || alias == "WINSLAYER" || alias == "LOUDMINING")) {
    AliasEntry aliasEntry;
    aliasEntry.alias = alias;
    aliasEntry.ownerAddress = "";  // Not stored on-chain for privacy — addressHash is sufficient
    aliasEntry.aliasHash = Crypto::cn_fast_hash(alias.data(), alias.size());
    aliasEntry.addressHash = Crypto::cn_fast_hash(wallet.data(), wallet.size());
    aliasEntry.aliasType = 0;  // Elderfier type
    aliasEntry.registeredBlock = static_cast<uint32_t>(m_current_block_height);

    m_aliasIndex->registerAlias(aliasEntry);
  }

  return true;
}

bool CommitmentIndex::getElderfierSigningPubkey(uint8_t efid, Crypto::PublicKey& pubkey_out) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (const auto& pair : m_elderfierRegistrations) {
    if (pair.second.elderfier_id == efid && pair.second.status == ElderfierStatus::ACTIVE) {
      pubkey_out = pair.second.signing_pubkey;
      return pubkey_out != Crypto::PublicKey();  // Only valid if non-zero
    }
  }
  return false;
}

// ============================================================================
// COMMITMENT STORAGE AND MERKLE TREE
// ============================================================================

Crypto::Hash CommitmentIndex::computeMerkleRoot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_current_merkle_root;
}

Crypto::Hash CommitmentIndex::computeMerkleRootInternal() const {
  // Build binary merkle tree from leaves (caller must hold m_mutex)
  if (m_merkle_leaves.empty()) {
    return Crypto::Hash();
  }

  std::vector<Crypto::Hash> level = m_merkle_leaves;

  while (level.size() > 1) {
    std::vector<Crypto::Hash> next_level;

    for (size_t i = 0; i < level.size(); i += 2) {
      if (i + 1 < level.size()) {
        // Hash pair: H(left || right)
        uint8_t combined[64];
        memcpy(combined, level[i].data, 32);
        memcpy(combined + 32, level[i + 1].data, 32);
        Crypto::Hash parent;
        Crypto::cn_fast_hash(combined, 64, parent);
        next_level.push_back(parent);
      } else {
        // Odd leaf: promote to next level (duplicate hash with itself)
        uint8_t combined[64];
        memcpy(combined, level[i].data, 32);
        memcpy(combined + 32, level[i].data, 32);
        Crypto::Hash parent;
        Crypto::cn_fast_hash(combined, 64, parent);
        next_level.push_back(parent);
      }
    }

    level = next_level;
  }

  return level[0];
}

std::vector<Crypto::Hash> CommitmentIndex::getMerkleProof(const Crypto::Hash& commitment) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_merkle_leaves.empty()) {
    return {};
  }

  // Find leaf index
  size_t leaf_idx = SIZE_MAX;
  for (size_t i = 0; i < m_merkle_leaves.size(); ++i) {
    if (m_merkle_leaves[i] == commitment) {
      leaf_idx = i;
      break;
    }
  }

  if (leaf_idx == SIZE_MAX) {
    return {};  // Commitment not found
  }

  // Build proof by walking up the merkle tree
  std::vector<Crypto::Hash> proof;
  std::vector<Crypto::Hash> level = m_merkle_leaves;
  size_t idx = leaf_idx;

  while (level.size() > 1) {
    // Find sibling
    size_t sibling_idx;
    if (idx % 2 == 0) {
      sibling_idx = (idx + 1 < level.size()) ? idx + 1 : idx;  // Right sibling, or self if odd
    } else {
      sibling_idx = idx - 1;  // Left sibling
    }
    proof.push_back(level[sibling_idx]);

    // Compute next level
    std::vector<Crypto::Hash> next_level;
    for (size_t i = 0; i < level.size(); i += 2) {
      uint8_t combined[64];
      memcpy(combined, level[i].data, 32);
      if (i + 1 < level.size()) {
        memcpy(combined + 32, level[i + 1].data, 32);
      } else {
        memcpy(combined + 32, level[i].data, 32);  // Duplicate for odd
      }
      Crypto::Hash parent;
      Crypto::cn_fast_hash(combined, 64, parent);
      next_level.push_back(parent);
    }

    idx = idx / 2;
    level = next_level;
  }

  return proof;
}

size_t CommitmentIndex::getLeafIndex(const Crypto::Hash& commitment) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (size_t i = 0; i < m_merkle_leaves.size(); ++i) {
    if (m_merkle_leaves[i] == commitment) {
      return i;
    }
  }
  return SIZE_MAX;  // Not found
}

CommitmentIndex::Height CommitmentIndex::highestBlock() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return static_cast<Height>(m_current_block_height);
}

size_t CommitmentIndex::rollbackToHeight(Height h) {
  std::lock_guard<std::mutex> lock(m_mutex);

  size_t removed = 0;

  // Find all commitments above height h and remove them
  auto height_it = m_heightIndex.upper_bound(h);
  while (height_it != m_heightIndex.end()) {
    for (const auto& commitHex : height_it->second) {
      auto it = m_commitments.find(commitHex);
      if (it != m_commitments.end()) {
        // Decrement type counters
        switch (it->second.type) {
          case CommitmentEntry::Type::HEAT: m_heat_count--; break;
          case CommitmentEntry::Type::COLD: m_cold_count--; break;
          case CommitmentEntry::Type::ELDERFIER_STAKING: m_elderfier_stake_count--; break;
        }
        m_commitments.erase(it);
        removed++;
      }
    }
    height_it = m_heightIndex.erase(height_it);
  }

  // Rebuild merkle leaves from remaining commitments (ordered by block height)
  m_merkle_leaves.clear();
  for (const auto& height_pair : m_heightIndex) {
    for (const auto& commitHex : height_pair.second) {
      auto it = m_commitments.find(commitHex);
      if (it != m_commitments.end()) {
        m_merkle_leaves.push_back(it->second.commitment);
      }
    }
  }

  // Update block height and merkle root
  if (!m_heightIndex.empty()) {
    m_current_block_height = m_heightIndex.rbegin()->first;
  } else {
    m_current_block_height = 0;
  }
  m_current_merkle_root = computeMerkleRootInternal();

  return removed;
}

void CommitmentIndex::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_commitments.clear();
  m_merkle_leaves.clear();
  m_heightIndex.clear();
  m_heat_count = 0;
  m_cold_count = 0;
  m_elderfier_stake_count = 0;
  m_signatures.clear();
  m_root_first_seen_block.clear();
  m_pendingElderfierStakes.clear();
  m_elderfier_ids.clear();
  // Note: AliasIndex is cleared/reset separately by Blockchain (owns its own lifecycle)
  m_elderfierRegistrations.clear();
  m_voidRegistrations.clear();
  m_epochHistory.clear();
  m_currentEpochTotalFees = 0;
  m_current_merkle_root = Crypto::Hash();
  m_current_block_height = 0;
}

CommitmentEntry CommitmentIndex::getByCommitment(const Crypto::Hash& commitment) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::string commitHex = Common::podToHex(commitment);
  auto it = m_commitments.find(commitHex);
  if (it != m_commitments.end()) {
    return it->second;
  }
  return CommitmentEntry();
}

bool CommitmentIndex::hasCommitment(const Crypto::Hash& commitment) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::string commitHex = Common::podToHex(commitment);
  return m_commitments.find(commitHex) != m_commitments.end();
}

size_t CommitmentIndex::size() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_commitments.size();
}

size_t CommitmentIndex::heatCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_heat_count;
}

size_t CommitmentIndex::coldCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_cold_count;
}

// ============================================================================
// PHASE 5: FEE TRACKING AND EPOCH MANAGEMENT
// ============================================================================

void CommitmentIndex::addElderfierFee(uint64_t feeAmount) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_currentEpochTotalFees += feeAmount;
}

void CommitmentIndex::addBlockBankingFee(uint64_t height, uint64_t fee) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_blockBankingFees[height] = fee;
}

uint64_t CommitmentIndex::getBlockBankingFee(uint64_t height) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_blockBankingFees.find(height);
  return (it != m_blockBankingFees.end()) ? it->second : 0;
}

bool CommitmentIndex::isEpochBoundary(uint64_t height) const {
  uint64_t epochDur = getEpochDuration();
  return height > 0 && (height / epochDur) > ((height - 1) / epochDur);
}

std::vector<std::pair<AccountPublicAddress, uint64_t>> CommitmentIndex::finalizeEpoch(uint64_t currentBlockHeight) {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<std::pair<AccountPublicAddress, uint64_t>> efierRewards;

  // Check if we're at an epoch boundary
  uint64_t currentEpoch = getCurrentEpoch(currentBlockHeight);

  if (m_epochHistory.empty() && m_currentEpochStartBlock == 0) {
    m_currentEpochStartBlock = currentBlockHeight;
    return efierRewards;  // First epoch, nothing to finalize yet
  }

  // Only finalize if we're entering a new epoch
  uint64_t lastEpoch = getCurrentEpoch(m_currentEpochStartBlock);
  if (currentEpoch <= lastEpoch) {
    return efierRewards;  // Not at epoch boundary yet
  }

  // Finalize the completed epoch
  ElderfierEpochRewards epochRewards;
  epochRewards.epochNumber = lastEpoch;
  epochRewards.epochStartBlock = m_currentEpochStartBlock;
  epochRewards.epochEndBlock = currentBlockHeight - 1;
  epochRewards.totalFeesCollected = m_currentEpochTotalFees;

  if (m_currentEpochTotalFees > 0) {
    // Only signing EFiers receive rewards (inline to avoid deadlock — we already hold m_mutex)
    {
      std::string current_root_hex = Common::podToHex(m_current_merkle_root);
      std::set<uint8_t> seen;
      for (auto it = m_signatures.begin(); it != m_signatures.end(); ++it) {
        if (it->first.second == current_root_hex && it->second.is_valid && seen.find(it->first.first) == seen.end()) {
          epochRewards.activeElderfiers.push_back(it->first.first);
          seen.insert(it->first.first);
        }
      }
    }

    if (!epochRewards.activeElderfiers.empty()) {
      // Distribute fees equally among signers only
      uint64_t feePerElderfier = m_currentEpochTotalFees / epochRewards.activeElderfiers.size();
      uint64_t remainder = m_currentEpochTotalFees % epochRewards.activeElderfiers.size();

      for (size_t i = 0; i < epochRewards.activeElderfiers.size(); ++i) {
        uint8_t efid = epochRewards.activeElderfiers[i];
        uint64_t share = feePerElderfier;
        if (i < remainder) {
          share += 1;  // Distribute remainder 1 atomic unit per signer
        }
        epochRewards.distribution[efid] = share;

        // Build coinbase reward output if we have the EFier's address
        auto addrIt = m_elderfierAddresses.find(efid);
        if (addrIt != m_elderfierAddresses.end()) {
          AccountPublicAddress addr;
          if (m_currency.parseAccountAddressString(addrIt->second, addr)) {
            efierRewards.push_back({addr, share});
          }
        }
      }

      // Reset fees only when successfully distributed to signers
      m_currentEpochTotalFees = 0;
    }
    // If no signers: DON'T reset m_currentEpochTotalFees — carry over to next epoch
  }

  m_epochHistory.push_back(epochRewards);

  // Reset for next epoch
  m_currentEpochStartBlock = currentBlockHeight;
  return efierRewards;
}

uint64_t CommitmentIndex::getCurrentEpoch(uint64_t blockHeight) const {
  // Each epoch is 1000 blocks
  // Epoch 0: blocks 0-999, Epoch 1: blocks 1000-1999, etc.
  return blockHeight / getEpochDuration();
}

std::vector<uint8_t> CommitmentIndex::getActiveElderfiers(uint64_t epochNumber) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return calculateActiveElderfiers(epochNumber);
}

std::vector<uint8_t> CommitmentIndex::calculateActiveElderfiers(uint64_t epochNumber) const {
  // Return all registered elderfiers - they all sign the merkle root
  // Fees only go to those who actually signed
  // (this method is kept for compatibility, but returns all EFs)
  return m_elderfier_ids;
}

uint64_t CommitmentIndex::getElderfierEarnings(uint8_t elderfier_id, uint64_t epochNumber) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (const auto& epoch : m_epochHistory) {
    if (epoch.epochNumber == epochNumber) {
      auto it = epoch.distribution.find(elderfier_id);
      if (it != epoch.distribution.end()) {
        return it->second;
      }
      return 0;
    }
  }

  return 0;  // Epoch not found or elderfier didn't earn in that epoch
}

void CommitmentIndex::registerElderfierAddress(uint8_t elderfier_id, const std::string& address) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_elderfierAddresses[elderfier_id] = address;
}

ElderfierEpochRewards CommitmentIndex::getEpochRewards(uint64_t epochNumber) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (const auto& epoch : m_epochHistory) {
    if (epoch.epochNumber == epochNumber) {
      return epoch;
    }
  }

  return ElderfierEpochRewards();  // Return empty rewards if epoch not found
}

std::vector<ElderfierEpochRewards> CommitmentIndex::getEpochHistory(uint64_t startEpoch, uint64_t endEpoch) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<ElderfierEpochRewards> result;
  for (const auto& epoch : m_epochHistory) {
    if (epoch.epochNumber >= startEpoch && epoch.epochNumber <= endEpoch) {
      result.push_back(epoch);
    }
  }

  return result;
}

uint64_t CommitmentIndex::getTotalFeesInEscrow() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_currentEpochTotalFees;
}

uint64_t CommitmentIndex::getTotalFeesDistributedAllTime() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  uint64_t total = 0;
  for (const auto& epoch : m_epochHistory) {
    total += epoch.totalFeesCollected;
  }

  return total;
}

// ============================================================================
// ELDERFIER REGISTRATION LIFECYCLE MANAGEMENT
// ============================================================================

bool CommitmentIndex::isAddressRegisteredAsElderfier(const std::string& address, uint8_t efid) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_elderfierRegistrations.find(address);
  if (it == m_elderfierRegistrations.end()) {
    return false;
  }

  return it->second.elderfier_id == efid &&
         it->second.status == ElderfierStatus::ACTIVE;
}

bool CommitmentIndex::canAddressRegisterNewElderfier(const std::string& address) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Check if any registration (active or unstaking) exists for this address
  if (m_elderfierRegistrations.find(address) != m_elderfierRegistrations.end()) {
    return false;  // Already registered (active or unstaking)
  }

  // Check void set - any pair with this address means permanently locked out
  for (const auto& vr : m_voidRegistrations) {
    if (vr.first == address) {
      return false;  // Address permanently VOID — cannot re-register
    }
  }

  return true;
}

bool CommitmentIndex::initiateElderfierUnstaking(const std::string& address, uint8_t efid,
                                                  uint32_t currentBlock, uint32_t reviewWindow) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_elderfierRegistrations.find(address);
  if (it == m_elderfierRegistrations.end()) {
    return false;
  }

  if (it->second.elderfier_id != efid) {
    return false;
  }

  if (it->second.status != ElderfierStatus::ACTIVE) {
    return false;  // Can only unstake from ACTIVE state
  }

  it->second.status = ElderfierStatus::UNSTAKING;
  it->second.unstaking_start_block = currentBlock;
  it->second.unstaking_review_window = reviewWindow;
  return true;
}

bool CommitmentIndex::completeElderfierUnstaking(const std::string& address, uint8_t efid,
                                                  uint32_t currentBlock) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_elderfierRegistrations.find(address);
  if (it == m_elderfierRegistrations.end()) {
    return false;
  }

  if (it->second.elderfier_id != efid) {
    return false;
  }

  if (!it->second.canCompleteUnstaking(currentBlock)) {
    return false;  // Review window not elapsed yet
  }

  // Move to VOID status permanently
  m_voidRegistrations.insert(std::make_pair(address, efid));

  // Remove EFiD from active list
  auto eid_it = std::find(m_elderfier_ids.begin(), m_elderfier_ids.end(), efid);
  if (eid_it != m_elderfier_ids.end()) {
    m_elderfier_ids.erase(eid_it);
  }

  // Void the alias tied to this EFiD (alias lifecycle follows EFiD)
  if (m_aliasIndex) {
    m_aliasIndex->voidAlias(address);
  }

  // Remove from registrations map (void set now tracks it permanently)
  m_elderfierRegistrations.erase(it);
  return true;
}

ElderfierStatus CommitmentIndex::getElderfierStatus(const std::string& address, uint8_t efid) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Check active registrations first
  auto it = m_elderfierRegistrations.find(address);
  if (it != m_elderfierRegistrations.end() && it->second.elderfier_id == efid) {
    return it->second.status;
  }

  // Check void set
  if (m_voidRegistrations.count(std::make_pair(address, efid)) > 0) {
    return ElderfierStatus::VOID;
  }

  // Not found at all — return VOID as the default "not registered" state
  return ElderfierStatus::VOID;
}

bool CommitmentIndex::isElderfierInReviewWindow(const std::string& address, uint8_t efid,
                                                 uint32_t currentBlock) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_elderfierRegistrations.find(address);
  if (it == m_elderfierRegistrations.end() || it->second.elderfier_id != efid) {
    return false;
  }

  return it->second.isInReviewWindow(currentBlock);
}

bool CommitmentIndex::isAddressBlacklisted(const std::string& address, uint8_t efid) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  return m_voidRegistrations.count(std::make_pair(address, efid)) > 0;
}

std::vector<ElderfierRegistration> CommitmentIndex::getElderfierRegistrationsByAddress(
    const std::string& address) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<ElderfierRegistration> results;

  auto it = m_elderfierRegistrations.find(address);
  if (it != m_elderfierRegistrations.end()) {
    results.push_back(it->second);
  }

  return results;
}

}  // namespace CryptoNote
