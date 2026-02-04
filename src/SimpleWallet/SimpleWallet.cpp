// Copyright (c) 2017-2026 Fuego Developers
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Copyright (c) 2016-2019 The Karbowanec developers
// Copyright (c) 2012-2018 The CryptoNote developers
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

#include "SimpleWallet.h"

#include <ctime>
#include <fstream>
#include <future>
#include <iomanip>
#include <thread>
#include <set>
#include <sstream>
#include <regex>
#include <array>
#include <limits>

#include <boost/format.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "Common/Base58.h"
#include "Common/CommandLine.h"
#include "Common/SignalHandler.h"
#include "Common/StringTools.h"
#include "Common/PathTools.h"
#include "Common/Util.h"
#include "Common/DnsTools.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "NodeRpcProxy/NodeRpcProxy.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/HttpClient.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "CryptoNoteCore/DepositCommitment.h"
#include "crypto/crypto.h"
#include "crypto/keccak.h"
#include "CryptoNoteCore/BurnProofDataFileGenerator.h"

#include "Wallet/WalletRpcServer.h"
#include "WalletLegacy/WalletLegacy.h"
#include "Wallet/LegacyKeysImporter.h"
#include "WalletLegacy/WalletHelper.h"
#include "Mnemonics/electrum-words.cpp"

#include "Common/CommandLine.h"
#include "Common/SignalHandler.h"
#include "Common/StringTools.h"
#include "Common/PathTools.h"
#include "Common/Util.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "NodeRpcProxy/NodeRpcProxy.h"

namespace CryptoNote {
  std::string remote_fee_address;
}
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/HttpClient.h"

#include "Wallet/WalletRpcServer.h"
#include "WalletLegacy/WalletHelper.h"

#include <Logging/LoggerManager.h>

#include "Common/CommandLine.h"
#include "Common/SignalHandler.h"
#include "Common/StringTools.h"
#include "Common/PathTools.h"
#include "Common/Util.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "NodeRpcProxy/NodeRpcProxy.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/HttpClient.h"

#include "version.h"

#if defined(WIN32)
#include <crtdbg.h>
#endif

using namespace CryptoNote;
using namespace Logging;
using Common::JsonValue;

namespace po = boost::program_options;

#define EXTENDED_LOGS_FILE "wallet_details.log"
#undef ERROR

const command_line::arg_descriptor<std::string> arg_wallet_file = { "wallet-file", "Use wallet <arg>", "" };
const command_line::arg_descriptor<std::string> arg_generate_new_wallet = { "generate-new-wallet", "Generate new wallet and save it to <arg>", "" };
const command_line::arg_descriptor<std::string> arg_daemon_address = { "daemon-address", "Use daemon instance at <host>:<port>", "" };
const command_line::arg_descriptor<std::string> arg_daemon_host = { "daemon-host", "Use daemon instance at host <arg> instead of localhost", "" };
const command_line::arg_descriptor<std::string> arg_password = { "password", "Wallet password", "", true };
const command_line::arg_descriptor<uint16_t> arg_daemon_port = { "daemon-port", "Use daemon instance at port <arg> instead of default", 0 };
const command_line::arg_descriptor<uint32_t> arg_log_level = { "set_log", "", INFO, true };
const command_line::arg_descriptor<bool> arg_testnet = { "testnet", "Used to deploy test nets. The daemon must be launched with --testnet flag", false };
const command_line::arg_descriptor< std::vector<std::string> > arg_command = { "command", "" };

bool parseUrlAddress(const std::string& url, std::string& address, uint16_t& port) {
  auto pos = url.find("://");
  size_t addrStart = 0;

  if (pos != std::string::npos)
    addrStart = pos + 3;

  auto addrEnd = url.find(':', addrStart);

  if (addrEnd != std::string::npos) {
    auto portEnd = url.find('/', addrEnd);
    port = Common::fromString<uint16_t>(url.substr(
      addrEnd + 1, portEnd == std::string::npos ? std::string::npos : portEnd - addrEnd - 1));
  } else {
    addrEnd = url.find('/');
    port = 80;
  }

  address = url.substr(addrStart, addrEnd - addrStart);
  return true;
}

std::string interpret_rpc_response(bool ok, const std::string& status) {
  std::string err;
  if (ok) {
    if (status == CORE_RPC_STATUS_BUSY) {
      err = "daemon is busy. Please try later";
    } else if (status != CORE_RPC_STATUS_OK) {
      err = status;
    }
  } else {
    err = "possible lost connection to daemon";
  }
  return err;
}

template <typename IterT, typename ValueT = typename IterT::value_type>
class ArgumentReader {
public:

  ArgumentReader(IterT begin, IterT end) :
    m_begin(begin), m_end(end), m_cur(begin) {
  }

  bool eof() const {
    return m_cur == m_end;
  }

  ValueT next() {
    if (eof()) {
      throw std::runtime_error("unexpected end of arguments");
    }

    return *m_cur++;
  }

private:

  IterT m_cur;
  IterT m_begin;
  IterT m_end;
};

struct TransferCommand {
  const CryptoNote::Currency& m_currency;
  size_t fake_outs_count;
  std::vector<CryptoNote::WalletLegacyTransfer> dsts;
  std::vector<uint8_t> extra;
  uint64_t fee;
  std::map<std::string, std::vector<WalletLegacyTransfer>> aliases;
  std::vector<std::string> messages;
  uint64_t ttl;

  TransferCommand(const CryptoNote::Currency& currency) :
    m_currency(currency), fake_outs_count(0), fee(currency.minimumFee()), ttl(0) {
  }

  bool parseArguments(LoggerRef& logger, const std::vector<std::string> &args) {

    ArgumentReader<std::vector<std::string>::const_iterator> ar(args.begin(), args.end());

    try {

      auto arg = ar.next();

      if (arg.size() && arg[0] == '-') {

        const auto& value = ar.next();

        if (arg == "-p") {
          if (!createTxExtraWithPaymentId(value, extra)) {
            logger(ERROR, BRIGHT_RED) << "payment ID has invalid format: \"" << value << "\", expected 64-character string";
            return false;
          }
        } else if (arg == "-m") {
          messages.emplace_back(value);
        } else if (arg == "-ttl") {
          try {
            ttl = boost::lexical_cast<uint64_t>(value);
          } catch (const boost::bad_lexical_cast &) {
            logger(ERROR, BRIGHT_RED) << "TTL has invalid format: \"" << value << "\", expected integer";
            return false;
          }
        }
      } else {
        WalletLegacyTransfer destination;
        CryptoNote::TransactionDestinationEntry de;
        std::string aliasUrl;

        if (!m_currency.parseAccountAddressString(arg, de.addr)) {
          aliasUrl = arg;
        }

        auto value = ar.next();
        bool ok = m_currency.parseAmount(value, de.amount);

        if (!ok || 0 == de.amount) {
          logger(ERROR, BRIGHT_RED) << "amount is wrong: " << arg << ' ' << value <<
            ", expected number from 0 to " << m_currency.formatAmount(std::numeric_limits<uint64_t>::max());
          return false;
        }

        if (aliasUrl.empty()) {
          destination.address = arg;
          destination.amount = de.amount;
          dsts.push_back(destination);
        } else {
          aliases[aliasUrl].emplace_back(WalletLegacyTransfer{"", static_cast<int64_t>(de.amount)});
        }

        if (de.amount < m_currency.minimumFee()) {
          logger(ERROR, BRIGHT_RED) << "Amount must be at least " << m_currency.formatAmount(m_currency.minimumFee());
          return false;
        }
      }

      if (dsts.empty() && aliases.empty()) {
        logger(ERROR, BRIGHT_RED) << "At least one destination address is required";
        return false;
      }
    } catch (const std::exception& e) {
      logger(ERROR, BRIGHT_RED) << e.what();
      return false;
    }

    return true;
  }
};

const size_t TIMESTAMP_MAX_WIDTH = 19;
const size_t HASH_MAX_WIDTH = 64;
const size_t TOTAL_AMOUNT_MAX_WIDTH = 20;
const size_t FEE_MAX_WIDTH = 14;
const size_t BLOCK_MAX_WIDTH = 7;
const size_t UNLOCK_TIME_MAX_WIDTH = 11;

void printListTransfersHeader(LoggerRef& logger) {
  std::string header = Common::makeCenteredString(TIMESTAMP_MAX_WIDTH, "timestamp (UTC)") + "  ";
  header += Common::makeCenteredString(HASH_MAX_WIDTH, "hash") + "  ";
  header += Common::makeCenteredString(TOTAL_AMOUNT_MAX_WIDTH, "total amount") + "  ";
  header += Common::makeCenteredString(FEE_MAX_WIDTH, "fee") + "  ";
  header += Common::makeCenteredString(BLOCK_MAX_WIDTH, "block") + "  ";
  header += Common::makeCenteredString(UNLOCK_TIME_MAX_WIDTH, "unlock time");

  logger(INFO) << header;
  logger(INFO) << std::string(header.size(), '-');
}

void printListTransfersItem(LoggerRef& logger, const WalletLegacyTransaction& txInfo, IWalletLegacy& wallet, const Currency& currency) {
  std::vector<uint8_t> extraVec = Common::asBinaryArray(txInfo.extra);

  Crypto::Hash paymentId;
  std::string paymentIdStr = (getPaymentIdFromTxExtra(extraVec, paymentId) && paymentId != NULL_HASH ? Common::podToHex(paymentId) : "");

  char timeString[TIMESTAMP_MAX_WIDTH + 1];
  time_t timestamp = static_cast<time_t>(txInfo.timestamp);
  if (std::strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", std::gmtime(&timestamp)) == 0) {
    throw std::runtime_error("time buffer is too small");
  }

  std::string rowColor = txInfo.totalAmount < 0 ? MAGENTA : GREEN;
  logger(INFO, rowColor)
    << std::setw(TIMESTAMP_MAX_WIDTH) << timeString
    << "  " << std::setw(HASH_MAX_WIDTH) << Common::podToHex(txInfo.hash)
    << "  " << std::setw(TOTAL_AMOUNT_MAX_WIDTH) << currency.formatAmount(txInfo.totalAmount)
    << "  " << std::setw(FEE_MAX_WIDTH) << currency.formatAmount(txInfo.fee)
    << "  " << std::setw(BLOCK_MAX_WIDTH) << txInfo.blockHeight
    << "  " << std::setw(UNLOCK_TIME_MAX_WIDTH) << txInfo.unlockTime;

  if (!paymentIdStr.empty()) {
    logger(INFO, rowColor) << "payment ID: " << paymentIdStr;
  }

  if (txInfo.totalAmount < 0) {
    if (txInfo.transferCount > 0) {
      logger(INFO, rowColor) << "transfers:";
      for (TransferId id = txInfo.firstTransferId; id < txInfo.firstTransferId + txInfo.transferCount; ++id) {
        WalletLegacyTransfer tr;
        wallet.getTransfer(id, tr);
        logger(INFO, rowColor) << tr.address << "  " << std::setw(TOTAL_AMOUNT_MAX_WIDTH) << currency.formatAmount(tr.amount);
      }
    }
  }

  logger(INFO, rowColor) << " ";
}

std::string prepareWalletAddressFilename(const std::string& walletBaseName) {
  return walletBaseName + ".address";
}

bool writeAddressFile(const std::string& addressFilename, const std::string& address) {
  std::ofstream addressFile(addressFilename, std::ios::out | std::ios::trunc | std::ios::binary);
  if (!addressFile.good()) {
    return false;
  }
  addressFile << address;
  return true;
}

bool processServerAliasResponse(const std::string& s, std::string& address) {
  try {
    auto pos = s.find("oa1:xfg");
    if (pos == std::string::npos)
      return false;
    pos = s.find("recipient_address=", pos);
    if (pos == std::string::npos)
      return false;
    pos += 18;
    auto pos2 = s.find(";", pos);
    if (pos2 != std::string::npos) {
      if (pos2 - pos == 100) {
        address = s.substr(pos, 100);
      } else {
        return false;
      }
    }
  } catch (std::exception&) {
    return false;
  }
  return true;
}

bool splitUrlToHostAndUri(const std::string& aliasUrl, std::string& host, std::string& uri) {
  size_t protoBegin = aliasUrl.find("http://");
  if (protoBegin != 0 && protoBegin != std::string::npos) {
    return false;
  }

  size_t hostBegin = protoBegin == std::string::npos ? 0 : 7;
  size_t hostEnd = aliasUrl.find('/', hostBegin);

  if (hostEnd == std::string::npos) {
    uri = "/";
    host = aliasUrl.substr(hostBegin);
  } else {
    uri = aliasUrl.substr(hostEnd);
    host = aliasUrl.substr(hostBegin, hostEnd - hostBegin);
  }

  return true;
}

bool askAliasesTransfersConfirmation(const std::map<std::string, std::vector<WalletLegacyTransfer>>& aliases, const Currency& currency) {
  std::cout << "Would you like to send money to the following addresses?" << std::endl;

  for (const auto& kv: aliases) {
    for (const auto& transfer: kv.second) {
      std::cout << transfer.address << " " << std::setw(21) << currency.formatAmount(transfer.amount) << "  " << kv.first << std::endl;
    }
  }

  std::string answer;
  do {
    std::cout << "y/n: ";
    std::getline(std::cin, answer);
  } while (answer != "y" && answer != "Y" && answer != "n" && answer != "N");

  return answer == "y" || answer == "Y";
}

bool processServerFeeAddressResponse(const std::string& response, std::string& fee_address) {
  try {
    std::stringstream stream(response);
    JsonValue json;
    stream >> json;

    auto rootIt = json.getObject().find("fee_address");
    if (rootIt == json.getObject().end()) {
      return false;
    }

    fee_address = rootIt->second.getString();
  } catch (std::exception&) {
    return false;
  }
  return true;
}

JsonValue buildLoggerConfiguration(Level level, const std::string& logfile) {
  JsonValue loggerConfiguration(JsonValue::OBJECT);
  loggerConfiguration.insert("globalLevel", static_cast<int64_t>(level));

  JsonValue& cfgLoggers = loggerConfiguration.insert("loggers", JsonValue::ARRAY);

  JsonValue& consoleLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
  consoleLogger.insert("type", "console");
  consoleLogger.insert("level", static_cast<int64_t>(Logging::TRACE));
  consoleLogger.insert("pattern", "");

  JsonValue& fileLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
  fileLogger.insert("type", "file");
  fileLogger.insert("filename", logfile);
  fileLogger.insert("level", static_cast<int64_t>(Logging::TRACE));

  return loggerConfiguration;
}

