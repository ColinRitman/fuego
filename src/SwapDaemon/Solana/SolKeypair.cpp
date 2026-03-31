// Copyright (c) 2017-2026 Fuego Developers
// Copyright (c) 2020-2026 Elderfire Privacy Group
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

#include "SolKeypair.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include <sys/stat.h>

#include <openssl/sha.h>

extern "C" {
#include "../../crypto/crypto-ops.h"
#include "../../crypto/random.h"
}

namespace XfgSwap {

// ---------------------------------------------------------------------------
// Base58 encode (Bitcoin/Solana alphabet) — duplicated from SolRpcClient.cpp
// to keep this translation unit self-contained.
// ---------------------------------------------------------------------------

static const char BASE58_ALPHABET[] =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

static std::string base58Encode(const std::vector<uint8_t>& data) {
  if (data.empty()) return "";

  // Count leading zero bytes
  size_t leadingZeros = 0;
  while (leadingZeros < data.size() && data[leadingZeros] == 0)
    ++leadingZeros;

  // Allocate enough space: log(256)/log(58) ~ 1.366
  size_t maxChars = data.size() * 138 / 100 + 1;
  std::vector<uint8_t> buf(maxChars, 0);

  for (size_t i = 0; i < data.size(); ++i) {
    int carry = data[i];
    for (int j = static_cast<int>(maxChars) - 1; j >= 0; --j) {
      carry += 256 * buf[static_cast<size_t>(j)];
      buf[static_cast<size_t>(j)] = static_cast<uint8_t>(carry % 58);
      carry /= 58;
    }
  }

  // Skip leading zeros in buffer
  size_t skip = 0;
  while (skip < maxChars && buf[skip] == 0) ++skip;

  std::string result;
  result.reserve(leadingZeros + maxChars - skip);
  result.append(leadingZeros, '1');
  for (size_t i = skip; i < maxChars; ++i) {
    result += BASE58_ALPHABET[buf[i]];
  }
  return result;
}

// ---------------------------------------------------------------------------
// Hex helpers
// ---------------------------------------------------------------------------

static std::string bytesToHex(const std::vector<uint8_t>& bytes) {
  std::ostringstream ss;
  ss << std::hex << std::setfill('0');
  for (uint8_t b : bytes) {
    ss << std::setw(2) << static_cast<int>(b);
  }
  return ss.str();
}

static std::vector<uint8_t> hexToBytes(const std::string& hex) {
  std::vector<uint8_t> out;
  if (hex.size() % 2 != 0) return out;
  out.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    unsigned int byte;
    std::istringstream iss(hex.substr(i, 2));
    iss >> std::hex >> byte;
    if (iss.fail()) return {};
    out.push_back(static_cast<uint8_t>(byte));
  }
  return out;
}

// ---------------------------------------------------------------------------
// SolKeypair
// ---------------------------------------------------------------------------

std::string SolKeypair::toBase58() const {
  // Solana keypair = seed (32) || pubkey (32) = 64 bytes
  std::vector<uint8_t> full;
  full.reserve(64);
  full.insert(full.end(), seed.begin(), seed.end());
  full.insert(full.end(), pubkey.begin(), pubkey.end());
  return base58Encode(full);
}

std::string SolKeypair::pubkeyBase58() const {
  return base58Encode(pubkey);
}

bool SolKeypair::isValid() const {
  return seed.size() == 32 && pubkey.size() == 32;
}

// ---------------------------------------------------------------------------
// SolKeypairStore
// ---------------------------------------------------------------------------

SolKeypairStore::SolKeypairStore(const std::string& dataDir)
    : m_dataDir(dataDir) {}

