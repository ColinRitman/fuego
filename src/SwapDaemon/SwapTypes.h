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
#include <vector>
#include <functional>
#include "crypto/hash.h"
#include "crypto/crypto.h"

namespace XfgSwap {

enum class SwapState : uint8_t {
  INITIATED = 0,
  XFG_LOCKED = 1,       // Bob created HTLC on Fuego
  CTR_LOCKED = 2,       // Alice locked on counterparty chain (XMR/ETH/BCH)
  XFG_CLAIMED = 3,      // Alice claimed XFG (preimage revealed)
  CTR_CLAIMED = 4,      // Bob claimed on counterparty chain
  XFG_REFUNDED = 5,     // Bob refunded XFG (timeout)
  CTR_REFUNDED = 6,     // Alice refunded counterparty chain (timeout)
  FAILED = 7
};

enum class SwapRole : uint8_t {
  ALICE = 0,  // Has counterparty coin, wants XFG
  BOB = 1     // Has XFG, wants counterparty coin
};

enum class SwapPair : uint8_t {
  XMR = 0,
  ETH = 1,
  BCH = 2
};

struct SwapParams {
  std::string swapId;           // unique swap identifier (hex hash)
  SwapPair pair;
  SwapRole role;
  uint64_t xfgAmount;           // atomic units
  uint64_t ctrAmount;           // counterparty amount (atomic units)

  // Keys
  Crypto::PublicKey aliceXfgPubKey;
  Crypto::PublicKey bobXfgPubKey;

  // HTLC params
  Crypto::Hash hashLock;
  Crypto::Hash preimage;        // known only by initiator until claim
  uint32_t xfgTimeoutHeight;
  uint64_t ctrTimeoutBlock;     // counterparty chain timeout

  // Chain state
  uint32_t htlcOutputIndex;     // global HTLC output index on Fuego
  std::string ctrLockTxId;      // counterparty lock tx hash

  // Counterparty-specific
  std::string ctrAddress;       // counterparty chain address (XMR/ETH/BCH)
  std::string peerEndpoint;     // swap counterparty's network address
};

const char* swapStateToString(SwapState s);
const char* swapPairToString(SwapPair p);
SwapPair swapPairFromString(const std::string& s);

} // namespace XfgSwap
