package app

type Config struct {
	DaemonRPC string // fuegod RPC endpoint
	Testnet   bool
	StartPair uint8 // initial pair to display
	NoSplash  bool
	Compact   bool
}

func DefaultConfig() Config {
	return Config{
		DaemonRPC: "http://127.0.0.1:18180",
		StartPair: PairSOL,
	}
}
