// Copyright (c) 2017-2026 Fuego Developers
// Copyright (c) 2014-2018 The Monero project
// Copyright (c) 2014-2018 The Forknote developers
// Copyright (c) 2016-2019 The Karbowanec developers
// Copyright (c) 2012-2018 The CryptoNote developers
// Copyright (c) 2018-2019 The Ryo Currency developers
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
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

#pragma once

#include <cstdint>
#include <initializer_list>
#include <boost/uuid/uuid.hpp>

namespace CryptoNote
{
	namespace parameters
	{
		const uint64_t DIFFICULTY_TARGET = 480;
		const uint64_t CRYPTONOTE_MAX_BLOCK_NUMBER = 500000000;
		const size_t CRYPTONOTE_MAX_BLOCK_BLOB_SIZE = 8000000;
		const size_t CRYPTONOTE_MAX_TX_SIZE = 1000000000;
        const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 1753191; /* "fire" address prefix */
        const uint64_t CRYPTONOTE_SUBADDRESS_BASE58_PREFIX = CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX; // same as main (fire) for max privacy
		const size_t CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW = 60;
		const size_t CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW_TESTNET = 0;
		const uint64_t DIFFICULTY_TARGET_DRGL = 81;
		const unsigned EMISSION_SPEED_FACTOR = 18;
        const unsigned EMISSION_SPEED_FACTOR_FANGO = 19;  //major version 8
        const unsigned EMISSION_SPEED_FACTOR_FUEGO = 20;   //major version 9
		const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT  = 60 * 60 * 2;
		const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V1 = DIFFICULTY_TARGET_DRGL * 6;
		const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V2 = DIFFICULTY_TARGET * 2;
		const uint64_t CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE = 3;
		const size_t BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW = 60;
		const size_t BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V1 = 11; /* LWMA3 */

		const uint64_t MONEY_SUPPLY = UINT64_C(80000088000008); /* max supply: 8M8 */
		const uint64_t COIN = UINT64_C(10000000);
		const uint64_t MINIMUM_FEE_V1 = UINT64_C(800000);
		const uint64_t MINIMUM_FEE_V2 = UINT64_C(80000);	/* 0.008 XFG  (80Kħ) */
		const uint64_t MINIMUM_FEE_8KH = UINT64_C(8000);	/* 0.0008 XFG (8Kħ)  BMv10+ Flat Fee */
		const uint64_t MINIMUM_FEE = MINIMUM_FEE_8KH;

		// Fire Alias registration fee: 1 XFG
		const uint64_t ALIAS_REGISTRATION_FEE = COIN;  /* 1 XFG sent to Fuego Development Fund */

		const uint64_t DEFAULT_DUST_THRESHOLD = UINT64_C(1000); /* < 0.0001 XFG v10 */

		const size_t   CRYPTONOTE_COIN_VERSION                       = 1;
		const size_t   CRYPTONOTE_DISPLAY_DECIMAL_POINT 	         = 7;
		const size_t   CRYPTONOTE_REWARD_BLOCKS_WINDOW               = 100;
		const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE     = 430080; 
		const size_t   CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE        = 600;

		const uint64_t EXPECTED_NUMBER_OF_BLOCKS_PER_DAY             = 24 * 60 * 60 / DIFFICULTY_TARGET;
		const size_t   DIFFICULTY_CUT                                = 60;  
		const size_t   DIFFICULTY_LAG                                = 15;  
		const size_t   DIFFICULTY_WINDOW                             = 1067; 
		const size_t   DIFFICULTY_WINDOW_V4                          = 45;  

		// DMWDA parameters
		const uint32_t DMWDA_SHORT_WINDOW                            = 15;
		const uint32_t DMWDA_MEDIUM_WINDOW                           = 45;
		const uint32_t DMWDA_LONG_WINDOW                             = 120;
		const double   DMWDA_MIN_ADJUSTMENT                          = 0.5;
		const double   DMWDA_MAX_ADJUSTMENT                          = 4.0;
		const double   DMWDA_SMOOTHING_FACTOR                        = 0.3;
		const uint32_t TESTNET_DMWDA_LONG_WINDOW                     = 60;

        // MIXIN
		const uint64_t MIN_TX_MIXIN_SIZE_V10                         = 8;  
        const uint64_t MIN_TX_MIXIN_SIZE                             = MIN_TX_MIXIN_SIZE_V10;
		const uint64_t MAX_TX_MIXIN_SIZE                             = 18;

		// AMOUNT TIERS (for ring signature pool selection)
        const uint64_t AMOUNT_TIER_0 =     8000000;  // 0.8 XFG
        const uint64_t AMOUNT_TIER_1 =    80000000;  // 8 XFG
        const uint64_t AMOUNT_TIER_2 =   800000000;  // 80 XFG
        const uint64_t AMOUNT_TIER_3 =  8000000000;  // 800 XFG
        const uint64_t TEST_AMOUNT_TIER_0 =     800000;
        const uint64_t TEST_AMOUNT_TIER_1 =    8000000;
        const uint64_t TEST_AMOUNT_TIER_2 =   80000000;
        const uint64_t TEST_AMOUNT_TIER_3 =  800000000;

