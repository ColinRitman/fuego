// Copyright (c) 2017-2025 Fuego Developers
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

#include "SimpleWallet/SimpleWallet.h"
#include "TestnetWallet/TestnetWallet.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <future>
#include <iomanip>
#include <thread>
#include <set>
#include <sstream>
#include <regex>
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

namespace CryptoNote
{
  //----------------------------------------------------------------------------------------------------
  // testnet_wallet constructor - extends simple_wallet with testnet-specific commands
  //----------------------------------------------------------------------------------------------------
  CryptoNote::testnet_wallet::testnet_wallet(System::Dispatcher& dispatcher, const CryptoNote::Currency& currency, Logging::LoggerManager& log) :
    simple_wallet(dispatcher, currency, log)
  {
    // Register testnet-specific deposit commands
    register_testnet_commands();
  }

  //----------------------------------------------------------------------------------------------------
  void CryptoNote::testnet_wallet::register_testnet_commands()
  {
    // Add testnet-specific deposit commands (in addition to inherited ones)
    m_consoleHandler.setHandler("burn", boost::bind(&testnet_wallet::burn, this, boost::arg<1>()), "burn <amount> - Create a HEAT burn deposit (0.8, 8, 80, 800 XFG). Term automatically set to FOREVER.");
    m_consoleHandler.setHandler("cold", boost::bind(&testnet_wallet::cold, this, boost::arg<1>()), "cold <amount> <term_code> - Create a COLD deposit (0.8, 8, 80, 800 XFG with terms 3=3mo, 12=1yr).");
    m_consoleHandler.setHandler("elderking_ceremony", boost::bind(&testnet_wallet::elderking_ceremony, this, boost::arg<1>()), "elderking_ceremony - Register as Elderfier: batch 5x 800 XFG deposits (0xEC tag, 4000 XFG total). Creates elderfier registration commitment.");
  }

  //----------------------------------------------------------------------------------------------------
  // Testnet-specific deposit command implementations
  //----------------------------------------------------------------------------------------------------

  bool CryptoNote::testnet_wallet::burn(const std::vector<std::string> &args)
  {
    // HEAT burn deposit - testnet version
    if (args.size() != 1)
    {
      fail_msg_writer() << "Usage: burn <amount>";
      fail_msg_writer() << "Valid amounts: 0.8, 8, 80, 800 XFG";
      return true;
    }

    try
    {
      uint64_t burn_amount = 0;
      bool ok = m_currency.parseAmount(args[0], burn_amount);

      if (!ok || 0 == burn_amount)
      {
        fail_msg_writer() << "Invalid amount format: " << args[0];
        return true;
      }

      std::vector<uint64_t> valid_amounts = {
        CryptoNote::parameters::AMOUNT_TIER_0,
        CryptoNote::parameters::AMOUNT_TIER_1,
        CryptoNote::parameters::AMOUNT_TIER_2,
        CryptoNote::parameters::AMOUNT_TIER_3
      };

      auto it = std::find(valid_amounts.begin(), valid_amounts.end(), burn_amount);
      if (it == valid_amounts.end()) {
        fail_msg_writer() << "Invalid amount. Valid tiers: 0.8, 8, 80, 800 XFG";
        return true;
      }

      uint32_t burn_term = CryptoNote::parameters::DEPOSIT_TERM_FOREVER;
      uint64_t fee = m_currency.minimumFee();
      std::string extraString = "";

      success_msg_writer() << "Creating HEAT burn deposit: " << m_currency.formatAmount(burn_amount) << " XFG";
      CryptoNote::TransactionId txId = m_wallet->deposit(burn_term, burn_amount, fee, extraString, 0);

      if (CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID == txId) {
        fail_msg_writer() << "Sending deposit transaction failed";
        return true;
      }

      success_msg_writer() << "HEAT burn deposit created. TX ID: " << txId;
      return true;
    }
    catch (const std::exception& e)
    {
      fail_msg_writer() << "Error: " << e.what();
      return true;
    }
  }

  //----------------------------------------------------------------------------------------------------
  bool CryptoNote::testnet_wallet::cold(const std::vector<std::string> &args)
  {
    // COLD deposit - testnet version with term code validation
    if (args.size() != 2)
    {
      fail_msg_writer() << "Usage: cold <amount> <term_code>";
      fail_msg_writer() << "Valid amounts: 0.8, 8, 80, 800 XFG";
      fail_msg_writer() << "Valid term codes: 3 (3 months), 12 (1 year)";
      return true;
    }

    try
    {
      uint64_t cold_amount = 0;
      bool ok = m_currency.parseAmount(args[0], cold_amount);

      if (!ok || 0 == cold_amount)
      {
        fail_msg_writer() << "Invalid amount format: " << args[0];
        return true;
      }

      std::vector<uint64_t> valid_amounts = {
        CryptoNote::parameters::AMOUNT_TIER_0,
        CryptoNote::parameters::AMOUNT_TIER_1,
        CryptoNote::parameters::AMOUNT_TIER_2,
        CryptoNote::parameters::AMOUNT_TIER_3
      };

      auto it = std::find(valid_amounts.begin(), valid_amounts.end(), cold_amount);
      if (it == valid_amounts.end()) {
        fail_msg_writer() << "Invalid amount. Valid tiers: 0.8, 8, 80, 800 XFG";
        return true;
      }

      uint32_t term_code = boost::lexical_cast<uint32_t>(args[1]);
      uint32_t cold_term = 0;
      std::string term_label = "";

      uint32_t min_term = CryptoNote::parameters::TESTNET_COLD_MIN_TERM;
      uint32_t max_term = CryptoNote::parameters::TESTNET_COLD_MAX_TERM;

      if (term_code == 3) {
        cold_term = min_term;
        term_label = "3 months";
      } else if (term_code == 12) {
        cold_term = max_term;
        term_label = "1 year";
      } else {
        fail_msg_writer() << "Invalid term code. Use: 3 (3 months) or 12 (1 year)";
        return true;
      }

      uint64_t fee = m_currency.minimumFee();
      std::string extraString = "";

      success_msg_writer() << "Creating COLD deposit: " << m_currency.formatAmount(cold_amount) << " XFG for " << term_label;
      CryptoNote::TransactionId txId = m_wallet->deposit(cold_term, cold_amount, fee, extraString, 0);

      if (CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID == txId) {
        fail_msg_writer() << "Sending deposit transaction failed";
        return true;
      }

      success_msg_writer() << "COLD deposit created. TX ID: " << txId;
      return true;
    }
    catch (const std::exception& e)
    {
      fail_msg_writer() << "Error: " << e.what();
      return true;
    }
  }

