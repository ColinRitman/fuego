# Fuego vs Monero: Protocol & Architecture Comparison

## Part 1: Protocol & Feature Similarities

| Feature | Fuego | Monero | Notes |
|---------|-------|--------|-------|
| **Base Protocol** | CryptoNote | CryptoNote | Both built on CryptoNote v1 |
| **Privacy Model** | Ring signatures + stealth addresses | Ring signatures + stealth addresses | Identical privacy foundation |
| **Blockchain Type** | Public, decentralized | Public, decentralized | Both permissionless |
| **Consensus** | Proof-of-Work | Proof-of-Work | PoW-based security |
| **Block Time** | 480 seconds (8 min) | 120 seconds (2 min) | Fuego is 4x slower |
| **Currency Unit** | XFG | XMR | Different tokens |
| **Atomic Unit** | 0.0000001 XFG | 0.000000000001 XMR | 7 decimals vs 12 decimals |
| **Ring Size** | 10-15 (dynamic) | 16 (fixed post-upgrade) | Monero increased for anonymity |
| **Transaction Type** | Standard + COLD/HEAT deposits | Standard only | Fuego has deposit extensions |
| **Daemon RPC** | `/json_rpc` | `/json_rpc` | Standard CryptoNote RPC interface |
| **P2P Protocol** | CryptoNote P2P | CryptoNote P2P | Compatible low-level format |
| **Wallet RPC** | `/json_rpc` | `/json_rpc` | Identical API pattern |
| **Smart Contracts** | None (STARK proofs on L2 EVM) | None (no smart contracts) | Fuego bridges to EVM, Monero doesn't |
| **Emission Curve** | Fixed emission | Tail emission | Fuego: fixed supply, Monero: infinite tail |
| **Hard Forks** | Periodic protocol upgrades | Periodic protocol upgrades | Both maintain network consensus |

---

## Part 2: Protocol & Feature Differences

| Aspect | Fuego | Monero | Key Difference |
|--------|-------|--------|---|
| **Deposit System** | ✅ COLD/HEAT deposits (0xCD, 0x08 tags) | ❌ No deposits | Fuego: Allows locked deposits with proof-of-burn |
| **Commitment Index** | ✅ Indexed merkle tree of all deposits | ❌ No index | Fuego: Efficient deposit tracking |
| **Elderfier System** | ✅ 5 elected elderfiers (staking, rotation, fee splits) | ❌ No elderfiers | Fuego: Community governance layer |
| **Investment Index** | ✅ Tracks deposit term lengths | ❌ No investment tracking | Fuego: Interest accrual support |
| **Banking Index** | ✅ Fee tracking per deposit | ❌ No banking features | Fuego: Fee distribution infrastructure |
| **STARK Proofs** | ✅ Off-chain STARK proof generation (cli tool) | ❌ No STARK proofs | Fuego: Bridges to Ethereum via STARK |
| **L1/L2 Bridge** | ✅ Arbitrum Sepolia ↔ Fuego via STARK proofs | ❌ No bridge | Fuego: Multi-chain interoperability |
| **Token Standard** | ✅ ERC-20 on Arbitrum (CD, HEAT tokens) | ❌ No ERC-20 tokens | Fuego: Cross-chain token bridge |
| **Burn Proof** | ✅ Burn2Mint (burn XFG → mint HEAT tokens) | ❌ No burn mechanism | Fuego: Proof-of-burn to L2 minting |
| **COLD Lock** | ✅ Time-locked deposits (3mo/12mo terms) | ❌ No time locks | Fuego: Interest-bearing locked coins |
| **EVM Integration** | ✅ Full smart contract integration | ❌ None | Fuego: Leverages Ethereum ecosystem |
| **DAO Governance** | ✅ COLDAO (CD token holders vote on APY) | ❌ No DAO | Fuego: On-chain governance for interest rates |
| **LP Rewards** | ✅ LPRewardsManager (Uniswap-style rewards) | ❌ No LP rewards | Fuego: DeFi-style liquidity incentives |
| **Mainnet RPC Port** | 18180 | 18081 | Fuego diverges from Monero convention |
| **Testnet RPC Port** | 28280 | 28081 | Same offset, different base |
| **P2P Port (Mainnet)** | 10808 | 18080 | Fuego uses lower port range |
| **P2P Port (Testnet)** | 20808 | 28080 | Pattern consistent within Fuego |
| **Block Difficulty** | Variable (standard PoW) | Variable (standard PoW) | Same algorithm |
| **ASIC Resistance** | Similar to Monero | CryptoNight (ASIC-resistant) | Both designed for GPU/CPU mining |

