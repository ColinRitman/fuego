// Copyright (c) 2018-2025, Fuego Development Team
// AliasIndex - Standalone on-chain @ alias registry
// Separated from CommitmentIndex for clean single-responsibility design.
// Aliases map 8-character names to wallet addresses on-chain.
// EFier aliases [A-Z0-9&] are assigned during elderking_ceremony and voided on unstake.
// Regular aliases [a-z0-9&] req donation of 1 XFG to Fuego Development (@fuegodev/@FUEGOXFG) per alias.

#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "CryptoNoteConfig.h"

namespace CryptoNote {

// @ Alias entry for on-chain alias registry
struct AliasEntry {
  std::string alias;            // "FUEGOXFG" (EFier) or "fuegodev" (regular)
  std::string ownerAddress;     // Full wallet address
  Crypto::Hash aliasHash;       // cn_fast_hash(alias) for fast lookup
  Crypto::Hash addressHash;     // cn_fast_hash(address) for privacy
  uint8_t aliasType = 0;        // 0 = Elderfier [A-Z0-9&], 1 = Regular [a-z0-9&]
  uint32_t registeredBlock = 0;
};

class AliasIndex {
public:
  AliasIndex();
  ~AliasIndex();

  // Registration
  bool registerAlias(const AliasEntry& entry);
  bool voidAlias(const std::string& ownerAddress);

  // Queries
  bool aliasExists(const std::string& alias) const;
  bool addressHasAlias(const std::string& address) const;
  std::optional<AliasEntry> getAliasByName(const std::string& alias) const;
  std::optional<AliasEntry> getAliasByAddress(const std::string& address) const;
  std::vector<AliasEntry> getAllAliases() const;

  // State
  void clear();
  size_t size() const;

  // Validation helpers (static, usable by callers before registration)
  static bool isValidElderfierAlias(const std::string& alias);
  static bool isValidRegularAlias(const std::string& alias);

private:
  mutable std::mutex m_mutex;

  // Alias storage
  std::map<std::string, AliasEntry> m_aliases;          // alias -> entry
  std::map<std::string, std::string> m_addressToAlias;  // address -> alias

  // Reserved alias names (registered at genesis / init)
  void reserveDevTeamAliases();
};

}  // namespace CryptoNote
