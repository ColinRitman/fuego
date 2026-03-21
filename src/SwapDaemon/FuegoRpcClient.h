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

#include <string>
#include <cstdint>
#include "crypto/hash.h"
#include "crypto/crypto.h"

namespace XfgSwap {

struct HtlcOutputInfo {
  uint64_t amount;
  Crypto::PublicKey recipientKey;
  Crypto::PublicKey refundKey;
  Crypto::Hash hashLock;
  uint32_t timeoutHeight;
  bool isSpent;
};

struct NodeInfo {
  uint32_t height;
  uint64_t difficulty;
  uint64_t txCount;
  std::string status;
};

class FuegoRpcClient {
public:
  FuegoRpcClient(const std::string& host, uint16_t port);

  // Query /getheight
  bool getHeight(uint32_t& height);

  // Query /gethtlc (future RPC endpoint)
  bool getHtlcOutput(uint32_t index, HtlcOutputInfo& out);

  // Query /gethtlccount (future RPC endpoint)
  bool getHtlcCount(size_t& count);

  // Relay via /sendrawtransaction
  bool sendRawTransaction(const std::string& txHex);

  // Query /getinfo
  bool getInfo(NodeInfo& info);

private:
  // Synchronous HTTP POST using POSIX sockets
  std::string httpPost(const std::string& path, const std::string& body);

  std::string m_host;
  uint16_t m_port;
};

} // namespace XfgSwap
