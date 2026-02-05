// Copyright (c) 2017-2025 Elderfire Privacy Council
// Simplified CommitmentIndex for Phase 3 Elderfier Consensus

#include "CommitmentIndex.h"
#include "Serialization/ISerializer.h"
#include "Common/StringTools.h"
#include "TransactionExtra.h"
#include <set>

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

CommitmentIndex::CommitmentIndex() {
}

CommitmentIndex::~CommitmentIndex() {
}

void CommitmentIndex::addCommitment(const CommitmentEntry& entry) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Track 0xEC deposits for elderfier registration
  if (isElderfierRegistrationDeposit(entry)) {
    std::string wallet = getWalletAddressFromTx(entry.txHash);
    if (!wallet.empty()) {
      m_pendingElderfierStakes[wallet].deposit_count++;
      m_pendingElderfierStakes[wallet].total_amount += entry.amount;

      // Auto-register when 5 deposits of 4000 XFG total are confirmed
      const uint64_t REGISTRATION_AMOUNT = 4000 * 100000;  // 4000 XFG in atomic units
      if (m_pendingElderfierStakes[wallet].deposit_count == 5 &&
          m_pendingElderfierStakes[wallet].total_amount >= REGISTRATION_AMOUNT) {
        tryRegisterElderfier(wallet, m_pendingElderfierStakes[wallet].signing_pubkey);
        m_pendingElderfierStakes.erase(wallet);
      }
    }
  }
}

void CommitmentIndex::addSignatureToCache(const CachedElderfierSignature& sig) {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::string merkle_root_hex = Common::podToHex(sig.merkle_root);
  auto key = std::make_pair(sig.elderfier_id, merkle_root_hex);
  m_signatures[key] = sig;

  // Track when root was first seen
  if (m_root_first_seen_block.find(merkle_root_hex) == m_root_first_seen_block.end()) {
    m_root_first_seen_block[merkle_root_hex] = sig.received_block_height;
  }

  // Update current merkle root if newer
  if (sig.received_block_height >= m_current_block_height) {
    m_current_merkle_root = sig.merkle_root;
    m_current_block_height = sig.received_block_height;
  }
}

void CommitmentIndex::checkAndFlushThreshold(uint64_t current_block_height) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Calculate consensus percentage
  std::string current_root_hex = Common::podToHex(m_current_merkle_root);
  size_t valid_signatures = 0;

  for (const auto& [key, sig] : m_signatures) {
    if (key.second == current_root_hex && sig.is_valid) {
      valid_signatures++;
    }
  }

  if (m_elderfier_ids.empty()) {
    return;
  }

  uint64_t total_elderfiers = m_elderfier_ids.size();
  uint64_t consensus_pct = (valid_signatures * 100) / total_elderfiers;

  // TODO: Implement fee distribution and signature cache flushing at 69% threshold
  if (consensus_pct >= 69) {
    // Flush signatures for current root
    std::vector<std::pair<uint8_t, std::string>> to_remove;
    for (const auto& [key, sig] : m_signatures) {
      if (key.second != current_root_hex) {
        to_remove.push_back(key);
      }
    }
    for (const auto& key : to_remove) {
      m_signatures.erase(key);
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

  for (const auto& [key, sig] : m_signatures) {
    if (key.second == current_root_hex && sig.is_valid) {
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

  for (const auto& [key, sig] : m_signatures) {
    if (key.second == current_root_hex && sig.is_valid && seen.find(key.first) == seen.end()) {
      signed_ids.push_back(key.first);
      seen.insert(key.first);
    }
  }

  return signed_ids;
}

std::vector<uint8_t> CommitmentIndex::getPendingElderfierIds() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::set<uint8_t> signed_set;
  std::string current_root_hex = Common::podToHex(m_current_merkle_root);

  for (const auto& [key, sig] : m_signatures) {
    if (key.second == current_root_hex && sig.is_valid) {
      signed_set.insert(key.first);
    }
  }

  std::vector<uint8_t> pending;
  for (uint8_t efid : m_elderfier_ids) {
    if (signed_set.find(efid) == signed_set.end()) {
      pending.push_back(efid);
    }
  }

  return pending;
}

bool CommitmentIndex::isElderfierRegistrationDeposit(const CommitmentEntry& entry) {
  // Check for stake deposits (which can be used for elderfier registration)
  // with 0xEC tag
  return entry.type == CommitmentEntry::Type::YIELD && entry.targetChainId == 0xEC;
}

std::string CommitmentIndex::getWalletAddressFromTx(const Crypto::Hash& txHash) {
  // NOTE: This is a placeholder that requires Core reference to look up transactions
  // In full implementation, we would:
  // 1. Query transaction from blockchain using Core::getTransaction()
  // 2. Extract transaction extra field
  // 3. Parse for TransactionExtraElderfierDeposit
  // 4. Return elderfierAddress field
  //
  // For now, return empty string - the actual wallet address is obtained from
  // the transaction inputs (which contain the sender's wallet address in signatures)
  // This can only be determined during block validation when we have full context

  // TODO: Implement with Core reference:
  //   TransactionExtraElderfierDeposit deposit;
  //   if (getElderfierDepositFromExtra(tx.extra, deposit)) {
  //     return deposit.elderfierAddress;
  //   }

  return "";  // Placeholder - requires Core integration
}

bool CommitmentIndex::tryRegisterElderfier(const std::string& wallet, const Crypto::PublicKey& pubkey) {
  // When 5 deposits of 800 XFG each (4000 XFG total) are confirmed:
  // 1. Create ElderfierDepositData with:
  //    - elderfierPublicKey = pubkey
  //    - elderfierAddress = wallet
  //    - depositAmount = 4000 * 100000 (in atomic units)
  //    - depositTimestamp = current block height
  //    - serviceId = STANDARD_ADDRESS
  //
  // 2. Call IEldernodeIndexManager::addElderfierDeposit(deposit)
  // 3. Call IEldernodeIndexManager::addElderfierToENindex(deposit)
  // 4. Assign EFiD (0-255) from manager's internal counter
  // 5. Add EFiD to m_elderfier_ids list
  // 6. Return true on success
  //
  // NOTE: This requires Core reference in CommitmentIndex constructor
  //       and proper integration with Blockchain/Core lifecycle

  // TODO: Implement with EldernodeIndexManager:
  //   ElderfierDepositData deposit;
  //   deposit.elderfierPublicKey = pubkey;
  //   deposit.elderfierAddress = wallet;
  //   deposit.depositAmount = REGISTRATION_AMOUNT;
  //   deposit.depositTimestamp = m_current_block_height;
  //   deposit.serviceId.type = ElderfierServiceId::Type::STANDARD_ADDRESS;
  //   deposit.serviceId.address = wallet;
  //
  //   if (m_eldernodeManager->addElderfierDeposit(deposit) &&
  //       m_eldernodeManager->addElderfierToENindex(deposit)) {
  //     uint8_t efid = getNextEFiD();
  //     m_elderfier_ids.push_back(efid);
  //     return true;
  //   }

  return true;  // Placeholder - requires Core/EldernodeIndexManager integration
}

// Legacy commitment methods (for backward compatibility)
Crypto::Hash CommitmentIndex::computeMerkleRoot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_current_merkle_root;
}

std::vector<Crypto::Hash> CommitmentIndex::getMerkleProof(const Crypto::Hash& commitment) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return {};  // TODO: Implement if needed
}

