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

#pragma once

#include "SwapTypes.h"
#include "SwapStateMachine.h"
#include "SwapDatabase.h"
#include "FuegoRpcClient.h"
#include "Solana/SolRpcClient.h"
#include "Solana/SolKeypair.h"
#include "Ethereum/EthRpcClient.h"
#include "SwapP2P.h"
#include "PriceOracle.h"
#include "Logging/LoggerRef.h"

#include <string>
#include <memory>

namespace XfgSwap {

class SwapDaemon {
public:
  SwapDaemon(const std::string& fuegodHost, uint16_t fuegodPort,
             const std::string& dataDir, Logging::ILogger& logger);

  // Configure wallet RPC endpoint for escrow funding.
  // Must be called before processSwap() can fund escrow.
  void setWalletRpc(const std::string& host, uint16_t port);

  // Configure Solana RPC endpoint for SOL HTLC operations.
  // programId: deployed xfg_htlc Anchor program ID (base58).
  void setSolanaRpc(const std::string& host, uint16_t port,
                    const std::string& programId);

  // Configure Ethereum RPC endpoint for ETH HTLC operations.
  // contractAddr: deployed HashedTimelock contract address (0x...).
  void setEthereumRpc(const std::string& host, uint16_t port,
                      const std::string& contractAddr);

  // Start P2P listener for swap message exchange.
  bool startP2P(uint16_t listenPort);

  // Start a new swap as initiator (Bob: has XFG, wants counterparty coin).
  bool initiate(SwapParams params);

  // Accept an incoming swap proposal.
  bool accept(const std::string& swapId);

  // Scan active swaps and refund any that have timed out.
  bool checkTimeouts();

  // Advance a specific swap to its next state based on chain observations.
  bool processSwap(const std::string& swapId);

  // Print a summary of all swaps.
  void listSwaps();

  // Print detailed info about a specific swap.
  void showSwap(const std::string& swapId);

  // Attempt to refund a specific swap (if timeout has elapsed).
  bool refund(const std::string& swapId);

  // Access the price oracle for configuration.
  PriceOracle& priceOracle();

private:
  // Generate a unique swap ID from the current time and random data.
  std::string generateSwapId();

  // Fund the XFG escrow by sending to the Musig2 joint key address.
  // Computes escrow address from params.escrowPubKey, sends XFG via
  // wallet RPC, and stores the resulting tx hash in params.
  // Returns true on success.
  bool fundEscrow(SwapParams& params);

  // Verify that the escrow funding tx exists and contains an output
  // with the expected amount to the joint escrow key.
  // Returns true if the escrow is confirmed on chain.
  bool verifyEscrowFunding(const SwapParams& params);

  // Lock SOL into the Solana HTLC (Bob's counterparty lock).
  // hashLockHex: Keccak-256 of the adaptor secret, 32 bytes hex.
  bool lockSolHtlc(SwapParams& params);

  // Lock ETH into the Ethereum HTLC (Bob's counterparty lock).
  bool lockEthHtlc(SwapParams& params);

  // Bob claims the ETH HTLC by revealing the adaptor secret as preimage.
  bool claimEthHtlc(SwapParams& params);

  // Check if the ETH HTLC has been claimed (adaptor secret revealed).
  bool checkEthHtlcClaimed(SwapParams& params);

  // Check if the SOL HTLC has been claimed (adaptor secret revealed).
  // If claimed, stores the revealed preimage (adaptor secret) in params.
  bool checkSolHtlcClaimed(SwapParams& params);

  // Round 1: Exchange swap pubkeys and chain addresses via P2P.
  bool exchangeKeysP2P(SwapStateMachine& sm);

  // Round 2: Bob sends adaptor point, hashLock, encrypted key share.
  bool sendAdaptorInfo(SwapParams& params);

  // Round 2: Alice receives and verifies adaptor info from Bob.
  bool receiveAdaptorInfo(SwapParams& params);

  // Exchange Musig2 nonces via P2P, init session, create partial sigs.
  // tx_prefix_hash: the hash of the escrow-spend transaction structure.
  bool exchangeNoncesAndPresign(SwapStateMachine& sm,
                                 const Crypto::Hash& tx_prefix_hash);

  // Build and broadcast the escrow spend transaction with the adapted
  // Musig2 signature. Returns true on success.
  bool broadcastEscrowSpend(SwapStateMachine& sm);

  // Reconstruct the full escrow private key from our key share and
  // the peer's key share (recovered via adaptor secret).
  bool reconstructEscrowKey(const SwapParams& params,
                            Crypto::SecretKey& fullKey);

  // Compute a deterministic tx prefix hash for the escrow spend.
  // In full integration this will be the actual transaction prefix hash;
  // for now it's domain-separated from the escrow tx hash.
  Crypto::Hash computeEscrowSpendHash(const SwapParams& params);

  FuegoRpcClient m_rpc;
  std::unique_ptr<SolRpcClient> m_solRpc;
  std::unique_ptr<EthRpcClient> m_ethRpc;
  std::unique_ptr<SwapP2P> m_p2p;
  SwapDatabase m_db;
  SolKeypairStore m_solKeys;
  PriceOracle m_oracle;
  Logging::LoggerRef m_logger;
  std::string m_ethContractAddr;
};

} // namespace XfgSwap
