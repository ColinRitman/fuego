# Fuego High-ROI Implementation Checklist

## Overview
This document consolidates the highest-impact, lowest-risk improvements from analyzing four CryptoNote-based projects:
- [Karbo](https://github.com/seredat/karbowanec) - Adaptive difficulty and P2P hardening
- [Monero](https://github.com/monero-project/monero) - Privacy features and performance optimizations  
- [Zano](https://github.com/hyle-team/zano) - Build reproducibility and tooling
- [Conceal](https://github.com/ConcealNetwork/conceal-core) - UX improvements and security hardening

## Phase 1: Immediate High-ROI (Low Risk, High Impact)

### ✅ P2P Network Hardening
**Priority: CRITICAL** | **Effort: Low** | **Risk: None (no consensus)**

- [ ] **Peer rate limiting and scoring**
  - Implement per-peer message budgets and backpressure
  - Add exponential backoff on protocol violations
  - Cap inflight requests per peer
  - **Files**: `src/P2p/NetNode.*`, `src/CryptoNoteProtocol/CryptoNoteProtocolHandler.*`
  - **Source**: [Karbo](https://github.com/seredat/karbowanec), [Zano](https://github.com/hyle-team/zano)

- [ ] **Connection diversity and seed strategy**
  - Reserved slots for priority/seed nodes
  - Stricter per-ASN/IP caps
  - Periodic address shuffling
  - **Files**: `src/P2p/NetNode.*`, `NetNodeConfig.*`
  - **Source**: [Karbo](https://github.com/seredat/karbowanec)

### ✅ Mempool and Fee Management
**Priority: HIGH** | **Effort: Medium** | **Risk: None (no consensus)**

- [ ] **Dynamic fee estimation and mempool histogram**
  - Rolling mempool fee histogram
  - RPC fee estimates by confirmation target
  - Fee floor scales with block median/weight
  - **Files**: `src/CryptoNoteCore/TransactionPool.*`, `src/Rpc/RpcServer.*`
  - **Source**: [Monero](https://github.com/monero-project/monero)

- [ ] **Mempool DoS controls**
  - Per-peer tx rate limits and scoring
  - Size/complexity caps
  - Fee-rate based eviction
  - Orphan/invalid cooldowns
  - **Files**: `src/CryptoNoteCore/TransactionPool.*`, `src/P2p/NetNode.*`
  - **Source**: [Monero](https://github.com/monero-project/monero), [Conceal](https://github.com/ConcealNetwork/conceal-core)

### ✅ Sync Optimization
**Priority: HIGH** | **Effort: Medium** | **Risk: None (no consensus)**

- [ ] **Lite/compact block sync tuning**
  - Headers-first + compact body prioritization
  - Tune missing-tx retrieval
  - Improve request batch sizes and backpressure
  - **Files**: `src/CryptoNoteProtocol/CryptoNoteProtocolHandler.*`, `src/Rpc/RpcServer.*`
  - **Source**: [Monero](https://github.com/monero-project/monero), [Karbo](https://github.com/seredat/karbowanec)

### ✅ RPC and API Enhancements
**Priority: HIGH** | **Effort: Low** | **Risk: None (additive only)**

- [ ] **RPC surface parity for tooling**
  - Header ranges, output distribution, fee estimate
  - Txpool stats, bulk endpoints
  - Better error codes
  - **Files**: `src/Rpc/RpcServer.*` and command definitions
  - **Source**: [Monero](https://github.com/monero-project/monero)

- [ ] **Restricted-RPC defaults for public nodes**
  - Default `--restricted-rpc` when `--rpc-bind-ip` is public
  - Add allowlist/regex for safe methods
  - Improve docs and warnings
  - **Files**: `src/Daemon/Daemon.cpp`, `src/Rpc/RpcServer.*`
  - **Source**: [Conceal](https://github.com/ConcealNetwork/conceal-core), [Monero](https://github.com/monero-project/monero)

### ✅ Security and Privacy
**Priority: HIGH** | **Effort: Medium** | **Risk: None (no consensus)**

- [ ] **Tor/proxy support for daemon and wallet**
  - Add `--proxy host:port`, `--no-igd`, `--p2p-bind-ip` for loopback
  - Ensure RPC relay over proxy
  - Document Tails usage
  - **Files**: `src/Daemon/Daemon.cpp`, P2P connector sockets, RPC client/server
  - **Source**: [Monero](https://github.com/monero-project/monero), [Zano](https://github.com/hyle-team/zano)

- [ ] **Wallet decoy selection improvements**
  - Age-weighted/gamma-like decoy sampling
  - Improved output selection and binning
  - Multi-threaded scanning
  - **Files**: `src/Wallet/WalletGreen.*`, `src/WalletLegacy/*`
  - **Source**: [Monero](https://github.com/monero-project/monero)

### ✅ Build and Developer Experience
**Priority: MEDIUM** | **Effort: Low** | **Risk: None (infra only)**

- [ ] **Build reproducibility and dependency pinning**
  - Document Boost 1.84 and OpenSSL 1.1.1 builds
  - Allow `BOOST_ROOT` and `OPENSSL_ROOT_DIR` overrides
  - Checksum verification in helper scripts
  - **Files**: `CMakeLists.txt`, CI workflows, new `utils/` scripts
  - **Source**: [Zano](https://github.com/hyle-team/zano)

- [ ] **Cross-platform build scripts**
  - Linux/Windows/macOS build automation
  - MSVC solution generation
  - macOS env setup/signing stubs
  - **Files**: New `utils/` scripts, README build sections
  - **Source**: [Zano](https://github.com/hyle-team/zano), [Conceal](https://github.com/ConcealNetwork/conceal-core)

- [ ] **Static analysis and CI hardening**
  - ASAN/UBSAN builds in CI
  - Clang-tidy/Cppcheck integration
  - Valgrind profiles
  - **Files**: CI workflows, `CMakeLists.txt` toggles
  - **Source**: [Zano](https://github.com/hyle-team/zano), [Monero](https://github.com/monero-project/monero)

## Phase 2: Medium-ROI (Higher Effort, Good Impact)

### 🔄 Optional Features
**Priority: MEDIUM** | **Effort: Medium-High** | **Risk: None (optional features)**

- [ ] **Pruned blockchain mode**
  - 60-70% disk reduction, faster initial sync
  - Pruning indexes and minimal block data retention
  - CLI option `--prune-blockchain`
  - **Files**: `src/CryptoNoteCore/Blockchain.*`, storage layer, `src/Daemon/Daemon.cpp`
  - **Source**: [Monero](https://github.com/monero-project/monero)

- [ ] **UPnP and NAT traversal defaults**
  - Auto-enable UPnP with safe timeouts
  - Opt-out flag
  - **Files**: miniupnpc usage, daemon args and startup sequence
  - **Source**: [Karbo](https://github.com/seredat/karbowanec)

- [ ] **Encrypted messaging via tx_extra**
  - Embed/retrieve encrypted payloads in `tx_extra`
  - CLI/RPC methods to send/receive/decode messages
  - Size caps and fee policy
  - **Files**: `src/Wallet/WalletGreen.*`, `src/SimpleWallet/*`, `src/Rpc/RpcServer.*`
  - **Source**: [Conceal](https://github.com/ConcealNetwork/conceal-core)

- [ ] **Deposit UX/RPC parity**
  - Explicit RPC endpoints for create/withdraw deposit
  - Query terms and address-level deposit balances
  - Height/term calculators
  - **Files**: `src/Wallet/WalletGreen.*`, `src/SimpleWallet/*`, `src/Rpc/RpcServer.*`
  - **Source**: [Conceal](https://github.com/ConcealNetwork/conceal-core)

## Phase 3: Consensus-Gated (High Impact, Requires Hardfork)

### 🔒 Consensus Changes
**Priority: LOW** | **Effort: High** | **Risk: High (consensus changes)**

- [ ] **Dynamic block weight & penalty curve refinement**
  - Block "weight" (tx count/size heuristic)
  - Improved penalty function for oversized blocks
  - **Files**: `Core::getBlockReward(...)`, reward constants in `src/CryptoNoteConfig.h`
  - **Source**: [Monero](https://github.com/monero-project/monero)
  - **Note**: Requires hardfork gating

- [ ] **Adaptive difficulty (LWMA hardening and clamps)**
  - Smoother difficulty under hashrate shocks
  - Less timestamp gaming
  - **Files**: `src/CryptoNoteCore` difficulty code, `src/CryptoNoteConfig.h`
  - **Source**: [Karbo](https://github.com/seredat/karbowanec)
  - **Note**: Requires hardfork gating

- [ ] **Database abstraction and LMDB path**
  - Storage abstraction interface with LMDB backend
  - Migration tooling and integrity check commands
  - **Files**: Blockchain storage modules and CLI tooling
  - **Source**: [Monero](https://github.com/monero-project/monero)
  - **Note**: Significant refactor, may impact validation

## Implementation Notes

### Quick Wins (Start Here)
1. **P2P rate limiting** - Immediate network stability improvement
2. **Restricted-RPC defaults** - Security hardening for public nodes
3. **Build reproducibility** - Reduces contributor friction

### Medium-Term Goals
1. **Dynamic fee estimation** - Better fee market and spam resistance
2. **Sync optimization** - Faster initial sync and reorg recovery
3. **Tor/proxy support** - Enhanced privacy and security

### Long-Term Vision
1. **Pruned blockchain** - Major disk savings and faster sync
2. **Consensus improvements** - Better difficulty and block weight management
3. **Database modernization** - Performance and tooling improvements

### Risk Mitigation
- **Phase 1**: No consensus changes, can be deployed immediately
- **Phase 2**: Optional features, no breaking changes
- **Phase 3**: Requires careful hardfork planning and community coordination

### Success Metrics
- Reduced bandwidth usage per peer
- Faster initial sync times
- Fewer build failures across platforms
- Improved fee market efficiency
- Better public node security posture

## References
- [Karbo Repository](https://github.com/seredat/karbowanec) - Adaptive difficulty and P2P hardening
- [Monero Repository](https://github.com/monero-project/monero) - Privacy features and performance optimizations
- [Zano Repository](https://github.com/hyle-team/zano) - Build reproducibility and tooling
- [Conceal Repository](https://github.com/ConcealNetwork/conceal-core) - UX improvements and security hardening