size_t CommitmentIndex::getLeafIndex(const Crypto::Hash& commitment) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return 0;  // TODO: Implement if needed
}

CommitmentIndex::Height CommitmentIndex::highestBlock() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return static_cast<Height>(m_current_block_height);
}

size_t CommitmentIndex::rollbackToHeight(Height h) {
  std::lock_guard<std::mutex> lock(m_mutex);
  // TODO: Implement rollback if needed
  return 0;
}

void CommitmentIndex::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_signatures.clear();
  m_root_first_seen_block.clear();
  m_pendingElderfierStakes.clear();
  m_elderfier_ids.clear();
}

CommitmentEntry CommitmentIndex::getByCommitment(const Crypto::Hash& commitment) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  // TODO: Implement if needed
  return CommitmentEntry();
}

bool CommitmentIndex::hasCommitment(const Crypto::Hash& commitment) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  // TODO: Implement if needed
  return false;
}

size_t CommitmentIndex::size() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_signatures.size();
}

size_t CommitmentIndex::heatCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  // TODO: Implement if needed
  return 0;
}

size_t CommitmentIndex::coldCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  // TODO: Implement if needed
  return 0;
}

// ============================================================================
// PHASE 5: FEE TRACKING AND EPOCH MANAGEMENT
// ============================================================================

void CommitmentIndex::addElderfierFee(uint64_t feeAmount) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_currentEpochTotalFees += feeAmount;
}

void CommitmentIndex::finalizeEpoch(uint64_t currentBlockHeight) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Check if we're at an epoch boundary
  uint64_t currentEpoch = getCurrentEpoch(currentBlockHeight);

  if (m_epochHistory.empty()) {
    m_currentEpochStartBlock = currentBlockHeight;
    return;  // First epoch, nothing to finalize yet
  }

  // Only finalize if we're entering a new epoch
  uint64_t lastEpoch = getCurrentEpoch(m_currentEpochStartBlock);
  if (currentEpoch <= lastEpoch) {
    return;  // Not at epoch boundary yet
  }

  // Finalize the completed epoch
  ElderfierEpochRewards epochRewards;
  epochRewards.epochNumber = lastEpoch;
  epochRewards.epochStartBlock = m_currentEpochStartBlock;
  epochRewards.epochEndBlock = currentBlockHeight - 1;
  epochRewards.totalFeesCollected = m_currentEpochTotalFees;

  if (!m_elderfier_ids.empty() && m_currentEpochTotalFees > 0) {
    // Get all elderfiers who signed during this epoch (from signature cache)
    // Elderfiers who signed get paid pro-rata
    // Elderfiers who didn't sign get 0 fees
    epochRewards.activeElderfiers = getSignedElderfierIds();

    if (!epochRewards.activeElderfiers.empty()) {
      // Distribute fees equally among signers only
      uint64_t feePerElderfier = m_currentEpochTotalFees / epochRewards.activeElderfiers.size();
      uint64_t remainder = m_currentEpochTotalFees % epochRewards.activeElderfiers.size();

      for (size_t i = 0; i < epochRewards.activeElderfiers.size(); ++i) {
        uint8_t efid = epochRewards.activeElderfiers[i];
        uint64_t share = feePerElderfier;
        if (i < remainder) {
          share += 1;  // Distribute remainder 1 satoshi per signer
        }
        epochRewards.distribution[efid] = share;
      }
    }
  }

  m_epochHistory.push_back(epochRewards);

  // Reset for next epoch
  m_currentEpochStartBlock = currentBlockHeight;
  m_currentEpochTotalFees = 0;
}

uint64_t CommitmentIndex::getCurrentEpoch(uint64_t blockHeight) const {
  // Each epoch is 1000 blocks
  // Epoch 0: blocks 0-999, Epoch 1: blocks 1000-1999, etc.
  return blockHeight / EPOCH_DURATION_BLOCKS;
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

}  // namespace CryptoNote
