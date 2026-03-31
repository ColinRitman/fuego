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

#include "FuegoRpcClient.h"
#include "Common/JsonValue.h"
#include "Common/StringTools.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace XfgSwap {

FuegoRpcClient::FuegoRpcClient(const std::string& host, uint16_t port)
  : m_host(host)
  , m_port(port) {
}

void FuegoRpcClient::setWalletRpc(const std::string& host, uint16_t port) {
  m_walletHost = host;
  m_walletPort = port;
}

// ── Low-level HTTP ───────────────────────────────────────────────────

std::string FuegoRpcClient::httpPost(const std::string& host, uint16_t port,
                                     const std::string& path, const std::string& body) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    throw std::runtime_error("Failed to create socket");
  }

  // Set socket timeout (10 seconds)
  struct timeval tv;
  tv.tv_sec = 10;
  tv.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  // Resolve host
  struct addrinfo hints, *result;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  std::string portStr = std::to_string(port);
  int gai = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
  if (gai != 0) {
    close(sock);
    throw std::runtime_error("Failed to resolve host: " + host);
  }

  // Connect
  int ret = connect(sock, result->ai_addr, result->ai_addrlen);
  freeaddrinfo(result);
  if (ret < 0) {
    close(sock);
    throw std::runtime_error("Failed to connect to " + host + ":" + portStr);
  }

  // Build HTTP request
  std::ostringstream req;
  req << "POST " << path << " HTTP/1.1\r\n";
  req << "Host: " << host << ":" << port << "\r\n";
  req << "Content-Type: application/json\r\n";
  req << "Content-Length: " << body.size() << "\r\n";
  req << "Connection: close\r\n";
  req << "\r\n";
  req << body;

  std::string request = req.str();
  ssize_t sent = send(sock, request.c_str(), request.size(), 0);
  if (sent < 0 || static_cast<size_t>(sent) != request.size()) {
    close(sock);
    throw std::runtime_error("Failed to send HTTP request");
  }

  // Read response
  std::string response;
  char buf[4096];
  while (true) {
    ssize_t n = recv(sock, buf, sizeof(buf), 0);
    if (n <= 0) break;
    response.append(buf, static_cast<size_t>(n));
  }
  close(sock);

  // Parse HTTP response: find body after \r\n\r\n
  size_t headerEnd = response.find("\r\n\r\n");
  if (headerEnd == std::string::npos) {
    throw std::runtime_error("Malformed HTTP response");
  }

  return response.substr(headerEnd + 4);
}

std::string FuegoRpcClient::daemonPost(const std::string& path, const std::string& body) {
  return httpPost(m_host, m_port, path, body);
}

std::string FuegoRpcClient::walletJsonRpc(const std::string& method, const std::string& params) {
  if (m_walletPort == 0) {
    throw std::runtime_error("Wallet RPC not configured (call setWalletRpc first)");
  }

  std::ostringstream body;
  body << "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"" << method << "\"";
  if (!params.empty()) {
    body << ",\"params\":" << params;
  }
  body << "}";

  return httpPost(m_walletHost, m_walletPort, "/json_rpc", body.str());
}

// ── Daemon RPC methods ───────────────────────────────────────────────

