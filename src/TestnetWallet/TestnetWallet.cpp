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
    m_consoleHandler.setHandler("burn", boost::bind(&testnet_wallet::burn, this, boost::arg<1>()), "burn <amount> - Create a HEAT burn (0.8, 8, 80, 800 TEST)");
    m_consoleHandler.setHandler("cold", boost::bind(&testnet_wallet::cold, this, boost::arg<1>()), "cold <amount> <term_code> - Create a Certificate of Ledger Deposit (0.8, 8, 80, 800 TEST with terms 3 (3months) or 12 (1yr)");
    m_consoleHandler.setHandler("elderking_ceremony", boost::bind(&testnet_wallet::elderking_ceremony, this, boost::arg<1>()), "elderking_ceremony <ALIAS> - Register as Testifier with alias [A-Z0-9&]: batch 5x 80 TEST deposits (0xEF tag, 400 TEST total).");
    m_consoleHandler.setHandler("list_burns", boost::bind(&testnet_wallet::list_burns, this, boost::arg<1>()), "list_burns - List all burn transactions.");

    // @ Alias system commands (inherited from simple_wallet)
    m_consoleHandler.setHandler("register_alias", boost::bind(&testnet_wallet::register_alias, this, boost::arg<1>()), "register_alias <alias> - Register a TEST alias (8 chars ONLY: [A-Z0-9] (CAPS-LOCK) req'd for Elderfiers, [a-z0-9] (lowercase) req'd for regular user wallets)");
    m_consoleHandler.setHandler("lookup_alias", boost::bind(&testnet_wallet::lookup_alias, this, boost::arg<1>()), "lookup_alias <alias_or_address> - Look up a TEST alias by name or wallet address");
    m_consoleHandler.setHandler("list_aliases", boost::bind(&testnet_wallet::list_aliases, this, boost::arg<1>()), "list_aliases - List all registered TEST aliases on the network");
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
      fail_msg_writer() << "Valid amounts: 0.08, 0.8, 8, 80 TEST";
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
        CryptoNote::parameters::TEST_AMOUNT_TIER_0,
        CryptoNote::parameters::TEST_AMOUNT_TIER_1,
        CryptoNote::parameters::TEST_AMOUNT_TIER_2,
        CryptoNote::parameters::TEST_AMOUNT_TIER_3
      };

      auto it = std::find(valid_amounts.begin(), valid_amounts.end(), burn_amount);
      if (it == valid_amounts.end()) {
        fail_msg_writer() << "Invalid amount. Valid tiers: 0.08, 0.8, 8, 80 TEST";
        return true;
      }

      uint32_t burn_term = CryptoNote::parameters::DEPOSIT_TERM_FOREVER;

      // Determine banking fee based on amount tier
      uint64_t banking_fee = 0;
      if (burn_amount == CryptoNote::parameters::TEST_AMOUNT_TIER_0) {
        banking_fee = CryptoNote::parameters::BANK_FEE_TIER_0;
      } else if (burn_amount == CryptoNote::parameters::TEST_AMOUNT_TIER_1) {
        banking_fee = CryptoNote::parameters::BANK_FEE_TIER_1;
      } else if (burn_amount == CryptoNote::parameters::TEST_AMOUNT_TIER_2) {
        banking_fee = CryptoNote::parameters::BANK_FEE_TIER_2;
      } else if (burn_amount == CryptoNote::parameters::TEST_AMOUNT_TIER_3) {
        banking_fee = CryptoNote::parameters::BANK_FEE_TIER_3;
      }
      uint64_t fee = m_currency.minimumFee();

      // Confirmation
      success_msg_writer() << "";
      success_msg_writer() << "TESTNET Burn Transaction Summary:";
      success_msg_writer() << "  Amount: " << m_currency.formatAmount(burn_amount) << " TEST (PERMANENT)";
      success_msg_writer() << "  Banking Fee: " << m_currency.formatAmount(banking_fee) << " TEST (0.1% of amount to Elderfiers)";
      success_msg_writer() << "  Network Fee: " << m_currency.formatAmount(fee) << " TEST (minimum txn fee to miners)";
      success_msg_writer() << "  Commitment Type: 〘HEAT〙 ✺ These funds will be BURNED (to mint HEAT)";
      success_msg_writer() << "";
      success_msg_writer() << "Confirm? (1) OK  (2) No ";

      std::string confirm;
      std::getline(std::cin, confirm);

      if (confirm != "1" && confirm != "OK" && confirm != "Ok" && confirm != "ok") {
        success_msg_writer() << "Cancelled.";
        return true;
      }

      // Create HEAT commitment for burn (0x08 tag)
      std::vector<uint8_t> extra;
      Crypto::PublicKey pubkey;
      Crypto::SecretKey seckey;
      Crypto::generate_keys(pubkey, seckey);
      Crypto::Hash heatCommit = Crypto::cn_fast_hash(pubkey.data, sizeof(pubkey.data));

      CryptoNote::TransactionExtraHeatCommitment heatCommitment;
      heatCommitment.commitment = heatCommit;
      heatCommitment.amount = burn_amount;
      heatCommitment.metadata = {0x08};  // Tag 0x08 for HEAT

      CryptoNote::addHeatCommitmentToExtra(extra, heatCommitment);
      std::string extraString = std::string(extra.begin(), extra.end());

      success_msg_writer() << "Creating TEST burn (HEAT): " << m_currency.formatAmount(burn_amount) << " TEST";
      CryptoNote::TransactionId txId = m_wallet->deposit(burn_term, burn_amount, fee, extraString, 0);

      if (CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID == txId) {
        fail_msg_writer() << "Sending deposit transaction failed";
        return true;
      }

      success_msg_writer() << "TEST burn transaction created. TX ID: " << txId;
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
      fail_msg_writer() << "Valid amounts: 0.08, 0.8, 8, 80 TEST";
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
        CryptoNote::parameters::TEST_AMOUNT_TIER_0,
        CryptoNote::parameters::TEST_AMOUNT_TIER_1,
        CryptoNote::parameters::TEST_AMOUNT_TIER_2,
        CryptoNote::parameters::TEST_AMOUNT_TIER_3
      };

      auto it = std::find(valid_amounts.begin(), valid_amounts.end(), cold_amount);
      if (it == valid_amounts.end()) {
        fail_msg_writer() << "Invalid amount. Valid tiers: 0.08, 0.8, 8, 80 TEST";
        return true;
      }

      uint32_t term_code = boost::lexical_cast<uint32_t>(args[1]);
      uint32_t cold_term = 0;
      std::string term_label = "";

      uint32_t min_term = CryptoNote::parameters::TESTNET_COLD_MIN_TERM;
      uint32_t max_term = CryptoNote::parameters::TESTNET_COLD_MAX_TERM;

      // Map term codes to valid testnet terms
      if (term_code == 3) {
        // For testnet, use a term that's within valid range
        cold_term = min_term;  // 16 blocks for shortest term
        term_label = "3 months (testnet: 16 blocks)";
      } else if (term_code == 12) {
        // For testnet, use a term that's within valid range
        cold_term = max_term;  // 65 blocks for longest term
        term_label = "1 year (testnet: 65 blocks)";
      } else {
        fail_msg_writer() << "Invalid term code. Use: 3 (3 months) or 12 (1 year)";
        return true;
      }

      // Determine banking fee based on amount tier
      uint64_t banking_fee = 0;
      if (cold_amount == CryptoNote::parameters::TEST_AMOUNT_TIER_0) {
        banking_fee = CryptoNote::parameters::BANK_FEE_TIER_0;
      } else if (cold_amount == CryptoNote::parameters::TEST_AMOUNT_TIER_1) {
        banking_fee = CryptoNote::parameters::BANK_FEE_TIER_1;
      } else if (cold_amount == CryptoNote::parameters::TEST_AMOUNT_TIER_2) {
        banking_fee = CryptoNote::parameters::BANK_FEE_TIER_2;
      } else if
          (cold_amount == CryptoNote::parameters::TEST_AMOUNT_TIER_3) {
        banking_fee = CryptoNote::parameters::BANK_FEE_TIER_3;
      }
      // Fee = minimum fee
      uint64_t fee = m_currency.minimumFee();

      // Confirmation
      success_msg_writer() << "";
      success_msg_writer() << "TESTNET Certificate Of Ledger Deposit Summary:";
      success_msg_writer() << "  Amount: " << m_currency.formatAmount(cold_amount) << " TEST";
      success_msg_writer() << "  Term: " << term_label << " (" << cold_term << " blocks)";
      success_msg_writer() << "  Banking Fee: " << m_currency.formatAmount(banking_fee) << " TEST (0.1% of amount to Elderfiers)";
      success_msg_writer() << "  Network Fee: " << m_currency.formatAmount(fee) << " TEST (minimum txn fee to miners)";
      success_msg_writer() << "  Commitment Type:【COLD】 ▋ Off-chain (CD) interest yield";
      success_msg_writer() << "";
      success_msg_writer() << "Confirm? (1) OK  (2) NO  ";

      std::string confirm;
      std::getline(std::cin, confirm);

      if (confirm != "1" && confirm != "OK" && confirm != "Ok" && confirm != "ok") {
        success_msg_writer() << "Cancelled.";
        return true;
      }

      // Create COLD commitment for yield deposit (0xCD tag)
      std::vector<uint8_t> extra;
      Crypto::PublicKey pubkey;
      Crypto::SecretKey seckey;
      Crypto::generate_keys(pubkey, seckey);
      Crypto::Hash coldCommit = Crypto::cn_fast_hash(pubkey.data, sizeof(pubkey.data));

      CryptoNote::TransactionExtraColdCommitment coldCommitment;
      coldCommitment.commitment = coldCommit;
      coldCommitment.amount = cold_amount;
      coldCommitment.term = cold_term;
      coldCommitment.claimChainCode = 1;  // Default to ETH chain

      CryptoNote::addColdCommitmentToExtra(extra, coldCommitment);
      std::string extraString = std::string(extra.begin(), extra.end());

      success_msg_writer() << "Creating COLD transaction: " << m_currency.formatAmount(cold_amount) << " XFG for " << term_label;
      CryptoNote::TransactionId txId = m_wallet->deposit(cold_term, cold_amount, fee, extraString, 0);

      if (CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID == txId) {
        fail_msg_writer() << "Sending deposit transaction failed";
        return true;
      }

      success_msg_writer() << "COLD txn created. TX ID: " << txId;
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
    // Testnet elderfier registration ceremony with alias (mirrors mainnet, testnet amounts)
    if (args.size() != 1)
    {
      fail_msg_writer() << "Usage: elderking_ceremony <ALIAS>";
      fail_msg_writer() << "  ALIAS must be exactly 8 characters [A-Z0-9&] (e.g., TESTKING)";
      return true;
    }

    std::string alias = args[0];

    // Validate alias: exactly 8 chars, uppercase + digits + & only
    if (alias.length() != 8) {
      fail_msg_writer() << "Alias must be exactly 8 characters. Got " << alias.length() << ".";
      return true;
    }
    for (char c : alias) {
      if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '&')) {
        fail_msg_writer() << "Elderfier alias must be [A-Z0-9&] only. Invalid character: '" << c << "'";
        return true;
      }
    }

    // Check alias availability via RPC
    try {
      HttpClient httpClient(m_dispatcher, m_daemon_host, m_daemon_port);
      COMMAND_RPC_GET_ALIAS::request checkReq;
      COMMAND_RPC_GET_ALIAS::response checkRes;
      checkReq.alias = alias;
      invokeJsonCommand(httpClient, "/get_alias", checkReq, checkRes);
      if (checkRes.found) {
        fail_msg_writer() << "Alias @" << alias << " is already taken. Choose another.";
        return true;
      }
    } catch (const ConnectException&) {
      printConnectionError();
      return true;
    } catch (const std::exception& e) {
      fail_msg_writer() << "Failed to check alias availability: " << e.what();
      return true;
    }

    // Check if address already has an alias
    try {
      HttpClient httpClient(m_dispatcher, m_daemon_host, m_daemon_port);
      COMMAND_RPC_GET_ALIAS_BY_ADDRESS::request addrReq;
      COMMAND_RPC_GET_ALIAS_BY_ADDRESS::response addrRes;
      addrReq.address = m_wallet->getAddress();
      invokeJsonCommand(httpClient, "/get_alias_by_address", addrReq, addrRes);
      if (addrRes.found) {
        fail_msg_writer() << "Your address already has alias @" << addrRes.alias;
        return true;
      }
    } catch (const ConnectException&) {
      printConnectionError();
      return true;
    } catch (const std::exception& e) {
      fail_msg_writer() << "Failed to check address alias: " << e.what();
      return true;
    }

    try
    {
      success_msg_writer() << "";
      success_msg_writer() << "╔════════════════════════════════════════════════════════════╗";
      success_msg_writer() << "║            🔥⚡  TESTIFIER STAYKING CEREMONY  ⚡🔥          ║";
      success_msg_writer() << "║               (TESTNET ELDERFIER REGISTRATION)             ║";
      success_msg_writer() << "╚════════════════════════════════════════════════════════════╝";
      success_msg_writer() << "";
      success_msg_writer() << "  Alias:    @" << alias;
      success_msg_writer() << "  Network:  TESTNET";
      success_msg_writer() << "  Deposits: 5 x 80 TEST (0xEF tag) = 400 TEST total";
      success_msg_writer() << "";

      uint64_t balance = m_wallet->actualBalance();
      uint64_t required = 5 * CryptoNote::parameters::TEST_AMOUNT_TIER_3;
      uint64_t fee = m_currency.minimumFee();

      success_msg_writer() << "Balance:   " << m_currency.formatAmount(balance) << " TEST";
      success_msg_writer() << "Required:  " << m_currency.formatAmount(required + (5 * fee)) << " TEST";
      success_msg_writer() << "";

      if (balance < required + (5 * fee)) {
        fail_msg_writer() << "Insufficient balance for ceremony.";
        fail_msg_writer() << "Need " << m_currency.formatAmount(required + (5 * fee) - balance) << " more TEST.";
        return true;
      }

      std::string confirm;
      success_msg_writer() << "Type 'TESTIFY' to begin ceremony, or press Enter to abort: ";
      std::getline(std::cin, confirm);

      if (confirm != "TESTIFY") {
        success_msg_writer() << "Ceremony cancelled.";
        return true;
      }

      uint64_t amount_per_deposit = CryptoNote::parameters::TEST_AMOUNT_TIER_3;  // 80 TEST
      success_msg_writer() << "";
      success_msg_writer() << "Creating 5 Testifier stakes with alias @" << alias << "...";
      success_msg_writer() << "";

      for (int i = 0; i < 5; ++i) {
        success_msg_writer() << "Ritual " << (i + 1) << " of 5: Creating 80 TEST stake...";

        std::vector<uint8_t> extra;
        std::string extraString = "";

        Crypto::PublicKey public_key;
        Crypto::SecretKey secret_key;
        Crypto::generate_keys(public_key, secret_key);
        Crypto::Hash commitment_hash = Crypto::cn_fast_hash(public_key.data, sizeof(public_key.data));

        CryptoNote::TransactionExtraElderfierDeposit elderfierDeposit;
        elderfierDeposit.depositHash = commitment_hash;
        elderfierDeposit.depositAmount = amount_per_deposit;
        elderfierDeposit.elderfierAddress = m_wallet->getAddress();
        elderfierDeposit.securityWindow = 28800;
        // Embed alias in every deposit metadata (0xEA prefix + 8 bytes)
        elderfierDeposit.metadata.clear();
        elderfierDeposit.metadata.push_back(0xEA);
        elderfierDeposit.metadata.insert(elderfierDeposit.metadata.end(), alias.begin(), alias.end());
        elderfierDeposit.signature.clear();
        elderfierDeposit.isSlashable = true;

        CryptoNote::addElderfierDepositToExtra(extra, elderfierDeposit);
        extraString = std::string(extra.begin(), extra.end());

        CryptoNote::TransactionId txId = m_wallet->deposit(
          CryptoNote::parameters::TESTNET_DEPOSIT_TERM_ELDERFIER_STAKING,
          amount_per_deposit,
          fee,
          extraString,
          0
        );

        if (CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID == txId) {
          fail_msg_writer() << "Failed at ritual " << (i + 1) << " of 5";
          return true;
        }

        success_msg_writer() << "  Stake " << (i + 1) << " forged! TX ID: " << txId;
      }

      success_msg_writer() << "";
      success_msg_writer() << "╔════════════════════════════════════════════════════════════╗";
      success_msg_writer() << "║         🔥⚡ TESTNET CEREMONY COMPLETE! ⚡🔥               ║";
      success_msg_writer() << "╚════════════════════════════════════════════════════════════╝";
      success_msg_writer() << "";
      success_msg_writer() << "All 5 Testifier stakes created (400 TEST total)";
      success_msg_writer() << "Alias @" << alias << " will be registered when deposits confirm.";
      success_msg_writer() << "Next: list_deposits  |  lookup_alias " << alias;
      success_msg_writer() << "";

      return true;
    }
    catch (const std::exception& e)
    {
      fail_msg_writer() << "Error during ceremony: " << e.what();
      return true;
    }
  }

  //----------------------------------------------------------------------------------------------------
  bool CryptoNote::testnet_wallet::list_burns(const std::vector<std::string> &args)
  {
    // List all HEAT burn deposits
    try
    {
      success_msg_writer() << "";
      success_msg_writer() << "=== TESTNET Burn Transactions ===";
      success_msg_writer() << "";

      size_t depositCount = m_wallet->getDepositCount();
      size_t burnCount = 0;

      for (size_t i = 0; i < depositCount; ++i) {
        Deposit deposit;
        if (m_wallet->getDeposit(i, deposit)) {
          // Check if this is a HEAT/burn deposit (FOREVER term)
          if (deposit.term == CryptoNote::parameters::DEPOSIT_TERM_FOREVER) {
            burnCount++;
            std::string status = deposit.locked ? "LOCKED" : "UNLOCKED";
            success_msg_writer() << "  [" << i << "] Amount: " << m_currency.formatAmount(deposit.amount)
                                 << " TEST | Status: " << status
                                 << " | Type: HEAT (0x08)";
          }
        }
      }

      if (burnCount == 0) {
        success_msg_writer() << "  No burn transactions found.";
      } else {
        success_msg_writer() << "";
        success_msg_writer() << "Total burn transactions: " << burnCount;
      }

      success_msg_writer() << "";
      return true;
    }
    catch (const std::exception& e)
    {
      fail_msg_writer() << "Error listing burns: " << e.what();
      return true;
    }
  }
}
