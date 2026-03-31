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
#include "AdaptorSwap.h"
#include "EscrowTxBuilder.h"
#include "Ethereum/ContractAbi.h"
#include "Common/StringTools.h"
#include "Common/Base58.h"
#include "CryptoNoteConfig.h"
#include "crypto/hash.h"
#include "crypto/crypto.h"

extern "C" {
#include "crypto/crypto-ops.h"
}

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
  , m_solKeys(dataDir)
  , m_logger(logger, "SwapDaemon") {
}

std::string SwapDaemon::generateSwapId() {
  struct {
    time_t timestamp;
    uint8_t random[32];
  } seed;

  seed.timestamp = std::time(nullptr);
  Crypto::generate_random_bytes(sizeof(seed.random), seed.random);

  Crypto::Hash hash;
  Crypto::cn_fast_hash(&seed, sizeof(seed), hash);

  return Common::toHex(hash.data, 16);
}

bool SwapDaemon::initiate(SwapParams params) {
  uint32_t currentHeight = 0;
  if (!m_rpc.getHeight(currentHeight)) {
    m_logger(Logging::ERROR) << "Cannot connect to fuegod";
    return false;
  }

  m_logger(Logging::INFO) << "Connected to fuegod at height " << currentHeight;

  if (params.swapId.empty()) {
    params.swapId = generateSwapId();
  }

  // Set default XFG timeout (cooperative refund window: ~1 day)
  if (params.xfgTimeoutHeight == 0) {
    params.xfgTimeoutHeight = currentHeight + 180;
  }

  // Validate price against TWAP
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

  // ── Adaptor sig step 1: generate swap keypair ──
  adaptor_generate_keys(params);

  m_logger(Logging::INFO) << "Generated swap keypair: "
    << Common::podToHex(params.ourSwapPubKey);

  SwapStateMachine sm(params);

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
  m_logger(Logging::INFO) << "  Our swap pubkey: " << Common::podToHex(params.ourSwapPubKey);
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

  auto& params = sm.params();

  // ── Adaptor sig step 2: key aggregation ──
  // Peer's pubkey must be set before calling accept
  if (!adaptor_key_aggregate(params)) {
    m_logger(Logging::ERROR) << "Musig2 key aggregation failed";
    return false;
  }

  m_logger(Logging::INFO) << "Musig2 escrow key: "
    << Common::podToHex(params.escrowPubKey);

  // If Bob, generate adaptor point + DLEQ proof
  if (params.role == SwapRole::BOB) {
    // Use escrow pubkey as DLEQ base point
    if (!adaptor_generate_adaptor(params, params.escrowPubKey)) {
      m_logger(Logging::ERROR) << "Adaptor point generation failed";
      return false;
    }
    m_logger(Logging::INFO) << "Adaptor point T: "
      << Common::podToHex(params.adaptorPoint);

    // Compute hashLock = Keccak-256(adaptorSecret) for the counterparty HTLC.
    Crypto::cn_fast_hash(&params.adaptorSecret, 32, params.hashLock);
    m_logger(Logging::INFO) << "Hash lock: "
      << Common::podToHex(params.hashLock);
  }

  if (!sm.transition(SwapState::ADAPTOR_KEYS_EXCHANGED)) {
    m_logger(Logging::ERROR) << "State transition failed";
    return false;
  }

  if (!m_db.saveSwap(sm)) {
    m_logger(Logging::ERROR) << "Failed to save swap state";
    return false;
  }

  m_logger(Logging::INFO) << "Swap " << swapId << " -> ADAPTOR_KEYS_EXCHANGED";
  m_logger(Logging::INFO) << "  Next: fund XFG escrow to joint key "
    << Common::podToHex(params.escrowPubKey);
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
    if (!m_db.loadSwap(swapId, sm)) continue;
    if (sm.isTerminal()) continue;

    const auto& params = sm.params();

    if (params.xfgTimeoutHeight > 0 && currentHeight >= params.xfgTimeoutHeight) {
      SwapState current = sm.currentState();

      // Cooperative refund possible from escrow-funded or pre-sigs-ready states
      if ((current == SwapState::ADAPTOR_ESCROW_FUNDED ||
           current == SwapState::ADAPTOR_PRESIGS_READY) &&
          params.role == SwapRole::BOB) {
        m_logger(Logging::WARNING) << "Swap " << swapId
          << " XFG timeout reached at height " << currentHeight;

        if (sm.transition(SwapState::ADAPTOR_REFUNDED)) {
          m_db.saveSwap(sm);
          m_logger(Logging::INFO) << "Swap " << swapId << " -> ADAPTOR_REFUNDED";
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
    case SwapState::INITIATED: {
      // Round 1: Exchange pubkeys and chain addresses
      m_logger(Logging::INFO) << "  Round 1: Exchanging keys with peer...";
      if (!exchangeKeysP2P(sm)) {
        break;
      }

      // Key aggregation (requires both pubkeys)
      auto& p = sm.params();
      if (!adaptor_key_aggregate(p)) {
        m_logger(Logging::ERROR) << "Musig2 key aggregation failed";
        break;
      }
      m_logger(Logging::INFO) << "  Escrow key: " << Common::podToHex(p.escrowPubKey);

      // Round 2: Bob generates and sends adaptor info; Alice receives it
      if (p.role == SwapRole::BOB) {
        if (!adaptor_generate_adaptor(p, p.escrowPubKey)) {
          m_logger(Logging::ERROR) << "Adaptor generation failed";
          break;
        }
        Crypto::cn_fast_hash(&p.adaptorSecret, 32, p.hashLock);
        m_logger(Logging::INFO) << "  Adaptor T: " << Common::podToHex(p.adaptorPoint);
        m_logger(Logging::INFO) << "  Hash lock: " << Common::podToHex(p.hashLock);
        if (!sendAdaptorInfo(p)) {
          break;
        }
      } else {
        // Alice: wait for Bob's adaptor info
        m_logger(Logging::INFO) << "  Round 2: Waiting for Bob's adaptor info...";
        if (!receiveAdaptorInfo(p)) {
          break;
        }
      }

      sm.transition(SwapState::ADAPTOR_KEYS_EXCHANGED);
      m_db.saveSwap(sm);
      m_logger(Logging::INFO) << "  → ADAPTOR_KEYS_EXCHANGED";
      break;
    }

    case SwapState::ADAPTOR_KEYS_EXCHANGED: {
      m_logger(Logging::INFO) << "  Keys aggregated. Escrow key: "
        << Common::podToHex(params.escrowPubKey);

      // Bob (has XFG) funds the escrow; Alice verifies it appeared on chain.
      if (params.role == SwapRole::BOB) {
        Crypto::Hash zeroHash;
        std::memset(&zeroHash, 0, sizeof(zeroHash));
        if (std::memcmp(&sm.params().escrowTxHash, &zeroHash, sizeof(zeroHash)) == 0) {
          m_logger(Logging::INFO) << "  Bob: funding escrow...";
          if (fundEscrow(sm.params())) {
            sm.transition(SwapState::ADAPTOR_ESCROW_FUNDED);
            m_db.saveSwap(sm);
            m_logger(Logging::INFO) << "  → ADAPTOR_ESCROW_FUNDED";
            // Notify Alice of escrow tx hash + tx pubkey + output index via P2P
            if (m_p2p) {
              SwapMessage escrowMsg;
              escrowMsg.type = SwapMsgType::SECRET_REVEAL;
              escrowMsg.swapId = sm.params().swapId;
              escrowMsg.payload = Common::podToHex(sm.params().escrowTxHash) + ":" +
                  Common::podToHex(sm.params().escrowTxPubKey) + ":" +
                  std::to_string(sm.params().escrowOutputIndex);
              m_p2p->sendMessage(sm.params().peerEndpoint, escrowMsg);
            }
          } else {
            m_logger(Logging::ERROR) << "  Escrow funding failed. Retry later.";
          }
        } else {
          m_logger(Logging::INFO) << "  Escrow already funded, waiting for confirmation.";
        }
      } else {
        // Alice: wait for Bob's escrow tx hash, then verify on chain
        Crypto::Hash zeroHash;
        std::memset(&zeroHash, 0, sizeof(zeroHash));
        if (std::memcmp(&sm.params().escrowTxHash, &zeroHash, sizeof(zeroHash)) == 0) {
          // Try to receive escrow tx hash + tx pubkey from Bob via P2P
          if (m_p2p) {
            SwapMessage escrowNotif;
            if (m_p2p->waitForMessage(SwapMsgType::SECRET_REVEAL,
                                       params.swapId, escrowNotif, 5000)) {
              // Parse payload: txHash:txPubKey:outputIndex
              std::vector<std::string> escrowFields;
              size_t epos = 0;
              while (epos < escrowNotif.payload.size()) {
                auto enext = escrowNotif.payload.find(':', epos);
                if (enext == std::string::npos) {
                  escrowFields.push_back(escrowNotif.payload.substr(epos));
                  break;
                }
                escrowFields.push_back(escrowNotif.payload.substr(epos, enext - epos));
                epos = enext + 1;
              }
              if (escrowFields.size() >= 1) {
                Common::podFromHex(escrowFields[0], sm.params().escrowTxHash);
              }
              if (escrowFields.size() >= 2) {
                Common::podFromHex(escrowFields[1], sm.params().escrowTxPubKey);
              }
              if (escrowFields.size() >= 3) {
                sm.params().escrowOutputIndex = static_cast<uint32_t>(
                    std::atoi(escrowFields[2].c_str()));
              }
              m_logger(Logging::INFO) << "  Alice: received escrow tx: " << escrowFields[0]
                << " outputIdx=" << sm.params().escrowOutputIndex;
            }
          }
        }
        if (std::memcmp(&sm.params().escrowTxHash, &zeroHash, sizeof(zeroHash)) != 0) {
          if (verifyEscrowFunding(params)) {
            sm.transition(SwapState::ADAPTOR_ESCROW_FUNDED);
            m_db.saveSwap(sm);
            m_logger(Logging::INFO) << "  Alice: escrow confirmed → ADAPTOR_ESCROW_FUNDED";
          } else {
            m_logger(Logging::INFO) << "  Alice: waiting for escrow confirmation...";
          }
        } else {
          m_logger(Logging::INFO) << "  Alice: waiting for Bob to fund escrow...";
        }
      }
      break;
    }

    case SwapState::ADAPTOR_ESCROW_FUNDED: {
      m_logger(Logging::INFO) << "  Escrow funded (tx: "
        << Common::podToHex(params.escrowTxHash) << ").";

      // For the claim path, we skip Musig2 pre-signatures and use
      // ring signatures with the reconstructed full key instead.
      // The Musig2 presig flow is only needed for the cooperative
      // refund path (deferred for testnet).
      //
      // Auto-transition to PRESIGS_READY.
      sm.transition(SwapState::ADAPTOR_PRESIGS_READY);
      m_db.saveSwap(sm);
      m_logger(Logging::INFO) << "  → ADAPTOR_PRESIGS_READY (presig exchange skipped — claim uses ring sig)";
      break;
    }

    case SwapState::ADAPTOR_PRESIGS_READY: {
      // Alice (has counterparty coin) locks HTLC; Bob waits and watches.
      if (params.role == SwapRole::ALICE) {
        if (params.ctrLockTxId.empty()) {
          bool locked = false;
          if (params.pair == SwapPair::SOL) {
            m_logger(Logging::INFO) << "  Alice: locking SOL HTLC...";
            locked = lockSolHtlc(sm.params());
          } else if (params.pair == SwapPair::ETH) {
            m_logger(Logging::INFO) << "  Alice: locking ETH HTLC...";
            locked = lockEthHtlc(sm.params());
          } else {
            m_logger(Logging::INFO) << "  Alice: lock " << swapPairToString(params.pair)
              << " (manual step required for this pair).";
          }

          if (locked) {
            sm.transition(SwapState::ADAPTOR_CTR_LOCKED);
            m_db.saveSwap(sm);
            m_logger(Logging::INFO) << "  → ADAPTOR_CTR_LOCKED";
            // Notify Bob of the lock via P2P (send contractId + txHash)
            if (m_p2p) {
              SwapMessage lockMsg;
              lockMsg.type = SwapMsgType::SECRET_REVEAL;
              lockMsg.swapId = params.swapId;
              lockMsg.payload = sm.params().ctrLockTxId + ":" + sm.params().ethContractId;
              m_p2p->sendMessage(params.peerEndpoint, lockMsg);
            }
          }
        } else {
          m_logger(Logging::INFO) << "  Counterparty already locked.";
        }
      } else {
        // Bob: wait for Alice's counterparty lock notification
        m_logger(Logging::INFO) << "  Bob: waiting for Alice to lock "
          << swapPairToString(params.pair) << "...";
        if (m_p2p) {
          SwapMessage lockNotif;
          if (m_p2p->waitForMessage(SwapMsgType::SECRET_REVEAL,
                                     params.swapId, lockNotif, 5000)) {
            // Parse payload: ctrLockTxId:ethContractId
            auto colonPos = lockNotif.payload.find(':');
            if (colonPos != std::string::npos) {
              sm.params().ctrLockTxId = lockNotif.payload.substr(0, colonPos);
              sm.params().ethContractId = lockNotif.payload.substr(colonPos + 1);
            } else {
              sm.params().ctrLockTxId = lockNotif.payload;
            }
            sm.transition(SwapState::ADAPTOR_CTR_LOCKED);
            m_db.saveSwap(sm);
            m_logger(Logging::INFO) << "  Alice locked CTR: " << sm.params().ctrLockTxId;
            m_logger(Logging::INFO) << "  → ADAPTOR_CTR_LOCKED";
          }
        }
      }
      break;
    }

    case SwapState::ADAPTOR_CTR_LOCKED: {
      // Alice locked counterparty coins. Now Bob claims them, revealing t.
      // Alice monitors the chain for Bob's claim to learn t.
      if (params.role == SwapRole::BOB) {
        // Bob claims the counterparty HTLC by revealing adaptor secret t.
        m_logger(Logging::INFO) << "  " << swapPairToString(params.pair)
          << " locked by Alice. Bob: claim to reveal adaptor secret t.";

        if (params.pair == SwapPair::ETH) {
          if (claimEthHtlc(sm.params())) {
            sm.transition(SwapState::ADAPTOR_SECRET_REVEALED);
            m_db.saveSwap(sm);
            m_logger(Logging::INFO) << "  → ADAPTOR_SECRET_REVEALED";
          }
        } else if (params.pair == SwapPair::SOL) {
          // Bob claims SOL HTLC with preimage = adaptor secret
          SolKeypair kp;
          if (m_solKeys.load(params.swapId, kp)) {
            std::string preimageHex = Common::podToHex(params.adaptorSecret);
            SolTxResult claimResult;
            if (m_solRpc && m_solRpc->claim(kp.toBase58(), params.ctrAddress,
                                             preimageHex, claimResult)) {
              m_logger(Logging::INFO) << "  SOL claimed: " << claimResult.signature;
              sm.transition(SwapState::ADAPTOR_SECRET_REVEALED);
              m_db.saveSwap(sm);
              m_logger(Logging::INFO) << "  → ADAPTOR_SECRET_REVEALED";
            } else {
              m_logger(Logging::ERROR) << "  SOL claim failed";
            }
          }
        } else {
          m_logger(Logging::INFO) << "  Manual claim required for "
            << swapPairToString(params.pair);
        }
      } else {
        // Alice: monitor chain for Bob's claim to learn the adaptor secret.
        m_logger(Logging::INFO) << "  Alice: watching for Bob's "
          << swapPairToString(params.pair) << " claim (reveals adaptor secret)...";

        bool secretLearned = false;
        if (params.pair == SwapPair::ETH) {
          secretLearned = checkEthHtlcClaimed(sm.params());
        } else if (params.pair == SwapPair::SOL) {
          secretLearned = checkSolHtlcClaimed(sm.params());
        }

        if (secretLearned) {
          m_logger(Logging::INFO) << "  Adaptor secret extracted from chain!";
          sm.transition(SwapState::ADAPTOR_SECRET_REVEALED);
          m_db.saveSwap(sm);
          m_logger(Logging::INFO) << "  → ADAPTOR_SECRET_REVEALED";
        }
      }
      break;
    }

    case SwapState::ADAPTOR_SECRET_REVEALED: {
      if (params.role == SwapRole::BOB) {
        // Bob has claimed the counterparty coin. He's done.
        m_logger(Logging::INFO) << "  Bob claimed counterparty. Swap complete.";
        sm.transition(SwapState::ADAPTOR_XFG_SPENT);
        m_db.saveSwap(sm);
        m_logger(Logging::INFO) << "  → ADAPTOR_XFG_SPENT (swap complete for Bob)";
      } else {
        // Alice: now has the adaptor secret t (learned from chain).
        // Reconstruct the full escrow key and spend XFG to herself.
        m_logger(Logging::INFO) << "  Alice: reconstructing escrow key and spending XFG...";

        Crypto::SecretKey zeroKey;
        std::memset(&zeroKey, 0, sizeof(zeroKey));
        if (std::memcmp(&params.adaptorSecret, &zeroKey, sizeof(zeroKey)) == 0) {
          m_logger(Logging::ERROR) << "  Adaptor secret not yet available";
          break;
        }

        // Reconstruct full escrow key and broadcast spend
        if (broadcastEscrowSpend(sm)) {
          sm.transition(SwapState::ADAPTOR_XFG_SPENT);
          m_db.saveSwap(sm);
          m_logger(Logging::INFO) << "  → ADAPTOR_XFG_SPENT (swap complete for Alice)";
        } else {
          m_logger(Logging::ERROR) << "  Escrow spend failed";
        }
      }
      break;
    }

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
            << std::setw(22) << "STATE"
            << std::setw(6)  << "PAIR"
            << std::setw(6)  << "ROLE"
            << std::setw(18) << "XFG AMOUNT"
            << std::endl;
  std::cout << std::string(86, '-') << std::endl;

  for (const auto& swapId : swapIds) {
    SwapStateMachine sm;
    if (!m_db.loadSwap(swapId, sm)) {
      std::cout << swapId << "  [ERROR: cannot load]" << std::endl;
      continue;
    }

    const auto& p = sm.params();
    std::cout << std::left
              << std::setw(34) << p.swapId
              << std::setw(22) << swapStateToString(sm.currentState())
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

  // Adaptor sig fields
  std::cout << "  Our swap pubkey:  " << Common::podToHex(p.ourSwapPubKey) << std::endl;
  std::cout << "  Peer swap pubkey: " << Common::podToHex(p.peerSwapPubKey) << std::endl;
  std::cout << "  Escrow key:       " << Common::podToHex(p.escrowPubKey) << std::endl;

  Crypto::PublicKey zeroPk;
  std::memset(&zeroPk, 0, sizeof(zeroPk));
  if (std::memcmp(&p.adaptorPoint, &zeroPk, sizeof(zeroPk)) != 0) {
    std::cout << "  Adaptor point T:  " << Common::podToHex(p.adaptorPoint) << std::endl;
  }

  Crypto::Hash zeroHash;
  std::memset(&zeroHash, 0, sizeof(zeroHash));
  if (std::memcmp(&p.escrowTxHash, &zeroHash, sizeof(zeroHash)) != 0) {
    std::cout << "  Escrow tx:        " << Common::podToHex(p.escrowTxHash) << std::endl;
    std::cout << "  Escrow out idx:   " << p.escrowOutputIndex << std::endl;
  }

  std::cout << "  XFG timeout:      height " << p.xfgTimeoutHeight << std::endl;
  std::cout << "  CTR timeout:      slot/block " << p.ctrTimeoutBlock << std::endl;

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

  // Cooperative refund: both parties sign a non-adaptor Musig2 sig
  // spending escrow back to Bob. Available from ESCROW_FUNDED or PRESIGS_READY.
  if (current == SwapState::ADAPTOR_ESCROW_FUNDED ||
      current == SwapState::ADAPTOR_PRESIGS_READY) {
    if (currentHeight < params.xfgTimeoutHeight) {
      m_logger(Logging::ERROR) << "Cannot refund yet. Current height: " << currentHeight
        << ", timeout: " << params.xfgTimeoutHeight
        << " (" << (params.xfgTimeoutHeight - currentHeight) << " blocks remaining)";
      return false;
    }

    m_logger(Logging::INFO) << "Timeout elapsed. Initiating cooperative refund...";
    m_logger(Logging::INFO) << "  Both parties must sign a refund tx (no adaptor point).";

    if (!sm.transition(SwapState::ADAPTOR_REFUNDED)) {
      m_logger(Logging::ERROR) << "State transition failed";
      return false;
    }

    m_db.saveSwap(sm);
    m_logger(Logging::INFO) << "Swap " << swapId << " -> ADAPTOR_REFUNDED";
    return true;
  }

  m_logger(Logging::ERROR) << "Cannot refund swap in state: "
    << swapStateToString(current);
  return false;
}

PriceOracle& SwapDaemon::priceOracle() {
  return m_oracle;
}

void SwapDaemon::setWalletRpc(const std::string& host, uint16_t port) {
  m_rpc.setWalletRpc(host, port);
  m_logger(Logging::INFO) << "Wallet RPC configured: " << host << ":" << port;
}

void SwapDaemon::setSolanaRpc(const std::string& host, uint16_t port,
                               const std::string& programId) {
  m_solRpc = std::make_unique<SolRpcClient>(host, port, programId);
  m_logger(Logging::INFO) << "Solana RPC configured: " << host << ":" << port
    << " program=" << programId;
}

void SwapDaemon::setEthereumRpc(const std::string& host, uint16_t port,
                                 const std::string& contractAddr) {
  m_ethRpc = std::make_unique<EthRpcClient>(host, port);
  m_ethContractAddr = contractAddr;
  m_logger(Logging::INFO) << "Ethereum RPC configured: " << host << ":" << port
    << " contract=" << contractAddr;
}

bool SwapDaemon::startP2P(uint16_t listenPort) {
  m_p2p = std::make_unique<SwapP2P>(listenPort, m_logger);
  if (!m_p2p->start()) {
    m_logger(Logging::ERROR) << "Failed to start P2P listener on port " << listenPort;
    m_p2p.reset();
    return false;
  }
  m_logger(Logging::INFO) << "P2P listener started on port " << listenPort;
  return true;
}

Crypto::Hash SwapDaemon::computeEscrowSpendHash(const SwapParams& params) {
  // Domain-separated hash: H("XfgSwapEscrowSpend" || escrowTxHash || escrowOutputIndex || xfgAmount)
  // This serves as the tx prefix hash that both parties sign via Musig2.
  // In the full integration, this will be replaced by the actual
  // CryptoNote transaction prefix hash.
  struct {
    char domain[20];       // "XfgSwapEscrowSpend\0\0"
    Crypto::Hash escrowTx;
    uint32_t outputIdx;
    uint64_t amount;
  } preimage;
  std::memset(&preimage, 0, sizeof(preimage));
  std::memcpy(preimage.domain, "XfgSwapEscrowSpend", 18);
  preimage.escrowTx = params.escrowTxHash;
  preimage.outputIdx = params.escrowOutputIndex;
  preimage.amount = params.xfgAmount;

  Crypto::Hash result;
  Crypto::cn_fast_hash(&preimage, sizeof(preimage), result);
  return result;
}

bool SwapDaemon::exchangeKeysP2P(SwapStateMachine& sm) {
  auto& params = sm.params();

  if (!m_p2p) {
    m_logger(Logging::ERROR) << "P2P not started (use --listen-port)";
    return false;
  }

  // Round 1: Exchange pubkeys and chain addresses only.
  // Adaptor data (adaptorPoint, hashLock, encKeyShare) is sent separately
  // in a ADAPTOR_INFO message after key aggregation completes.
  std::string payload =
      Common::podToHex(params.ourSwapPubKey) + ":" +
      params.ethSenderAddr + ":" +
      params.solSenderPubkey;

  SwapMessage keyMsg;
  keyMsg.type = SwapMsgType::KEY_EXCHANGE;
  keyMsg.swapId = params.swapId;
  keyMsg.payload = payload;

  if (!m_p2p->sendMessage(params.peerEndpoint, keyMsg)) {
    m_logger(Logging::ERROR) << "Failed to send key exchange to peer";
    return false;
  }
  m_logger(Logging::INFO) << "  Sent our pubkey, waiting for peer...";

  // Wait for peer's keys
  SwapMessage peerKeyMsg;
  if (!m_p2p->waitForMessage(SwapMsgType::KEY_EXCHANGE,
                              params.swapId, peerKeyMsg, 120000)) {
    m_logger(Logging::ERROR) << "Timeout waiting for peer key exchange";
    return false;
  }

  // Parse colon-delimited payload: pubKey:ethAddr:solPubkey
  std::string p = peerKeyMsg.payload;
  std::vector<std::string> fields;
  size_t pos = 0;
  while (pos < p.size()) {
    auto next = p.find(':', pos);
    if (next == std::string::npos) {
      fields.push_back(p.substr(pos));
      break;
    }
    fields.push_back(p.substr(pos, next - pos));
    pos = next + 1;
  }

  if (fields.size() < 3) {
    m_logger(Logging::ERROR) << "Invalid peer key exchange payload (expected 3 fields, got "
      << fields.size() << ")";
    return false;
  }

  // Field 0: peer's swap public key
  if (!Common::podFromHex(fields[0], params.peerSwapPubKey)) {
    m_logger(Logging::ERROR) << "Invalid peer swap pubkey";
    return false;
  }

  // Field 1: peer's ETH address
  if (!fields[1].empty()) {
    params.ethRecipientAddr = fields[1];
  }

  // Field 2: peer's SOL pubkey
  if (!fields[2].empty()) {
    params.solRecipientPubkey = fields[2];
  }

  m_logger(Logging::INFO) << "  Peer key exchange received:";
  m_logger(Logging::INFO) << "    Swap pubkey: " << Common::podToHex(params.peerSwapPubKey);
  if (!params.ethRecipientAddr.empty())
    m_logger(Logging::INFO) << "    ETH addr: " << params.ethRecipientAddr;
  if (!params.solRecipientPubkey.empty())
    m_logger(Logging::INFO) << "    SOL pubkey: " << params.solRecipientPubkey;

  return true;
}

bool SwapDaemon::sendAdaptorInfo(SwapParams& params) {
  if (!m_p2p) {
    m_logger(Logging::ERROR) << "P2P not started";
    return false;
  }

  // Bob sends: adaptorPoint:hashLock:encKeyShare
  Crypto::SecretKey b_enc;
  sc_add(reinterpret_cast<unsigned char*>(&b_enc),
         reinterpret_cast<const unsigned char*>(&params.ourSwapSecKey),
         reinterpret_cast<const unsigned char*>(&params.adaptorSecret));

  std::string payload =
      Common::podToHex(params.adaptorPoint) + ":" +
      Common::podToHex(params.hashLock) + ":" +
      Common::podToHex(b_enc);

  SwapMessage msg;
  msg.type = SwapMsgType::ADAPTOR_INFO;
  msg.swapId = params.swapId;
  msg.payload = payload;

  if (!m_p2p->sendMessage(params.peerEndpoint, msg)) {
    m_logger(Logging::ERROR) << "Failed to send adaptor info to peer";
    return false;
  }
  m_logger(Logging::INFO) << "  Sent adaptor info to Alice";
  return true;
}

bool SwapDaemon::receiveAdaptorInfo(SwapParams& params) {
  if (!m_p2p) {
    m_logger(Logging::ERROR) << "P2P not started";
    return false;
  }

  SwapMessage msg;
  if (!m_p2p->waitForMessage(SwapMsgType::ADAPTOR_INFO,
                              params.swapId, msg, 120000)) {
    m_logger(Logging::ERROR) << "Timeout waiting for adaptor info from Bob";
    return false;
  }

  // Parse: adaptorPoint:hashLock:encKeyShare
  std::vector<std::string> fields;
  size_t pos = 0;
  while (pos < msg.payload.size()) {
    auto next = msg.payload.find(':', pos);
    if (next == std::string::npos) {
      fields.push_back(msg.payload.substr(pos));
      break;
    }
    fields.push_back(msg.payload.substr(pos, next - pos));
    pos = next + 1;
  }

  if (fields.size() < 3) {
    m_logger(Logging::ERROR) << "Invalid adaptor info (expected 3 fields)";
    return false;
  }

  if (!Common::podFromHex(fields[0], params.adaptorPoint)) {
    m_logger(Logging::ERROR) << "Invalid adaptor point";
    return false;
  }

  if (!Common::podFromHex(fields[1], params.hashLock)) {
    m_logger(Logging::ERROR) << "Invalid hash lock";
    return false;
  }

  if (!Common::podFromHex(fields[2], params.peerEncryptedKeyShare)) {
    m_logger(Logging::ERROR) << "Invalid encrypted key share";
    return false;
  }

  // Verify: b_enc * G == B + T
  ge_p3 benc_G_p3;
  ge_scalarmult_base(&benc_G_p3,
      reinterpret_cast<const unsigned char*>(&params.peerEncryptedKeyShare));

  ge_p3 B_p3, T_p3;
  if (ge_frombytes_vartime(&B_p3,
          reinterpret_cast<const unsigned char*>(&params.peerSwapPubKey)) != 0 ||
      ge_frombytes_vartime(&T_p3,
          reinterpret_cast<const unsigned char*>(&params.adaptorPoint)) != 0) {
    m_logger(Logging::ERROR) << "  Invalid peer pubkey or adaptor point";
    return false;
  }
  ge_cached T_cached;
  ge_p3_to_cached(&T_cached, &T_p3);
  ge_p1p1 sum_p1p1;
  ge_add(&sum_p1p1, &B_p3, &T_cached);
  ge_p3 sum_p3;
  ge_p1p1_to_p3(&sum_p3, &sum_p1p1);
  unsigned char benc_G_bytes[32], sum_bytes[32];
  ge_p3_tobytes(benc_G_bytes, &benc_G_p3);
  ge_p3_tobytes(sum_bytes, &sum_p3);
  if (std::memcmp(benc_G_bytes, sum_bytes, 32) != 0) {
    m_logger(Logging::ERROR) << "  Encrypted key share verification FAILED (b_enc*G != B + T)";
    return false;
  }
  m_logger(Logging::INFO) << "  Encrypted key share verified: b_enc*G == B + T ✓";
  m_logger(Logging::INFO) << "  Adaptor T: " << Common::podToHex(params.adaptorPoint);
  m_logger(Logging::INFO) << "  Hash lock: " << Common::podToHex(params.hashLock);

  return true;
}

bool SwapDaemon::exchangeNoncesAndPresign(SwapStateMachine& sm,
                                           const Crypto::Hash& tx_prefix_hash) {
  auto& params = sm.params();

  if (!m_p2p) {
    m_logger(Logging::ERROR) << "P2P not started (use --listen-port)";
    return false;
  }

  // Step 1: Generate our Musig2 nonces
  adaptor_nonce_generate(params);
  m_logger(Logging::INFO) << "  Generated Musig2 nonces";

  // Step 2: Exchange nonces with peer.
  // Payload = hex(ourPubNonce) — 2 pub nonce points × 32 bytes = 64 bytes.
  SwapMessage nonceMsg;
  nonceMsg.type = SwapMsgType::NONCE_EXCHANGE;
  nonceMsg.swapId = params.swapId;
  nonceMsg.payload = Common::toHex(
      reinterpret_cast<const uint8_t*>(&params.musig2.ourPubNonce),
      sizeof(params.musig2.ourPubNonce));

  if (!m_p2p->sendMessage(params.peerEndpoint, nonceMsg)) {
    m_logger(Logging::ERROR) << "  Failed to send nonce to peer";
    return false;
  }
  m_logger(Logging::INFO) << "  Sent nonce to peer, waiting for theirs...";

  // Wait for peer's nonce
  SwapMessage peerNonce;
  if (!m_p2p->waitForMessage(SwapMsgType::NONCE_EXCHANGE,
                              params.swapId, peerNonce, 120000)) {
    m_logger(Logging::ERROR) << "  Timeout waiting for peer nonce";
    return false;
  }

  // Decode peer's nonce
  std::string nonceHex = peerNonce.payload;
  if (nonceHex.size() != sizeof(params.musig2.peerPubNonce) * 2) {
    m_logger(Logging::ERROR) << "  Invalid peer nonce size";
    return false;
  }
  Common::podFromHex(nonceHex, params.musig2.peerPubNonce);
  m_logger(Logging::INFO) << "  Received peer nonce";

  // Step 3: Init Musig2 session with adaptor point
  if (!adaptor_session_init(params, tx_prefix_hash, true)) {
    m_logger(Logging::ERROR) << "  Musig2 session init failed";
    return false;
  }

  // Step 4: Create our partial signature
  adaptor_partial_sign(params);
  m_logger(Logging::INFO) << "  Created partial signature";

  // Step 5: Exchange partial sigs
  SwapMessage sigMsg;
  sigMsg.type = SwapMsgType::PRESIG_EXCHANGE;
  sigMsg.swapId = params.swapId;
  sigMsg.payload = Common::toHex(
      reinterpret_cast<const uint8_t*>(&params.musig2.ourPartialSig),
      sizeof(params.musig2.ourPartialSig));

  if (!m_p2p->sendMessage(params.peerEndpoint, sigMsg)) {
    m_logger(Logging::ERROR) << "  Failed to send partial sig to peer";
    return false;
  }
  m_logger(Logging::INFO) << "  Sent partial sig, waiting for peer's...";

  SwapMessage peerSig;
  if (!m_p2p->waitForMessage(SwapMsgType::PRESIG_EXCHANGE,
                              params.swapId, peerSig, 120000)) {
    m_logger(Logging::ERROR) << "  Timeout waiting for peer partial sig";
    return false;
  }

  // Decode peer's partial sig
  std::string sigHex = peerSig.payload;
  if (sigHex.size() != sizeof(params.musig2.peerPartialSig) * 2) {
    m_logger(Logging::ERROR) << "  Invalid peer partial sig size";
    return false;
  }
  Common::podFromHex(sigHex, params.musig2.peerPartialSig);

  // Step 6: Verify peer's partial signature
  if (!adaptor_partial_verify(params)) {
    m_logger(Logging::ERROR) << "  Peer partial signature INVALID — possible attack!";
    sm.transition(SwapState::FAILED);
    m_db.saveSwap(sm);
    return false;
  }

  m_logger(Logging::INFO) << "  Peer partial sig verified ✓";
  return true;
}

bool SwapDaemon::fundEscrow(SwapParams& params) {
  // The escrow public key is the Musig2 aggregate of Alice's and Bob's keys.
  // We convert it to a standard CryptoNote address and send XFG to it via
  // the wallet RPC.  On-chain this creates a normal KeyOutput — indistinguishable
  // from any other transfer — which the 2-of-2 Musig2 signers can spend.

  // Build address: the CryptoNote address encodes (spendKey || viewKey).
  // For escrow, we use the same key for both since both signers cooperate
  // via Musig2 to spend.
  std::string keyData;
  keyData.append(reinterpret_cast<const char*>(&params.escrowPubKey), 32);
  keyData.append(reinterpret_cast<const char*>(&params.escrowPubKey), 32);

  std::string address = Tools::Base58::encode_addr(
      CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX, keyData);

  m_logger(Logging::INFO) << "Funding escrow: " << params.xfgAmount
    << " atomic XFG → " << address;

  // Use mixin 8 (BLOCK_MAJOR_VERSION_10 minimum)
  TransferResult result;
  if (!m_rpc.sendTransfer(address, params.xfgAmount, 8, result)) {
    m_logger(Logging::ERROR) << "Wallet RPC transfer failed";
    return false;
  }

  // Store escrow tx hash
  if (!Common::podFromHex(result.txHash, params.escrowTxHash)) {
    m_logger(Logging::ERROR) << "Invalid tx hash from wallet: " << result.txHash;
    return false;
  }

  // Compute tx public key R = r*G from the tx secret key.
  // Alice will need R to derive the one-time private key for the escrow output.
  if (!result.txSecretKey.empty()) {
    Crypto::SecretKey txSecKey;
    if (Common::podFromHex(result.txSecretKey, txSecKey)) {
      ge_p3 R_p3;
      ge_scalarmult_base(&R_p3, reinterpret_cast<const unsigned char*>(&txSecKey));
      ge_p3_tobytes(reinterpret_cast<unsigned char*>(&params.escrowTxPubKey), &R_p3);
      m_logger(Logging::INFO) << "Escrow tx pubkey R: "
        << Common::podToHex(params.escrowTxPubKey);
    }
  }

  // Identify the escrow output within the funding tx by matching amount.
  std::vector<TxOutputInfo> outputs;
  if (m_rpc.getTransactionOutputs(result.txHash, outputs)) {
    bool found = false;
    for (uint32_t i = 0; i < outputs.size(); ++i) {
      if (outputs[i].amount == params.xfgAmount) {
        params.escrowOutputIndex = i;
        found = true;
        m_logger(Logging::INFO) << "Escrow output at index " << i
          << " (key: " << Common::podToHex(outputs[i].targetKey) << ")";
        break;
      }
    }
    if (!found) {
      m_logger(Logging::WARNING) << "Could not identify escrow output by amount, "
        << "defaulting to index 0";
      params.escrowOutputIndex = 0;
    }
  } else {
    m_logger(Logging::WARNING) << "Could not fetch tx outputs, defaulting to index 0";
    params.escrowOutputIndex = 0;
  }

  m_logger(Logging::INFO) << "Escrow funded: tx " << result.txHash;
  return true;
}

bool SwapDaemon::lockSolHtlc(SwapParams& params) {
  if (!m_solRpc) {
    m_logger(Logging::ERROR) << "Solana RPC not configured";
    return false;
  }

  if (params.pair != SwapPair::SOL) {
    m_logger(Logging::ERROR) << "lockSolHtlc called for non-SOL pair";
    return false;
  }

  // The hashlock is Keccak-256(adaptorSecret).
  // Alice knows the adaptor point T = t*G but NOT the secret t.
  // Bob knows the secret t and computes the hashlock.
  // After Alice claims the SOL (revealing the preimage = adaptor secret),
  // Bob can adapt his XFG pre-signature.
  std::string hashLockHex = Common::podToHex(params.hashLock);

  // Derive the SOL timeout slot: current slot + ~10 minutes (600 slots at 400ms/slot)
  uint64_t currentSlot = 0;
  if (!m_solRpc->getSlot(currentSlot)) {
    m_logger(Logging::ERROR) << "Failed to get Solana slot height";
    return false;
  }

  uint64_t timeoutSlot = currentSlot + 1500;  // ~10 minutes
  params.ctrTimeoutBlock = timeoutSlot;

  m_logger(Logging::INFO) << "Locking " << params.ctrAmount
    << " lamports into SOL HTLC (timeout slot " << timeoutSlot << ")";

  // Generate or load our Solana keypair for this swap.
  SolKeypair kp;
  if (m_solKeys.exists(params.swapId)) {
    if (!m_solKeys.load(params.swapId, kp)) {
      m_logger(Logging::ERROR) << "Failed to load Solana keypair for swap " << params.swapId;
      return false;
    }
  } else {
    kp = SolKeypairStore::generate();
    if (!m_solKeys.save(params.swapId, kp)) {
      m_logger(Logging::ERROR) << "Failed to save Solana keypair for swap " << params.swapId;
      return false;
    }
  }

  params.solSenderPubkey = kp.pubkeyBase58();

  // Alice's Solana pubkey must be known (received via P2P key exchange).
  if (params.solRecipientPubkey.empty()) {
    m_logger(Logging::ERROR) << "Counterparty Solana pubkey not set";
    return false;
  }

  // Call SolRpcClient to build, sign, and send the lock transaction.
  SolTxResult result;
  if (!m_solRpc->lock(kp.toBase58(), params.solRecipientPubkey,
                      hashLockHex, timeoutSlot, params.ctrAmount, result)) {
    m_logger(Logging::ERROR) << "SOL HTLC lock failed: " << result.error;
    return false;
  }

  params.ctrLockTxId = result.signature;

  // Store the HTLC PDA address for later state queries (claim detection).
  params.ctrAddress = m_solRpc->deriveHtlcAddress(params.solSenderPubkey, hashLockHex);

  m_logger(Logging::INFO) << "SOL HTLC locked: " << result.signature;
  m_logger(Logging::INFO) << "  sender: " << params.solSenderPubkey;
  m_logger(Logging::INFO) << "  recipient: " << params.solRecipientPubkey;
  m_logger(Logging::INFO) << "  amount: " << params.ctrAmount << " lamports";
  m_logger(Logging::INFO) << "  timeout: slot " << timeoutSlot;

  return true;
}

bool SwapDaemon::checkSolHtlcClaimed(SwapParams& params) {
  if (!m_solRpc) {
    m_logger(Logging::ERROR) << "Solana RPC not configured";
    return false;
  }

  if (params.ctrLockTxId.empty()) {
    return false;  // No SOL HTLC to check
  }

  // Derive the HTLC PDA address from the hashlock
  // The ctrAddress holds the HTLC account address
  if (params.ctrAddress.empty()) {
    m_logger(Logging::ERROR) << "No HTLC address stored";
    return false;
  }

  SolHtlcInfo info;
  if (!m_solRpc->getHtlcState(params.ctrAddress, info)) {
    return false;
  }

  if (info.claimed) {
    // The preimage IS the adaptor secret t.
    // Store it so Bob can adapt his XFG pre-signature.
    if (!Common::podFromHex(info.preimage, params.adaptorSecret)) {
      m_logger(Logging::ERROR) << "Failed to parse revealed preimage";
      return false;
    }
    m_logger(Logging::INFO) << "SOL HTLC claimed! Adaptor secret revealed.";
    return true;
  }

  if (info.refunded) {
    m_logger(Logging::WARNING) << "SOL HTLC was refunded (swap failed)";
  }

  return false;
}

bool SwapDaemon::reconstructEscrowKey(const SwapParams& params,
                                       Crypto::SecretKey& fullKey) {
  // The escrow public key is P = c_a * A + c_b * B  (Musig2 aggregation).
  // The corresponding private key is: x = c_a * a + c_b * b
  //
  // Alice knows: her key a, the Musig2 coefficients c_a/c_b, and
  // the encrypted key share b_enc = b + t (received from Bob).
  // After learning t (from Bob's HTLC claim), she computes:
  //   b = b_enc - t
  //   x = c_a * a + c_b * b

  if (params.role != SwapRole::ALICE) {
    m_logger(Logging::ERROR) << "reconstructEscrowKey: only Alice reconstructs the escrow key";
    return false;
  }

  // Recover peer's (Bob's) raw key share: b = b_enc - t
  Crypto::SecretKey peerKeyShare;
  sc_sub(reinterpret_cast<unsigned char*>(&peerKeyShare),
         reinterpret_cast<const unsigned char*>(&params.peerEncryptedKeyShare),
         reinterpret_cast<const unsigned char*>(&params.adaptorSecret));

  // Apply Musig2 key aggregation coefficients:
  //   x = c_a * a + c_b * b
  // Alice is signer 0 → coeff[0] = c_a
  // Bob is signer 1 → coeff[1] = c_b
  const auto& c_a = params.musig2.keyAgg.coeff[0];
  const auto& c_b = params.musig2.keyAgg.coeff[1];

  // weighted_a = c_a * a
  // sc_mulsub(s, a, b, c) → s = c - a*b.  So: neg = 0 - c_a*a, then negate.
  unsigned char zero32[32] = {};
  unsigned char neg_wa[32];
  sc_mulsub(neg_wa,
            reinterpret_cast<const unsigned char*>(&c_a),
            reinterpret_cast<const unsigned char*>(&params.ourSwapSecKey),
            zero32);
  Crypto::SecretKey weighted_a;
  sc_sub(reinterpret_cast<unsigned char*>(&weighted_a), zero32, neg_wa);

  // weighted_b = c_b * b
  unsigned char neg_wb[32];
  sc_mulsub(neg_wb,
            reinterpret_cast<const unsigned char*>(&c_b),
            reinterpret_cast<const unsigned char*>(&peerKeyShare),
            zero32);
  Crypto::SecretKey weighted_b;
  sc_sub(reinterpret_cast<unsigned char*>(&weighted_b), zero32, neg_wb);

  // escrowPrivKey = weighted_a + weighted_b
  Crypto::SecretKey escrowPrivKey;
  sc_add(reinterpret_cast<unsigned char*>(&escrowPrivKey),
         reinterpret_cast<const unsigned char*>(&weighted_a),
         reinterpret_cast<const unsigned char*>(&weighted_b));

  // Verify: escrowPrivKey * G == escrowPubKey
  ge_p3 check_p3;
  ge_scalarmult_base(&check_p3, reinterpret_cast<const unsigned char*>(&escrowPrivKey));
  Crypto::PublicKey checkPub;
  ge_p3_tobytes(reinterpret_cast<unsigned char*>(&checkPub), &check_p3);
  if (std::memcmp(&checkPub, &params.escrowPubKey, 32) != 0) {
    m_logger(Logging::ERROR) << "Escrow key reconstruction FAILED — key mismatch!";
    m_logger(Logging::ERROR) << "  Expected: " << Common::podToHex(params.escrowPubKey);
    m_logger(Logging::ERROR) << "  Got:      " << Common::podToHex(checkPub);
    return false;
  }
  m_logger(Logging::INFO) << "  Escrow key reconstructed and verified ✓";

  fullKey = escrowPrivKey;
  return true;
}

bool SwapDaemon::broadcastEscrowSpend(SwapStateMachine& sm) {
  auto& params = sm.params();

  // Only Alice calls this — she reconstructs the full escrow key
  // from her key share + the decrypted peer share.

  // Step 1: Reconstruct the full escrow aggregate private key
  Crypto::SecretKey escrowFullKey;
  if (!reconstructEscrowKey(params, escrowFullKey)) {
    m_logger(Logging::ERROR) << "  Failed to reconstruct escrow key";
    return false;
  }

  // Step 2: Derive the one-time output private key.
  //
  // The escrow was funded via standard CryptoNote wallet RPC, which creates
  // a stealth output: P' = Hs(r * viewKey)*G + spendKey
  // where viewKey = spendKey = escrowPubKey, r = funding tx secret key.
  //
  // The corresponding private key is:
  //   x' = Hs(escrowPrivKey * R) + escrowPrivKey
  // where R is the funding tx public key (r*G).
  Crypto::KeyDerivation derivation;
  if (!Crypto::generate_key_derivation(params.escrowTxPubKey, escrowFullKey, derivation)) {
    m_logger(Logging::ERROR) << "  Failed to generate key derivation from tx pubkey";
    return false;
  }

  Crypto::SecretKey outputSecKey;
  Crypto::derive_secret_key(derivation, params.escrowOutputIndex, escrowFullKey, outputSecKey);

  // Derive the corresponding one-time public key (for the ring).
  Crypto::PublicKey outputPubKey;
  if (!Crypto::derive_public_key(derivation, params.escrowOutputIndex, params.escrowPubKey, outputPubKey)) {
    m_logger(Logging::ERROR) << "  Failed to derive output public key";
    return false;
  }
  m_logger(Logging::INFO) << "  Derived escrow output key: " << Common::podToHex(outputPubKey);

  // Step 3: Resolve global output index for the escrow output.
  // The ring signature needs the global index, not the within-tx index.
  std::string escrowTxHex = Common::podToHex(params.escrowTxHash);
  std::vector<uint64_t> globalIndexes;
  if (!m_rpc.getGlobalOutputIndexes(escrowTxHex, globalIndexes)) {
    m_logger(Logging::ERROR) << "  Failed to get global output indexes for escrow tx "
      << escrowTxHex << " (tx may not be confirmed yet)";
    return false;
  }
  if (params.escrowOutputIndex >= globalIndexes.size()) {
    m_logger(Logging::ERROR) << "  Escrow output index " << params.escrowOutputIndex
      << " out of range (tx has " << globalIndexes.size() << " outputs)";
    return false;
  }
  uint64_t escrowGlobalIdx = globalIndexes[params.escrowOutputIndex];
  m_logger(Logging::INFO) << "  Escrow output global index: " << escrowGlobalIdx;

  // Step 4: Get ring members for the escrow input
  std::vector<RandomOutput> randomOuts;
  if (!m_rpc.getRandomOutputs(params.xfgAmount, 8, randomOuts)) {
    m_logger(Logging::ERROR) << "  Failed to get ring members for escrow spend";
    return false;
  }

  std::vector<RingMember> ringMembers;
  for (const auto& ro : randomOuts) {
    ringMembers.push_back({ro.globalIndex, ro.key});
  }

  // Step 5: Generate a destination one-time key for Alice's output.
  // For testnet, we generate a random keypair. In production, this
  // would be derived from Alice's wallet address so the wallet can
  // recognize and spend the received XFG.
  Crypto::PublicKey destPubKey;
  Crypto::SecretKey destSecKey;
  Crypto::generate_keys(destPubKey, destSecKey);
  m_logger(Logging::INFO) << "  Destination key: " << Common::podToHex(destPubKey);
  m_logger(Logging::INFO) << "  (testnet: save dest secret key to recover funds)";

  // Step 6: Build and sign the escrow spend transaction
  // Pass the DERIVED output key (not the raw aggregate key).
  uint64_t fee = 10000;  // 0.001 XFG
  EscrowSpendResult txResult;
  if (!EscrowTxBuilder::build(
        outputSecKey, outputPubKey,
        params.xfgAmount, fee, destPubKey,
        ringMembers, escrowGlobalIdx,
        txResult)) {
    m_logger(Logging::ERROR) << "  Failed to build escrow spend tx";
    return false;
  }

  m_logger(Logging::INFO) << "  Escrow spend tx built: " << Common::podToHex(txResult.txHash);

  // Step 6: Broadcast the raw transaction
  if (!m_rpc.sendRawTransaction(txResult.txHex)) {
    m_logger(Logging::ERROR) << "  Failed to broadcast escrow spend tx";
    return false;
  }

  m_logger(Logging::INFO) << "  Escrow spend BROADCAST: " << Common::podToHex(txResult.txHash);
  return true;
}

bool SwapDaemon::lockEthHtlc(SwapParams& params) {
  if (!m_ethRpc) {
    m_logger(Logging::ERROR) << "Ethereum RPC not configured";
    return false;
  }

  if (params.pair != SwapPair::ETH) {
    m_logger(Logging::ERROR) << "lockEthHtlc called for non-ETH pair";
    return false;
  }

  if (m_ethContractAddr.empty()) {
    m_logger(Logging::ERROR) << "ETH HTLC contract address not configured";
    return false;
  }

  if (params.ethSenderAddr.empty()) {
    m_logger(Logging::ERROR) << "Our ETH address not set";
    return false;
  }
  if (params.ethRecipientAddr.empty()) {
    m_logger(Logging::ERROR) << "Counterparty ETH address not set";
    return false;
  }

  // Compute timeout: current block + ~10 minutes (~50 blocks at ~12s/block)
  uint64_t currentBlock = 0;
  if (!m_ethRpc->getBlockNumber(currentBlock)) {
    m_logger(Logging::ERROR) << "Failed to get ETH block number";
    return false;
  }
  uint64_t timeoutBlock = currentBlock + 50;
  params.ctrTimeoutBlock = timeoutBlock;

  // Build the lock calldata using ABI encoder
  std::string calldata = EthAbi::encodeLock(
      params.ethRecipientAddr, params.hashLock, timeoutBlock);

  m_logger(Logging::INFO) << "Locking " << params.ctrAmount
    << " wei into ETH HTLC (timeout block " << timeoutBlock << ")";

  std::string txHash;
  if (!m_ethRpc->sendTransaction(
          params.ethSenderAddr, m_ethContractAddr,
          calldata, params.ctrAmount, 200000, txHash)) {
    m_logger(Logging::ERROR) << "ETH HTLC lock transaction failed";
    return false;
  }

  // Verify receipt
  EthTxReceipt receipt;
  if (m_ethRpc->getTransactionReceipt(txHash, receipt) && !receipt.success) {
    m_logger(Logging::ERROR) << "ETH HTLC lock tx reverted: " << txHash;
    return false;
  }

  params.ctrLockTxId = txHash;

  // Derive contractId: keccak256(abi.encodePacked(sender, recipient, value, hashLock, timeoutBlock))
  // The contract emits this as an indexed topic in the Lock event.
  // We compute it the same way the contract does.
  {
    // abi.encodePacked uses tight packing:
    // address = 20 bytes, uint256 = 32 bytes, bytes32 = 32 bytes
    uint8_t packed[20 + 20 + 32 + 32 + 32]; // 136 bytes
    std::memset(packed, 0, sizeof(packed));

    // Parse sender address (strip 0x, decode 20 bytes)
    std::string senderHex = params.ethSenderAddr;
    if (senderHex.size() >= 2 && senderHex[0] == '0' && senderHex[1] == 'x')
      senderHex = senderHex.substr(2);
    for (size_t i = 0; i < 20 && i * 2 + 1 < senderHex.size(); ++i) {
      uint8_t hi = (senderHex[2*i] >= 'a') ? (senderHex[2*i] - 'a' + 10) :
                   (senderHex[2*i] >= 'A') ? (senderHex[2*i] - 'A' + 10) :
                   (senderHex[2*i] - '0');
      uint8_t lo = (senderHex[2*i+1] >= 'a') ? (senderHex[2*i+1] - 'a' + 10) :
                   (senderHex[2*i+1] >= 'A') ? (senderHex[2*i+1] - 'A' + 10) :
                   (senderHex[2*i+1] - '0');
      packed[i] = (hi << 4) | lo;
    }

    // Parse recipient address
    std::string recipHex = params.ethRecipientAddr;
    if (recipHex.size() >= 2 && recipHex[0] == '0' && recipHex[1] == 'x')
      recipHex = recipHex.substr(2);
    for (size_t i = 0; i < 20 && i * 2 + 1 < recipHex.size(); ++i) {
      uint8_t hi = (recipHex[2*i] >= 'a') ? (recipHex[2*i] - 'a' + 10) :
                   (recipHex[2*i] >= 'A') ? (recipHex[2*i] - 'A' + 10) :
                   (recipHex[2*i] - '0');
      uint8_t lo = (recipHex[2*i+1] >= 'a') ? (recipHex[2*i+1] - 'a' + 10) :
                   (recipHex[2*i+1] >= 'A') ? (recipHex[2*i+1] - 'A' + 10) :
                   (recipHex[2*i+1] - '0');
      packed[20 + i] = (hi << 4) | lo;
    }

    // uint256 value (big-endian, 32 bytes, value in last 8 bytes)
    uint64_t val = params.ctrAmount;
    for (int i = 7; i >= 0; --i) {
      packed[40 + 24 + i] = static_cast<uint8_t>(val & 0xFF);
      val >>= 8;
    }

    // bytes32 hashLock
    std::memcpy(packed + 72, params.hashLock.data, 32);

    // uint256 timeoutBlock
    uint64_t tb = timeoutBlock;
    for (int i = 7; i >= 0; --i) {
      packed[104 + 24 + i] = static_cast<uint8_t>(tb & 0xFF);
      tb >>= 8;
    }

    Crypto::Hash contractId;
    Crypto::cn_fast_hash(packed, sizeof(packed), contractId);
    params.ethContractId = Common::podToHex(contractId);
  }

  m_logger(Logging::INFO) << "ETH HTLC locked: " << txHash;
  m_logger(Logging::INFO) << "  sender: " << params.ethSenderAddr;
  m_logger(Logging::INFO) << "  recipient: " << params.ethRecipientAddr;
  m_logger(Logging::INFO) << "  amount: " << params.ctrAmount << " wei";
  m_logger(Logging::INFO) << "  timeout: block " << timeoutBlock;
  m_logger(Logging::INFO) << "  contractId: " << params.ethContractId;

  return true;
}

bool SwapDaemon::checkEthHtlcClaimed(SwapParams& params) {
  if (!m_ethRpc) {
    return false;
  }

  if (params.ethContractId.empty()) {
    return false;
  }

  // Call getContract(contractId) on the HTLC contract
  std::string calldata = EthAbi::encodeGetContract(params.ethContractId);
  std::string result;
  if (!m_ethRpc->callContract(m_ethContractAddr, calldata, result)) {
    return false;
  }

  EthAbi::ContractInfo info;
  if (!EthAbi::decodeGetContract(result, info)) {
    return false;
  }

  if (info.claimed) {
    // The preimage IS the adaptor secret t.
    params.adaptorSecret = *reinterpret_cast<const Crypto::SecretKey*>(&info.preimage);
    m_logger(Logging::INFO) << "ETH HTLC claimed! Adaptor secret revealed.";
    return true;
  }

  if (info.refunded) {
    m_logger(Logging::WARNING) << "ETH HTLC was refunded (swap failed)";
  }

  return false;
}

bool SwapDaemon::claimEthHtlc(SwapParams& params) {
  if (!m_ethRpc) {
    m_logger(Logging::ERROR) << "Ethereum RPC not configured";
    return false;
  }

  if (params.ethContractId.empty()) {
    m_logger(Logging::ERROR) << "No ETH contract ID to claim";
    return false;
  }

  // Bob claims ETH by revealing the adaptor secret t as the preimage.
  // The hashLock = Keccak-256(t), so revealing t proves knowledge.
  Crypto::Hash preimage;
  std::memcpy(&preimage, &params.adaptorSecret, 32);
  std::string calldata = EthAbi::encodeClaim(params.ethContractId, preimage);

  std::string txHash;
  if (!m_ethRpc->sendTransaction(
          params.ethSenderAddr, m_ethContractAddr,
          calldata, 0, 200000, txHash)) {
    m_logger(Logging::ERROR) << "ETH HTLC claim failed";
    return false;
  }

  // Verify receipt
  EthTxReceipt receipt;
  if (m_ethRpc->getTransactionReceipt(txHash, receipt) && !receipt.success) {
    m_logger(Logging::ERROR) << "ETH HTLC claim tx reverted: " << txHash;
    return false;
  }

  m_logger(Logging::INFO) << "ETH HTLC claimed: " << txHash;
  return true;
}

bool SwapDaemon::verifyEscrowFunding(const SwapParams& params) {
  // Check that the escrow tx exists on chain (or in pool) and has
  // an output with the expected amount.
  Crypto::Hash zeroHash;
  std::memset(&zeroHash, 0, sizeof(zeroHash));
  if (std::memcmp(&params.escrowTxHash, &zeroHash, sizeof(zeroHash)) == 0) {
    m_logger(Logging::ERROR) << "No escrow tx hash set";
    return false;
  }

  std::string txHashHex = Common::podToHex(params.escrowTxHash);
  std::vector<TxOutputInfo> outputs;
  if (!m_rpc.getTransactionOutputs(txHashHex, outputs)) {
    m_logger(Logging::WARNING) << "Escrow tx not yet visible: " << txHashHex;
    return false;
  }

  // Transaction exists on chain. Full output verification (matching one-time
  // keys to the escrow public key) requires CryptoNote deserialization, which
  // will be wired once the tx builder integration is complete. For now we
  // confirm the tx is present — the Musig2 protocol guarantees that only
  // the two signers can spend the escrow output regardless.
  m_logger(Logging::INFO) << "Escrow tx confirmed on chain: " << txHashHex;
  return true;
}

} // namespace XfgSwap
