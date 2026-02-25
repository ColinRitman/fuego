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
#include "CryptoNoteCore/AliasIndex.h"

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
    m_consoleHandler.setHandler("elderking_ceremony", boost::bind(&testnet_wallet::elderking_ceremony, this, boost::arg<1>()), "elderking_ceremony - Begin the Testifier Staking Ceremony to become an Elderfier (interactive, 5x 80 TEST stakes).");
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
    // Interactive Testifier Staking Ceremony (Testnet).
    // Alias chosen interactively — no command-line arg needed.

    // ── PART I: THE CALLING ────────────────────────────────────────────────
    success_msg_writer() << "";
    success_msg_writer() << "╔════════════════════════════════════════════════════════════╗";
    success_msg_writer() << "║                                                            ║";
    success_msg_writer() << "║         THE TESTIFIER STAKING CEREMONY  [TESTNET]          ║";
    success_msg_writer() << "║                                                            ║";
    success_msg_writer() << "╚════════════════════════════════════════════════════════════╝";
    success_msg_writer() << "";
    success_msg_writer() << "  You stand at the threshold of the Elder Council.";
    success_msg_writer() << "  Before you lies a path of honour, sacrifice, and purpose.";
    success_msg_writer() << "";
    success_msg_writer() << "  ── WHAT IS AN ΞLDERFIER? ────────────────────────────────";
    success_msg_writer() << "";
    success_msg_writer() << "  Elderfiers are the guardians of the Fuego realm — keepers";
    success_msg_writer() << "  of cross-chain truth. They validate deposit commitments,";
    success_msg_writer() << "  sign merkle roots, and ensure that XFG burned on Fuego is";
    success_msg_writer() << "  faithfully reborn on Ethereum. Without Elderfiers, the";
    success_msg_writer() << "  bridge between worlds cannot hold.";
    success_msg_writer() << "";
    success_msg_writer() << "  On testnet, Testifiers rehearse this sacred duty — testing";
    success_msg_writer() << "  the ceremony, the staking mechanism, and the network before";
    success_msg_writer() << "  the mainnet Elderfiers are crowned.";
    success_msg_writer() << "";
    success_msg_writer() << "  ── WHAT IT TAKES TO BE AN ELDER KING ───────────────────";
    success_msg_writer() << "";
    success_msg_writer() << "  COURAGE   You must be brave in the face of adversity.";
    success_msg_writer() << "            When the network is under attack, when nodes";
    success_msg_writer() << "            fall, when pressure mounts — you do not falter.";
    success_msg_writer() << "";
    success_msg_writer() << "  JUSTICE   You must be just in all your judgments.";
    success_msg_writer() << "            Sign only the roots you believe true. Never";
    success_msg_writer() << "            collude, deceive, or act for gain over the good.";
    success_msg_writer() << "";
    success_msg_writer() << "  PROTECTION  You must protect the weak and innocent.";
    success_msg_writer() << "            The small holders, newcomers, the silent burners";
    success_msg_writer() << "            who trust the bridge — their safety is yours.";
    success_msg_writer() << "";
    success_msg_writer() << "  VIGILANCE   You must guard the Realm without rest.";
    success_msg_writer() << "            Run your Testifier node at all times. A guardian";
    success_msg_writer() << "            who sleeps while the Realm burns is no guardian.";
    success_msg_writer() << "";
    success_msg_writer() << "  HONOUR    Your 400 TEST stake is your bond on testnet.";
    success_msg_writer() << "            Betrayal of the Realm means slashing — your";
    success_msg_writer() << "            stake burned and name struck from the registry.";
    success_msg_writer() << "";
    success_msg_writer() << "  ── WHAT THE CEREMONY REQUIRES ───────────────────────────";
    success_msg_writer() << "";
    success_msg_writer() << "    5 deposits of 80 TEST each  (400 TEST total stake)";
    success_msg_writer() << "    Tagged 0xEF — the Elderfier mark — slashable stake";
    success_msg_writer() << "    A unique 8-character Ælder King name (your on-chain ID)";
    success_msg_writer() << "    You MUST run an Elderfier node to sign roots & earn fees";
    success_msg_writer() << "";
    success_msg_writer() << "  Do you accept the calling of the Ælder Council?";
    success_msg_writer() << "  Do you pledge to be brave, be just, protect the innocent,";
    success_msg_writer() << "  and guard the Realm with all your might and honour?";
    success_msg_writer() << "";
    success_msg_writer() << "  Type 'yes' to step forward, or Enter to walk away: ";

    std::string acceptance;
    std::getline(std::cin, acceptance);
    if (acceptance.empty() || (acceptance[0] != 'y' && acceptance[0] != 'Y')) {
      success_msg_writer() << "";
      success_msg_writer() << "  The Ælder Council watches. Return when you are ready.";
      success_msg_writer() << "  The Realm awaits those worthy of its flame.";
      success_msg_writer() << "";
      return true;
    }

    success_msg_writer() << "";
    success_msg_writer() << "  The Ælder Council nods. You have answered the call.";
    success_msg_writer() << "";

    // ── PART II: CHOOSE YOUR ELDER KING NAME ──────────────────────────────
    success_msg_writer() << "  ── CHOOSE YOUR ÆLDER KING NAME ─────────────────────────";
    success_msg_writer() << "";
    success_msg_writer() << "  Your Ælder King name is your identity on the Fuego testnet.";
    success_msg_writer() << "  It will be embedded in all 5 of your stakes and registered";
    success_msg_writer() << "  on-chain when your deposits confirm.";
    success_msg_writer() << "";
    success_msg_writer() << "  Rules:  Exactly 8 characters  |  A-Z  0-9  & only";
    success_msg_writer() << "  Special exemptions: GALAPAGOS  |  WINSLAYER  |  LOUDMINING  (reserved names)";
    success_msg_writer() << "  No two Testifiers may share a name.";
    success_msg_writer() << "";
    success_msg_writer() << "  Examples:";
    success_msg_writer() << "    TESTKING  |  IGNITE88  |  NETGUARD  |  BLAZE&TN";
    success_msg_writer() << "    REALM001  |  VAULT888  |  DRAGON&1  |  XFG&TEST";
    success_msg_writer() << "";

    std::string alias;
    while (true) {
      success_msg_writer() << "  Enter your Ælder King name: ";
      std::getline(std::cin, alias);
      // Trim whitespace
      while (!alias.empty() && std::isspace((unsigned char)alias.front())) alias.erase(alias.begin());
      while (!alias.empty() && std::isspace((unsigned char)alias.back()))  alias.pop_back();

      if (alias.empty()) {
        success_msg_writer() << "";
        success_msg_writer() << "  The Ælder Council watches. Return when you are ready.";
        return true;
      }
      if (!CryptoNote::AliasIndex::isValidElderfierAlias(alias)) {
        fail_msg_writer() << "  Invalid name '" << alias << "'.";
        fail_msg_writer() << "  Use exactly 8 characters [A-Z 0-9 &], unless, if you own one of the 2 reserved exceptional aliases:";
        fail_msg_writer() << "  GALAPAGOS or WINSLAYER, and LOUDMINING";
        fail_msg_writer() << "  (Press Enter with no input to abort.)";
        continue;
      }
      break;
    }

    // ── PART III: THE OATH ─────────────────────────────────────────────────
    success_msg_writer() << "";
    success_msg_writer() << "  Before the Eternal Flame and the eyes of the Ælder Council,";
    success_msg_writer() << "  hear the oath of Elder King " << alias << ":";
    success_msg_writer() << "";
    success_msg_writer() << "  ════════════════════════════════════════════════════════";
    success_msg_writer() << "";
    success_msg_writer() << "  Ælder King " << alias << ", do you vow:";
    success_msg_writer() << "";
    success_msg_writer() << "    to be BRAVE in the face of all adversity — to stand";
    success_msg_writer() << "    firm when the network is tested, threatened, or besieged?";
    success_msg_writer() << "";
    success_msg_writer() << "    to be JUST in all your judgments — to sign only the";
    success_msg_writer() << "    roots you believe true, and never act for dishonest gain?";
    success_msg_writer() << "";
    success_msg_writer() << "    to PROTECT the weak and innocent — to safeguard those";
    success_msg_writer() << "    who burn their TEST in trust that the bridge will hold?";
    success_msg_writer() << "";
    success_msg_writer() << "    to GUARD the Realm with all your might and honour —";
    success_msg_writer() << "    running your node with vigilance and without deceit?";
    success_msg_writer() << "";
    success_msg_writer() << "    to honour the ETERNAL FLAME — knowing that betrayal";
    success_msg_writer() << "    brings slashing, and your stake is your sacred bond";
    success_msg_writer() << "    to this network, now and for all time?";
    success_msg_writer() << "";
    success_msg_writer() << "  ════════════════════════════════════════════════════════";
    success_msg_writer() << "";
    success_msg_writer() << "  To seal these vows, enter your Ælder King name again: ";

    std::string confirmAlias;
    std::getline(std::cin, confirmAlias);
    while (!confirmAlias.empty() && std::isspace((unsigned char)confirmAlias.front())) confirmAlias.erase(confirmAlias.begin());
    while (!confirmAlias.empty() && std::isspace((unsigned char)confirmAlias.back()))  confirmAlias.pop_back();

    if (confirmAlias != alias) {
      fail_msg_writer() << "";
      fail_msg_writer() << "  The names do not match.";
      fail_msg_writer() << "  Entered '" << confirmAlias << "' — your chosen name was '" << alias << "'.";
      fail_msg_writer() << "  The Realm demands certainty. Return when your resolve is firm.";
      return true;
    }

    success_msg_writer() << "";
    success_msg_writer() << "  So be it.  Ælder King " << alias << " rises.";
    success_msg_writer() << "";

    // ── RPC: check alias availability ──────────────────────────────────────
    try {
      HttpClient httpClient(m_dispatcher, m_daemon_host, m_daemon_port);
      COMMAND_RPC_GET_ALIAS::request checkReq;
      COMMAND_RPC_GET_ALIAS::response checkRes;
      checkReq.alias = alias;
      invokeJsonCommand(httpClient, "/get_alias", checkReq, checkRes);
      if (checkRes.found) {
        fail_msg_writer() << "  The name @" << alias << " is already claimed by another Testifier.";
        fail_msg_writer() << "  Ceremony aborted. Run elderking_ceremony again to choose a new name.";
        return true;
      }
    } catch (const ConnectException&) {
      printConnectionError();
      return true;
    } catch (const std::exception& e) {
      fail_msg_writer() << "Failed to check alias availability: " << e.what();
      return true;
    }

    // ── RPC: check address not already registered ───────────────────────────
    try {
      HttpClient httpClient(m_dispatcher, m_daemon_host, m_daemon_port);
      COMMAND_RPC_GET_ALIAS_BY_ADDRESS::request addrReq;
      COMMAND_RPC_GET_ALIAS_BY_ADDRESS::response addrRes;
      addrReq.address = m_wallet->getAddress();
      invokeJsonCommand(httpClient, "/get_alias_by_address", addrReq, addrRes);
      if (addrRes.found) {
        fail_msg_writer() << "  Your address already bears the name @" << addrRes.alias;
        fail_msg_writer() << "  A Testifier may not be crowned twice. Ceremony aborted.";
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
      // ── Balance check ──────────────────────────────────────────────────
      uint64_t balance = m_wallet->actualBalance();
      uint64_t amount_per_deposit = CryptoNote::parameters::TEST_AMOUNT_TIER_3;  // 80 TEST
      uint64_t required = 5 * amount_per_deposit;
      uint64_t fee = m_currency.minimumFee();

      success_msg_writer() << "  ── PREPARING THE RITUAL ─────────────────────────────────";
      success_msg_writer() << "";
      success_msg_writer() << "  Wallet balance:    " << m_currency.formatAmount(balance) << " TEST";
      success_msg_writer() << "  5 stakes (80x5):   " << m_currency.formatAmount(required) << " TEST";
      success_msg_writer() << "  Network fees (x5): " << m_currency.formatAmount(5 * fee) << " TEST";
      success_msg_writer() << "  Total required:    " << m_currency.formatAmount(required + (5 * fee)) << " TEST";
      success_msg_writer() << "";

      if (balance < required + (5 * fee)) {
        fail_msg_writer() << "  The flame requires more fuel.";
        fail_msg_writer() << "  You need " << m_currency.formatAmount(required + (5 * fee) - balance) << " more TEST.";
        fail_msg_writer() << "  Ceremony aborted. Return when your coffers are ready.";
        return true;
      }

      success_msg_writer() << "  The balance holds. The Ritual of Five Flames begins.";
      success_msg_writer() << "";
      success_msg_writer() << "╔════════════════════════════════════════════════════════════╗";
      success_msg_writer() << "║            THE RITUAL OF FIVE FLAMES  [TESTNET]            ║";
      success_msg_writer() << "╚════════════════════════════════════════════════════════════╝";
      success_msg_writer() << "";

      static const char* const flameNames[] = { "First", "Second", "Third", "Fourth", "Fifth" };

      // Fetch spend public key once — used to build per-deposit commitment
      CryptoNote::AccountKeys walletKeys;
      m_wallet->getAccountKeys(walletKeys);

      for (int i = 0; i < 5; ++i) {
        success_msg_writer() << "  The " << flameNames[i] << " Flame — forging stake " << (i + 1) << " of 5...";

        std::vector<uint8_t> extra;
        Crypto::PublicKey public_key;
        Crypto::SecretKey secret_key;
        Crypto::generate_keys(public_key, secret_key);
        Crypto::Hash commitment_hash = Crypto::cn_fast_hash(public_key.data, sizeof(public_key.data));

        // Build one-way commitment: H(spendPublicKey || ephemeralPublicKey)
        // Binds deposit to staker without embedding wallet address on-chain.
        // The ephemeral public key is unique per deposit, so each commitment is distinct.
        uint8_t commit_preimage[64];
        std::memcpy(commit_preimage,      walletKeys.address.spendPublicKey.data, 32);
        std::memcpy(commit_preimage + 32, public_key.data,                         32);
        Crypto::Hash elderfier_commitment = Crypto::cn_fast_hash(commit_preimage, sizeof(commit_preimage));

        CryptoNote::TransactionExtraElderfierDeposit elderfierDeposit;
        elderfierDeposit.depositHash        = commitment_hash;
        elderfierDeposit.depositAmount      = amount_per_deposit;
        elderfierDeposit.elderfierCommitment = elderfier_commitment;
        elderfierDeposit.securityWindow     = 28800;
        elderfierDeposit.metadata.clear();
        elderfierDeposit.metadata.push_back(0xEA);
        elderfierDeposit.metadata.insert(elderfierDeposit.metadata.end(), alias.begin(), alias.end());
        elderfierDeposit.signature.clear();
        elderfierDeposit.isSlashable        = true;

        CryptoNote::addElderfierDepositToExtra(extra, elderfierDeposit);
        std::string extraString = std::string(extra.begin(), extra.end());

        CryptoNote::TransactionId txId = m_wallet->deposit(
          CryptoNote::parameters::TESTNET_DEPOSIT_TERM_ELDERFIER_STAKING,
          amount_per_deposit,
          fee,
          extraString,
          0
        );

        if (CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID == txId) {
          fail_msg_writer() << "";
          fail_msg_writer() << "  The ritual faltered at flame " << (i + 1) << " of 5.";
          fail_msg_writer() << "  " << i << " stake(s) were forged before it broke.";
          fail_msg_writer() << "  Check your balance and connection, then try again.";
          return true;
        }

        success_msg_writer() << "    Sealed.  TX: " << txId;
      }

      // ── Completion ─────────────────────────────────────────────────────
      success_msg_writer() << "";
      success_msg_writer() << "╔════════════════════════════════════════════════════════════╗";
      success_msg_writer() << "║          TESTNET CEREMONY COMPLETE — TESTIFIER IGNITED         ║";
      success_msg_writer() << "╚════════════════════════════════════════════════════════════╝";
      success_msg_writer() << "";
      success_msg_writer() << "  Ælder King " << alias << " — all 5 Testifier stakes have been";
      success_msg_writer() << "  broadcast to the testnet. Your name is embedded in each.";
      success_msg_writer() << "";
      success_msg_writer() << "  When all 5 deposits confirm on-chain, the testnet will";
      success_msg_writer() << "  register you as Elder King @" << alias;
      success_msg_writer() << "  and add you to the active Testifiers registry.";
      success_msg_writer() << "";
      success_msg_writer() << "  Your Elderfire burns bright.";
      success_msg_writer() << "  Guard the Realm well,  " << alias << ".";
      success_msg_writer() << "";
      success_msg_writer() << "  Commands:  list_deposits  |  lookup_alias " << alias;
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
            success_msg_writer() << "  [" << i << "] Amount: " << m_currency.formatAmount(deposit.amount)
                                 << " TEST | Status: Burned"
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