SolKeypair SolKeypairStore::generate() {
  SolKeypair kp;
  kp.seed.resize(32);
  kp.pubkey.resize(32);

  // 1. Generate 32 random seed bytes using Fuego's crypto RNG.
  generate_random_bytes(32, kp.seed.data());

  // 2. Standard NaCl Ed25519 derivation:
  //    az = SHA-512(seed)
  //    Clamp az[0..31] as scalar a
  //    pubkey = a * B
  unsigned char az[64];
  SHA512(kp.seed.data(), 32, az);

  // Clamp the scalar (first 32 bytes of the hash)
  az[0] &= 248;
  az[31] &= 63;
  az[31] |= 64;

  // Derive public key: pubkey = a * B (Ed25519 basepoint)
  ge_p3 A;
  ge_scalarmult_base(&A, az);
  ge_p3_tobytes(kp.pubkey.data(), &A);

  // Zero out the hash to avoid leaving key material on the stack
  memset(az, 0, sizeof(az));

  return kp;
}

bool SolKeypairStore::save(const std::string& swapId, const SolKeypair& kp) {
  if (!kp.isValid()) return false;

  // Create sol_keys directory if it doesn't exist
  std::string dir = m_dataDir + "/sol_keys";
  mkdir(m_dataDir.c_str(), 0700);
  mkdir(dir.c_str(), 0700);

  std::string path = keyPath(swapId);

  // Write JSON: {"seed":"<hex>","pubkey":"<hex>"}
  std::string json = "{\"seed\":\"" + bytesToHex(kp.seed) +
                     "\",\"pubkey\":\"" + bytesToHex(kp.pubkey) + "\"}";

  std::ofstream out(path, std::ios::out | std::ios::trunc);
  if (!out.is_open()) return false;
  out << json;
  out.close();

  if (out.fail()) return false;

  // Set file permissions to 0600 (owner read/write only — secrets!)
  chmod(path.c_str(), S_IRUSR | S_IWUSR);

  return true;
}

bool SolKeypairStore::load(const std::string& swapId, SolKeypair& kp) {
  std::string path = keyPath(swapId);

  std::ifstream in(path);
  if (!in.is_open()) return false;

  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  in.close();

  // Simple JSON parsing: find "seed":"..." and "pubkey":"..."
  // Format: {"seed":"<hex>","pubkey":"<hex>"}
  auto extractField = [&](const std::string& key) -> std::string {
    std::string needle = "\"" + key + "\":\"";
    size_t pos = content.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    size_t end = content.find('"', pos);
    if (end == std::string::npos) return "";
    return content.substr(pos, end - pos);
  };

  std::string seedHex = extractField("seed");
  std::string pubkeyHex = extractField("pubkey");

  if (seedHex.empty() || pubkeyHex.empty()) return false;

  kp.seed = hexToBytes(seedHex);
  kp.pubkey = hexToBytes(pubkeyHex);

  return kp.isValid();
}

bool SolKeypairStore::exists(const std::string& swapId) const {
  struct stat st;
  return stat(keyPath(swapId).c_str(), &st) == 0;
}

bool SolKeypairStore::remove(const std::string& swapId) {
  std::string path = keyPath(swapId);

  // Overwrite file contents with zeros before unlinking (best-effort
  // secure erasure — not guaranteed on all filesystems/SSDs, but better
  // than leaving plaintext key material on disk).
  std::ifstream check(path, std::ios::ate | std::ios::binary);
  if (check.is_open()) {
    auto fileSize = check.tellg();
    check.close();

    if (fileSize > 0) {
      std::ofstream wipe(path, std::ios::out | std::ios::trunc | std::ios::binary);
      if (wipe.is_open()) {
        std::vector<char> zeros(static_cast<size_t>(fileSize), 0);
        wipe.write(zeros.data(), fileSize);
        wipe.flush();
        wipe.close();
      }
    }
  }

  return std::remove(path.c_str()) == 0;
}

std::string SolKeypairStore::keyPath(const std::string& swapId) const {
  return m_dataDir + "/sol_keys/" + swapId + ".json";
}

} // namespace XfgSwap