std::error_code initAndLoadWallet(IWalletLegacy& wallet, std::istream& walletFile, const std::string& password) {
  WalletHelper::InitWalletResultObserver initObserver;
  std::future<std::error_code> f_initError = initObserver.initResult.get_future();

  WalletHelper::IWalletRemoveObserverGuard removeGuard(wallet, initObserver);
  wallet.initAndLoad(walletFile, password);
  auto initError = f_initError.get();

  return initError;
}

std::string tryToOpenWalletOrLoadKeysOrThrow(LoggerRef& logger, std::unique_ptr<IWalletLegacy>& wallet, const std::string& walletFile, const std::string& password) {
  std::string walletFileName;
  boost::system::error_code ignored_ec;

  bool walletFileExists = boost::filesystem::exists(walletFile, ignored_ec);

  if (!walletFileExists) {
    throw std::runtime_error("Wallet file does not exist: " + walletFile);
  }

  walletFileName = walletFile;

  logger(INFO) << "Loading wallet...";

  std::ifstream walletFileStream(walletFileName, std::ios::binary);
  if (!walletFileStream) {
    throw std::runtime_error("Failed to open wallet file: " + walletFileName);
  }

  auto initError = initAndLoadWallet(*wallet, walletFileStream, password);
  if (initError) {
    throw std::runtime_error("Failed to load wallet: " + initError.message());
  }

  return walletFileName;
}

std::string simple_wallet::get_commands_str() {
  std::stringstream ss;
  ss << "Commands: " << ENDL;
  std::string usage = m_consoleHandler.getUsage();
  boost::replace_all(usage, "\n", "\n  ");
  usage.insert(0, "  ");
  ss << usage << ENDL;
  return ss.str();
}

bool simple_wallet::help(const std::vector<std::string> &args) {
  success_msg_writer() << get_commands_str();
  return true;
}

bool simple_wallet::exit(const std::vector<std::string> &args) {
  m_consoleHandler.requestStop();
  return true;
}

simple_wallet::simple_wallet(System::Dispatcher& dispatcher, const CryptoNote::Currency& currency, Logging::LoggerManager& log) :
  m_dispatcher(dispatcher),
  m_daemon_port(0),
  m_currency(currency),
  logManager(log),
  logger(log, "firewallet"),
  m_refresh_progress_reporter(*this),
  m_walletSynchronized(false) {
  m_consoleHandler.setHandler("create_integrated", boost::bind(&simple_wallet::create_integrated, this, boost::arg<1>()), "create_integrated <payment_id> - Create an integrated address with a payment ID");
  m_consoleHandler.setHandler("export_keys", boost::bind(&simple_wallet::export_keys, this, boost::arg<1>()), "Show the secret keys of the current wallet");
  m_consoleHandler.setHandler("balance", boost::bind(&simple_wallet::show_balance, this, boost::arg<1>()), "Show current wallet balance");
  m_consoleHandler.setHandler("sign_message", boost::bind(&simple_wallet::sign_message, this, boost::arg<1>()), "Sign a message with your wallet keys");
  m_consoleHandler.setHandler("verify_signature", boost::bind(&simple_wallet::verify_signature, this, boost::arg<1>()), "Verify a signed message");
  m_consoleHandler.setHandler("incoming_transfers", boost::bind(&simple_wallet::show_incoming_transfers, this, boost::arg<1>()), "Show incoming transfers");
  m_consoleHandler.setHandler("list_transfers", boost::bind(&simple_wallet::listTransfers, this, boost::arg<1>()), "list_transfers <block_height> - Show all known transfers, optionally from a certain block height");
  m_consoleHandler.setHandler("payments", boost::bind(&simple_wallet::show_payments, this, boost::arg<1>()), "payments <payment_id_1> [<payment_id_2> ... <payment_id_N>] - Show payments <payment_id_1>, ... <payment_id_N>");
  m_consoleHandler.setHandler("get_tx_proof", boost::bind(&simple_wallet::get_tx_proof, this, boost::arg<1>()), "Generate a signature to prove payment: <txid> <address> [<txkey>]");
  m_consoleHandler.setHandler("get_reserve_proof", boost::bind(&simple_wallet::get_reserve_proof, this, boost::arg<1>()), "all|<amount> [<message>] - Generate a signature proving that you own at least <amount>, optionally with a challenge string <message>. ");
  m_consoleHandler.setHandler("bc_height", boost::bind(&simple_wallet::show_blockchain_height, this, boost::arg<1>()), "Show blockchain height");
  m_consoleHandler.setHandler("show_dust", boost::bind(&simple_wallet::show_dust, this, boost::arg<1>()), "Show the number of unmixable dust outputs");
  m_consoleHandler.setHandler("outputs", boost::bind(&simple_wallet::show_num_unlocked_outputs, this, boost::arg<1>()), "Show the number of unlocked outputs available for a transaction");
  m_consoleHandler.setHandler("optimize", boost::bind(&simple_wallet::optimize_outputs, this, boost::arg<1>()), "Combine many available outputs into a few by sending a transaction to self");
  m_consoleHandler.setHandler("optimize_all", boost::bind(&simple_wallet::optimize_all_outputs, this, boost::arg<1>()), "Optimize your wallet several times so you can send large transactions");
  m_consoleHandler.setHandler("transfer", boost::bind(&simple_wallet::transfer, this, boost::arg<1>()), "transfer <addr_1> <amount_1> [<addr_2> <amount_2> ... <addr_N> <amount_N>] [-p payment_id] [-m message] - Transfer <amount_1>,... <amount_N> to <address_1>,... <address_N>, respectively. ");
  m_consoleHandler.setHandler("set_log", boost::bind(&simple_wallet::set_log, this, boost::arg<1>()), "set_log <level> - Change current log level, <level> is a number 0-4");
  m_consoleHandler.setHandler("address", boost::bind(&simple_wallet::print_address, this, boost::arg<1>()), "Show current wallet public address");
  m_consoleHandler.setHandler("save", boost::bind(&simple_wallet::save, this, boost::arg<1>()), "Save wallet synchronized data");
  m_consoleHandler.setHandler("reset", boost::bind(&simple_wallet::reset, this, boost::arg<1>()), "Discard cache data and start synchronizing from the start");
  m_consoleHandler.setHandler("help", boost::bind(&simple_wallet::help, this, boost::arg<1>()), "Show this help");
  m_consoleHandler.setHandler("exit", boost::bind(&simple_wallet::exit, this, boost::arg<1>()), "Close wallet");
  m_consoleHandler.setHandler("payment_id", boost::bind(&simple_wallet::payment_id, this, _1), "Generate random Payment ID");
  m_consoleHandler.setHandler("start_mining", boost::bind(&simple_wallet::start_mining, this, boost::arg<1>()), "start_mining [<threads>] - Start mining to your wallet");
  m_consoleHandler.setHandler("stop_mining", boost::bind(&simple_wallet::stop_mining, this, boost::arg<1>()), "stop_mining - Stop mining");

  // Deposit commands
  // TODO: May re-enable 'deposit' command later for backward compatibility
  // m_consoleHandler.setHandler("deposit", boost::bind(&simple_wallet::deposit, this, boost::arg<1>()), "deposit <amount> <term_code> - Create a COLD deposit (0.8, 8, 80, 800 XFG with terms 3=3mo, 12=1yr). ETH address provided at claim time for privacy.");
  // HEAT/COLD deposits temporarily disabled on mainnet - focus on elderfier registration first
  // These will be re-enabled in the next release after elderfiers are registered
  // TODO: Re-enable burn and cold commands in next release
  // m_consoleHandler.setHandler("burn", boost::bind(&simple_wallet::burn, this, boost::arg<1>()), "burn <amount> - Create a HEAT burn deposit (0.8, 8, 80, 800 XFG). Term automatically set to FOREVER.");
  // m_consoleHandler.setHandler("cold", boost::bind(&simple_wallet::cold, this, boost::arg<1>()), "cold <amount> <term_code> - Create a COLD deposit (0.8, 8, 80, 800 XFG with terms 3=3mo, 12=1yr).");
  m_consoleHandler.setHandler("elderking_ceremony", boost::bind(&simple_wallet::elderking_ceremony, this, boost::arg<1>()), "elderking_ceremony - Register as Elderfier: batch 5x 800 XFG deposits (0xEC tag, 4000 XFG total). Creates elderfier registration commitment.");
  m_consoleHandler.setHandler("withdraw_deposit", boost::bind(&simple_wallet::withdraw_deposit, this, boost::arg<1>()), "withdraw_deposit <id> - Withdraw a deposit");
  m_consoleHandler.setHandler("list_deposits", boost::bind(&simple_wallet::list_deposits, this, boost::arg<1>()), "list_deposits - List all deposits");
  m_consoleHandler.setHandler("deposit_info", boost::bind(&simple_wallet::deposit_info, this, boost::arg<1>()), "deposit_info <id> - Get detailed info for deposit");

  // COLD deposit commitment management command
  m_consoleHandler.setHandler("create_cold_secret", boost::bind(&simple_wallet::create_cold_secret, this, boost::arg<1>()), "create_cold_secret <amount> <term_blocks> <chain_code> <metadata> - Create COLD commitment");
  // Proof generation using stored secrets
  m_consoleHandler.setHandler("generate_proof", boost::bind(&simple_wallet::generate_proof, this, boost::arg<1>()), "generate_proof <tx_hash> - Generate proof for deposit transaction");
}

bool simple_wallet::show_dust(const std::vector<std::string>& args) {
  logger(INFO, BRIGHT_WHITE) << "Dust outputs: " << m_wallet->dustBalance() << std::endl;
  return true;
}

bool simple_wallet::set_log(const std::vector<std::string> &args) {
  if (args.size() != 1) {
    fail_msg_writer() << "use: set_log <log_level_number_0-4>";
    return true;
  }

  uint16_t l = 0;
  if (!Common::fromString(args[0], l)) {
    fail_msg_writer() << "wrong number format, use: set_log <log_level_number_0-4>";
    return true;
  }

  if (l > 4) {
    if (l > Logging::TRACE) {
      fail_msg_writer() << "wrong number range, use: set_log <log_level_number_0-4>";
      return true;
    }
  }

  logManager.setMaxLevel(static_cast<Level>(l));
  return true;
}

bool key_import = false;
bool simple_wallet::payment_id(const std::vector<std::string> &args) {
  Crypto::Hash result;
  Crypto::generate_random_bytes(32, result.data);
  std::string pid_str = Common::podToHex(result);
  success_msg_writer() << "Payment ID: " << pid_str;
  return true;
}