        // Epoch duration
        const uint64_t EPOCH_DURATION_BLOCKS = 900;              // Mainnet: 900 blocks
        const uint64_t TESTNET_EPOCH_DURATION_BLOCKS = 10;       // Testnet: 10 blocks

        // CD Fee Pool constants
        const uint64_t SWAP_FEE_RATE_BPS = 100;                // 1%
        const uint64_t SWAP_FEE_RATE_DIVISOR = 10000;
        const uint64_t FEE_POOL_RATE_PRECISION = 1000000ULL;
        const uint64_t TESTNET_SWAP_FEE_RATE_BPS = 100;

        // Swap fee split: 90% CD yield / 10% Fuego Treasury
        const uint64_t SWAP_FEE_CD_SHARE_PCT = 90;
        const uint64_t SWAP_FEE_TREASURY_SHARE_PCT = 10;

        // CD (Certificate of Deposit) term limits (in epochs)
        const uint32_t CD_MIN_EPOCHS = 1;
        const uint32_t CD_MAX_EPOCHS = 52;
        const uint32_t TESTNET_CD_MIN_EPOCHS = 1;
        const uint32_t TESTNET_CD_MAX_EPOCHS = 52;

        // Deposit amount limits
        const uint64_t DEPOSIT_MIN_AMOUNT = AMOUNT_TIER_0;   // 0.8 XFG
        const uint32_t DEPOSIT_TERM_MIN = 16000;  // ~3 months
        const uint32_t DEPOSIT_TERM_MAX = 65000;  // ~1 year

		const uint32_t UPGRADE_HEIGHT_V10                            = 999999; 

		const char CRYPTONOTE_BLOCKS_FILENAME[] = "blocks.dat";
 		const char CRYPTONOTE_BLOCKINDEXES_FILENAME[] = "blockindexes.dat";
 		const char CRYPTONOTE_BLOCKSCACHE_FILENAME[] = "blockscache.dat";
 		const char CRYPTONOTE_POOLDATA_FILENAME[] = "poolstate.bin";
 		const char P2P_NET_DATA_FILENAME[] = "p2pstate.bin";
 		const char CRYPTONOTE_BLOCKCHAIN_INDICES_FILENAME[] = "blockchainindices.dat";
 		const char MINER_CONFIG_FILE_NAME[] = "miner_conf.json";

	} // namespace parameters

	const char FUEGO_DEV_FUND_ADDRESS[] = "fireVHx639SLMhzmBoJ8drTXbVyv2eRG6A8aMLc1taTiRNwk8pnwXpBDUSjH1dT5fg7yVVZrKkvm31CmigAMdVDg7sgxJmAUNp";
    const char CRYPTONOTE_NAME[] = "fuego";
	const char GENESIS_COINBASE_TX_HEX[] = "013c01ff0001b4bcc29101029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd0880712101bd4e0bf284c04d004fd016a21405046e8267ef81328cabf3017c4c24b273b25a";

	const uint8_t  TRANSACTION_VERSION_1                         =  1;
	const uint8_t  TRANSACTION_VERSION_2                         =  2;

	const uint8_t  BLOCK_MAJOR_VERSION_1                         =  1;
	const uint8_t  BLOCK_MAJOR_VERSION_10                        = 10; 

	const int P2P_DEFAULT_PORT = 10808;
 	const int RPC_DEFAULT_PORT = 18180;

	const std::initializer_list<const char *> SEED_NODES = {
	  "207.244.247.64:10808",
	    "195.88.57.158:10808",
 		   "80.89.228.157:10808",
	         "216.145.84.248:10808"
	};

	const char GENESIS_COINBASE_TX_HEX_TESTNET[] = "010001ff0001b4bcc29101029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd0880712101eae9a3035cf3facc4a723c8334d5d3836950188703b407793c020741c46c1466";
 	const int P2P_DEFAULT_PORT_TESTNET = 20808;
 	const int RPC_DEFAULT_PORT_TESTNET = 28280;
 	const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX_TESTNET = 1075740;

	struct CheckpointData
	{
		uint32_t height;
		const char *blockId;
	};

	const std::initializer_list<CheckpointData>
		CHECKPOINTS = {
 			{ 800,    "c1c64f752f6f5f6f69671b3794f741af0707c71b35302ea4fc96b0befdce8ce9" },
 		    { 6484,   "6378b6899aebdf73da9d56ac9db5257af024490d68e6dd8dfb284ee8bd0fb004" }
		};

} // namespace CryptoNote