  //----------------------------------------------------------------------------------------------------
  bool CryptoNote::testnet_wallet::elderking_ceremony(const std::vector<std::string> &args)
  {
    // Testnet elderfier registration ceremony
    if (args.size() != 0)
    {
      fail_msg_writer() << "Usage: elderking_ceremony";
      return true;
    }

    try
    {
      success_msg_writer() << "";
      success_msg_writer() << "╔════════════════════════════════════════════════════════════╗";
      success_msg_writer() << "║            🔥⚡  ELDERFIRE STAYKING CEREMONY  ⚡🔥          ║";
      success_msg_writer() << "║                    (TESTNET ELDERFIER REGISTRATION)         ║";
      success_msg_writer() << "╚════════════════════════════════════════════════════════════╝";
      success_msg_writer() << "";
      success_msg_writer() << "Testing elderfier registration on testnet!";
      success_msg_writer() << "This creates 5 deposits of 800 XFG each (4000 XFG total)";
      success_msg_writer() << "";

      uint64_t balance = m_wallet->actualBalance();
      uint64_t required = 4000 * CryptoNote::parameters::COIN;
      uint64_t fee = m_currency.minimumFee();

      success_msg_writer() << "Balance: " << m_currency.formatAmount(balance) << " XFG";
      success_msg_writer() << "Required: " << m_currency.formatAmount(required + (5 * fee)) << " XFG";
      success_msg_writer() << "";

      if (balance < required + (5 * fee)) {
        fail_msg_writer() << "Insufficient balance for ceremony.";
        return true;
      }

      std::string confirm;
      success_msg_writer() << "⚡ Type 'IGNITE' to begin ceremony: ";
      std::getline(std::cin, confirm);

      if (confirm != "IGNITE") {
        success_msg_writer() << "Ceremony cancelled.";
        return true;
      }

      uint64_t amount_per_deposit = 800 * CryptoNote::parameters::COIN;
      success_msg_writer() << "";
      success_msg_writer() << "🔥 Creating 5 elderfier stakes...";
      success_msg_writer() << "";

      for (int i = 0; i < 5; ++i) {
        success_msg_writer() << "Ritual " << (i + 1) << " of 5: Creating 800 XFG stake...";

        std::vector<uint8_t> extra;
        std::string extraString = "";

        Crypto::PublicKey public_key;
        Crypto::SecretKey secret_key;
        Crypto::generate_keys(public_key, secret_key);
        Crypto::Hash commitment_hash = Crypto::cn_fast_hash(public_key.data, sizeof(public_key.data));

        CryptoNote::TransactionExtraElderfierDeposit elderfierDeposit;
        elderfierDeposit.depositHash = commitment_hash;
        elderfierDeposit.depositAmount = amount_per_deposit;
        elderfierDeposit.elderfierAddress = "";
        elderfierDeposit.securityWindow = 28800;
        elderfierDeposit.metadata.clear();
        elderfierDeposit.signature.clear();
        elderfierDeposit.isSlashable = true;

        CryptoNote::addElderfierDepositToExtra(extra, elderfierDeposit);

        CryptoNote::TransactionId txId = m_wallet->deposit(
          CryptoNote::parameters::DEPOSIT_TERM_FOREVER,
          amount_per_deposit,
          fee,
          extraString,
          0
        );

        if (CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID == txId) {
          fail_msg_writer() << "Failed at ritual " << (i + 1) << " of 5";
          return true;
        }

        success_msg_writer() << "✨ Stake " << (i + 1) << " forged! TX ID: " << txId;
      }

      success_msg_writer() << "";
      success_msg_writer() << "╔════════════════════════════════════════════════════════════╗";
      success_msg_writer() << "║         🔥⚡ TESTNET CEREMONY COMPLETE! ⚡🔥               ║";
      success_msg_writer() << "╚════════════════════════════════════════════════════════════╝";
      success_msg_writer() << "";
      success_msg_writer() << "✅ All 5 elderfier stakes created (4000 XFG total)";
      success_msg_writer() << "🎉 Ready for elderfier testing on testnet!";
      success_msg_writer() << "";

      return true;
    }
    catch (const std::exception& e)
    {
      fail_msg_writer() << "Error during ceremony: " << e.what();
      return true;
    }
  }
}
