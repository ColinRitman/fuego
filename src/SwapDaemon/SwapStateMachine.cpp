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

#include "SwapStateMachine.h"
#include "Common/JsonValue.h"
#include "Common/StringTools.h"
#include <sstream>
#include <cstring>

namespace XfgSwap {

SwapStateMachine::SwapStateMachine()
  : m_params()
  , m_state(SwapState::INITIATED)
  , m_createdAt(std::time(nullptr))
  , m_updatedAt(m_createdAt) {
  // Zero-init crypto POD fields
  std::memset(&m_params.aliceXfgPubKey, 0, sizeof(m_params.aliceXfgPubKey));
  std::memset(&m_params.bobXfgPubKey, 0, sizeof(m_params.bobXfgPubKey));
  std::memset(&m_params.hashLock, 0, sizeof(m_params.hashLock));
  std::memset(&m_params.preimage, 0, sizeof(m_params.preimage));
  m_params.pair = SwapPair::XMR;
  m_params.role = SwapRole::BOB;
  m_params.xfgAmount = 0;
  m_params.ctrAmount = 0;
  m_params.xfgTimeoutHeight = 0;
  m_params.ctrTimeoutBlock = 0;
  m_params.htlcOutputIndex = 0;
}

SwapStateMachine::SwapStateMachine(SwapParams params)
  : m_params(std::move(params))
  , m_state(SwapState::INITIATED)
  , m_createdAt(std::time(nullptr))
  , m_updatedAt(m_createdAt) {
}

bool SwapStateMachine::isValidTransition(SwapState newState) const {
  // Any state can transition to FAILED
  if (newState == SwapState::FAILED) {
    return true;
  }

  switch (m_state) {
    case SwapState::INITIATED:
      return newState == SwapState::XFG_LOCKED;

    case SwapState::XFG_LOCKED:
      return newState == SwapState::CTR_LOCKED ||
             newState == SwapState::XFG_REFUNDED;

    case SwapState::CTR_LOCKED:
      return newState == SwapState::XFG_CLAIMED ||
             newState == SwapState::CTR_REFUNDED;

    case SwapState::XFG_CLAIMED:
      return newState == SwapState::CTR_CLAIMED;

    // Terminal states -- no further transitions except FAILED (handled above)
    case SwapState::CTR_CLAIMED:
    case SwapState::XFG_REFUNDED:
    case SwapState::CTR_REFUNDED:
    case SwapState::FAILED:
      return false;

    default:
      return false;
  }
}

bool SwapStateMachine::transition(SwapState newState) {
  if (!isValidTransition(newState)) {
    return false;
  }
  m_state = newState;
  m_updatedAt = std::time(nullptr);
  return true;
}

SwapState SwapStateMachine::currentState() const {
  return m_state;
}

SwapParams& SwapStateMachine::params() {
  return m_params;
}

const SwapParams& SwapStateMachine::params() const {
  return m_params;
}

time_t SwapStateMachine::createdAt() const {
  return m_createdAt;
}

time_t SwapStateMachine::updatedAt() const {
  return m_updatedAt;
}

bool SwapStateMachine::isTerminal() const {
  return m_state == SwapState::CTR_CLAIMED ||
         m_state == SwapState::XFG_REFUNDED ||
         m_state == SwapState::CTR_REFUNDED ||
         m_state == SwapState::FAILED;
}

std::string SwapStateMachine::serialize() const {
  Common::JsonValue root(Common::JsonValue::OBJECT);

  root.insert("swapId", m_params.swapId);
  root.insert("pair", static_cast<int64_t>(static_cast<uint8_t>(m_params.pair)));
  root.insert("role", static_cast<int64_t>(static_cast<uint8_t>(m_params.role)));
  root.insert("xfgAmount", static_cast<int64_t>(m_params.xfgAmount));
  root.insert("ctrAmount", static_cast<int64_t>(m_params.ctrAmount));
  root.insert("state", static_cast<int64_t>(static_cast<uint8_t>(m_state)));

  root.insert("aliceXfgPubKey", Common::podToHex(m_params.aliceXfgPubKey));
  root.insert("bobXfgPubKey", Common::podToHex(m_params.bobXfgPubKey));
  root.insert("hashLock", Common::podToHex(m_params.hashLock));
  root.insert("preimage", Common::podToHex(m_params.preimage));

  root.insert("xfgTimeoutHeight", static_cast<int64_t>(m_params.xfgTimeoutHeight));
  root.insert("ctrTimeoutBlock", static_cast<int64_t>(m_params.ctrTimeoutBlock));
  root.insert("htlcOutputIndex", static_cast<int64_t>(m_params.htlcOutputIndex));

  root.insert("ctrLockTxId", m_params.ctrLockTxId);
  root.insert("ctrAddress", m_params.ctrAddress);
  root.insert("peerEndpoint", m_params.peerEndpoint);

  root.insert("createdAt", static_cast<int64_t>(m_createdAt));
  root.insert("updatedAt", static_cast<int64_t>(m_updatedAt));

  return root.toString();
}

SwapStateMachine SwapStateMachine::deserialize(const std::string& json) {
  Common::JsonValue root = Common::JsonValue::fromString(json);

  SwapParams params;
  params.swapId = root("swapId").getString();
  params.pair = static_cast<SwapPair>(static_cast<uint8_t>(root("pair").getInteger()));
  params.role = static_cast<SwapRole>(static_cast<uint8_t>(root("role").getInteger()));
  params.xfgAmount = static_cast<uint64_t>(root("xfgAmount").getInteger());
  params.ctrAmount = static_cast<uint64_t>(root("ctrAmount").getInteger());

  Common::podFromHex(root("aliceXfgPubKey").getString(), params.aliceXfgPubKey);
  Common::podFromHex(root("bobXfgPubKey").getString(), params.bobXfgPubKey);
  Common::podFromHex(root("hashLock").getString(), params.hashLock);
  Common::podFromHex(root("preimage").getString(), params.preimage);

  params.xfgTimeoutHeight = static_cast<uint32_t>(root("xfgTimeoutHeight").getInteger());
  params.ctrTimeoutBlock = static_cast<uint64_t>(root("ctrTimeoutBlock").getInteger());
  params.htlcOutputIndex = static_cast<uint32_t>(root("htlcOutputIndex").getInteger());

  params.ctrLockTxId = root("ctrLockTxId").getString();
  params.ctrAddress = root("ctrAddress").getString();
  params.peerEndpoint = root("peerEndpoint").getString();

  SwapStateMachine sm(params);
  sm.m_state = static_cast<SwapState>(static_cast<uint8_t>(root("state").getInteger()));
  sm.m_createdAt = static_cast<time_t>(root("createdAt").getInteger());
  sm.m_updatedAt = static_cast<time_t>(root("updatedAt").getInteger());

  return sm;
}

} // namespace XfgSwap
