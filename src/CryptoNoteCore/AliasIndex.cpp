// Copyright (c) 2018-2025, Fuego Development Team
// AliasIndex - Standalone on-chain @ alias registry

#include "AliasIndex.h"
#include "CryptoNoteConfig.h"

namespace CryptoNote {

AliasIndex::AliasIndex() {
  reserveDevTeamAliases();
}

AliasIndex::~AliasIndex() {}

void AliasIndex::reserveDevTeamAliases() {
  // Reserve dev team aliases at genesis (block 0)
  // These are permanently owned by the Fuego Developer Fund address
  const std::string devAddress = CryptoNote::FUEGO_DEV_FUND_ADDRESS;

  struct ReservedAlias {
    std::string name;
    uint8_t type;  // 0 = Elderfier, 1 = Regular
  };

  const ReservedAlias reserved[] = {
    { "FUEGOXFG", 0 },
    { "fuegoxfg", 1 },
    { "FUEGODEV", 0 },
    { "fuegodev", 1 },
  };

  for (const auto& r : reserved) {
    AliasEntry entry;
    entry.alias = r.name;
    entry.ownerAddress = devAddress;
    entry.aliasHash = Crypto::cn_fast_hash(r.name.data(), r.name.size());
    entry.addressHash = Crypto::cn_fast_hash(devAddress.data(), devAddress.size());
    entry.aliasType = r.type;
    entry.registeredBlock = 0;  // Genesis

    m_aliases[entry.alias] = entry;
    // Note: only one address->alias mapping per address, so we use the first one
    if (m_addressToAlias.find(devAddress) == m_addressToAlias.end()) {
      m_addressToAlias[devAddress] = entry.alias;
    }
  }
}

// ============================================================================
// VALIDATION HELPERS
// ============================================================================

bool AliasIndex::isValidElderfierAlias(const std::string& alias) {
  if (alias.length() != 8) return false;
  for (char c : alias) {
    bool isUpper = (c >= 'A' && c <= 'Z');
    bool isDigit = (c >= '0' && c <= '9');
    bool isAmpersand = (c == '&');
    if (!isUpper && !isDigit && !isAmpersand) {
      return false;
    }
  }
  return true;
}

bool AliasIndex::isValidRegularAlias(const std::string& alias) {
  if (alias.length() != 8) return false;
  for (char c : alias) {
    bool isLower = (c >= 'a' && c <= 'z');
    bool isDigit = (c >= '0' && c <= '9');
    bool isAmpersand = (c == '&');
    if (!isLower && !isDigit && !isAmpersand) {
      return false;
    }
  }
  return true;
}

// ============================================================================
// REGISTRATION
// ============================================================================

bool AliasIndex::registerAlias(const AliasEntry& entry) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Check alias does not already exist
  if (m_aliases.find(entry.alias) != m_aliases.end()) {
    return false;  // Alias already taken
  }

  // Check address does not already have an alias
  if (m_addressToAlias.find(entry.ownerAddress) != m_addressToAlias.end()) {
    return false;  // Address already has an alias
  }

  // Validate format based on alias type
  if (entry.aliasType == 0) {
    // Elderfier alias: [A-Z0-9&] only (ALLCAPS)
    if (!isValidElderfierAlias(entry.alias)) {
      return false;
    }
  } else if (entry.aliasType == 1) {
    // Regular user alias: [a-z0-9&] only (lowercase)
    if (!isValidRegularAlias(entry.alias)) {
      return false;
    }
  } else {
    return false;  // Unknown alias type
  }

  // Register the alias
  m_aliases[entry.alias] = entry;
  m_addressToAlias[entry.ownerAddress] = entry.alias;
  return true;
}

bool AliasIndex::voidAlias(const std::string& ownerAddress) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto alias_it = m_addressToAlias.find(ownerAddress);
  if (alias_it == m_addressToAlias.end()) {
    return false;  // No alias for this address
  }

  std::string aliasName = alias_it->second;
  m_aliases.erase(aliasName);
  m_addressToAlias.erase(alias_it);
  return true;
}

// ============================================================================
// QUERIES
// ============================================================================

bool AliasIndex::aliasExists(const std::string& alias) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_aliases.find(alias) != m_aliases.end();
}

bool AliasIndex::addressHasAlias(const std::string& address) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_addressToAlias.find(address) != m_addressToAlias.end();
}

std::optional<AliasEntry> AliasIndex::getAliasByName(const std::string& alias) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_aliases.find(alias);
  if (it != m_aliases.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<AliasEntry> AliasIndex::getAliasByAddress(const std::string& address) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto alias_it = m_addressToAlias.find(address);
  if (alias_it == m_addressToAlias.end()) {
    return std::nullopt;
  }

  auto entry_it = m_aliases.find(alias_it->second);
  if (entry_it != m_aliases.end()) {
    return entry_it->second;
  }
  return std::nullopt;
}

std::vector<AliasEntry> AliasIndex::getAllAliases() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<AliasEntry> result;
  result.reserve(m_aliases.size());
  for (const auto& pair : m_aliases) {
    result.push_back(pair.second);
  }
  return result;
}

// ============================================================================
// STATE
// ============================================================================

void AliasIndex::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_aliases.clear();
  m_addressToAlias.clear();
  // Re-reserve dev team aliases after clear
  reserveDevTeamAliases();
}

size_t AliasIndex::size() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_aliases.size();
}

}  // namespace CryptoNote