---

## Part 3: Source Code Structure Comparison

### Fuego Source Tree (752 C++/H files)

```
fuego/
├── src/
│   ├── CryptoNoteCore/           [Core blockchain]
│   │   ├── CommitmentIndex.*     [NEW: Deposit merkle tree]
│   │   ├── BankingIndex.*        [NEW: Fee tracking]
│   │   ├── InvestmentIndex.*     [NEW: Term deposit tracking]
│   │   ├── BurnProofDataFileGenerator.* [NEW: STARK proof generation]
│   │   ├── DepositCommitment.*   [NEW: Deposit commitment structures]
│   │   ├── Blockchain.*          [Modified: Deposit validation]
│   │   ├── Core.*                [Modified: Elderfier support]
│   │   ├── TransactionExtra.*    [Modified: 0xCD/0x08 tags]
│   │   ├── Currency.*            [Modified: Emission curve]
│   │   ├── Account.h             [From CryptoNote]
│   │   ├── Difficulty.*          [From CryptoNote]
│   │   └── [40+ other files]
│   ├── CryptoNoteProtocol/       [P2P protocol]
│   ├── Rpc/                      [RPC servers + handlers]
│   │   ├── RpcServer.*           [Modified: New RPC endpoints]
│   │   ├── CoreRpcServerCommandsDefinitions.h [Modified: +3 elderfier endpoints]
│   │   └── [RPC implementation]
│   ├── Wallet/                   [Wallet implementation]
│   ├── WalletLegacy/             [Legacy wallet support]
│   ├── P2p/                      [P2P networking]
│   ├── Common/                   [Utilities]
│   ├── Logging/                  [Logging framework]
│   └── [20+ other directories]
├── include/                      [Header files]
│   ├── EldernodeIndexTypes.h     [NEW: Elderfier tier definitions]
│   ├── IWalletLegacy.h           [Modified: Deposit support]
│   └── [Standard CryptoNote headers]
├── tui/                          [NEW: Terminal UI (Go)]
├── tui-testnet/                  [NEW: Testnet TUI (Go)]
├── xfg-stark/                    [NEW: STARK proof + L2 bridge]
│   ├── contracts/solidity/       [NEW: EVM smart contracts]
│   ├── api/                      [NEW: Claim validation API]
│   ├── frontend/                 [NEW: React dApp]
│   └── scripts/                  [Deployment helpers]
└── docker/                       [Container configs]
```

### Monero Source Tree (Similar size, different focus)

```
monero/
├── src/
│   ├── cryptonote_core/          [Core blockchain]
│   │   ├── blockchain.cpp        [Standard CryptoNote]
│   │   ├── tx_pool.cpp           [Transaction pool]
│   │   ├── [No commitment index]
│   │   ├── [No banking features]
│   │   ├── [No elderfier system]
│   │   └── [35+ standard files]
│   ├── cryptonote_protocol/      [P2P protocol]
│   ├── rpc/                      [RPC servers]
│   │   ├── core_rpc_server.cpp   [Standard RPC]
│   │   └── [No custom RPC endpoints]
│   ├── wallet/                   [Wallet implementation]
│   ├── p2p/                      [P2P networking]
│   ├── common/                   [Utilities]
│   └── [15+ other directories]
├── external/                     [Dependencies]
└── tests/                        [C++ unit tests only]
```

---

## Part 4: Key Module Comparison

### Blockchain Core