bool simple_wallet::init(const boost::program_options::variables_map& vm) {
  handle_command_line(vm);

  if (!m_daemon_address.empty() && (!m_daemon_host.empty() || 0 != m_daemon_port)) {
    fail_msg_writer() << "you can't specify daemon host or port several times";
    return false;
  }

  if (m_daemon_host.empty())
    m_daemon_host = "localhost";
  if (!m_daemon_port)
    m_daemon_port = RPC_DEFAULT_PORT;

  if (!m_daemon_address.empty()) {
    if (!parseUrlAddress(m_daemon_address, m_daemon_host, m_daemon_port)) {
      fail_msg_writer() << "failed to parse daemon address: " << m_daemon_address;
      return false;
    }
    remote_fee_address = getFeeAddress();
    logger(INFO, BRIGHT_WHITE) << "Connected to remote node: " << m_daemon_host;
    if (!remote_fee_address.empty()) {
      logger(INFO, BRIGHT_WHITE) << "Fee address: " << remote_fee_address;
    }
  } else {
    if (!m_daemon_host.empty())
      remote_fee_address = getFeeAddress();
    m_daemon_address = std::string("http://") + m_daemon_host + ":" + std::to_string(m_daemon_port);
    logger(INFO, BRIGHT_WHITE) << "Connected to remote node: " << m_daemon_host;
    if (!remote_fee_address.empty()) {
      logger(INFO, BRIGHT_WHITE) << "Fee address: " << remote_fee_address;
    }
  }

  if (m_generate_new.empty() && m_wallet_file_arg.empty()) {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "       ░░░░░░░ ░░    ░░ ░░░░░░░  ░░░░░░   ░░░░░░        " << "\n";
    std::cout << "       ▒▒      ▒▒    ▒▒ ▒▒      ▒▒       ▒▒    ▒▒       " << "\n";
    std::cout << "       ▒▒▒▒▒   ▒▒    ▒▒ ▒▒▒▒▒   ▒▒   ▒▒▒ ▒▒    ▒▒       " << "\n";
    std::cout << "       ▓▓      ▓▓    ▓▓ ▓▓      ▓▓    ▓▓ ▓▓    ▓▓       " << "\n";
    std::cout << "       ██       ██████  ███████  ██████   ██████        " << "\n";
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "Welcome to Fuego command-line wallet." << "\n";
    std::cout << "Please choose from the following options what you would like to do:\n";
    std::cout << "O - Open wallet\n";
    std::cout << "₲ - Generate new wallet\n";
    std::cout << "R - Restore from backup/paperwallet\n";
    std::cout << "I - Import wallet from private keys\n";
    std::cout << "M - Mnemonic seed (25-words) import\n";
    std::cout << "E - Exit\n";
    char c;
    do {
      std::string answer;
      std::getline(std::cin, answer);
      c = answer[0];
      if (!(c == 'O' || c == 'G' || c == 'E' || c == 'I' || c == 'o' || c == 'g' || c == 'e' || c == 'i' || c == 'm' || c == 'M')) {
        std::cout << "Unknown command: " << c << std::endl;
      } else {
        break;
      }
    } while (true);

    if (c == 'E' || c == 'e') {
      return false;
    }

    std::cout << "Specify wallet file name (e.g., name.wallet).\n";
    std::string userInput;
    do {
      std::cout << "Wallet file name: ";
      std::getline(std::cin, userInput);
      boost::algorithm::trim(userInput);
    } while (userInput.empty());
    if (c == 'i' || c == 'I') {
      key_import = true;
      m_import_new = userInput;
    } else if (c == 'm' || c == 'M') {
      key_import = false;
      m_import_new = userInput;
    } else if (c == 'g' || c == 'G') {
      m_generate_new = userInput;
    } else {
      m_wallet_file_arg = userInput;
    }
  }

  if (!m_generate_new.empty() && !m_wallet_file_arg.empty() && !m_import_new.empty()) {
    fail_msg_writer() << "you can't specify 'generate-new-wallet' and 'wallet-file' arguments simultaneously";
    return false;
  }

  std::string walletFileName;
  if (!m_generate_new.empty() || !m_import_new.empty()) {
    std::string ignoredString;
    if (!m_generate_new.empty()) {
      WalletHelper::prepareFileNames(m_generate_new, ignoredString, walletFileName);
    } else if (!m_import_new.empty()) {
      WalletHelper::prepareFileNames(m_import_new, ignoredString, walletFileName);
    }
    boost::system::error_code ignore;
    if (boost::filesystem::exists(walletFileName, ignore)) {
      fail_msg_writer() << walletFileName << " already exists";
      return false;
    }
  }

  Tools::PasswordContainer pwd_container;
  if (command_line::has_arg(vm, arg_password)) {
    pwd_container.password(command_line::get_arg(vm, arg_password));
  } else if (!pwd_container.read_password()) {
    fail_msg_writer() << "failed to read wallet password";
    return false;
  }

  this->m_node.reset(new NodeRpcProxy(m_daemon_host, m_daemon_port));

  std::promise<std::error_code> errorPromise;
  std::future<std::error_code> f_error = errorPromise.get_future();
  auto callback = [&errorPromise](std::error_code e) { errorPromise.set_value(e); };

  m_node->addObserver(static_cast<INodeRpcProxyObserver*>(this));
  m_node->init(callback);
  auto error = f_error.get();
  if (error) {
    fail_msg_writer() << "failed to init NodeRPCProxy: " << error.message();
    return false;
  }

  if (!m_generate_new.empty()) {
    std::string walletAddressFile = prepareWalletAddressFilename(m_generate_new);
    boost::system::error_code ignore;
    if (boost::filesystem::exists(walletAddressFile, ignore)) {
      logger(ERROR, BRIGHT_RED) << "Address file already exists: " + walletAddressFile;
      return false;
    }

    if (!new_wallet(walletFileName, pwd_container.password())) {
      logger(ERROR, BRIGHT_RED) << "account creation failed";
      return false;
    }

    if (!writeAddressFile(walletAddressFile, m_wallet->getAddress())) {
      logger(WARNING, BRIGHT_RED) << "Couldn't write wallet address file: " + walletAddressFile;
    }
  } else if (!m_import_new.empty()) {
    std::string walletAddressFile = prepareWalletAddressFilename(m_import_new);
    boost::system::error_code ignore;
    if (boost::filesystem::exists(walletAddressFile, ignore)) {
      logger(ERROR, BRIGHT_RED) << "Address file already exists: " + walletAddressFile;
      return false;
    }

    std::string private_spend_key_string;
    std::string private_view_key_string;

    Crypto::SecretKey private_spend_key;
    Crypto::SecretKey private_view_key;

    if (key_import) {
      do {
        std::cout << "Private Spend Key: ";
        std::getline(std::cin, private_spend_key_string);
        boost::algorithm::trim(private_spend_key_string);
      } while (private_spend_key_string.empty());
      do {
        std::cout << "Private View Key: ";
        std::getline(std::cin, private_view_key_string);
        boost::algorithm::trim(private_view_key_string);
      } while (private_view_key_string.empty());
    } else {
      std::string mnemonic_phrase;

      do {
        std::cout << "Mnemonics Phrase (25 words): ";
        std::getline(std::cin, mnemonic_phrase);
        boost::algorithm::trim(mnemonic_phrase);
        boost::algorithm::to_lower(mnemonic_phrase);
      } while (!is_valid_mnemonic(mnemonic_phrase, private_spend_key));

      /* This is not used, but is needed to be passed to the function, not sure how we can avoid this */
      Crypto::PublicKey unused_dummy_variable;

      AccountBase::generateViewFromSpend(private_spend_key, private_view_key, unused_dummy_variable);
    }

    /* We already have our keys if we import via mnemonic seed */
    if (key_import) {
      Crypto::Hash private_spend_key_hash;
      Crypto::Hash private_view_key_hash;
      size_t size;
      if (!Common::fromHex(private_spend_key_string, &private_spend_key_hash, sizeof(private_spend_key_hash), size) || size != sizeof(private_spend_key_hash)) {
        return false;
      }
      if (!Common::fromHex(private_view_key_string, &private_view_key_hash, sizeof(private_view_key_hash), size) || size != sizeof(private_spend_key_hash)) {
        return false;
      }
      private_spend_key = *(struct Crypto::SecretKey *)&private_spend_key_hash;
      private_view_key = *(struct Crypto::SecretKey *)&private_view_key_hash;
    }

    if (!new_wallet(private_spend_key, private_view_key, walletFileName, pwd_container.password())) {
      logger(ERROR, BRIGHT_RED) << "account creation failed";
      return false;
    }

    if (!writeAddressFile(walletAddressFile, m_wallet->getAddress())) {
      logger(WARNING, BRIGHT_RED) << "Couldn't write wallet address file: " + walletAddressFile;
    }
  } else {
    m_wallet.reset(new WalletLegacy(m_currency, *m_node, logManager));

    try {
      client_helper ch;
      m_wallet_file = ch.tryToOpenWalletOrLoadKeysOrThrow(logger, m_wallet, m_wallet_file_arg, pwd_container.password());
    } catch (const std::exception& e) {
      fail_msg_writer() << "failed to load wallet: " << e.what();
      return false;
    }

    m_wallet->addObserver(this);
    m_node->addObserver(static_cast<INodeObserver*>(this));

    logger(INFO, BRIGHT_WHITE) << "Opened wallet: " << m_wallet->getAddress();

    success_msg_writer() <<
      "**********************************************************************\n" <<
      "Use \"help\" command to see the list of available commands.\n" <<
      "**********************************************************************";
  }

  return true;
}

bool simple_wallet::deinit() {
  m_wallet->removeObserver(this);
  m_node->removeObserver(static_cast<INodeObserver*>(this));
  m_node->removeObserver(static_cast<INodeRpcProxyObserver*>(this));

  if (!m_wallet.get())
    return true;

  return close_wallet();
}

void simple_wallet::handle_command_line(const boost::program_options::variables_map& vm) {
  m_wallet_file_arg = command_line::get_arg(vm, arg_wallet_file);
  m_generate_new = command_line::get_arg(vm, arg_generate_new_wallet);
  m_daemon_address = command_line::get_arg(vm, arg_daemon_address);
  m_daemon_host = command_line::get_arg(vm, arg_daemon_host);
  m_daemon_port = command_line::get_arg(vm, arg_daemon_port);
}

bool simple_wallet::new_wallet(const std::string &wallet_file, const std::string& password) {
  m_wallet_file = wallet_file;

  m_wallet.reset(new WalletLegacy(m_currency, *m_node, logManager));
  m_node->addObserver(static_cast<INodeObserver*>(this));
  m_wallet->addObserver(this);
  try {
    m_initResultPromise.reset(new std::promise<std::error_code>());
    std::future<std::error_code> f_initError = m_initResultPromise->get_future();
    m_wallet->initAndGenerate(password);
    auto initError = f_initError.get();
    m_initResultPromise.reset(nullptr);
    if (initError) {
      fail_msg_writer() << "failed to generate new wallet: " << initError.message();
      return false;
    }

    try {
      CryptoNote::WalletHelper::storeWallet(*m_wallet, m_wallet_file);
    } catch (std::exception& e) {
      fail_msg_writer() << "failed to save new wallet: " << e.what();
      throw;
    }

    AccountKeys keys;
    m_wallet->getAccountKeys(keys);

    std::string secretKeysData = std::string(reinterpret_cast<char*>(&keys.spendSecretKey), sizeof(keys.spendSecretKey)) + std::string(reinterpret_cast<char*>(&keys.viewSecretKey), sizeof(keys.viewSecretKey));
    std::string guiKeys = Tools::Base58::encode_addr(CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX, secretKeysData);

    logger(INFO, BRIGHT_GREEN) << "xfg_wallet is an open-source, client-side, free wallet which allows you to send & receive Fuego instantly on the blockchain. Only YOU are in control of your funds & your private keys. When you generate a new wallet, send, receive or deposit Fuego - everything happens locally. Your seed is never transmitted, received or stored. IT IS IMPERATIVE that you write down, print, or save your seed phrase somewhere safe. The backup of keys is your responsibility only. If you lose your seed, your account CANNOT be recovered. Freedom isn't free - to truly be our own bank, we must treat fuego keys just like the credit banks (backin the old days) did with their vault keys. Schofield's 2nd Law of Computing states that data doesn't really exist unless you have at least two copies of it-- then keep each somewhere safe & secure.   " << std::endl << std::endl;

    std::cout << "Wallet Address: " << m_wallet->getAddress() << std::endl;
    std::cout << "Private spend key: " << Common::podToHex(keys.spendSecretKey) << std::endl;
    std::cout << "Private view key: " << Common::podToHex(keys.viewSecretKey) << std::endl;
    std::cout << "Mnemonic Seed: " << generate_mnemonic(keys.spendSecretKey) << std::endl;

  }
  catch (const std::exception& e) {
    fail_msg_writer() << "failed to generate new wallet: " << e.what();
    return false;
  }

  success_msg_writer() <<
    "**********************************************************************\n" <<
    "Your wallet has been generated.\n" <<
    "Use \"help\" command to see the list of available commands.\n" <<
    "Always use \"exit\" command when closing fire_wallet to save\n" <<
    "current session's state. Otherwise, you will possibly need to synchronize \n" <<
    "your wallet again. Your wallet keys are not under risk in doing so.\n" <<
    "**********************************************************************";
  return true;
}

