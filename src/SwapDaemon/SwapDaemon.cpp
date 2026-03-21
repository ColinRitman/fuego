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

#include "SwapDaemon.h"
#include "Common/StringTools.h"
#include "crypto/hash.h"
#include "crypto/crypto.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <ctime>

namespace XfgSwap {

SwapDaemon::SwapDaemon(const std::string& fuegodHost, uint16_t fuegodPort,
                       const std::string& dataDir, Logging::ILogger& logger)
  : m_rpc(fuegodHost, fuegodPort)
  , m_db(dataDir)
  , m_logger(logger, "SwapDaemon") {
}

std::string SwapDaemon::generateSwapId() {
  // Hash current timestamp + random bytes for a unique swap ID
  struct {
    time_t timestamp;
    uint8_t random[32];
  } seed;

  seed.timestamp = std::time(nullptr);
  Crypto::generate_random_bytes(sizeof(seed.random), seed.random);

  Crypto::Hash hash;
  Crypto::cn_fast_hash(&seed, sizeof(seed), hash);

  // Use first 16 bytes (32 hex chars) for a compact but unique ID
  return Common::toHex(hash.data, 16);
}

bool SwapDaemon::initiate(SwapParams params) {
  // Validate connection to fuegod
  uint32_t currentHeight = 0;
  if (!m_rpc.getHeight(currentHeight)) {
    m_logger(Logging::ERROR) << "Cannot connect to fuegod";
    return false;
  }

  m_logger(Logging::INFO) << "Connected to fuegod at height " << currentHeight;

  // Generate swap ID if not already set
  if (params.swapId.empty()) {
    params.swapId = generateSwapId();
  }

  // Set default timeout height if not specified (current + 180 blocks = ~1 day)
  if (params.xfgTimeoutHeight == 0) {
    params.xfgTimeoutHeight = currentHeight + 180;
  }

  // Validate price against TWAP (one-directional floor protection)
  RateCheck rc = m_oracle.validateSwapAmounts(params.pair, params.xfgAmount, params.ctrAmount);
  if (rc == RateCheck::BELOW_FLOOR) {
    m_logger(Logging::ERROR)
      << "Swap rate rejected: XFG priced >= 50% below TWAP floor. "
      << PriceOracle::rateCheckToString(rc);
    return false;
  }
  if (rc == RateCheck::ABOVE_MARKET) {
    m_logger(Logging::WARNING)
      << "Swap rate is significantly above market TWAP. Proceeding.";
  }
  if (rc == RateCheck::NO_DATA) {
    m_logger(Logging::INFO)
      << "No TWAP data yet (bootstrap mode). Seed rate: "
      << PriceOracle::getSeedRate(params.pair) << " XFG per 1 "
      << swapPairToString(params.pair);
  }

  // Generate hashlock: create random preimage, hash it
  Crypto::generate_random_bytes(sizeof(params.preimage.data), params.preimage.data);
  Crypto::cn_fast_hash(params.preimage.data, sizeof(params.preimage.data), params.hashLock);

  // Create state machine
  SwapStateMachine sm(params);

  // Save to database
  if (!m_db.saveSwap(sm)) {
    m_logger(Logging::ERROR) << "Failed to save swap to database";
    return false;
  }

  m_logger(Logging::INFO) << "Swap initiated: " << params.swapId;
  m_logger(Logging::INFO) << "  Pair: XFG/" << swapPairToString(params.pair);
  m_logger(Logging::INFO) << "  Role: " << (params.role == SwapRole::BOB ? "BOB (selling XFG)" : "ALICE (buying XFG)");
  m_logger(Logging::INFO) << "  XFG amount: " << params.xfgAmount << " atomic";
  m_logger(Logging::INFO) << "  CTR amount: " << params.ctrAmount << " atomic";
  m_logger(Logging::INFO) << "  Timeout height: " << params.xfgTimeoutHeight;
  m_logger(Logging::INFO) << "  HashLock: " << Common::podToHex(params.hashLock);
  m_logger(Logging::INFO) << "  Share this swap ID with your counterparty: " << params.swapId;

  return true;
}

bool SwapDaemon::accept(const std::string& swapId) {
  SwapStateMachine sm;
  if (!m_db.loadSwap(swapId, sm)) {
    m_logger(Logging::ERROR) << "Swap not found: " << swapId;
    return false;
  }

  if (sm.currentState() != SwapState::INITIATED) {
    m_logger(Logging::ERROR) << "Swap is not in INITIATED state (current: "
                             << swapStateToString(sm.currentState()) << ")";
    return false;
  }

  m_logger(Logging::INFO) << "Accepting swap " << swapId;
  m_logger(Logging::INFO) << "  Next step: lock XFG in HTLC";

  // In a full implementation, this would:
  // 1. Construct the HTLC transaction
  // 2. Sign and broadcast it
  // 3. Transition to XFG_LOCKED
  // For now, we just mark it as accepted by transitioning state

  if (!sm.transition(SwapState::XFG_LOCKED)) {
    m_logger(Logging::ERROR) << "State transition failed";
    return false;
  }

  if (!m_db.saveSwap(sm)) {
    m_logger(Logging::ERROR) << "Failed to save swap state";
    return false;
  }

  m_logger(Logging::INFO) << "Swap " << swapId << " -> XFG_LOCKED (HTLC broadcast pending)";
  return true;
}

bool SwapDaemon::checkTimeouts() {
  uint32_t currentHeight = 0;
  if (!m_rpc.getHeight(currentHeight)) {
    m_logger(Logging::ERROR) << "Cannot query fuegod height";
    return false;
  }

  auto swapIds = m_db.listSwaps();
  bool anyExpired = false;

  for (const auto& swapId : swapIds) {
    SwapStateMachine sm;
    if (!m_db.loadSwap(swapId, sm)) {
      continue;
    }

    if (sm.isTerminal()) {
      continue;
    }

    const auto& params = sm.params();

    // Check XFG timeout
    if (params.xfgTimeoutHeight > 0 && currentHeight >= params.xfgTimeoutHeight) {
      SwapState current = sm.currentState();

      if (current == SwapState::XFG_LOCKED && params.role == SwapRole::BOB) {
        // Bob can refund XFG
        m_logger(Logging::WARNING) << "Swap " << swapId
          << " XFG timeout reached at height " << currentHeight
          << " (deadline was " << params.xfgTimeoutHeight << ")";

        if (sm.transition(SwapState::XFG_REFUNDED)) {
          m_db.saveSwap(sm);
          m_logger(Logging::INFO) << "Swap " << swapId << " -> XFG_REFUNDED";
          anyExpired = true;
        }
      } else if (current == SwapState::CTR_LOCKED && params.role == SwapRole::ALICE) {
        // Alice can refund counterparty chain
        m_logger(Logging::WARNING) << "Swap " << swapId
          << " CTR timeout reached";

        if (sm.transition(SwapState::CTR_REFUNDED)) {
          m_db.saveSwap(sm);
          m_logger(Logging::INFO) << "Swap " << swapId << " -> CTR_REFUNDED";
          anyExpired = true;
        }
      }
    }
  }

  if (!anyExpired) {
    m_logger(Logging::DEBUGGING) << "No swaps timed out at height " << currentHeight;
  }

  return true;
}

bool SwapDaemon::processSwap(const std::string& swapId) {
  SwapStateMachine sm;
  if (!m_db.loadSwap(swapId, sm)) {
    m_logger(Logging::ERROR) << "Swap not found: " << swapId;
    return false;
  }

  if (sm.isTerminal()) {
    m_logger(Logging::INFO) << "Swap " << swapId
      << " is in terminal state: " << swapStateToString(sm.currentState());
    return true;
  }

  uint32_t currentHeight = 0;
  if (!m_rpc.getHeight(currentHeight)) {
    m_logger(Logging::ERROR) << "Cannot query fuegod height";
    return false;
  }

  const auto& params = sm.params();
  SwapState current = sm.currentState();

  m_logger(Logging::INFO) << "Processing swap " << swapId
    << " state=" << swapStateToString(current)
    << " height=" << currentHeight;

  switch (current) {
    case SwapState::INITIATED:
      // Waiting for HTLC creation -- nothing to do automatically
      m_logger(Logging::INFO) << "  Waiting for HTLC lock. Use 'accept' to proceed.";
      break;

    case SwapState::XFG_LOCKED:
      // Check if counterparty has locked their funds
      // In full implementation: poll counterparty chain for lock tx
      m_logger(Logging::INFO) << "  XFG locked. Waiting for counterparty to lock "
        << swapPairToString(params.pair) << ".";
      break;

    case SwapState::CTR_LOCKED:
      // Both sides locked. Alice should claim XFG (reveals preimage).
      if (params.role == SwapRole::ALICE) {
        m_logger(Logging::INFO) << "  Both sides locked. Ready to claim XFG (will reveal preimage).";
      } else {
        m_logger(Logging::INFO) << "  Both sides locked. Waiting for Alice to claim XFG.";
      }
      break;

    case SwapState::XFG_CLAIMED:
      // Preimage is now public. Bob should claim counterparty funds.
      if (params.role == SwapRole::BOB) {
        m_logger(Logging::INFO) << "  XFG claimed by Alice. Preimage revealed. Claim "
          << swapPairToString(params.pair) << " now.";
      }
      break;

    default:
      break;
  }

  return true;
}

void SwapDaemon::listSwaps() {
  auto swapIds = m_db.listSwaps();

  if (swapIds.empty()) {
    std::cout << "No swaps found." << std::endl;
    return;
  }

  std::cout << std::left
            << std::setw(34) << "SWAP ID"
            << std::setw(14) << "STATE"
            << std::setw(6)  << "PAIR"
            << std::setw(6)  << "ROLE"
            << std::setw(18) << "XFG AMOUNT"
            << std::endl;
  std::cout << std::string(78, '-') << std::endl;

  for (const auto& swapId : swapIds) {
    SwapStateMachine sm;
    if (!m_db.loadSwap(swapId, sm)) {
      std::cout << swapId << "  [ERROR: cannot load]" << std::endl;
      continue;
    }

    const auto& p = sm.params();
    std::cout << std::left
              << std::setw(34) << p.swapId
              << std::setw(14) << swapStateToString(sm.currentState())
              << std::setw(6)  << swapPairToString(p.pair)
              << std::setw(6)  << (p.role == SwapRole::BOB ? "BOB" : "ALICE")
              << std::setw(18) << p.xfgAmount
              << std::endl;
  }
}

void SwapDaemon::showSwap(const std::string& swapId) {
  SwapStateMachine sm;
  if (!m_db.loadSwap(swapId, sm)) {
    std::cout << "Swap not found: " << swapId << std::endl;
    return;
  }

  const auto& p = sm.params();

  std::cout << "=== Swap Details ===" << std::endl;
  std::cout << "  Swap ID:          " << p.swapId << std::endl;
  std::cout << "  State:            " << swapStateToString(sm.currentState()) << std::endl;
  std::cout << "  Pair:             XFG/" << swapPairToString(p.pair) << std::endl;
  std::cout << "  Role:             " << (p.role == SwapRole::BOB ? "BOB (selling XFG)" : "ALICE (buying XFG)") << std::endl;
  std::cout << "  XFG amount:       " << p.xfgAmount << " atomic ("
            << (static_cast<double>(p.xfgAmount) / 10000000.0) << " XFG)" << std::endl;
  std::cout << "  CTR amount:       " << p.ctrAmount << " atomic" << std::endl;
  std::cout << "  HashLock:         " << Common::podToHex(p.hashLock) << std::endl;

  // Only show preimage if we know it (non-zero)
  Crypto::Hash zeroHash;
  std::memset(&zeroHash, 0, sizeof(zeroHash));
  if (std::memcmp(&p.preimage, &zeroHash, sizeof(zeroHash)) != 0) {
    std::cout << "  Preimage:         " << Common::podToHex(p.preimage) << std::endl;
  } else {
    std::cout << "  Preimage:         [unknown]" << std::endl;
  }

  std::cout << "  XFG timeout:      height " << p.xfgTimeoutHeight << std::endl;
  std::cout << "  CTR timeout:      block " << p.ctrTimeoutBlock << std::endl;
  std::cout << "  HTLC output idx:  " << p.htlcOutputIndex << std::endl;
  std::cout << "  Alice XFG pubkey: " << Common::podToHex(p.aliceXfgPubKey) << std::endl;
  std::cout << "  Bob XFG pubkey:   " << Common::podToHex(p.bobXfgPubKey) << std::endl;

  if (!p.ctrLockTxId.empty()) {
    std::cout << "  CTR lock tx:      " << p.ctrLockTxId << std::endl;
  }
  if (!p.ctrAddress.empty()) {
    std::cout << "  CTR address:      " << p.ctrAddress << std::endl;
  }
  if (!p.peerEndpoint.empty()) {
    std::cout << "  Peer endpoint:    " << p.peerEndpoint << std::endl;
  }

  // Timestamps
  char timeBuf[64];
  struct tm* tm;

  time_t created = sm.createdAt();
  tm = std::localtime(&created);
  std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", tm);
  std::cout << "  Created:          " << timeBuf << std::endl;

  time_t updated = sm.updatedAt();
  tm = std::localtime(&updated);
  std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", tm);
  std::cout << "  Updated:          " << timeBuf << std::endl;

  std::cout << "  Terminal:         " << (sm.isTerminal() ? "yes" : "no") << std::endl;
}

bool SwapDaemon::refund(const std::string& swapId) {
  SwapStateMachine sm;
  if (!m_db.loadSwap(swapId, sm)) {
    m_logger(Logging::ERROR) << "Swap not found: " << swapId;
    return false;
  }

  uint32_t currentHeight = 0;
  if (!m_rpc.getHeight(currentHeight)) {
    m_logger(Logging::ERROR) << "Cannot query fuegod height";
    return false;
  }

  const auto& params = sm.params();
  SwapState current = sm.currentState();

  // Can only refund XFG if we're Bob and XFG is locked
  if (current == SwapState::XFG_LOCKED && params.role == SwapRole::BOB) {
    if (currentHeight < params.xfgTimeoutHeight) {
      m_logger(Logging::ERROR) << "Cannot refund yet. Current height: " << currentHeight
        << ", timeout: " << params.xfgTimeoutHeight
        << " (" << (params.xfgTimeoutHeight - currentHeight) << " blocks remaining)";
      return false;
    }

    // In full implementation: construct and broadcast refund tx
    m_logger(Logging::INFO) << "Timeout elapsed. Constructing XFG refund transaction...";

    if (!sm.transition(SwapState::XFG_REFUNDED)) {
      m_logger(Logging::ERROR) << "State transition failed";
      return false;
    }

    m_db.saveSwap(sm);
    m_logger(Logging::INFO) << "Swap " << swapId << " -> XFG_REFUNDED";
    return true;
  }

  // Can refund counterparty if we're Alice and CTR is locked
  if (current == SwapState::CTR_LOCKED && params.role == SwapRole::ALICE) {
    m_logger(Logging::INFO) << "Constructing counterparty refund transaction...";

    if (!sm.transition(SwapState::CTR_REFUNDED)) {
      m_logger(Logging::ERROR) << "State transition failed";
      return false;
    }

    m_db.saveSwap(sm);
    m_logger(Logging::INFO) << "Swap " << swapId << " -> CTR_REFUNDED";
    return true;
  }

  m_logger(Logging::ERROR) << "Cannot refund swap in state: "
    << swapStateToString(current);
  return false;
}

PriceOracle& SwapDaemon::priceOracle() {
  return m_oracle;
}

} // namespace XfgSwap