| Module | Fuego | Monero | Purpose |
|--------|-------|--------|---------|
| `Blockchain.h/cpp` | 3,500+ lines (extended) | ~2,500 lines (standard) | Block validation + deposit processing |
| `Core.h/cpp` | 1,500+ lines (extended) | ~1,200 lines (standard) | Core daemon logic |
| `Currency.h/cpp` | 800+ lines (extended) | ~400 lines (standard) | Emission + deposit interest calcs |
| `TransactionExtra.h/cpp` | 1,200+ lines (extended) | ~600 lines (standard) | 0xCD/0x08 commitment tags |

### Fuego-Specific Modules (Don't exist in Monero)

| Module | Lines | Purpose |
|--------|-------|---------|
| `CommitmentIndex.h/cpp` | ~500 | Indexed merkle tree of deposits |
| `BankingIndex.h/cpp` | ~400 | Fee accumulation tracking |
| `InvestmentIndex.h/cpp` | ~400 | Deposit term tracking |
| `DepositCommitment.h/cpp` | ~300 | Commitment structure definitions |
| `BurnProofDataFileGenerator.h/cpp` | ~250 | STARK proof file generation |
| `EldernodeIndexTypes.h` | ~200 | Elderfier tier definitions |

### RPC Endpoints

| Endpoint | Fuego | Monero | Status |
|----------|-------|--------|--------|
| `getheight` | ✅ | ✅ | Standard |
| `getblockcount` | ✅ | ✅ | Standard |
| `getblocktemplate` | ✅ | ✅ | Standard |
| `submitblock` | ✅ | ✅ | Standard |
| `gettransactions` | ✅ | ✅ | Standard |
| `sendrawtransaction` | ✅ | ✅ | Standard |
| **check_commitment_exists** | ✅ NEW | ❌ | Fuego deposit query |
| **get_commitment** | ✅ NEW | ❌ | Fuego deposit details |
| **get_commitment_merkle_root** | ✅ NEW | ❌ | Fuego merkle proof |
| **get_commitment_merkle_proof** | ✅ NEW | ❌ | Fuego proof generation |
| **get_commitment_stats** | ✅ NEW | ❌ | Fuego deposit stats |
| **get_elderfier_candidates** | ✅ NEW | ❌ | Elderfier listing |
| **get_elderfier_stake_info** | ✅ NEW | ❌ | Elderfier details |
| **get_elderfier_earnings** | ✅ NEW | ❌ | Elderfier fee distribution |

---

## Part 5: Configuration & Defaults Comparison

### Mainnet Configuration

| Parameter | Fuego | Monero | Notes |
|-----------|-------|--------|-------|
| **RPC Port** | 18180 | 18081 | Fuego adds 99 offset |
| **P2P Port** | 10808 | 18080 | Fuego uses lower port |
| **Block Time (target)** | 480 sec (8 min) | 120 sec (2 min) | Fuego: 4x slower |
| **Block Size Limit** | ~300 KB (typical) | ~300 KB (typical) | Similar |
| **Min Ring Size** | 10 | 16 | Monero: higher privacy |
| **Max Ring Size** | 15 | 16 | Near-identical |
| **Emission Pattern** | Fixed supply | Tail emission (0.3 XMR/block) | Fuego: finite, Monero: infinite |
| **Network ID** | 93385046440755750514194170694064996624 | Different large integer | Prevents accidental cross-network |
| **Fork Heights** | Multiple (periodic upgrades) | Multiple (periodic upgrades) | Protocol evolution |

### Testnet Configuration

| Parameter | Fuego | Monero | Notes |
|-----------|-------|--------|-------|
| **RPC Port** | 28280 | 28081 | Fuego adds 199 offset |
| **P2P Port** | 20808 | 28080 | Pattern: +10000 from mainnet (Fuego) |
| **Network ID** | 112015110234323138517908755257434054688 | Different | Testnet separation |
| **Bootstrap Nodes** | Custom | Monero's testnet | Different networks |

---

## Part 6: Development & Build Comparison

### Build System

