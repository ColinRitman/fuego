// Copyright (c) 2017-2025 Fuego Developers
// Copyright (c) 2020-2025 Elderfire Privacy Group
//
// This file is part of Fuego.

#include "ElderfierSignatureDaemon.h"
#include "ICore.h"
#include "EldernodeIndexManager.h"
#include "CommitmentIndex.h"
#include "P2p/P2pProtocolDefinitions.h"
#include "crypto/crypto.h"
#include "Common/StringTools.h"
#include <chrono>
#include <algorithm>

namespace CryptoNote {

ElderfierSignatureDaemon::ElderfierSignatureDaemon(
  ICore& core,
  IEldernodeIndexManager& eldernodeIndex,
  IP2pEndpoint* p2pEndpoint,
  Logging::ILogger& logger)
  : m_core(core), m_eldernodeIndex(eldernodeIndex), m_p2pEndpoint(p2pEndpoint),
    m_logger(logger, "ElderfierSignatureDaemon") {
  m_logger(Logging::INFO, Logging::BRIGHT_CYAN) << "ElderfierSignatureDaemon initialized";
}

ElderfierSignatureDaemon::~ElderfierSignatureDaemon() {
  stop();
}

void ElderfierSignatureDaemon::start() {
  if (m_running) {
    m_logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Daemon already running";
    return;
  }

  m_running = true;
  m_daemonThread = std::thread([this] { daemonThread(); });
  m_logger(Logging::INFO, Logging::BRIGHT_GREEN) << "ElderfierSignatureDaemon started";
}

void ElderfierSignatureDaemon::stop() {
  if (!m_running) {
    return;
  }

  m_running = false;
  if (m_daemonThread.joinable()) {
    m_daemonThread.join();
  }

  m_logger(Logging::INFO, Logging::BRIGHT_GREEN) << "ElderfierSignatureDaemon stopped";
}

bool ElderfierSignatureDaemon::isRunning() const {
  return m_running.load();
}

void ElderfierSignatureDaemon::setSigningEnabled(bool enabled) {
  m_signingEnabled = enabled;
  m_logger(Logging::DEBUGGING, Logging::BRIGHT_WHITE)
      << "Signature generation " << (enabled ? "enabled" : "disabled");
}

void ElderfierSignatureDaemon::setElderfierIds(const std::vector<uint8_t>& elfIds) {
  std::unique_lock<std::mutex> lock(m_mutex);
  m_elderfierIds = elfIds;
  m_logger(Logging::DEBUGGING, Logging::BRIGHT_GREEN)
      << "Registered " << elfIds.size() << " elderfiers for signing";
}

void ElderfierSignatureDaemon::addElderfier(uint8_t elderfier_id) {
  std::unique_lock<std::mutex> lock(m_mutex);
  if (std::find(m_elderfierIds.begin(), m_elderfierIds.end(), elderfier_id) == m_elderfierIds.end()) {
    m_elderfierIds.push_back(elderfier_id);
    m_logger(Logging::DEBUGGING, Logging::BRIGHT_GREEN)
        << "Added elderfier " << static_cast<int>(elderfier_id) << " for signing";
  }
}

void ElderfierSignatureDaemon::removeElderfier(uint8_t elderfier_id) {
  std::unique_lock<std::mutex> lock(m_mutex);
  auto it = std::find(m_elderfierIds.begin(), m_elderfierIds.end(), elderfier_id);
  if (it != m_elderfierIds.end()) {
    m_elderfierIds.erase(it);
    m_logger(Logging::DEBUGGING, Logging::BRIGHT_GREEN)
        << "Removed elderfier " << static_cast<int>(elderfier_id) << " from signing";
  }
}

uint64_t ElderfierSignatureDaemon::getSignaturesGenerated() const {
  return m_signaturesGenerated.load();
}

uint64_t ElderfierSignatureDaemon::getSignaturesFailed() const {
  return m_signaturesFailed.load();
}

uint64_t ElderfierSignatureDaemon::getLastSignatureHeight() const {
  return m_lastSignatureHeight.load();
}

uint64_t ElderfierSignatureDaemon::getLastSignatureTime() const {
  return m_lastSignatureTime.load();
}

void ElderfierSignatureDaemon::onNewBlock(uint32_t blockHeight, const Crypto::Hash& merkleRoot) {
  if (!m_running || !m_signingEnabled) {
    return;
  }

  // Queue block for signature generation (will process after delay)
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_pendingBlocks.push(std::make_pair(blockHeight, merkleRoot));
  }
}

bool ElderfierSignatureDaemon::shouldSign(uint8_t elderfier_id, uint32_t blockHeight) const {
  // Elderfier should sign if registered and not recently signed
  // Can add more sophisticated logic here (e.g., random selection, rotation)
  return true;
}