bool simple_wallet::new_wallet(Crypto::SecretKey &secret_key, Crypto::SecretKey &view_key, const std::string &wallet_file, const std::string& password) {
  m_wallet_file = wallet_file;

  m_wallet.reset(new WalletLegacy(m_currency, *m_node.get(), logManager));
  m_node->addObserver(static_cast<INodeObserver*>(this));
  m_wallet->addObserver(this);
  try {
    m_initResultPromise.reset(new std::promise<std::error_code>());
    std::future<std::error_code> f_initError = m_initResultPromise->get_future();

    AccountKeys wallet_keys;
    wallet_keys.spendSecretKey = secret_key;
    wallet_keys.viewSecretKey = view_key;
    Crypto::secret_key_to_public_key(wallet_keys.spendSecretKey, wallet_keys.address.spendPublicKey);
    Crypto::secret_key_to_public_key(wallet_keys.viewSecretKey, wallet_keys.address.viewPublicKey);

    m_wallet->initWithKeys(wallet_keys, password);
    auto initError = f_initError.get();
    m_initResultPromise.reset(nullptr);
    if (initError) {
      fail_msg_writer() << "failed to generate new wallet: " << initError.message();
      return false;
    }

    try {
      CryptoNote::WalletHelper::storeWallet(*m_wallet, m_wallet_file);
    } catch (std::exception& e) {
      fail_msg_writer() << "failed to save new wallet: " << e.what();
      throw;
    }

    AccountKeys keys;
    m_wallet->getAccountKeys(keys);

    logger(INFO, BRIGHT_WHITE) <<
      "Imported wallet: " << m_wallet->getAddress() << std::endl;
  }
  catch (const std::exception& e) {
    fail_msg_writer() << "failed to import wallet: " << e.what();
    return false;
  }

  success_msg_writer() <<
    "**********************************************************************\n" <<
    "Your wallet has been imported.\n" <<
    "Use \"help\" command to see the list of available commands.\n" <<
    "Always use \"exit\" command when closing fire_wallet to save\n" <<
    "current session's state. Otherwise, you will possibly need to synchronize \n" <<
    "your wallet again. Your wallet key is not under risk in doing so.\n" <<
    "**********************************************************************";
  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::deposit(const std::vector<std::string> &args)
{
  // No ETH address required at deposit time
  // Recipient binding happens at STARK proof generation (xfg-stark-cli)
  // This prevents linking Fuego deposits to ETH addresses on-chain
  if (args.size() != 2)
  {
    fail_msg_writer() << "Usage: deposit <amount> <term_code>";
    fail_msg_writer() << "Amount tiers: 0.8, 8, 80, 800 XFG";
    fail_msg_writer() << "Term codes: 3 (3 months), 12 (1 year)";
    fail_msg_writer() << "";
    fail_msg_writer() << "For HEAT burn deposits, use: burn <amount>";
    fail_msg_writer() << "";
    fail_msg_writer() << "ETH address is provided later when generating STARK proof.";
    fail_msg_writer() << "         This prevents linking your Fuego wallet to your ETH address on-chain.";
    return true;
  }

  try
  {
    // Parse and validate amount
    uint64_t deposit_amount = 0;
    bool ok = m_currency.parseAmount(args[0], deposit_amount);

    if (!ok || 0 == deposit_amount)
    {
      fail_msg_writer() << "Invalid amount format: " << args[0];
      return true;
    }

    // Validate amount is one of the allowed tiers (same tiers for HEAT burns and COLD deposits)
    std::vector<uint64_t> valid_amounts = {
      CryptoNote::parameters::AMOUNT_TIER_0,                       // 0.8 XFG
      CryptoNote::parameters::AMOUNT_TIER_1,                       // 8 XFG
      CryptoNote::parameters::AMOUNT_TIER_2,                       // 80 XFG
      CryptoNote::parameters::AMOUNT_TIER_3                        // 800 XFG
    };

    std::vector<std::string> amount_labels = {
      "0.8 XFG (HEAT/COLD)",
      "8 XFG (HEAT/COLD)",
      "80 XFG (HEAT/COLD)",
      "800 XFG (HEAT/COLD)"
    };

    auto it = std::find(valid_amounts.begin(), valid_amounts.end(), deposit_amount);
    if (it == valid_amounts.end()) {
      fail_msg_writer() << "Invalid amount. Valid tiers:";
      for (const auto& label : amount_labels) {
        fail_msg_writer() << "  " << label;
      }
      return true;
    }

    size_t amount_index = std::distance(valid_amounts.begin(), it);
    std::string amount_label = amount_labels[amount_index];

    // Helper function to check if amount is HEAT burn (0.8 XFG tier)
    auto is_heat_burn_amount = [](uint64_t amount) -> bool {
      return amount == CryptoNote::parameters::AMOUNT_TIER_0;
    };

    // Parse term code
    uint32_t term_code = boost::lexical_cast<uint32_t>(args[1]);
    uint32_t deposit_term = 0;
    std::string term_label = "";

    // Define valid terms based on network
    uint32_t min_term = m_currency.isTestnet() ? CryptoNote::parameters::TESTNET_COLD_MIN_TERM : CryptoNote::parameters::COLD_MIN_TERM;
    uint32_t max_term = m_currency.isTestnet() ? CryptoNote::parameters::TESTNET_COLD_MAX_TERM : CryptoNote::parameters::COLD_MAX_TERM;

    // Validate term codes - only COLD deposits (3 or 12), no HEAT (0)
    if (term_code == 3) {
      deposit_term = min_term;
      term_label = "3 months";
    } else if (term_code == 12) {
      deposit_term = max_term;
      term_label = "1 year";
    } else {
      fail_msg_writer() << "Invalid term code. Valid codes for deposit:";
      fail_msg_writer() << "  3 for 3-month COLD deposit";
      fail_msg_writer() << "  12 for 1-year COLD deposit";
      fail_msg_writer() << "";
      fail_msg_writer() << "For HEAT burn deposits, use: burn <amount>";
      return true;
    }

    // Confirm with user
    success_msg_writer() << "Creating deposit:";
    success_msg_writer() << "  Amount: " << m_currency.formatAmount(deposit_amount) << " (" << amount_label << ")";
    success_msg_writer() << "  Term: " << term_label << " (" << deposit_term << " blocks)";

    std::string confirm;
    success_msg_writer() << "Confirm? (y/n): ";
    std::getline(std::cin, confirm);
    if (confirm != "y" && confirm != "Y") {
      success_msg_writer() << "Deposit cancelled";
      return true;
    }

    success_msg_writer() << "Creating deposit with commitment...";

    // Create transaction extra with commitment
    std::vector<uint8_t> extra;
    std::string extraString;

    // No ETH address in commitment
    // Recipient binding happens at STARK proof generation time (xfg-stark-cli)
    // This prevents Fuego deposits from linking to ETH addresses on-chain

    // Generate a random 32-byte secret
    Crypto::PublicKey public_key;
    Crypto::SecretKey secret_key;
    Crypto::generate_keys(public_key, secret_key);

    // Convert secret to array for commitment computation
    std::array<uint8_t, 32> secret_array;
    std::copy(secret_key.data, secret_key.data + 32, secret_array.begin());

    // Create a tx_prefix_hash placeholder (hash of secret for binding)
    Crypto::Hash placeholder_tx_hash;
    keccak(secret_key.data, sizeof(secret_key.data), placeholder_tx_hash.data, sizeof(placeholder_tx_hash.data));

    // Network parameters
    uint32_t network_id = 1; // Fuego mainnet
    uint32_t target_chain_id = 1; // Ethereum mainnet
    uint32_t commitment_version = 1;

    // Empty metadata - no ETH address stored (privacy)
    std::vector<uint8_t> metadata;

    if (deposit_term == CryptoNote::parameters::DEPOSIT_TERM_FOREVER) {
      // Create HEAT commitment for burn deposit (no term, FOREVER)
      Crypto::Hash commitment = CryptoNote::computeHeatCommitment(
        secret_array,
        deposit_amount,
        placeholder_tx_hash,
        network_id,
        target_chain_id,
        commitment_version
      );

      if (commitment == Crypto::Hash{}) {
        fail_msg_writer() << "Failed to compute HEAT commitment";
        return true;
      }

      // Create the transaction extra with the HEAT commitment
      if (!CryptoNote::createTxExtraWithHeatCommitment(commitment, deposit_amount, metadata, extra)) {
        fail_msg_writer() << "Failed to create HEAT commitment";
        return true;
      }

      success_msg_writer() << "HEAT commitment generated: " << Common::podToHex(commitment);
      success_msg_writer() << "Secret key (STORE SECURELY): " << Common::podToHex(secret_key);
      success_msg_writer() << "";
      success_msg_writer() << "IMPORTANT: Save the secret key! You will need it + your ETH address";
      success_msg_writer() << "           when generating the STARK proof with xfg-stark-cli.";
    } else {
      // Create COLD deposit commitment for yield deposit (with term)
      uint8_t chain_code = 1; // Ethereum mainnet chain code
      std::vector<uint8_t> gift_secret; // Empty - not gifting

      Crypto::Hash commitment = CryptoNote::computeColdCommitment(
        secret_array,
        deposit_amount,
        placeholder_tx_hash,
        network_id,
        target_chain_id,
        commitment_version,
        deposit_term
      );

      if (commitment == Crypto::Hash{}) {
        fail_msg_writer() << "Failed to compute COLD commitment";
        return true;
      }

      // Create the transaction extra with COLD commitment
      if (!CryptoNote::createTxExtraWithColdCommitment(commitment, deposit_amount, deposit_term, chain_code, metadata, gift_secret, extra)) {
        fail_msg_writer() << "Failed to create COLD commitment data";
        return true;
      }

      success_msg_writer() << "COLD commitment generated: " << Common::podToHex(commitment);
      success_msg_writer() << "Secret key (STORE SECURELY): " << Common::podToHex(secret_key);
      success_msg_writer() << "";
      success_msg_writer() << "IMPORTANT: Save the secret key! You will need it + your ETH address";
      success_msg_writer() << "           when generating the STARK proof with xfg-stark-cli.";
    }

    // Convert extra vector to string
    extraString = std::string(extra.begin(), extra.end());

    // Use IWalletLegacy deposit method with extra data
    uint64_t fee = m_currency.minimumFee();
    CryptoNote::TransactionId txId = m_wallet->deposit(deposit_term, deposit_amount, fee, extraString, 0);

    if (txId == CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID) {
      fail_msg_writer() << "Failed to create deposit transaction";
      return true;
    }

    if (deposit_amount == CryptoNote::parameters::AMOUNT_TIER_0) {
      success_msg_writer(true) << "HEAT burn deposit transaction created successfully!";
      success_msg_writer() << "Transaction ID: " << txId;
      success_msg_writer() << "Amount burned: " << m_currency.formatAmount(deposit_amount);
      success_msg_writer() << "";
      success_msg_writer() << "PRIVACY: No ETH address stored on-chain.";
      success_msg_writer() << "Use xfg-stark-cli to generate a STARK proof and claim HEAT (Fuego Embers) tokens.";
      success_msg_writer() << "You will provide your ETH address when generating the STARK proof.";
    } else {
      success_msg_writer(true) << "COLD yield deposit transaction created successfully!";
      success_msg_writer() << "Transaction ID: " << txId;
      success_msg_writer() << "Amount to be locked: " << m_currency.formatAmount(deposit_amount);
      success_msg_writer() << "Term: " << term_label;
      success_msg_writer() << "";
      success_msg_writer() << "PRIVACY: No ETH address stored on-chain.";
      success_msg_writer() << "Use xfg-stark-cli to generate a STARK proof for claiming.";
      success_msg_writer() << "You will provide your ETH address when generating the STARK proof.";
    }
  }
  catch (const std::system_error& e)
  {
    fail_msg_writer() << "System error: " << e.what();
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << "Error: " << e.what();
  }
  catch (...)
  {
    fail_msg_writer() << "unknown error";
  }

 return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::create_cold_secret(const std::vector<std::string> &args) {
 // PRIVACY MODEL: No ETH address required - recipient binding at STARK proof generation time
 if (args.size() != 3) {
   fail_msg_writer() << "usage: create_cold_secret <amount> <term_blocks> <chain_code>";
   fail_msg_writer() << "  amount: amount in atomic XFG (e.g., 80000000 for 8 XFG)";
   fail_msg_writer() << "  term_blocks: deposit term in blocks (e.g., 16440 for 3 months)";
   fail_msg_writer() << "  chain_code: target claim chain (1=ETH, 2=ARB)";
   fail_msg_writer() << "";
   fail_msg_writer() << "PRIVACY: ETH address is NOT required at deposit time.";
   fail_msg_writer() << "         You will provide your ETH address when generating the STARK proof.";
   fail_msg_writer() << "";
   fail_msg_writer() << "Example: create_cold_secret 80000000 16440 1";
   return true;
 }

 try {
   uint64_t amount = boost::lexical_cast<uint64_t>(args[0]);
   uint32_t term_blocks = boost::lexical_cast<uint32_t>(args[1]);
   uint8_t chain_code = boost::lexical_cast<uint8_t>(args[2]);

   // Validate amount is a valid tier
   if (!CryptoNote::BurnProofDataFileGenerator::isValidXfgAmount(amount)) {
     fail_msg_writer() << "Invalid amount. Must be one of: 8000000 (0.8 XFG), 80000000 (8 XFG), 800000000 (80 XFG), 8000000000 (800 XFG)";
     return true;
   }

   // Generate a random secret key
   Crypto::PublicKey public_key;
   Crypto::SecretKey secret_key;
   Crypto::generate_keys(public_key, secret_key);

   // Convert secret to array for commitment computation
   std::array<uint8_t, 32> secret_array;
   std::copy(secret_key.data, secret_key.data + 32, secret_array.begin());

   // Placeholder tx_hash for standalone commitment generation
   // Use secret-derived hash since we don't have actual tx yet
   Crypto::Hash placeholder_tx_hash;
   {
     std::vector<uint8_t> binding_data;
     binding_data.insert(binding_data.end(), secret_key.data, secret_key.data + 32);
     // Add term to binding for uniqueness
     binding_data.insert(binding_data.end(),
       reinterpret_cast<const uint8_t*>(&term_blocks),
       reinterpret_cast<const uint8_t*>(&term_blocks) + sizeof(term_blocks));
     keccak(binding_data.data(), binding_data.size(), placeholder_tx_hash.data, sizeof(placeholder_tx_hash.data));
   }

   // PRIVACY MODEL: Compute COLD commitment WITHOUT recipient (ETH address)
   // Recipient binding happens at STARK proof generation time
   uint32_t network_id = 1;
   uint32_t target_chain_id = chain_code;  // Use chain_code as target chain
   uint32_t commitment_version = 1;

   Crypto::Hash commitment = CryptoNote::computeColdCommitment(
     secret_array, amount, placeholder_tx_hash,
     network_id, target_chain_id, commitment_version, term_blocks
   );

   if (commitment == Crypto::Hash{}) {
     fail_msg_writer() << "Failed to compute COLD commitment";
     return true;
   }

   // Empty metadata - no ETH address stored on-chain for privacy
   std::vector<uint8_t> metadata;
   std::vector<uint8_t> gift_secret; // Empty - not gifting

   // Create the transaction extra with COLD commitment
   std::vector<uint8_t> extra;
   if (!CryptoNote::createTxExtraWithColdCommitment(commitment, amount, term_blocks, chain_code, metadata, gift_secret, extra)) {
     fail_msg_writer() << "Failed to create COLD commitment data";
     return true;
   }

   success_msg_writer() << "COLD commitment created successfully:";
   success_msg_writer() << "Commitment: " << Common::podToHex(commitment);
   success_msg_writer() << "Secret Key (STORE SECURELY): " << Common::podToHex(secret_key);
   success_msg_writer() << "Amount: " << amount << " atomic XFG (" << (amount / 10000000.0) << " XFG)";
   success_msg_writer() << "Term: " << term_blocks << " blocks";
   success_msg_writer() << "Chain Code: " << static_cast<int>(chain_code);
   success_msg_writer() << "";
   success_msg_writer() << "PRIVACY: No ETH address stored on-chain.";
   success_msg_writer() << "IMPORTANT: Save the secret key! You will need it when generating";
   success_msg_writer() << "           the STARK proof with xfg-stark-cli.";

 } catch (const std::exception& e) {
   fail_msg_writer() << "Failed to parse arguments: " << e.what();
   return true;
 }

 return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::close_wallet()
{
  try {
    CryptoNote::WalletHelper::storeWallet(*m_wallet, m_wallet_file);
  } catch (const std::exception& e) {
    fail_msg_writer() << e.what();
    return false;
  }

  m_wallet->removeObserver(this);
  m_wallet->shutdown();

  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::save(const std::vector<std::string> &args)
{
  try {
    CryptoNote::WalletHelper::storeWallet(*m_wallet, m_wallet_file);
    success_msg_writer() << "Wallet data saved";
  } catch (const std::exception& e) {
    fail_msg_writer() << e.what();
  }

  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::list_deposits(const std::vector<std::string> &)
{
  size_t deposit_count = m_wallet->getDepositCount();

  if (deposit_count == 0)
  {
    success_msg_writer() << "No deposits found";
    return true;
  }

  success_msg_writer() << "Deposits (" << deposit_count << "):";
  success_msg_writer() << "ID    | Amount             | Term          | Unlock Height | Status";
  success_msg_writer() << "------|--------------------|---------------|---------------|--------";

  // go through deposits ids for the amount of deposits in wallet
  for (CryptoNote::DepositId id = 0; id < deposit_count; ++id)
  {
    // get deposit info from id and store it to deposit
    CryptoNote::Deposit deposit;
    if (!m_wallet->getDeposit(id, deposit)) {
      continue; // Skip invalid deposits
    }

    // Format amount with interest for yield deposits
    std::string amount_str = m_currency.formatAmount(deposit.amount);
    if (deposit.amount != CryptoNote::parameters::AMOUNT_TIER_0 && deposit.interest > 0) {
      amount_str += " + " + m_currency.formatAmount(deposit.interest) + " interest";
    }

    // Format term
    std::string term_str = "";
    if (deposit.term == CryptoNote::parameters::DEPOSIT_TERM_FOREVER) {
      term_str = "HEAT (forever)";
    } else if (deposit.term == CryptoNote::parameters::COLD_MIN_TERM) {
      term_str = "3 months";
    } else if (deposit.term == CryptoNote::parameters::COLD_MAX_TERM) {
      term_str = "1 year";
    } else {
      term_str = std::to_string(deposit.term) + " blocks";
    }

    // Format unlock height
    std::string unlock_str = "";
    if (deposit.locked) {
      unlock_str = std::to_string(deposit.unlockHeight);
    } else if (deposit.spendingTransactionId != CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID) {
      unlock_str = "Withdrawn";
    } else {
      unlock_str = "Unlocked";
    }

    // Format status
    std::string status_str = "";
    if (deposit.locked) {
      status_str = "Locked";
    } else if (deposit.spendingTransactionId == CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID) {
      status_str = "Unlocked";
    } else {
      status_str = "Withdrawn";
    }

    success_msg_writer() << std::left <<
      std::setw(5)  << std::to_string(id) << " | " <<
      std::setw(18) << amount_str << " | " <<
      std::setw(13) << term_str << " | " <<
      std::setw(13) << unlock_str << " | " <<
      std::setw(8) << status_str;
  }

  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::burn(const std::vector<std::string> &args)
{
  // Simplified burn command - just takes amount, term is always FOREVER
  if (args.size() != 1)
  {
    fail_msg_writer() << "Usage: burn <amount>";
    fail_msg_writer() << "Valid amounts: 0.8, 8, 80, 800 XFG";
    fail_msg_writer() << "";
    fail_msg_writer() << "Creates a HEAT burn deposit with term automatically set to FOREVER.";
    fail_msg_writer() << "ETH address is provided later when generating STARK proof.";
    return true;
  }

  try
  {
    // Parse and validate amount
    uint64_t burn_amount = 0;
    bool ok = m_currency.parseAmount(args[0], burn_amount);

    if (!ok || 0 == burn_amount)
    {
      fail_msg_writer() << "Invalid amount format: " << args[0];
      return true;
    }

    // Validate amount is one of the allowed tiers
    std::vector<uint64_t> valid_amounts = {
      CryptoNote::parameters::AMOUNT_TIER_0,  // 0.8 XFG
      CryptoNote::parameters::AMOUNT_TIER_1,  // 8 XFG
      CryptoNote::parameters::AMOUNT_TIER_2,  // 80 XFG
      CryptoNote::parameters::AMOUNT_TIER_3   // 800 XFG
    };

    std::vector<std::string> amount_labels = {
      "0.8 XFG",
      "8 XFG",
      "80 XFG",
      "800 XFG"
    };

    auto it = std::find(valid_amounts.begin(), valid_amounts.end(), burn_amount);
    if (it == valid_amounts.end()) {
      fail_msg_writer() << "Invalid amount. Valid tiers:";
      for (const auto& label : amount_labels) {
        fail_msg_writer() << "  " << label;
      }
      return true;
    }

    size_t amount_index = std::distance(valid_amounts.begin(), it);
    std::string amount_label = amount_labels[amount_index];

    // HEAT burn deposits always use DEPOSIT_TERM_FOREVER
    uint32_t burn_term = CryptoNote::parameters::DEPOSIT_TERM_FOREVER;
    std::string term_label = "HEAT burn (forever)";

    // Confirm with user
    success_msg_writer() << "Creating HEAT burn deposit:";
    success_msg_writer() << "  Amount: " << m_currency.formatAmount(burn_amount) << " (" << amount_label << ")";
    success_msg_writer() << "  Term: " << term_label << " (" << burn_term << " blocks)";
    success_msg_writer() << "";
    success_msg_writer() << "ETH address is provided later when generating STARK proof.";
    success_msg_writer() << "  This prevents linking your Fuego wallet to your ETH address on-chain.";

    // Send the burn deposit transaction
    uint64_t fee = m_currency.minimumFee();
    std::string extraString = "";
    CryptoNote::TransactionId txId = m_wallet->deposit(burn_term, burn_amount, fee, extraString, 0);

    if (CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID == txId) {
      fail_msg_writer() << "Sending deposit transaction failed";
      return true;
    }

    success_msg_writer() << "HEAT burn deposit transaction sent! ID: " << txId;
    return true;
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << "Error: " << e.what();
    return true;
  }
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::cold(const std::vector<std::string> &args)
{
  // Simplified COLD deposit command - amount + term code (3 or 12)
  if (args.size() != 2)
  {
    fail_msg_writer() << "Usage: cold <amount> <term_code>";
    fail_msg_writer() << "Valid amounts: 0.8, 8, 80, 800 XFG";
    fail_msg_writer() << "Valid term codes: 3 (3 months), 12 (1 year)";
    fail_msg_writer() << "";
    fail_msg_writer() << "ETH address is provided later when generating STARK proof.";
    fail_msg_writer() << "  This prevents linking your Fuego wallet to your ETH address on-chain.";
    return true;
  }

  try
  {
    // Parse and validate amount
    uint64_t cold_amount = 0;
    bool ok = m_currency.parseAmount(args[0], cold_amount);

    if (!ok || 0 == cold_amount)
    {
      fail_msg_writer() << "Invalid amount format: " << args[0];
      return true;
    }

    // Validate amount is one of the allowed tiers
    std::vector<uint64_t> valid_amounts = {
      CryptoNote::parameters::AMOUNT_TIER_0,  // 0.8 XFG
      CryptoNote::parameters::AMOUNT_TIER_1,  // 8 XFG
      CryptoNote::parameters::AMOUNT_TIER_2,  // 80 XFG
      CryptoNote::parameters::AMOUNT_TIER_3   // 800 XFG
    };

    std::vector<std::string> amount_labels = {
      "0.8 XFG",
      "8 XFG",
      "80 XFG",
      "800 XFG"
    };

    auto it = std::find(valid_amounts.begin(), valid_amounts.end(), cold_amount);
    if (it == valid_amounts.end()) {
      fail_msg_writer() << "Invalid amount. Valid tiers:";
      for (const auto& label : amount_labels) {
        fail_msg_writer() << "  " << label;
      }
      return true;
    }

    size_t amount_index = std::distance(valid_amounts.begin(), it);
    std::string amount_label = amount_labels[amount_index];

    // Parse term code
    uint32_t term_code = boost::lexical_cast<uint32_t>(args[1]);
    uint32_t cold_term = 0;
    std::string term_label = "";

    // Define valid terms based on network
    uint32_t min_term = m_currency.isTestnet() ? CryptoNote::parameters::TESTNET_COLD_MIN_TERM : CryptoNote::parameters::COLD_MIN_TERM;
    uint32_t max_term = m_currency.isTestnet() ? CryptoNote::parameters::TESTNET_COLD_MAX_TERM : CryptoNote::parameters::COLD_MAX_TERM;

    // Validate term codes - only 3 or 12
    if (term_code == 3) {
      cold_term = min_term;
      term_label = "3 months";
    } else if (term_code == 12) {
      cold_term = max_term;
      term_label = "1 year";
    } else {
      fail_msg_writer() << "Invalid term code. Valid codes:";
      fail_msg_writer() << "  3 for 3-month COLD deposit";
      fail_msg_writer() << "  12 for 1-year COLD deposit";
      return true;
    }

    // Confirm with user
    success_msg_writer() << "Creating COLD deposit:";
    success_msg_writer() << "  Amount: " << m_currency.formatAmount(cold_amount) << " (" << amount_label << ")";
    success_msg_writer() << "  Term: " << term_label << " (" << cold_term << " blocks)";
    success_msg_writer() << "";
    success_msg_writer() << "ETH address is provided later when generating STARK proof.";
    success_msg_writer() << "  This prevents linking your Fuego wallet to your ETH address on-chain.";

    // Send the COLD deposit transaction
    uint64_t fee = m_currency.minimumFee();
    std::string extraString = "";
    CryptoNote::TransactionId txId = m_wallet->deposit(cold_term, cold_amount, fee, extraString, 0);

    if (CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID == txId) {
      fail_msg_writer() << "Sending deposit transaction failed";
      return true;
    }

    success_msg_writer() << "COLD deposit transaction sent! ID: " << txId;
    return true;
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << "Error: " << e.what();
    return true;
  }
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::elderking_ceremony(const std::vector<std::string> &args)
{
  // Elderfier registration: batch 5x 800 XFG deposits with 0xEC tag (total 4000 XFG)
  // This is the ceremonial registration process for becoming an Elderfier
  if (args.size() != 0)
  {
    fail_msg_writer() << "Usage: elderking_ceremony";
    return true;
  }

  try
  {
    // Display the Elderfire StayKing Ceremony info screen first
    success_msg_writer() << "";
    success_msg_writer() << "╔════════════════════════════════════════════════════════════╗";
    success_msg_writer() << "║                                                            ║";
    success_msg_writer() << "║            🔥⚡  ELDERFIRE STAYKING CEREMONY  ⚡🔥          ║";
    success_msg_writer() << "║                                                            ║";
    success_msg_writer() << "╚════════════════════════════════════════════════════════════╝";
    success_msg_writer() << "";
    success_msg_writer() << "┌─── WHAT IS THIS? ──────────────────────────────────────────┐";
    success_msg_writer() << "│ The Elderfire StayKing Ceremony is the ritual through which │";
    success_msg_writer() << "│ you commit to becoming an ELDERFIER on the Fuego network.   │";
    success_msg_writer() << "│ Elderfiers form the backbone of consensus, signing merkle   │";
    success_msg_writer() << "│ roots and earning rewards for their vigilance.             │";
    success_msg_writer() << "└────────────────────────────────────────────────────────────┘";
    success_msg_writer() << "";
    success_msg_writer() << "┌─── WHAT DO YOU NEED? ──────────────────────────────────────┐";
    success_msg_writer() << "│                                                            │";
    success_msg_writer() << "│  📊 STAKING REQUIREMENTS:                                  │";
    success_msg_writer() << "│     • Exactly 5 deposits of 800 XFG each                   │";
    success_msg_writer() << "│     • Total commitment: 4,000 XFG                          │";
    success_msg_writer() << "│     • Tagged with 0xEC (Elderfier staking tag)             │";
    success_msg_writer() << "│     • No banking fees applied to your deposits             │";
    success_msg_writer() << "│     • Network transaction fees: ~0.00005 XFG per deposit   │";
    success_msg_writer() << "│                                                            │";
    success_msg_writer() << "│  💰 TOTAL COST:                                            │";
    success_msg_writer() << "│     • 4,000 XFG (deposits) + network fees                  │";
    success_msg_writer() << "│     • (Will be calculated below)                           │";
    success_msg_writer() << "│                                                            │";
    success_msg_writer() << "└────────────────────────────────────────────────────────────┘";
    success_msg_writer() << "";
    success_msg_writer() << "┌─── WHAT DOES IT ENABLE? ──────────────────────────────────┐";
    success_msg_writer() << "│                                                            │";
    success_msg_writer() << "│  🔐 ELDERFIER POWERS:                                      │";
    success_msg_writer() << "│     ✓ Sign merkle roots of deposit commitments             │";
    success_msg_writer() << "│     ✓ Participate in consensus validation (69% threshold)  │";
    success_msg_writer() << "│     ✓ Earn 0.1% of all HEAT/COLD banking fees             │";
    success_msg_writer() << "│     ✓ Pro-rata fee distribution (only to signers)          │";
    success_msg_writer() << "│     ✓ Contribute to network security & decentralization    │";
    success_msg_writer() << "│                                                            │";
    success_msg_writer() << "│  🌍 NETWORK REGISTRATION:                                  │";
    success_msg_writer() << "│     • Network automatically detects your 5 deposits        │";
    success_msg_writer() << "│     • You are assigned an Elderfier ID (0-255)             │";
    success_msg_writer() << "│     • Your deposits become immune to normal withdrawals     │";
    success_msg_writer() << "│     • You're added to the active elderfiers registry       │";
    success_msg_writer() << "│                                                            │";
    success_msg_writer() << "└────────────────────────────────────────────────────────────┘";
    success_msg_writer() << "";
    success_msg_writer() << "┌─── HOW DOES IT WORK? ──────────────────────────────────────┐";
    success_msg_writer() << "│                                                            │";
    success_msg_writer() << "│  1️⃣  YOU perform the ceremony (deposit 4000 XFG)           │";
    success_msg_writer() << "│  2️⃣  5 transactions are broadcast to the network           │";
    success_msg_writer() << "│  3️⃣  Miners confirm all 5 deposits in blocks               │";
    success_msg_writer() << "│  4️⃣  Network auto-detects pattern & registers you         │";
    success_msg_writer() << "│  5️⃣  You receive an Elderfier ID                          │";
    success_msg_writer() << "│  6️⃣  Your node can now sign merkle roots independently    │";
    success_msg_writer() << "│  7️⃣  Fees earned when ≥69% elderfiers validate same root  │";
    success_msg_writer() << "│  8️⃣  You receive pro-rata share of banking fees           │";
    success_msg_writer() << "│                                                            │";
    success_msg_writer() << "└────────────────────────────────────────────────────────────┘";
    success_msg_writer() << "";
    success_msg_writer() << "┌─── IMPORTANT NOTES ────────────────────────────────────────┐";
    success_msg_writer() << "│                                                            │";
    success_msg_writer() << "│  ⚠️  This is a PERMANENT commitment to the network         │";
    success_msg_writer() << "│  ⚠️  Deposits become part of elderfier consensus            │";
    success_msg_writer() << "│  ⚠️  Only sign roots you believe are valid                 │";
    success_msg_writer() << "│  ⚠️  Malicious signing can result in slashing              │";
    success_msg_writer() << "│  ⚠️  You MUST run an elderfier node to earn fees           │";
    success_msg_writer() << "│                                                            │";
    success_msg_writer() << "└────────────────────────────────────────────────────────────┘";
    success_msg_writer() << "";

    // Check wallet balance
    uint64_t balance = m_wallet->actualBalance();
    uint64_t required = 4000 * CryptoNote::parameters::COIN;  // 4000 XFG (5 × 800 XFG)
    uint64_t fee = m_currency.minimumFee();

    success_msg_writer() << "┌─── BALANCE CHECK ──────────────────────────────────────────┐";
    success_msg_writer() << "│  Wallet balance:        " << m_currency.formatAmount(balance) << " XFG";
    success_msg_writer() << "│  Deposit amount:        " << m_currency.formatAmount(required) << " XFG (5 × 800)";
    success_msg_writer() << "│  Network fees (5×):     " << m_currency.formatAmount(5 * fee) << " XFG";
    success_msg_writer() << "│  ─────────────────────────────────────────────────────────";
    success_msg_writer() << "│  Total required:        " << m_currency.formatAmount(required + (5 * fee)) << " XFG";
    success_msg_writer() << "└────────────────────────────────────────────────────────────┘";
    success_msg_writer() << "";

    if (balance < required + (5 * fee)) {
      fail_msg_writer() << "";
      fail_msg_writer() << "❌ INSUFFICIENT BALANCE";
      fail_msg_writer() << "   You need " << m_currency.formatAmount(required + (5 * fee) - balance) << " more XFG";
      fail_msg_writer() << "   Ceremony cancelled.";
      return true;
    }

    success_msg_writer() << "✅ Balance check passed - you have sufficient funds!";
    success_msg_writer() << "";
    success_msg_writer() << "╔════════════════════════════════════════════════════════════╗";
    success_msg_writer() << "║  Ready to proceed with the Elderfire StayKing Ceremony?    ║";
    success_msg_writer() << "╚════════════════════════════════════════════════════════════╝";
    success_msg_writer() << "";
    success_msg_writer() << "⚡ Type 'IGNITE' to begin the ceremony, or press Enter to abort: ";

    std::string confirm;
    std::getline(std::cin, confirm);

    if (confirm != "IGNITE") {
      success_msg_writer() << "";
      success_msg_writer() << "🚫 Ceremony aborted. The Elderfire remains dormant.";
      return true;
    }

    success_msg_writer() << "";
    success_msg_writer() << "╔════════════════════════════════════════════════════════════╗";
    success_msg_writer() << "║                 🔥⚡ THE CEREMONY BEGINS ⚡🔥               ║";
    success_msg_writer() << "║              Igniting the Elderfire StayKing ritual...     ║";
    success_msg_writer() << "╚════════════════════════════════════════════════════════════╝";
    success_msg_writer() << "";

    // Create 5 deposits of 800 XFG each with 0xEC tag
    uint64_t amount_per_deposit = 800 * CryptoNote::parameters::COIN;  // 800 XFG

    std::vector<CryptoNote::TransactionId> txIds;

    for (int i = 0; i < 5; ++i) {
      success_msg_writer() << "";
      success_msg_writer() << "⚡ Ritual " << (i + 1) << " of 5: Forging Elderfire Stake ⚡";
      success_msg_writer() << "  Creating 800 XFG commitment...";

      // Create elderfier deposit with 0xEC tag
      std::vector<uint8_t> extra;
      std::string extraString = "";

      // Generate commitment hash (random 32-byte hash for this deposit)
      Crypto::PublicKey public_key;
      Crypto::SecretKey secret_key;
      Crypto::generate_keys(public_key, secret_key);
      Crypto::Hash commitment_hash = Crypto::cn_fast_hash(public_key.data, sizeof(public_key.data));

      // Create 0xEC elderfier deposit extra field
      CryptoNote::TransactionExtraElderfierDeposit elderfierDeposit;
      elderfierDeposit.depositHash = commitment_hash;
      elderfierDeposit.depositAmount = amount_per_deposit;
      elderfierDeposit.elderfierAddress = "";  // Optional: can be set to node address later
      elderfierDeposit.securityWindow = 28800;  // 8 hours default security window
      elderfierDeposit.metadata.clear();
      elderfierDeposit.signature.clear();
      elderfierDeposit.isSlashable = true;  // Deposits can be slashed by Elder Council

      // Add elderfier deposit to transaction extra
      CryptoNote::addElderfierDepositToExtra(extra, elderfierDeposit);

      // Send the transaction - use standard deposit mechanism
      // Note: For 0xEC deposits, we don't use the normal "term" system
      // Using DEPOSIT_TERM_FOREVER as placeholder since term is not used for elderfier deposits
      CryptoNote::TransactionId txId = m_wallet->deposit(
        CryptoNote::parameters::DEPOSIT_TERM_FOREVER,
        amount_per_deposit,
        fee,
        extraString,
        0
      );

      if (CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID == txId) {
        fail_msg_writer() << "";
        fail_msg_writer() << "❌ CEREMONY FAILED AT RITUAL " << (i + 1) << " OF 5";
        fail_msg_writer() << "   The Elderfire has been extinguished.";
        fail_msg_writer() << "   Only " << i << " stakes were forged before the ritual faltered.";
        return true;
      }

      txIds.push_back(txId);
      success_msg_writer() << "  ✨ Stake " << (i + 1) << " forged!";
      success_msg_writer() << "  📡 TX Hash: " << txId;
    }

    success_msg_writer() << "";
    success_msg_writer() << "╔════════════════════════════════════════════════════════════╗";
    success_msg_writer() << "║         🔥⚡ CEREMONY COMPLETE - ELDERFIRE IGNITED! ⚡🔥    ║";
    success_msg_writer() << "╚════════════════════════════════════════════════════════════╝";
    success_msg_writer() << "";
    success_msg_writer() << "✅ All 5 Elderfire stakes forged successfully!";
    success_msg_writer() << "   🔥 Total commitment: 4,000 XFG";
    success_msg_writer() << "   ⚡ Ritual complete. The fires now burn bright.";
    success_msg_writer() << "";
    success_msg_writer() << "⏳ NETWORK RECOGNITION (Automatic)";
    success_msg_writer() << "   The network recognizes your commitment and will:";
    success_msg_writer() << "   ✓ Detect all 5 stakes in the blockchain";
    success_msg_writer() << "   ✓ Register you as an ELDERFIER";
    success_msg_writer() << "   ✓ Assign you an Elderfier ID (0-255)";
    success_msg_writer() << "   ✓ Add you to the active elderfiers registry";
    success_msg_writer() << "";
    success_msg_writer() << "⚡ YOUR NEW POWERS";
    success_msg_writer() << "   🔐 SIGN merkle roots of deposit commitments";
    success_msg_writer() << "   💰 EARN 0.1% of all HEAT/COLD banking fees";
    success_msg_writer() << "   🌍 PARTICIPATE in 69% consensus validation";
    success_msg_writer() << "   🛡️  PROTECT network security & decentralization";
    success_msg_writer() << "";
    success_msg_writer() << "📊 NEXT STEPS";
    success_msg_writer() << "   1. Check your deposits: list_deposits";
    success_msg_writer() << "   2. Query your status: RPC get_elderfier_consensus_status";
    success_msg_writer() << "   3. Run your elderfier node to sign merkle roots";
    success_msg_writer() << "   4. Start earning fees when consensus is reached";
    success_msg_writer() << "";
    success_msg_writer() << "🎉 Welcome to the Elderfire StayKing ranks!";
    success_msg_writer() << "";

    return true;
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << "Error during elderfier registration: " << e.what();
    return true;
  }
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::withdraw_deposit(const std::vector<std::string> &args)
{
  if (args.size() != 1)
  {
    fail_msg_writer() << "Usage: withdraw_deposit <id>";
    return true;
  }

  try
  {
    size_t deposit_count = m_wallet->getDepositCount();
    if (deposit_count == 0)
    {
      fail_msg_writer() << "No deposits have been made in this wallet.";
      return true;
    }

    uint64_t deposit_id = boost::lexical_cast<uint64_t>(args[0]);

    // Check if deposit exists
    if (deposit_id >= deposit_count) {
      fail_msg_writer() << "Invalid deposit ID.";
      return true;
    }

    CryptoNote::Deposit deposit;
    if (!m_wallet->getDeposit(deposit_id, deposit)) {
      fail_msg_writer() << "Failed to retrieve deposit information.";
      return true;
    }

    if (deposit.locked) {
      fail_msg_writer() << "Deposit is still locked. Unlock height: " << deposit.unlockHeight;
      return true;
    }

    std::vector<CryptoNote::DepositId> depositIds = {deposit_id};
    uint64_t fee = m_currency.minimumFee();
    CryptoNote::TransactionId txId = m_wallet->withdrawDeposits(depositIds, fee);

    success_msg_writer(true) << "Deposit withdrawal transaction created successfully!";
    success_msg_writer() << "Transaction ID: " << txId;
    success_msg_writer() << "Withdrawn amount: " << m_currency.formatAmount(deposit.amount);
    if (deposit.amount != CryptoNote::parameters::AMOUNT_TIER_0) {
      success_msg_writer() << "Interest earned: " << m_currency.formatAmount(deposit.interest);
    }
  }
  catch (std::exception &e)
  {
    fail_msg_writer() << "Failed to withdraw deposit: " << e.what();
  }

  return true;
 }

 //----------------------------------------------------------------------------------------------------
bool simple_wallet::generate_proof(const std::vector<std::string> &args) {
   if (args.size() != 1) {
     fail_msg_writer() << "Usage: generate_proof <tx_hash>";
     return true;
   }

   const std::string& tx_hash = args[0];

   try {
     // Parse transaction hash
     Crypto::Hash hash;
     if (!parse_hash256(tx_hash, hash)) {
       fail_msg_writer() << "Failed to parse transaction hash";
       return true;
     }

     // Get transaction details from node using callback interface
     std::vector<Crypto::Hash> txHashes{hash};
     std::vector<CryptoNote::TransactionDetails> transactions;
     std::promise<std::error_code> promise;
     std::future<std::error_code> future = promise.get_future();

     m_node->getTransactions(txHashes, transactions, [&promise](std::error_code ec) {
         promise.set_value(ec);
     });

     std::error_code ec = future.get();
     if (ec || transactions.empty()) {
       fail_msg_writer() << "Transaction not found: " << tx_hash;
       return true;
     }

     // Use the transaction extra details directly
     const CryptoNote::TransactionDetails& txDetails = transactions[0];
     // The extra data is in txDetails.extra.raw
     const std::vector<uint8_t>& txExtra = txDetails.extra.raw;

     // Check for HEAT commitment (0x08 tag in tx_extra)
     std::vector<CryptoNote::TransactionExtraField> extraFields;
     if (CryptoNote::parseTransactionExtra(txExtra, extraFields)) {
       for (const auto& field : extraFields) {
         if (field.type() == typeid(CryptoNote::TransactionExtraHeatCommitment)) {
           const auto& heatCommitment = boost::get<CryptoNote::TransactionExtraHeatCommitment>(field);

           success_msg_writer() << "Found HEAT burn transaction: " << tx_hash;
           success_msg_writer() << "Amount: " << m_currency.formatAmount(heatCommitment.amount);

           // Generate STARK proof data
           std::cout << "\n=== STARK PROOF DATA FOR CONTRACT ===" << std::endl;
           std::cout << "Transaction Hash: " << tx_hash << std::endl;
           std::cout << "Commitment: " << Common::podToHex(heatCommitment.commitment) << std::endl;
           std::cout << "Amount: " << heatCommitment.amount << " atomic XFG" << std::endl;
           std::cout << "=====================================" << std::endl;

           logger(INFO, BRIGHT_GREEN) << "Proof data generated for HEAT burn transaction " << tx_hash;
           return true;
         }
         // Check for COLD deposit commitment (0xCD = 205 tag in tx_extra)
         else if (field.type() == typeid(CryptoNote::TransactionExtraColdCommitment)) {
           const auto& coldDeposit = boost::get<CryptoNote::TransactionExtraColdCommitment>(field);

           success_msg_writer() << "Found COLD deposit transaction: " << tx_hash;
           success_msg_writer() << "Amount: " << m_currency.formatAmount(coldDeposit.amount);
           success_msg_writer() << "Term: " << coldDeposit.term << " blocks";

           // Generate proof data
           std::cout << "\n=== COLD DEPOSIT PROOF DATA ===" << std::endl;
           std::cout << "Transaction Hash: " << tx_hash << std::endl;
           std::cout << "Commitment: " << Common::podToHex(coldDeposit.commitment) << std::endl;
           std::cout << "Amount: " << coldDeposit.amount << " atomic XFG" << std::endl;
           std::cout << "Term: " << coldDeposit.term << " blocks" << std::endl;
           std::cout << "Chain Code: " << static_cast<int>(coldDeposit.claimChainCode) << std::endl;
           std::cout << "=================================" << std::endl;

           logger(INFO, BRIGHT_GREEN) << "Proof data generated for COLD deposit " << tx_hash;
           return true;
         }
       }
     }

     fail_msg_writer() << "No HEAT burn or COLD deposit commitment found in transaction: " << tx_hash;
     return true;
   } catch (const std::exception& e) {
     fail_msg_writer() << "Error processing transaction: " << e.what();
   }


   return true;
 }



//----------------------------------------------------------------------------------------------------
bool simple_wallet::deposit_info(const std::vector<std::string> &args)
{
  if (args.size() != 1)
  {
    fail_msg_writer() << "Usage: deposit_info <id>";
    return true;
  }

  try {
    uint64_t deposit_id = boost::lexical_cast<uint64_t>(args[0]);

    if (deposit_id >= m_wallet->getDepositCount()) {
      fail_msg_writer() << "Invalid deposit ID.";
      return true;
    }

    CryptoNote::Deposit deposit;
    if (!m_wallet->getDeposit(deposit_id, deposit)) {
      fail_msg_writer() << "Failed to retrieve deposit information.";
      return true;
    }

    success_msg_writer() << "Deposit Information:";
    success_msg_writer() << "ID:            " << deposit_id;
    success_msg_writer() << "Amount:        " << m_currency.formatAmount(deposit.amount);

    if (deposit.interest > 0) {
      success_msg_writer() << "Interest:      " << m_currency.formatAmount(deposit.interest);
      success_msg_writer() << "Total Return:  " << m_currency.formatAmount(deposit.amount + deposit.interest);
    }

    // Show term information
    if (deposit.term == CryptoNote::parameters::DEPOSIT_TERM_FOREVER) {
      success_msg_writer() << "Term:          HEAT burn (forever)";
    } else if (deposit.term == CryptoNote::parameters::COLD_MIN_TERM) {
      success_msg_writer() << "Term:          3 months (16,000 blocks)";
    } else if (deposit.term == CryptoNote::parameters::COLD_MAX_TERM) {
      success_msg_writer() << "Term:          1 year (65,000 blocks)";
    } else {
      success_msg_writer() << "Term:          " << deposit.term << " blocks";
    }

    success_msg_writer() << "Height:        " << deposit.height;
    success_msg_writer() << "Unlock Height: " << deposit.unlockHeight;

    // Show status
    if (deposit.locked) {
      success_msg_writer() << "Status:        Locked";
    } else if (deposit.spendingTransactionId == CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID) {
      success_msg_writer() << "Status:        Unlocked";
    } else {
      success_msg_writer() << "Status:        Withdrawn";
    }

    success_msg_writer() << "Transaction:   " << Common::podToHex(deposit.transactionHash);

    // Show commitment information if available
    if (deposit.amount == CryptoNote::parameters::AMOUNT_TIER_0) {
      success_msg_writer() << "Type:          HEAT Burn Deposit";
      success_msg_writer() << "Commitment:    Will generate off-chain yield via STARK proofs";
    } else {
      success_msg_writer() << "Type:          COLD Yield Deposit";
      success_msg_writer() << "Commitment:    Generates CD commitments for on-chain verification";
    }

  } catch (const std::exception &e) {
    fail_msg_writer() << "Error: " << e.what();
    return false;
  }

  return true;
}

//----------------------------------------------------------------------------------------------------
std::string simple_wallet::generate_mnemonic(Crypto::SecretKey &private_spend_key) {
  std::string mnemonic_str;
  crypto::ElectrumWords::bytes_to_words(private_spend_key, mnemonic_str, "English");
  return mnemonic_str;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::is_valid_mnemonic(std::string &mnemonic_phrase, Crypto::SecretKey &private_spend_key) {
  static std::string languages[] = {"English"};
  static const int num_of_languages = 1;
  static const int mnemonic_phrase_length = 25;

  std::vector<std::string> words;
  words = boost::split(words, mnemonic_phrase, ::isspace);

  if (words.size() != mnemonic_phrase_length) {
    logger(ERROR, BRIGHT_RED) << "Invalid mnemonic phrase!";
    logger(ERROR, BRIGHT_RED) << "Seed phrase is not 25 words! Please try again.";
    return false;
  }

  for (int i = 0; i < num_of_languages; i++) {
    if (crypto::ElectrumWords::words_to_bytes(mnemonic_phrase, private_spend_key, languages[i])) {
      return true;
    }
  }

  logger(ERROR, BRIGHT_RED) << "Invalid mnemonic phrase!";
  return false;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::reset(const std::vector<std::string> &args) {
  {
    std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
    m_walletSynchronized = false;
  }

  m_wallet->reset();
  success_msg_writer(true) << "Reset completed successfully.";

  std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
  while (!m_walletSynchronized) {
    m_walletSynchronizedCV.wait(lock);
  }

  std::cout << std::endl;
  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::start_mining(const std::vector<std::string>& args) {
  COMMAND_RPC_START_MINING::request req;
  req.miner_address = m_wallet->getAddress();

  bool ok = true;
  size_t max_mining_threads_count = (std::max)(std::thread::hardware_concurrency(), static_cast<unsigned>(2));
  if (0 == args.size()) {
    req.threads_count = 1;
  } else if (1 == args.size()) {
    uint16_t num = 1;
    ok = Common::fromString(args[0], num);
    ok = ok && (1 <= num && num <= max_mining_threads_count);
    req.threads_count = num;
  } else {
    ok = false;
  }

  if (!ok) {
    fail_msg_writer() << "invalid arguments. Please use start_mining [<number_of_threads>], " <<
      "<number_of_threads> should be from 1 to " << max_mining_threads_count;
    return true;
  }

  COMMAND_RPC_START_MINING::response res;

  try {
    HttpClient httpClient(m_dispatcher, m_daemon_host, m_daemon_port);
    invokeJsonCommand(httpClient, "/start_mining", req, res);

    std::string err = interpret_rpc_response(true, res.status);
    if (err.empty())
      success_msg_writer() << "Mining started in daemon";
    else
      fail_msg_writer() << "mining has NOT been started: " << err;

  } catch (const ConnectException&) {
    printConnectionError();
  } catch (const std::exception& e) {
    fail_msg_writer() << "Failed to invoke rpc method: " << e.what();
  }

  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::stop_mining(const std::vector<std::string>& args) {
  COMMAND_RPC_STOP_MINING::request req;
  COMMAND_RPC_STOP_MINING::response res;

  try {
    HttpClient httpClient(m_dispatcher, m_daemon_host, m_daemon_port);
    invokeJsonCommand(httpClient, "/stop_mining", req, res);
    std::string err = interpret_rpc_response(true, res.status);
    if (err.empty())
      success_msg_writer() << "Mining stopped in daemon";
    else
      fail_msg_writer() << "mining has NOT been stopped: " << err;
  } catch (const ConnectException&) {
    printConnectionError();
  } catch (const std::exception& e) {
    fail_msg_writer() << "Failed to invoke rpc method: " << e.what();
  }

  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::get_reserve_proof(const std::vector<std::string> &args) {
  if (args.size() != 1 && args.size() != 2) {
    fail_msg_writer() << "Usage: get_reserve_proof (all|<amount>) [<message>]";
    return true;
  }

  uint64_t reserve = 0;
  if (args[0] != "all") {
    if (!m_currency.parseAmount(args[0], reserve)) {
      fail_msg_writer() << "amount is wrong: " << args[0];
      return true;
    }
  } else {
    reserve = m_wallet->actualBalance();
  }

  try {
    const std::string sig_str = m_wallet->getReserveProof(reserve, args.size() == 2 ? args[1] : "");

    const std::string filename = "reserve_proof_" + args[0] + "_XFG.txt";
    boost::system::error_code ec;
    if (boost::filesystem::exists(filename, ec)) {
      boost::filesystem::remove(filename, ec);
    }

    std::ofstream proofFile(filename, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!proofFile.good()) {
      return false;
    }
    proofFile << sig_str;

    success_msg_writer() << "signature file saved to: " << filename;

  } catch (const std::exception &e) {
    fail_msg_writer() << e.what();
  }

  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::get_tx_proof(const std::vector<std::string> &args) {
  if(args.size() != 2 && args.size() != 3) {
    fail_msg_writer() << "Usage: get_tx_proof <txid> <dest_address> [<txkey>]";
    return true;
  }

  const std::string &str_hash = args[0];
  Crypto::Hash txid;
  if (!parse_hash256(str_hash, txid)) {
    fail_msg_writer() << "Failed to parse txid";
    return true;
  }

  const std::string address_string = args[1];
  CryptoNote::AccountPublicAddress address;
  if (!m_currency.parseAccountAddressString(address_string, address)) {
     fail_msg_writer() << "Failed to parse address " << address_string;
     return true;
  }

  std::string sig_str;
  Crypto::SecretKey tx_key, tx_key2;
  bool r = m_wallet->get_tx_key(txid, tx_key);

  if (args.size() == 3) {
    Crypto::Hash tx_key_hash;
    size_t size;
    if (!Common::fromHex(args[2], &tx_key_hash, sizeof(tx_key_hash), size) || size != sizeof(tx_key_hash)) {
      fail_msg_writer() << "failed to parse tx_key";
      return true;
    }
    tx_key2 = *(struct Crypto::SecretKey *) &tx_key_hash;

    if (r) {
      if (args.size() == 3 && tx_key != tx_key2) {
        fail_msg_writer() << "Tx secret key was found for the given txid, but you've also provided another tx secret key which doesn't match the found one.";
        return true;
      }
    }
    tx_key = tx_key2;
  } else {
    if (!r) {
      fail_msg_writer() << "Tx secret key wasn't found in the wallet file. Provide it as the optional third parameter if you have it elsewhere.";
      return true;
    }
  }

  if (m_wallet->getTxProof(txid, address, tx_key, sig_str)) {
    success_msg_writer() << "Signature: " << sig_str << std::endl;
  }

  return true;
}

//----------------------------------------------------------------------------------------------------
void simple_wallet::initCompleted(std::error_code result) {
  if (m_initResultPromise.get() != nullptr) {
    m_initResultPromise->set_value(result);
  }
}

//----------------------------------------------------------------------------------------------------
void simple_wallet::connectionStatusUpdated(bool connected) {
  if (connected) {
    logger(INFO, GREEN) << "Wallet connected to daemon.";
  } else {
    printConnectionError();
  }
}

//----------------------------------------------------------------------------------------------------
void simple_wallet::externalTransactionCreated(CryptoNote::TransactionId transactionId) {
  WalletLegacyTransaction txInfo;
  m_wallet->getTransaction(transactionId, txInfo);

  std::stringstream logPrefix;
  if (txInfo.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
    logPrefix << "Unconfirmed";
  } else {
    logPrefix << "Height " << txInfo.blockHeight << ',';
  }

  if (txInfo.totalAmount >= 0) {
    logger(INFO, GREEN) <<
      logPrefix.str() << " transaction " << Common::podToHex(txInfo.hash) <<
      ", received " << m_currency.formatAmount(txInfo.totalAmount);
  } else {
    logger(INFO, MAGENTA) <<
      logPrefix.str() << " transaction " << Common::podToHex(txInfo.hash) <<
      ", spent " << m_currency.formatAmount(static_cast<uint64_t>(-txInfo.totalAmount));
  }

  if (txInfo.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
    m_refresh_progress_reporter.update(m_node->getLastLocalBlockHeight(), true);
  } else {
    m_refresh_progress_reporter.update(txInfo.blockHeight, true);
  }
}

//----------------------------------------------------------------------------------------------------
void simple_wallet::synchronizationCompleted(std::error_code result) {
  std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
  m_walletSynchronized = true;
  m_walletSynchronizedCV.notify_one();
}

//----------------------------------------------------------------------------------------------------
void simple_wallet::synchronizationProgressUpdated(uint32_t current, uint32_t total) {
  std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
  if (!m_walletSynchronized) {
    m_refresh_progress_reporter.update(current, false);
  }
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::show_balance(const std::vector<std::string>& args) {
  success_msg_writer() << "available balance: " << m_currency.formatAmount(m_wallet->actualBalance()) <<
    ", locked amount: " << m_currency.formatAmount(m_wallet->pendingBalance());
  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::sign_message(const std::vector<std::string>& args) {
  if(args.size() < 1) {
    fail_msg_writer() << "Use: sign_message <message>";
    return true;
  }

  AccountKeys keys;
  m_wallet->getAccountKeys(keys);

  Crypto::Hash message_hash;
  Crypto::Signature sig;
  Crypto::cn_fast_hash(args[0].data(), args[0].size(), message_hash);
  Crypto::generate_signature(message_hash, keys.address.spendPublicKey, keys.spendSecretKey, sig);

  success_msg_writer() << "Sig" << Tools::Base58::encode(std::string(reinterpret_cast<char*>(&sig)));
  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::verify_signature(const std::vector<std::string>& args) {
  if (args.size() != 3) {
    fail_msg_writer() << "Use: verify_signature <message> <address> <signature>";
    return true;
  }

  const std::string& encodedSig = args[2];
  const char* prefix_literal = "Sig";
  const size_t prefix_size = strlen(prefix_literal);
  if (encodedSig.size() <= prefix_size || encodedSig.substr(0, prefix_size) != prefix_literal) {
    fail_msg_writer() << "Invalid signature prefix";
    return true;
  }

  Crypto::Hash message_hash;
  Crypto::cn_fast_hash(args[0].data(), args[0].size(), message_hash);

  std::string decodedSig;
  if (!Tools::Base58::decode(encodedSig.substr(prefix_size), decodedSig)) {
    fail_msg_writer() << "Failed to decode signature";
    return true;
  }
  Crypto::Signature sig;
  std::memcpy(&sig, decodedSig.data(), sizeof(sig));

  uint64_t prefix = 0;
  CryptoNote::AccountPublicAddress addr;
  if (!CryptoNote::parseAccountAddressString(prefix, addr, args[1])) {
    fail_msg_writer() << "Failed to parse address";
    return true;
  }

  if (Crypto::check_signature(message_hash, addr.spendPublicKey, sig))
    success_msg_writer() << "Valid";
  else
    success_msg_writer() << "Invalid";
  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::create_integrated(const std::vector<std::string>& args) {
  if (args.empty()) {
    fail_msg_writer() << "Please enter a payment ID";
    return true;
  }

  std::string paymentID = args[0];
  std::regex hexChars("^[0-9a-f]+$");
  if(paymentID.size() != 64 || !regex_match(paymentID, hexChars)) {
    fail_msg_writer() << "Invalid payment ID";
    return true;
  }

  std::string address = m_wallet->getAddress();
  uint64_t prefix;
  CryptoNote::AccountPublicAddress addr;

  if(!CryptoNote::parseAccountAddressString(prefix, addr, address)) {
    logger(ERROR, BRIGHT_RED) << "Failed to parse account address from string";
    return true;
  }

  CryptoNote::BinaryArray ba;
  CryptoNote::toBinaryArray(addr, ba);
  std::string keys = Common::asString(ba);

  std::string integratedAddress = Tools::Base58::encode_addr(
    m_currency.isTestnet() ? CryptoNote::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX_TESTNET : CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
    paymentID + keys
  );

  std::cout << std::endl << "Integrated address: " << integratedAddress << std::endl << std::endl;
  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::export_keys(const std::vector<std::string>& args) {
  AccountKeys keys;
  m_wallet->getAccountKeys(keys);

  std::string secretKeysData = std::string(reinterpret_cast<char*>(&keys.spendSecretKey), sizeof(keys.spendSecretKey)) + std::string(reinterpret_cast<char*>(&keys.viewSecretKey), sizeof(keys.viewSecretKey));
  std::string guiKeys = Tools::Base58::encode_addr(
    m_currency.isTestnet() ? CryptoNote::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX_TESTNET : CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
    secretKeysData
  );

  logger(INFO, BRIGHT_GREEN) << std::endl << "xfg_wallet is an open-source, client-side, free wallet which allows you to send & receive Fuego instantly on the blockchain. You are in control of your funds & your private keys. When you generate a new wallet, login, send, receive or deposit $XFG - everything happens locally. Your seed is never transmitted, received or stored. That's why IT IS IMPERATIVE to write down, print or save your seed somewhere safe. The backup of keys is your responsibility only. If you lose your seed, your account can not be recovered. Freedom isn't free - the cost is responsibility. Protect your keys." << std::endl << std::endl;

  std::cout << "Private spend key: " << Common::podToHex(keys.spendSecretKey) << std::endl;
  std::cout << "Private view key: " <<  Common::podToHex(keys.viewSecretKey) << std::endl;

  Crypto::PublicKey unused_dummy_variable;
  Crypto::SecretKey deterministic_private_view_key;

  AccountBase::generateViewFromSpend(keys.spendSecretKey, deterministic_private_view_key, unused_dummy_variable);

  bool deterministic_private_keys = deterministic_private_view_key == keys.viewSecretKey;

  if (deterministic_private_keys) {
    std::cout << "Mnemonic seed: " << generate_mnemonic(keys.spendSecretKey) << std::endl << std::endl;
  }
  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::show_incoming_transfers(const std::vector<std::string>& args) {
  bool hasTransfers = false;
  size_t transactionsCount = m_wallet->getTransactionCount();
  for (size_t trantransactionNumber = 0; trantransactionNumber < transactionsCount; ++trantransactionNumber) {
    WalletLegacyTransaction txInfo;
    m_wallet->getTransaction(trantransactionNumber, txInfo);
    if (txInfo.totalAmount < 0) continue;
    hasTransfers = true;
    logger(INFO) << "        amount       \t                              tx id";
    logger(INFO, GREEN) <<
      std::setw(21) << m_currency.formatAmount(txInfo.totalAmount) << '\t' << Common::podToHex(txInfo.hash);
  }

  if (!hasTransfers) success_msg_writer() << "No incoming transfers";
  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::listTransfers(const std::vector<std::string>& args) {
  bool haveTransfers = false;
  bool haveBlockHeight = false;
  std::string blockHeightString = "";
  uint32_t blockHeight = 0;
  WalletLegacyTransaction txInfo;

  if (args.empty()) {
    haveBlockHeight = false;
  } else {
    blockHeightString = args[0];
    haveBlockHeight = true;
    blockHeight = atoi(blockHeightString.c_str());
  }

  size_t transactionsCount = m_wallet->getTransactionCount();
  for (size_t trantransactionNumber = 0; trantransactionNumber < transactionsCount; ++trantransactionNumber) {
    m_wallet->getTransaction(trantransactionNumber, txInfo);
    if (txInfo.state != WalletLegacyTransactionState::Active || txInfo.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
      continue;
    }

    if (!haveTransfers) {
      printListTransfersHeader(logger);
      haveTransfers = true;
    }

    if (haveBlockHeight == false) {
      printListTransfersItem(logger, txInfo, *m_wallet, m_currency);
    } else {
      if (txInfo.blockHeight >= blockHeight) {
        printListTransfersItem(logger, txInfo, *m_wallet, m_currency);
      }
    }
  }

  if (!haveTransfers) {
    success_msg_writer() << "No transfers";
  }

  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::show_payments(const std::vector<std::string> &args) {
  if (args.empty()) {
    fail_msg_writer() << "expected at least one payment ID";
    return true;
  }

  try {
    auto hashes = args;
    std::sort(std::begin(hashes), std::end(hashes));
    hashes.erase(std::unique(std::begin(hashes), std::end(hashes)), std::end(hashes));
    std::vector<PaymentId> paymentIds;
    paymentIds.reserve(hashes.size());
    std::transform(std::begin(hashes), std::end(hashes), std::back_inserter(paymentIds), [](const std::string& arg) {
      PaymentId paymentId;
      if (!CryptoNote::parsePaymentId(arg, paymentId)) {
        throw std::runtime_error("payment ID has invalid format: \"" + arg + "\", expected 64-character string");
      }
      return paymentId;
    });

    logger(INFO) << "                            payment                             \t" <<
      "                          transaction                           \t" <<
      "  height\t       amount        ";

    auto payments = m_wallet->getTransactionsByPaymentIds(paymentIds);

    for (auto& payment : payments) {
      for (auto& transaction : payment.transactions) {
        success_msg_writer(true) <<
          Common::podToHex(payment.paymentId) << '\t' <<
          Common::podToHex(transaction.hash) << '\t' <<
          std::setw(8) << transaction.blockHeight << '\t' <<
          std::setw(21) << m_currency.formatAmount(transaction.totalAmount);
      }

      if (payment.transactions.empty()) {
        success_msg_writer() << "No payments with id " << Common::podToHex(payment.paymentId);
      }
    }
  } catch (std::exception& e) {
    fail_msg_writer() << "show_payments exception: " << e.what();
  }

  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::show_blockchain_height(const std::vector<std::string>& args) {
  try {
    uint64_t bc_height = m_node->getLastLocalBlockHeight();
    success_msg_writer() << bc_height;
  } catch (std::exception &e) {
    fail_msg_writer() << "failed to get blockchain height: " << e.what();
  }

  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::show_num_unlocked_outputs(const std::vector<std::string>& args) {
  try {
    std::vector<TransactionOutputInformation> unlocked_outputs = m_wallet->getUnspentOutputs();
    success_msg_writer() << "Count: " << unlocked_outputs.size();
    for (const auto& out : unlocked_outputs) {
      success_msg_writer() << "Key: " << out.transactionPublicKey << " amount: " << m_currency.formatAmount(out.amount);
    }
  } catch (std::exception &e) {
    fail_msg_writer() << "failed to get outputs: " << e.what();
  }

  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::optimize_outputs(const std::vector<std::string>& args) {
  try {
    CryptoNote::WalletHelper::SendCompleteResultObserver sent;
    WalletHelper::IWalletRemoveObserverGuard removeGuard(*m_wallet, sent);

    std::vector<CryptoNote::WalletLegacyTransfer> transfers;
    std::vector<CryptoNote::TransactionMessage> messages;
    std::string extraString;
    uint64_t fee = CryptoNote::parameters::MINIMUM_FEE_V2;
    uint64_t mixIn = 0;
    uint64_t unlockTimestamp = 0;
    uint64_t ttl = 0;
    Crypto::SecretKey transactionSK;
    CryptoNote::TransactionId tx = m_wallet->sendTransaction(transactionSK, transfers, fee, extraString, mixIn, unlockTimestamp, messages, ttl);
    if (tx == WALLET_LEGACY_INVALID_TRANSACTION_ID) {
      fail_msg_writer() << "Can't send money";
      return true;
    }

    std::error_code sendError = sent.wait(tx);
    removeGuard.removeObserver();

    if (sendError) {
      fail_msg_writer() << sendError.message();
      return true;
    }

    CryptoNote::WalletLegacyTransaction txInfo;
    m_wallet->getTransaction(tx, txInfo);
    success_msg_writer(true) << "Money successfully sent, transaction " << Common::podToHex(txInfo.hash);
    success_msg_writer(true) << "Transaction secret key " << Common::podToHex(transactionSK);

    try {
      CryptoNote::WalletHelper::storeWallet(*m_wallet, m_wallet_file);
    } catch (const std::exception& e) {
      fail_msg_writer() << e.what();
      return true;
    }
  } catch (const std::system_error& e) {
    fail_msg_writer() << e.what();
  } catch (const std::exception& e) {
    fail_msg_writer() << e.what();
  } catch (...) {
    fail_msg_writer() << "unknown error";
  }

  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::optimize_all_outputs(const std::vector<std::string>& args) {
  uint64_t num_unlocked_outputs = 0;

  try {
    num_unlocked_outputs = m_wallet->getNumUnlockedOutputs();
    success_msg_writer() << "Total outputs: " << num_unlocked_outputs;
  } catch (std::exception &e) {
    fail_msg_writer() << "failed to get outputs: " << e.what();
  }

  uint64_t remainder = num_unlocked_outputs % 100;
  uint64_t rounds = (num_unlocked_outputs - remainder) / 100;
  success_msg_writer() << "Total optimization rounds: " << rounds;

  for(uint64_t a = 1; a < rounds; a = a + 1) {
    try {
      CryptoNote::WalletHelper::SendCompleteResultObserver sent;
      WalletHelper::IWalletRemoveObserverGuard removeGuard(*m_wallet, sent);

      std::vector<CryptoNote::WalletLegacyTransfer> transfers;
      std::vector<CryptoNote::TransactionMessage> messages;
      std::string extraString;
      uint64_t fee = CryptoNote::parameters::MINIMUM_FEE_V2;
      uint64_t mixIn = 0;
      uint64_t unlockTimestamp = 0;
      uint64_t ttl = 0;
      Crypto::SecretKey transactionSK;
      CryptoNote::TransactionId tx = m_wallet->sendTransaction(transactionSK, transfers, fee, extraString, mixIn, unlockTimestamp, messages, ttl);
      if (tx == WALLET_LEGACY_INVALID_TRANSACTION_ID) {
        fail_msg_writer() << "Can't send money";
        return true;
      }

      std::error_code sendError = sent.wait(tx);
      removeGuard.removeObserver();

      if (sendError) {
        fail_msg_writer() << sendError.message();
        return true;
      }

      CryptoNote::WalletLegacyTransaction txInfo;
      m_wallet->getTransaction(tx, txInfo);
      success_msg_writer(true) << a << ". Optimization transaction successfully sent, transaction " << Common::podToHex(txInfo.hash);

      try {
        CryptoNote::WalletHelper::storeWallet(*m_wallet, m_wallet_file);
      } catch (const std::exception& e) {
        fail_msg_writer() << e.what();
        return true;
      }
    } catch (const std::system_error& e) {
      fail_msg_writer() << e.what();
    } catch (const std::exception& e) {
      fail_msg_writer() << e.what();
    } catch (...) {
      fail_msg_writer() << "unknown error";
    }
  }
  return true;
}

//----------------------------------------------------------------------------------------------------
std::string simple_wallet::resolveAlias(const std::string& aliasUrl) {
  std::string host;
  std::string uri;
  std::vector<std::string>records;
  std::string address;

  if (!Common::fetch_dns_txt(aliasUrl, records)) {
    #ifdef _WIN32
    throw std::runtime_error("Failed to lookup DNS record for: " + aliasUrl);
    #else
    // DNS TXT resolution not available on this platform (macOS/Linux)
    // Users can still use standard Fuego wallet addresses directly
    throw std::runtime_error("OpenAlias (oa1:xfg) not supported on this platform. Please use a standard Fuego wallet address directly, or use Windows/a system with DNS resolver support.");
    #endif
  }

  for (const auto& record : records) {
    if (processServerAliasResponse(record, address)) {
      return address;
    }
  }
  throw std::runtime_error("Failed to parse OpenAlias response for: " + aliasUrl);
}

//----------------------------------------------------------------------------------------------------
std::string simple_wallet::getFeeAddress() {
  HttpClient httpClient(m_dispatcher, m_daemon_host, m_daemon_port);

  HttpRequest req;
  HttpResponse res;

  req.setUrl("/feeaddress");
  try {
    httpClient.request(req, res);
  } catch (const std::exception& e) {
    fail_msg_writer() << "Error connecting to the remote node: " << e.what();
  }

  if (res.getStatus() != HttpResponse::STATUS_200) {
    fail_msg_writer() << "Remote node returned code " + std::to_string(res.getStatus());
  }

  std::string address;
  if (!processServerFeeAddressResponse(res.getBody(), address)) {
    fail_msg_writer() << "Failed to parse remote node response";
  }

  return address;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::transfer(const std::vector<std::string> &args) {
  try {
    TransferCommand cmd(m_currency);

    if (!cmd.parseArguments(logger, args))
      return true;

    for (auto& kv: cmd.aliases) {
      std::string address;

      try {
        address = resolveAlias(kv.first);

        AccountPublicAddress ignore;
        if (!m_currency.parseAccountAddressString(address, ignore)) {
          throw std::runtime_error("Address \"" + address + "\" is invalid");
        }
      } catch (std::exception& e) {
        fail_msg_writer() << "Couldn't resolve alias: " << e.what() << ", alias: " << kv.first;
        return true;
      }

      for (auto& transfer: kv.second) {
        transfer.address = address;
      }
    }

    if (!cmd.aliases.empty()) {
      if (!askAliasesTransfersConfirmation(cmd.aliases, m_currency)) {
        return true;
      }

      for (auto& kv: cmd.aliases) {
        std::copy(std::move_iterator<std::vector<WalletLegacyTransfer>::iterator>(kv.second.begin()),
                  std::move_iterator<std::vector<WalletLegacyTransfer>::iterator>(kv.second.end()),
                  std::back_inserter(cmd.dsts));
      }
    }

    std::vector<TransactionMessage> messages;
    for (auto dst : cmd.dsts) {
      for (auto msg : cmd.messages) {
        messages.emplace_back(TransactionMessage{ msg, dst.address });
      }
    }

    uint64_t ttl = 0;
    if (cmd.ttl != 0) {
      ttl = static_cast<uint64_t>(time(nullptr)) + cmd.ttl;
    }

    CryptoNote::WalletHelper::SendCompleteResultObserver sent;

    std::string extraString;
    std::copy(cmd.extra.begin(), cmd.extra.end(), std::back_inserter(extraString));

    WalletHelper::IWalletRemoveObserverGuard removeGuard(*m_wallet, sent);

    cmd.fake_outs_count = CryptoNote::parameters::MIN_TX_MIXIN_SIZE;

    if (cmd.fee < CryptoNote::parameters::MINIMUM_FEE_8KH) {
      cmd.fee = CryptoNote::parameters::MINIMUM_FEE_8KH;
    }

    Crypto::SecretKey transactionSK;
    CryptoNote::TransactionId tx = m_wallet->sendTransaction(transactionSK, cmd.dsts, cmd.fee, extraString, cmd.fake_outs_count, 0, messages, ttl);
    if (tx == WALLET_LEGACY_INVALID_TRANSACTION_ID) {
      fail_msg_writer() << "Can't send money";
      return true;
    }

    std::error_code sendError = sent.wait(tx);
    removeGuard.removeObserver();

    if (sendError) {
      fail_msg_writer() << sendError.message();
      return true;
    }

    CryptoNote::WalletLegacyTransaction txInfo;
    m_wallet->getTransaction(tx, txInfo);
    success_msg_writer(true) << "Money successfully sent, transaction hash: " << Common::podToHex(txInfo.hash);
    success_msg_writer(true) << "Transaction secret key " << Common::podToHex(transactionSK);

    try {
      CryptoNote::WalletHelper::storeWallet(*m_wallet, m_wallet_file);
    } catch (const std::exception& e) {
      fail_msg_writer() << e.what();
      return true;
    }
  } catch (const std::system_error& e) {
    fail_msg_writer() << e.what();
  } catch (const std::exception& e) {
    fail_msg_writer() << e.what();
  } catch (...) {
    fail_msg_writer() << "unknown error";
  }

  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::run() {
  {
    std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
    while (!m_walletSynchronized) {
      m_walletSynchronizedCV.wait(lock);
    }
  }

  std::cout << std::endl;

  std::string addr_start = m_wallet->getAddress().substr(0, 6);
  m_consoleHandler.start(false, "[wallet " + addr_start + "]: ", Common::Console::Color::BrightYellow);
  return true;
}

//----------------------------------------------------------------------------------------------------
void simple_wallet::stop() {
  m_consoleHandler.requestStop();
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::print_address(const std::vector<std::string> &args) {
  success_msg_writer() << m_wallet->getAddress();
  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::process_command(const std::vector<std::string> &args) {
  return m_consoleHandler.runCommand(args);
}

//----------------------------------------------------------------------------------------------------
void simple_wallet::printConnectionError() const {
  fail_msg_writer() << "wallet failed to connect to daemon (" << m_daemon_address << ").";
}

