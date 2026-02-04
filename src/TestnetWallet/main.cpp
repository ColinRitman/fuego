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

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include "Common/CommandLine.h"
#include "Common/SignalHandler.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/Currency.h"
#include "version.h"

namespace po = boost::program_options;

//----------------------------------------------------------------------------------------------------
// Main entry point for testnet wallet CLI
//----------------------------------------------------------------------------------------------------
int main(int argc, char* argv[]) {
  try {
    // This binary uses testnet_wallet which extends simple_wallet with testnet-specific commands
    po::options_description desc_general("General options");
    po::options_description desc_params("Parameters");

    desc_general.add_options()
      ("wallet-file,w", po::value<std::string>(), "use wallet <arg>")
      ("generate-new-wallet,g", po::value<std::string>(), "generate new wallet and save it to <arg>")
      ("import-new-wallet,i", po::value<std::string>(), "import new wallet")
      ("daemon-address,d", po::value<std::string>(), "use daemon instance at <host:port>")
      ("daemon-host,h", po::value<std::string>(), "use daemon instance at host <arg>")
      ("daemon-port,p", po::value<uint16_t>(), "use daemon instance at port <arg>")
      ("testnet", "testnet mode")
      ("stagenet", "stagenet mode")
      ("help", "produce help message")
      ("version", "show version");

    po::variables_map vm;
    bool r = Command_Line_Interpreter::parse_command_line(argc, argv, desc_general, vm);

    if (!r) {
      std::cerr << "Failed to parse command line options" << std::endl;
      return 1;
    }

    if (vm.count("help")) {
      std::cout << "Fuego Testnet Wallet CLI" << std::endl;
      std::cout << desc_general << std::endl;
      return 0;
    }

    if (vm.count("version")) {
      std::cout << "Fuego Testnet Wallet CLI v" << FUEGO_VERSION << std::endl;
      return 0;
    }

    // Initialize system and logging
    System::Dispatcher dispatcher;
    Logging::LoggerManager logManager;

    // Get currency (testnet)
    const CryptoNote::Currency& currency = CryptoNote::Currency::instance();

    // Create testnet wallet (extends simple_wallet)
    CryptoNote::testnet_wallet wallet(dispatcher, currency, logManager);

    if (!wallet.init(vm)) {
      return 1;
    }

    return wallet.run() ? 0 : 1;
  }
  catch (const std::exception& e) {
    std::cerr << "Testnet Wallet Error: " << e.what() << std::endl;
    return 1;
  }
}