bool FuegoRpcClient::getHeight(uint32_t& height) {
  try {
    std::string responseBody = daemonPost("/getheight", "{}");
    Common::JsonValue json = Common::JsonValue::fromString(responseBody);

    if (!json.isObject() || !json.contains("height")) {
      return false;
    }

    height = static_cast<uint32_t>(json("height").getInteger());
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool FuegoRpcClient::sendRawTransaction(const std::string& txHex) {
  try {
    Common::JsonValue reqJson(Common::JsonValue::OBJECT);
    reqJson.insert("tx_as_hex", txHex);

    std::string responseBody = daemonPost("/sendrawtransaction", reqJson.toString());
    Common::JsonValue json = Common::JsonValue::fromString(responseBody);

    if (!json.isObject() || !json.contains("status")) {
      return false;
    }

    return json("status").getString() == "OK";
  } catch (const std::exception&) {
    return false;
  }
}

bool FuegoRpcClient::getInfo(NodeInfo& info) {
  try {
    std::string responseBody = daemonPost("/getinfo", "{}");
    Common::JsonValue json = Common::JsonValue::fromString(responseBody);

    if (!json.isObject() || !json.contains("height")) {
      return false;
    }

    info.height = static_cast<uint32_t>(json("height").getInteger());
    info.difficulty = json.contains("difficulty")
      ? static_cast<uint64_t>(json("difficulty").getInteger()) : 0;
    info.txCount = json.contains("tx_count")
      ? static_cast<uint64_t>(json("tx_count").getInteger()) : 0;
    info.status = json.contains("status")
      ? json("status").getString() : "UNKNOWN";

    return true;
  } catch (const std::exception&) {
    return false;
  }
}

// ── Wallet RPC methods ───────────────────────────────────────────────

bool FuegoRpcClient::sendTransfer(const std::string& address, uint64_t amount,
                                  uint64_t mixin, TransferResult& result) {
  try {
    // Build JSON-RPC params for the "transfer" method:
    //   { "destinations": [{"amount": N, "address": "fire..."}],
    //     "fee": 10000, "mixin": M, "unlock_time": 0 }
    //
    // fee: 10000 atomic = 0.001 XFG (minimum fee)
    std::ostringstream params;
    params << "{\"destinations\":[{\"amount\":" << amount
           << ",\"address\":\"" << address << "\"}]"
           << ",\"fee\":10000"
           << ",\"mixin\":" << mixin
           << ",\"unlock_time\":0}";

    std::string responseBody = walletJsonRpc("transfer", params.str());
    Common::JsonValue json = Common::JsonValue::fromString(responseBody);

    if (!json.isObject()) {
      return false;
    }

    // Check for JSON-RPC error
    if (json.contains("error")) {
      return false;
    }

    if (!json.contains("result")) {
      return false;
    }

    const auto& res = json("result");
    if (!res.isObject() || !res.contains("tx_hash")) {
      return false;
    }

    result.txHash = res("tx_hash").getString();
    result.txSecretKey = res.contains("tx_secret_key")
      ? res("tx_secret_key").getString() : "";

    return true;
  } catch (const std::exception&) {
    return false;
  }
}

// ── Minimal CryptoNote binary parser ─────────────────────────────────
//
// We parse the transaction wire format directly to avoid linking
// CryptoNoteCore and Serialization (SwapDaemon only links Crypto,
// Common, Logging).  The wire format uses 7-bit varint encoding
// (high bit = "more bytes") and typed variant tags for inputs/outputs.

namespace {

// RAII-free binary reader over a byte vector.
struct BlobReader {
  const uint8_t* data;
  size_t         size;
  size_t         pos;

  BlobReader(const std::vector<uint8_t>& blob)
    : data(blob.data()), size(blob.size()), pos(0) {}

  bool eof() const { return pos >= size; }

  uint8_t readByte() {
    if (pos >= size) throw std::runtime_error("BlobReader: unexpected end");
    return data[pos++];
  }

  void readBytes(void* dst, size_t n) {
    if (pos + n > size) throw std::runtime_error("BlobReader: unexpected end");
    std::memcpy(dst, data + pos, n);
    pos += n;
  }

  void skip(size_t n) {
    if (pos + n > size) throw std::runtime_error("BlobReader: unexpected end");
    pos += n;
  }

  uint64_t readVarint() {
    uint64_t result = 0;
    unsigned shift = 0;
    while (true) {
      uint8_t b = readByte();
      result |= static_cast<uint64_t>(b & 0x7F) << shift;
      if ((b & 0x80) == 0) break;
      shift += 7;
      if (shift >= 64) throw std::runtime_error("BlobReader: varint overflow");
    }
    return result;
  }

  // Skip a varint-length-prefixed array of varints (e.g. key_offsets).
  void skipVarintArray() {
    uint64_t count = readVarint();
    for (uint64_t i = 0; i < count; ++i) {
      readVarint();
    }
  }

  // Skip one serialized input based on its type tag.
  // Wire tags (from CryptoNoteSerialization.cpp BinaryVariantTagGetter):
  //   0xFF  BaseInput:                    varint blockIndex
  //   0x02  KeyInput:                     varint amount, varintArray offsets, 32B keyImage
  //   0x03  MultisignatureInput:          varint amount, varint sigCount, varint outputIndex, varint term
  //   0x04  TransactionInputCommitmentSpend:   varint amount, varintArray offsets, 32B keyImage, varint claimedInterest
  //   0x05  TransactionInputUnified:      varintArray offsets, 32B keyImage, 32B pseudoCommitment, 32B sigC0
  void skipInput() {
    uint8_t tag = readByte();
    switch (tag) {
    case 0xFF: // BaseInput
      readVarint(); // blockIndex
      break;
    case 0x02: // KeyInput
      readVarint(); // amount
      skipVarintArray(); // outputIndexes
      skip(32); // keyImage
      break;
    case 0x03: // MultisignatureInput
      readVarint(); // amount
      readVarint(); // signatureCount
      readVarint(); // outputIndex
      readVarint(); // term
      break;
    case 0x04: // TransactionInputCommitmentSpend
      readVarint(); // amount
      skipVarintArray(); // outputIndexes
      skip(32); // keyImage
      readVarint(); // claimedInterest
      break;
    case 0x05: // TransactionInputUnified
      skipVarintArray(); // outputIndexes
      skip(32); // keyImage
      skip(32); // pseudoCommitment (EllipticCurvePoint)
      skip(32); // sigC0 (EllipticCurveScalar)
      break;
    default:
      throw std::runtime_error("BlobReader: unknown input tag " + std::to_string(tag));
    }
  }
};

// MembershipProof size: FUEGO_MEMBERSHIP_N * 2 * 32 bytes (e/s scalar arrays).
static constexpr size_t MEMBERSHIP_PROOF_BYTES = FUEGO_MEMBERSHIP_N * 2 * 32; // 256

} // anonymous namespace

// ── Daemon RPC: transaction inspection ───────────────────────────────

bool FuegoRpcClient::getTransactionOutputs(const std::string& txHashHex,
                                           std::vector<TxOutputInfo>& outputs) {
  try {
    // POST /gettransactions with {"txs_hashes": ["<hash>"]}
    std::ostringstream body;
    body << "{\"txs_hashes\":[\"" << txHashHex << "\"]}";

    std::string responseBody = daemonPost("/gettransactions", body.str());
    Common::JsonValue json = Common::JsonValue::fromString(responseBody);

    if (!json.isObject()) {
      return false;
    }

    // Check that the tx was found (not in missed_tx)
    if (json.contains("missed_tx")) {
      const auto& missed = json("missed_tx");
      if (missed.isArray() && missed.size() > 0) {
        return false;  // tx not found on chain or in pool
      }
    }

    if (!json.contains("txs_as_hex")) {
      return false;
    }

    const auto& txsHex = json("txs_as_hex");
    if (!txsHex.isArray() || txsHex.size() == 0) {
      return false;
    }

    std::string txHex = txsHex[0].getString();
    std::vector<uint8_t> blob = Common::fromHex(txHex);
    BlobReader r(blob);

    // ── TransactionPrefix ──
    // version (varint)
    r.readVarint();
    // unlockTime (varint)
    r.readVarint();

    // ── inputs (vin) ──
    uint64_t inputCount = r.readVarint();
    for (uint64_t i = 0; i < inputCount; ++i) {
      r.skipInput();
    }

    // ── outputs (vout) ──
    uint64_t outputCount = r.readVarint();
    outputs.clear();
    outputs.reserve(static_cast<size_t>(outputCount));

    for (uint64_t i = 0; i < outputCount; ++i) {
      uint64_t amount = r.readVarint();
      uint8_t tag = r.readByte();

      switch (tag) {
      case 0x02: { // KeyOutput — 32-byte public key
        TxOutputInfo info;
        info.amount = amount;
        r.readBytes(info.targetKey.data, 32);
        outputs.push_back(info);
        break;
      }
      case 0x03: { // MultisignatureOutput — skip: varint-array keys, varint reqSigs, varint term
        uint64_t keyCount = r.readVarint();
        r.skip(static_cast<size_t>(keyCount) * 32); // PublicKey array
        r.readVarint(); // requiredSignatureCount
        r.readVarint(); // term
        break;
      }
      case 0x04: { // TransactionOutputCommitment — 32B commitKey, varint term, 32B amountCommitment, MEMBERSHIP_PROOF_BYTES
        r.skip(32); // commitKey
        r.readVarint(); // term
        r.skip(32); // amountCommitment (EllipticCurvePoint)
        r.skip(MEMBERSHIP_PROOF_BYTES); // amountProof
        break;
      }
      case 0x05: { // TransactionOutputUnified — 32B key, varint term, 32B commitment, MEMBERSHIP_PROOF_BYTES
        r.skip(32); // key
        r.readVarint(); // term
        r.skip(32); // commitment (EllipticCurvePoint)
        r.skip(MEMBERSHIP_PROOF_BYTES); // proof
        break;
      }
      default:
        throw std::runtime_error("Unknown output tag " + std::to_string(tag));
      }
    }

    return true;
  } catch (const std::exception&) {
    return false;
  }
}

// ── Daemon RPC: random outputs for ring construction ─────────────────

bool FuegoRpcClient::getRandomOutputs(uint64_t amount, uint32_t count,
                                      std::vector<RandomOutput>& outputs) {
  try {
    std::ostringstream body;
    body << "{\"amounts\":[" << amount << "],\"outs_count\":" << count << "}";

    std::string responseBody = daemonPost("/getrandom_outs", body.str());
    Common::JsonValue json = Common::JsonValue::fromString(responseBody);

    if (!json.isObject() || !json.contains("status")) {
      return false;
    }

    if (json("status").getString() != "OK") {
      return false;
    }

    if (!json.contains("outs") || !json("outs").isArray()) {
      return false;
    }

    const auto& outsArray = json("outs");
    if (outsArray.size() == 0) {
      return false;
    }

    // We requested one amount, so expect one entry.
    const auto& entry = outsArray[0];
    if (!entry.isObject() || !entry.contains("outs") || !entry("outs").isArray()) {
      return false;
    }

    const auto& entryOuts = entry("outs");
    outputs.clear();
    outputs.reserve(entryOuts.size());

    for (size_t i = 0; i < entryOuts.size(); ++i) {
      const auto& out = entryOuts[i];
      if (!out.isObject() ||
          !out.contains("global_amount_index") ||
          !out.contains("out_key")) {
        continue;
      }

      RandomOutput ro;
      ro.globalIndex = static_cast<uint64_t>(out("global_amount_index").getInteger());

      std::string keyHex = out("out_key").getString();
      if (!Common::podFromHex(keyHex, ro.key)) {
        continue;
      }

      outputs.push_back(ro);
    }

    return !outputs.empty();
  } catch (const std::exception&) {
    return false;
  }
}

// ── Daemon RPC: global output indexes for a transaction ──────────────

bool FuegoRpcClient::getGlobalOutputIndexes(const std::string& txHashHex,
                                            std::vector<uint64_t>& indexes) {
  try {
    std::ostringstream body;
    body << "{\"txid\":\"" << txHashHex << "\"}";

    std::string responseBody = daemonPost("/get_o_indexes", body.str());
    Common::JsonValue json = Common::JsonValue::fromString(responseBody);

    if (!json.isObject() || !json.contains("status")) {
      return false;
    }

    if (json("status").getString() != "OK") {
      return false;
    }

    if (!json.contains("o_indexes") || !json("o_indexes").isArray()) {
      return false;
    }

    const auto& arr = json("o_indexes");
    indexes.clear();
    indexes.reserve(arr.size());

    for (size_t i = 0; i < arr.size(); ++i) {
      indexes.push_back(static_cast<uint64_t>(arr[i].getInteger()));
    }

    return !indexes.empty();
  } catch (const std::exception&) {
    return false;
  }
}

} // namespace XfgSwap
