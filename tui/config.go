package main

type Config struct {
	NetworkName  string
	CoinName     string
	AddressPrefix string
	NodeBinary   string
	WalletBinary string
	IsTestnet    bool
	NodeRPCPort  int
	WalletRPCPort int
	NodeP2PPort  int
	DataDir      string
	CoinUnits    int64
	StakeAmount  int64
	BurnTiers    []int64 // 0.8, 8, 80, 800 in atomic
	TestTxAmount int64
	// RPC method names
	CreateStakeRPC      string
	GetStakeStatusRPC   string
	CreateBurnRPC       string
	RequestConsensusRPC string
	GetConsensusRPC     string
	GetPendingVotesRPC  string
	RegisterEnindexRPC  string
	IncreaseStakeRPC    string
	UpdateEnindexRPC    string
	GetAddressesRPC     string
	GetBalanceRPC       string
	SendTransactionRPC  string
	CreateAddressRPC    string
}

var MainnetConfig = Config{
	NetworkName:   "Mainnet",
	CoinName:      "XFG",
	AddressPrefix: "1753191",
	NodeBinary:    "fuegod",
	WalletBinary:  "fire_wallet",
	IsTestnet:     false,
	NodeRPCPort:   18180,
	WalletRPCPort: 18183,
	NodeP2PPort:   10808,
	DataDir:       ".fuego",
	CoinUnits:     10000000,
	StakeAmount:   8000000000,
	BurnTiers:     []int64{8000000, 80000000, 800000000, 8000000000}, // 0.8, 8, 80, 800 XFG
	TestTxAmount:  10000000,
	CreateStakeRPC:      "create_stake_deposit",
	GetStakeStatusRPC:   "get_stake_status",
	CreateBurnRPC:       "create_burn_deposit",
	RequestConsensusRPC: "request_elderfier_consensus",
	GetConsensusRPC:     "get_consensus_requests",
	GetPendingVotesRPC:  "get_pending_votes",
	RegisterEnindexRPC:  "register_to_enindex",
	IncreaseStakeRPC:    "increase_stake",
	UpdateEnindexRPC:    "update_enindex",
	GetAddressesRPC:     "get_addresses",
	GetBalanceRPC:       "get_balance",
	SendTransactionRPC:  "send_transaction",
	CreateAddressRPC:    "create_address",
}

var TestnetConfig = Config{
	NetworkName:   "Fuego Testnet",
	CoinName:      "TEST",
	AddressPrefix: "1075740",
	NodeBinary:    "testnetd",
	WalletBinary:  "test_wallet",
	IsTestnet:     true,
	NodeRPCPort:   28280,
	WalletRPCPort: 28283,
	NodeP2PPort:   20808,
	DataDir:       ".fuego-testnet",
	CoinUnits:     10000000,
	StakeAmount:   40000000000,
	BurnTiers:     []int64{8000000, 80000000, 800000000, 8000000000}, // 0.8, 8, 80, 800 TEST
	TestTxAmount:  1000,
	CreateStakeRPC:      "create_stake_deposit",
	GetStakeStatusRPC:   "get_stake_status",
	CreateBurnRPC:       "create_burn_deposit",
	RequestConsensusRPC: "request_elderfier_consensus",
	GetConsensusRPC:     "get_consensus_requests",
	GetPendingVotesRPC:  "get_pending_votes",
	RegisterEnindexRPC:  "register_to_enindex",
	IncreaseStakeRPC:    "increase_stake",
	UpdateEnindexRPC:    "update_enindex",
	GetAddressesRPC:     "get_addresses",
	GetBalanceRPC:       "get_balance",
	SendTransactionRPC:  "send_transaction",
	CreateAddressRPC:    "create_address",
}

var CurrentConfig Config