| Aspect | Fuego | Monero |
|--------|-------|--------|
| **Build Tool** | CMake | CMake |
| **Language** | C++ (90%) + Go (10% TUI) | C++ |
| **Compiler** | GCC 4.7+, Clang, MSVC | GCC 4.7+, Clang, MSVC |
| **Boost Version** | 1.55+ (≤1.86) | 1.55+ |
| **C++ Standard** | C++11/14 | C++11/14 |
| **Go Version** | 1.20+ (for TUI) | N/A |
| **Test Framework** | GTest + manual | GTest |
| **CI/CD** | GitHub Actions | GitHub Actions |
| **Docker Support** | ✅ Yes | ✅ Yes |
| **Cross-Compile** | ✅ (ARM64, Android, etc.) | ✅ |

### Development Features (Fuego Additions)

| Feature | Status |
|---------|--------|
| **Go-based TUI** | ✅ Fuego-specific |
| **STARK proof CLI** | ✅ Fuego-specific |
| **Solidity contracts** | ✅ Fuego-specific |
| **TypeScript API** | ✅ Fuego-specific |
| **React dApp** | ✅ Fuego-specific |

---

## Part 7: Code Metrics Comparison

### Lines of Code (Estimate)

| Component | Fuego | Monero | Delta |
|-----------|-------|--------|-------|
| **C++ Core** | ~250,000 | ~200,000 | +25% (deposits, elderfiers) |
| **RPC Handlers** | ~8,000 | ~5,000 | +60% (new endpoints) |
| **Wallet Logic** | ~40,000 | ~45,000 | -11% (simplified) |
| **Tests** | ~15,000 | ~30,000 | -50% (Fuego lighter) |
| **Documentation** | ~50,000 | ~30,000 | +67% (Fuego verbose) |
| **Go TUI** | ~5,000 | ~0 | New component |
| **Smart Contracts** | ~3,000 | ~0 | New component |
| **API Backend** | ~2,000 | ~0 | New component |
| **Frontend** | ~1,500 | ~0 | New component |
| **TOTAL** | ~374,500 | ~310,000 | **+21% larger** |

### Files Comparison

| Type | Fuego | Monero |
|------|-------|--------|
| **.cpp** | ~400 | ~350 |
| **.h** | ~350 | ~320 |
| **.go** | ~30 | 0 |
| **.sol** | ~20 | 0 |
| **.ts** | ~10 | 0 |
| **.tsx** | ~5 | 0 |
| **Total** | ~815 | ~670 |

---

## Part 8: Summary Table: At a Glance

| Category | Fuego | Monero | Winner |
|----------|-------|--------|--------|
| **Privacy** | Ring sigs + stealth | Ring sigs + stealth | TIE |
| **Anonymity Set** | 10-15 | 16 | Monero |
| **Block Time** | 480 sec | 120 sec | Monero (faster) |
| **Deposit System** | ✅ COLD/HEAT | ❌ None | Fuego |
| **Smart Contracts** | ✅ Via L2 EVM | ❌ None | Fuego |
| **Governance** | ✅ Elderfiers + DAO | ❌ None | Fuego |
| **Code Maturity** | Forked + Extended | Original CryptoNote | Monero |
| **Community Size** | Small but growing | Large, established | Monero |
| **Exchange Listings** | Limited | Major exchanges | Monero |
| **Mining Ecosystem** | CPU/GPU friendly | ASIC-resistant | TIE |
| **Testnet Quality** | Good (TUI) | Excellent | Monero |
| **Documentation** | Improving | Comprehensive | Monero |
| **Innovation** | High (new features) | Conservative | Fuego |
| **Security Audits** | Pending | Professional (multiple) | Monero |

---

## Conclusion

**Fuego** is a CryptoNote derivative that extends Monero's privacy foundation with:
- Deposit/investment features
- Elderfier governance
- L2 EVM bridge (STARK proofs)
- DAO-controlled interest rates

**Monero** maintains the original CryptoNote philosophy:
- Pure privacy-focused cryptocurrency
- No smart contracts
- No additional features
- Conservative protocol upgrades

Both are solid implementations of CryptoNote, but serve different purposes:
- **Monero** = Privacy coin
- **Fuego** = Privacy coin + DeFi bridge
