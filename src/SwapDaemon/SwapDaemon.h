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
#include "PriceOracle.h"
#include "Logging/LoggerRef.h"

#include <string>
#include <memory>

namespace XfgSwap {

class SwapDaemon {
public:
  SwapDaemon(const std::string& fuegodHost, uint16_t fuegodPort,
             const std::string& dataDir, Logging::ILogger& logger);

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

  FuegoRpcClient m_rpc;
  SwapDatabase m_db;
  PriceOracle m_oracle;
  Logging::LoggerRef m_logger;
};

} // namespace XfgSwap