void ElderfierSignatureDaemon::daemonThread() {
  m_logger(Logging::DEBUGGING, Logging::BRIGHT_WHITE) << "Daemon thread started";

  while (m_running) {
    try {
      uint32_t currentHeight;
      Crypto::Hash topId;
      m_core.get_blockchain_top(currentHeight, topId);

      // Process pending blocks
      {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_pendingBlocks.empty()) {
          auto [blockHeight, merkleRoot] = m_pendingBlocks.front();

          // Only process if we're past the signature generation delay
          if (currentHeight >= blockHeight + SIGNATURE_GENERATION_DELAY_BLOCKS) {
            m_pendingBlocks.pop();

            // Get list of elderfiers to sign for
            std::vector<uint8_t> elderfierIds = m_elderfierIds;
            lock.unlock();

            // Generate signatures for each elderfier
            for (uint8_t elf_id : elderfierIds) {
              if (shouldSign(elf_id, blockHeight)) {
                if (generateAndBroadcastSignature(elf_id, blockHeight, merkleRoot)) {
                  m_signaturesGenerated++;
                  m_lastSignatureHeight = blockHeight;
                  m_lastSignatureTime = std::time(nullptr);
                } else {
                  m_signaturesFailed++;
                }
              }
            }

            lock.lock();
          } else {
            // Not yet time to process this block
            break;
          }
        }
      }

      // Sleep briefly to avoid busy waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(SIGNATURE_GENERATION_INTERVAL_MS));

    } catch (const std::exception& e) {
      m_logger(Logging::ERROR, Logging::BRIGHT_RED)
          << "Error in signature daemon: " << e.what();
      std::this_thread::sleep_for(std::chrono::milliseconds(SIGNATURE_GENERATION_INTERVAL_MS));
    }
  }

  m_logger(Logging::DEBUGGING, Logging::BRIGHT_WHITE) << "Daemon thread exited";
}

bool ElderfierSignatureDaemon::generateAndBroadcastSignature(
  uint8_t elderfier_id,
  uint32_t blockHeight,
  const Crypto::Hash& merkleRoot) {
  try {
    // STEP 1: Validate elderfier registration
    if (!validateElderfierRegistration(elderfier_id)) {
      m_logger(Logging::WARNING, Logging::BRIGHT_YELLOW)
          << "Elderfier " << static_cast<int>(elderfier_id) << " not registered for signing";
      return false;
    }

    // STEP 2: Generate signature
    Crypto::Signature sig = signMerkleRoot(merkleRoot);

    // STEP 3: Broadcast signature
    if (broadcastSignature(elderfier_id, blockHeight, merkleRoot, sig)) {
      m_logger(Logging::DEBUGGING, Logging::BRIGHT_GREEN)
          << "Signature generated and broadcast: EFiD " << static_cast<int>(elderfier_id)
          << " at height " << blockHeight;
      return true;
    } else {
      m_logger(Logging::WARNING, Logging::BRIGHT_YELLOW)
          << "Failed to broadcast signature for EFiD " << static_cast<int>(elderfier_id);
      return false;
    }

  } catch (const std::exception& e) {
    m_logger(Logging::ERROR, Logging::BRIGHT_RED)
        << "Error generating signature: " << e.what();
    return false;
  }
}

bool ElderfierSignatureDaemon::validateElderfierRegistration(uint8_t elderfier_id) {
  try {
    // Check if elderfier is registered in EldernodeIndex
    auto deposit = m_eldernodeIndex.getElderfierDeposit(Crypto::PublicKey{});  // TODO: Get actual pubkey
    return deposit.isActive && !deposit.isSpent && deposit.depositAmount >= 800000000000;  // 800 XFG
  } catch (const std::exception&) {
    return false;
  }
}

Crypto::Signature ElderfierSignatureDaemon::signMerkleRoot(const Crypto::Hash& merkleRoot) {
  // Generate ephemeral keypair for this signature
  Crypto::PublicKey pubkey;
  Crypto::SecretKey seckey;
  Crypto::generate_keys(pubkey, seckey);

  // Sign the merkle root
  Crypto::Signature signature;
  Crypto::generate_signature(merkleRoot, pubkey, seckey, signature);
  return signature;
}

bool ElderfierSignatureDaemon::broadcastSignature(
  uint8_t elderfier_id,
  uint32_t blockHeight,
  const Crypto::Hash& merkleRoot,
  const Crypto::Signature& signature) {
  if (!m_p2pEndpoint) {
    m_logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "P2P endpoint not configured";
    return false;
  }

  try {
    // Create P2P message
    COMMAND_ELDERFIER_SIGNATURE::request sig_msg;
    sig_msg.merkle_root = merkleRoot;
    sig_msg.signature = signature;
    sig_msg.elderfier_id = elderfier_id;
    sig_msg.block_height = blockHeight;
    sig_msg.timestamp = std::time(nullptr);
    sig_msg.version = 1;

    // Broadcast via P2P (relay to all peers)
    // TODO: Integrate with actual P2P broadcast mechanism
    m_logger(Logging::DEBUGGING, Logging::BRIGHT_GREEN)
        << "Broadcasting elderfier signature: EFiD " << static_cast<int>(elderfier_id)
        << ", height " << blockHeight;

    return true;

  } catch (const std::exception& e) {
    m_logger(Logging::ERROR, Logging::BRIGHT_RED)
        << "Error broadcasting signature: " << e.what();
    return false;
  }
}

}  // namespace CryptoNote
