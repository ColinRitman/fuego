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

std::string FuegoRpcClient::httpPost(const std::string& path, const std::string& body) {
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

  std::string portStr = std::to_string(m_port);
  int gai = getaddrinfo(m_host.c_str(), portStr.c_str(), &hints, &result);
  if (gai != 0) {
    close(sock);
    throw std::runtime_error("Failed to resolve host: " + m_host);
  }

  // Connect
  int ret = connect(sock, result->ai_addr, result->ai_addrlen);
  freeaddrinfo(result);
  if (ret < 0) {
    close(sock);
    throw std::runtime_error("Failed to connect to " + m_host + ":" + portStr);
  }

  // Build HTTP request
  std::ostringstream req;
  req << "POST " << path << " HTTP/1.1\r\n";
  req << "Host: " << m_host << ":" << m_port << "\r\n";
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

bool FuegoRpcClient::getHeight(uint32_t& height) {
  try {
    std::string responseBody = httpPost("/getheight", "{}");
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

bool FuegoRpcClient::getHtlcOutput(uint32_t index, HtlcOutputInfo& out) {
  try {
    Common::JsonValue reqJson(Common::JsonValue::OBJECT);
    reqJson.insert("index", static_cast<int64_t>(index));

    std::string responseBody = httpPost("/gethtlc", reqJson.toString());
    Common::JsonValue json = Common::JsonValue::fromString(responseBody);

    if (!json.isObject() || !json.contains("amount")) {
      return false;
    }

    out.amount = static_cast<uint64_t>(json("amount").getInteger());
    Common::podFromHex(json("recipientKey").getString(), out.recipientKey);
    Common::podFromHex(json("refundKey").getString(), out.refundKey);
    Common::podFromHex(json("hashLock").getString(), out.hashLock);
    out.timeoutHeight = static_cast<uint32_t>(json("timeoutHeight").getInteger());
    out.isSpent = json("isSpent").getBool();

    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool FuegoRpcClient::getHtlcCount(size_t& count) {
  try {
    std::string responseBody = httpPost("/gethtlccount", "{}");
    Common::JsonValue json = Common::JsonValue::fromString(responseBody);

    if (!json.isObject() || !json.contains("count")) {
      return false;
    }

    count = static_cast<size_t>(json("count").getInteger());
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool FuegoRpcClient::sendRawTransaction(const std::string& txHex) {
  try {
    Common::JsonValue reqJson(Common::JsonValue::OBJECT);
    reqJson.insert("tx_as_hex", txHex);

    std::string responseBody = httpPost("/sendrawtransaction", reqJson.toString());
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
    std::string responseBody = httpPost("/getinfo", "{}");
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

} // namespace XfgSwap
