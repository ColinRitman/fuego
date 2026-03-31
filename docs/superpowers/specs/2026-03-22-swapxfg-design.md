# SwapXFG Design Spec

**Date:** 2026-03-22
**Status:** Draft
**Branch:** HTLC

## Overview

SwapXFG is Fuego's unified cross-chain atomic swap trading terminal. It replaces the three separate Go TUI clients (`xfg-eth-swap`, `xfg-xmr-swap`, `xfg-bch-swap`) and the broken C++ ncurses `SwapTerminal` with a single, polished multi-pair trading interface. All swap pairs use **adaptor signatures** on the XFG side — swap transactions are indistinguishable from normal ring-sig spends, preserving privacy. Counterparty chains use their native HTLC mechanisms (Solidity contracts, Solana Programs), except XMR which uses adaptor sigs on both sides (COMIT protocol). BCH has been dropped in favor of XMR — since adaptor sigs are built for all pairs anyway, XMR support comes at no additional crypto cost.

SwapXFG has two surfaces:
1. **`swapxfg` Go TUI binary** — full-featured terminal trading client for all pairs
2. **EFier-hosted web UI** — read-only swap explorer embedded in `fuegod`

## Design Principles

- **All pairs in one view.** ( SOL, ETH, XMR )  incl erc20 HEAT & LUSD tokens via ETH; all visible simultaneously.
- **Privacy by default.** Swaps go P2P. EFiers relay gossip, never see user data.
- **Standalone + wallet-connected.** Read-only explorer mode without a wallet, full trading with one.
- **EFiers are the only public infra.** Free community service funded by consensus rewards.
- **Sleek, fast, professional.** Terminal UI that feels like a proper trading terminal.

## Non-Goals (This Spec)

- COLD3 rail / Ethereum settlement contracts (separate spec)
- Swap fee pool design (separate spec)
- Dandelion++ P2P privacy (v11 priority, separate spec)
- Solana HTLC Program (Rust) — Keccak-256 hashlock, separate deliverable
- Ed25519 adaptor signature + DLEQ proof implementation — separate crypto spec
- XMR COMIT protocol state machine — separate spec

## Prerequisites (daemon-side work before swapxfg)

- **Ed25519 adaptor signatures + DLEQ proofs (~2000 LoC).** All XFG-side swap operations use adaptor sigs instead of on-chain HTLCs. Swap outputs are normal `KeyOutput` to Musig2 joint addresses — indistinguishable from regular transactions.
- **Extend pair validation to support HEAT (3), LUSD (4).** `SwapOfferRelay::validateOffer()` currently rejects `pair > 2`. Pair 2 (XMR, replacing BCH) already passes validation. Pairs 3-4 need P2P serialization, wallet pair mapping, seed rates, and price sources.
- **Add wallet RPC endpoints:** `initiate_swap`, `complete_swap`, `refund_swap`, `sign_offer`, `getaddress` — adaptor sig swap lifecycle replaces the HTLC-based `create_htlc`/`claim_htlc`/`refund_htlc` endpoints.
- **Counterparty-chain contracts:** Solana Program (Anchor, Keccak-256), Ethereum HashedTimelock.sol (exists). XMR pair uses COMIT adaptor sigs on both sides (no HTLC needed on either chain).

---

## 1. SwapXFG TUI (`swapxfg` binary)

### 1.1 Tech Stack

| Component | Library | Purpose |
|-----------|---------|---------|
| TUI framework | bubbletea v1.3+ | Model-update-view architecture |
| Styling | lipgloss v1.1+ | Per-cell color, borders, layout |
| Charts | bubbletea-chart or custom | Candlestick rendering |
| Tables | bubbles/table | Orderbook, trade tape |
| Text input | bubbles/textinput | Order entry, commands |
| Tabs/nav | bubbles/tabs or custom | Pair switching |
| Viewport | bubbles/viewport | Scrollable regions |
| Spinner | bubbles/spinner | Loading/connection states |
| Key bindings | bubbles/key | Hotkey navigation |
| WebSocket | gorilla/websocket | MetaMask bridge communication |
| HTTP server | net/http (stdlib) | MetaMask bridge page |
| HTTP client | net/http (stdlib) | Daemon + wallet RPC |

### 1.2 Layout

```
┌────────────────────────────────────────────────────────────────────────┐
│ ◆ SWAPHUB    SOL/XFG ▲0.46  ETH/XFG ▲1.82  XMR/XFG ▲21.3  HEAT 1:10M   │ 
│              LUSD $0.01     BLK 184209                                 │
├─────────────────────────────────────────┬──────────────────────────────┤
│                                         │         ORDER BOOK           │
│          CANDLESTICK CHART              │  ─────────────────────────── │
│          (selected pair)                │  ASK  0.00047   800.0 XFG    │
│                                         │  ASK  0.00046    80.0 XFG    │
│    ╻                                    │  ASK  0.00045     8.0 XFG    │
│    ┃  ╻                                 │  ━━━ spread 0.00001 ━━━━━━   │
│   ╺┫  ┃ ╻                               │  BID  0.00044   800.0 XFG    │
│    ┃ ╺┫ ┃                               │  BID  0.00043    80.0 XFG    │
│    ╹  ┃╺┫                               │  BID  0.00042     0.8 XFG    │
│       ╹ ┃                               ├───────────────────────────── │
│         ╹                               │         TRADE TAPE           │
│                                         │  0.00046  80 XFG  BUY  2m    │
│   TWAP: 0.00045  Composite: 0.00045    │  0.00045   8 XFG  SELL 5m     │
│                                         │  0.00044 800 XFG  BUY  11m   │
├─────────────────────────────────────────┴──────────────────────────────┤
│ > _                                     BAL 420.00 XFG  ■  :18180      │
└────────────────────────────────────────────────────────────────────────┘
```

**Market ticker bar (top):** All 5 active pairs with live prices and directional arrows. HEAT conversion rate and LUSD price always visible. Current block height. Updates every tick.

**Chart area (left 60%):** Candlestick chart for the selected pair. Price scale on right edge. TWAP and composite price below chart. Renders using box-drawing characters with green (bullish) / red (bearish) coloring.

**Order book (right top):** Asks sorted price-descending (cheapest near spread), bids sorted price-descending (best bid near spread). Color-coded red/green. Shows rate, XFG amount, and depth bar.

**Trade tape (right bottom):** Recent trades with rate, amount, buy/sell indicator, and age. Color-coded by direction.

**Command bar (bottom):** Text input for commands. Balance display. Connection status indicator (green dot = connected, red = disconnected).

### 1.3 Hotkeys

| Key | Action |
|-----|--------|
| `0` | Select SOL/XFG pair |
| `1` | Select ETH/XFG pair |
| `2` | Select XMR/XFG pair |
| `3` | Select LUSD/XFG pair |
| `4` | Select HEAT/XFG pair |
| `Tab` | Cycle through pairs |
| `b` | Open buy order entry |
| `s` | Open sell order entry |
| `h` | Show HTLC monitor |
| `/` | Focus command input |
| `q` / `Esc` | Quit (or back from sub-view) |
| `r` | Force refresh all data |
| `?` | Help overlay |

### 1.4 Commands

```
accept <offer_id>       Accept a swap offer for the selected pair (see Accept Flow below)
cancel <offer_id>       Cancel own offer (wallet signs cancellation)
offer <amount> <rate>   Submit a new offer for the selected pair (wallet signs offer)
swap                    Show active swap status (adaptor sig state machine)
swap <id>               Query specific swap by ID
pair <name>             Switch active pair (sol, eth, xmr, heat, lusd)
connect wallet          Open wallet connection prompt
connect metamask        Open MetaMask bridge
connect phantom         Open Phantom bridge (Solana)
help                    Show command help
```

**Accept flow (end-to-end, adaptor sig protocol):**
1. User types `accept <offer_id>` — swapxfg fetches offer details from daemon RPC
2. Swaphub calls wallet RPC `initiate_swap` with: offerId, pair, amount, maker pubkey
3. Wallet generates adaptor sig components: secret scalar `t`, Musig2 joint address, DLEQ proof
4. Swaphub exchanges adaptor parameters with counterparty via P2P (key exchange + nonce commitment)
5. Maker locks counterparty-side funds (HTLC on ETH/SOL, or adaptor lock on XMR)
6. Taker locks XFG to Musig2 joint address — **looks like a normal transaction on-chain**
7. Maker claims counterparty HTLC (reveals preimage) or completes XMR adaptor
8. Taker extracts secret scalar from published signature, completes XFG spend
9. Swaphub monitors swap state machine — displays progress, timeout warnings

**Offer submission flow:**
1. User types `offer <amount> <rate>` — swapxfg constructs offer payload (pair from selected tab)
2. Swaphub calls wallet RPC `sign_offer` with: xfgAmount, rateNum, pair, ttlBlocks
3. Wallet constructs offerId hash, signs with spend key, returns: offerId, makerPubKey, signature
4. Swaphub submits to daemon `/submitswap` with the signed payload

### 1.5 Connection Architecture

```
swapxfg
├── Fuego daemon RPC (:18180 mainnet / :28280 testnet)
│   ├── /getswapoffers   → orderbook data (read)
│   ├── /getswapprice    → TWAP + composite pricing (read)
│   ├── /getswaptrades   → trade history (read)
│   ├── /gethtlc         → HTLC status (read)
│   ├── /gethtlccount    → HTLC count (read)
│   ├── /getinfo         → node status, block height (read)
│   ├── /submitswap      → submit signed offer (write, via wallet sign_offer)
│   └── /cancelswap      → cancel own offer (write, via wallet signature)
│
├── Fuego wallet RPC (:18182 mainnet / :28282 testnet)  [optional]
│   ├── getbalance       → XFG balance
│   ├── getaddress       → receive address
│   ├── initiate_swap    → start adaptor sig swap (generate joint address, DLEQ proof)
│   ├── complete_swap    → extract secret, complete XFG spend
│   ├── refund_swap      → reclaim after timeout (cooperative or unilateral)
│   └── sign_offer       → sign offer with wallet keys
│
├── MetaMask bridge (localhost:random)  [optional]
│   ├── WebSocket ←→ swapxfg TUI
│   ├── Serves bridge.html (tiny page with ethers.js)
│   ├── ETH/HEAT/LUSD balance queries
│   ├── ERC-20 HTLC contract interactions (counterparty side)
│   └── Transaction signing via MetaMask popup
│
├── Phantom bridge (localhost:random)  [optional]
│   ├── WebSocket ←→ swapxfg TUI
│   ├── Serves sol-bridge.html (tiny page with @solana/web3.js)
│   ├── SOL balance queries
│   ├── Solana HTLC Program interactions (counterparty side)
│   └── Transaction signing via Phantom popup
│
└── Monero wallet RPC (:18081)  [optional]
    ├── XMR balance + transfer
    ├── COMIT adaptor sig operations (both sides use adaptor sigs)
    └── Key exchange for 2-of-2 joint address
```

### 1.6 CLI Flags

```
swapxfg [flags]

Connection:
  --daemon, -d      Fuego daemon RPC endpoint (default: http://127.0.0.1:18180)
                    Alias: --efier, -e (connects to any node, not just EFiers)
  --wallet, -w      Fuego wallet RPC endpoint (optional)
  --testnet         Use testnet defaults (:20808 / :28280)
  --eth-rpc         Ethereum RPC endpoint (optional, for ETH/HEAT/LUSD)
  --sol-rpc         Solana RPC endpoint (optional, for SOL pair)
  --xmr-rpc         Monero wallet RPC endpoint (optional, for XMR pair)

Display:
  --pair, -p        Starting pair (sol, eth, xmr, heat, lusd; default: sol)
  --no-splash       Skip splash screen
  --compact         Compact layout for small terminals

Authentication:
  --wallet-user     Wallet RPC username (if auth enabled)
  --wallet-password Wallet RPC password (if auth enabled)

MetaMask bridge:
  --bridge-port     Port for MetaMask bridge server (default: random)
  --no-bridge       Disable MetaMask bridge
```

### 1.7 Splash Screen

Single unified splash with Fuego fire animation (doom fire algorithm from existing clients). No per-pair logo — SwapXFG is the brand.

```
        ◆
       ◆◆◆
      ◆◆◆◆◆
     ◆◆◆◆◆◆◆
    ◆◆◆◆◆◆◆◆◆
  ░▒▓█ fire ████
  ░▒▓████████▓▒░

  ███████╗██╗    ██╗ █████╗ ██████╗ ██╗  ██╗██╗   ██╗██████╗
  ██╔════╝██║    ██║██╔══██╗██╔══██╗██║  ██║██║   ██║██╔══██╗
  ███████╗██║ █╗ ██║███████║██████╔╝███████║██║   ██║██████╔╝
  ╚════██║██║███╗██║██╔══██║██╔═══╝ ██╔══██║██║   ██║██╔══██╗
  ███████║╚███╔███╔╝██║  ██║██║     ██║  ██║╚██████╔╝██████╔╝
  ╚══════╝ ╚══╝╚══╝ ╚═╝  ╚═╝╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚═════╝

  cross-chain atomic swaps  ·  press any key
```

Auto-advances after 3 seconds or on any keypress.

### 1.8 Data Flow

**Refresh cycle:** Every 5 seconds (configurable), swapxfg fetches:
1. Offers for active pairs (5 parallel calls to `/getswapoffers` — pairs 0-4)
2. Trades for the selected pair (`/getswaptrades`)
3. Prices for active pairs (5 parallel calls to `/getswapprice`)
4. Wallet balance (if connected)
5. Swap state (if active adaptor sig swap in progress)

Offer and price fetches are parallelized via goroutines to avoid sequential latency on the daemon's RPC. Total: ~13 requests per tick, but only 3-4 sequential round-trips due to parallelism. If this proves too heavy, a future daemon-side `/getallswapoffers` batch endpoint can reduce it to 1 call.

**Candlestick construction:** Trades are bucketed into 5-minute candles client-side. OHLCV calculated per bucket. Chart shows last N candles that fit the terminal width.

**Orderbook sorting:**
- Asks: displayed bottom-to-top, cheapest nearest the spread line
- Bids: displayed top-to-bottom, best bid nearest the spread line
- Own offers highlighted with cyan marker

### 1.9 MetaMask Bridge Detail

The bridge is a minimal localhost HTTP server that swapxfg starts when `--eth-rpc` is not provided and Ethereum pairs are active. A separate bridge page handles Solana via Phantom.

**bridge.html** (~50 lines, Ethereum):
- Loads ethers.js from bundled JS (no CDN — works offline)
- Connects to `window.ethereum` (MetaMask/Rabby/Frame)
- Opens WebSocket back to swapxfg on the same port
- Handles messages: `getBalance`, `getNetwork`, `sendTransaction`, `callContract`
- Shows minimal status: "Connected to SwapXFG" + chain ID

**sol-bridge.html** (~50 lines, Solana):
- Loads @solana/web3.js from bundled JS (no CDN — works offline)
- Connects to `window.solana` (Phantom/Solflare)
- Opens WebSocket back to swapxfg on the same port
- Handles messages: `getBalance`, `getSlot`, `sendTransaction`, `callProgram`
- Shows minimal status: "Connected to SwapXFG" + cluster

**Security:**
- Binds to `127.0.0.1` only (not `0.0.0.0`)
- Random port unless overridden
- WebSocket origin check
- No external network calls from bridge page

### 1.10 Graceful Degradation

| Missing connection | Effect |
|-------------------|--------|
| No wallet RPC | Orderbook visible, order submission disabled ("connect wallet to trade") |
| No MetaMask | ETH/HEAT/LUSD offers visible, can't complete counterparty-side HTLC claim |
| No Phantom | SOL offers visible, can't complete counterparty-side HTLC claim |
| No XMR wallet | XMR offers visible, can't initiate XMR-side adaptor sig lock |
| No daemon RPC | Nothing works — show connection error with retry |

---

## 2. EFier Web UI

### 2.1 Embedding

Static HTML/JS/CSS embedded into the `fuegod` binary. The RpcServer's `processRequest()` is extended: if the request path doesn't match a JSON-RPC endpoint, serve the embedded static files.

```cpp
// In RpcServer::processRequest():
if (url == "/" || url.find("/static/") == 0) {
    serveEmbeddedFile(url, response);
    return true;
}
// ... existing JSON-RPC handling
```

### 2.2 Web UI Features

- **All-pairs dashboard** — same layout concept as the TUI but in a browser
- **Price charts** — TradingView charting library (lightweight version, previously used on usexfg.org). Candlestick, line, area charts with standard indicators. Data fed from `/getswaptrades` history.
- **Live orderbook** — polls `/getswapoffers` every 5 seconds, standard depth visualization
- **Trade tape** — recent trades with age
- **Price display** — TWAP, composite, cross-pair implied USD
- **HTLC explorer** — browse on-chain HTLCs by index
- **Network stats** — connected peers, offer count, block height

### 2.3 What It Does NOT Do

- No order submission
- No wallet connection
- No user accounts or authentication
- No cookies, no analytics, no external fetches
- No JavaScript that phones home

### 2.4 Tech Stack

- Vanilla HTML + CSS + JS (no framework — keeps bundle tiny)
- TradingView Lightweight Charts for price charts (previously deployed on usexfg.org, check usexfg GitHub for existing copy)
- Standard HTML/CSS for orderbook, trade tape, stats
- Total embedded size target: < 500KB gzipped (TradingView lib adds ~45KB)

### 2.5 Serving

Every `fuegod` instance serves the web UI. EFiers automatically host it. Regular nodes also serve it (useful for personal use). CORS is already supported in RpcServer.

---

## 3. Wallet Integration

### 3.1 `fire_wallet` / `test_wallet` `swap` Command

The wallet's `swap` command launches `swapxfg` as a child process instead of the ncurses SwapTerminal:

The wallet constructs argv and launches `swapxfg` via `fork()/execvp()` (not `system()` — avoids shell injection). It passes:
- `--efier http://<daemon_host>:<daemon_port>` from the wallet's existing connection
- `--wallet http://127.0.0.1:<wallet_rpc_port>` (requires plumbing the wallet RPC port through to simple_wallet)
- `--wallet-user` / `--wallet-password` if wallet RPC auth is configured
- `--testnet` if on testnet
- `--pair <pair>` if user passed a pair argument to the `swap` command

The wallet blocks on `waitpid()` until swapxfg exits, then resumes the command prompt.

This completely replaces the ncurses SwapTerminal and its dispatcher deadlock issues.

### 3.2 Wallet RPC Requirements

The wallet RPC server (`WalletRpcServer`) needs these endpoints for swapxfg:

| Endpoint | Status | Notes |
|----------|--------|-------|
| `getbalance` | Exists | Already in WalletRpcServer |
| `getaddress` | Needs adding | Return wallet's public address |
| `transfer` | Exists | Standard send |
| `create_htlc` | Needs adding | Wraps WalletLegacy::createHtlc(amount, fee, recipientKey, hashLock, timeoutHeight) |
| `claim_htlc` | Needs adding | Wraps WalletLegacy::claimHtlc(htlcIndex, preimage) |
| `refund_htlc` | Needs adding | Wraps WalletLegacy::refundHtlc(htlcIndex) |
| `sign_offer` | Needs adding | Constructs offerId, signs with spend key. Request: {xfgAmount, rateNum, pair, ttlBlocks}. Response: {offerId, makerPubKey, signature, timestamp} |

---

## 4. Infrastructure Model

### 4.1 Roles

| Role | Runs | Serves | Cost |
|------|------|--------|------|
| EFier | `fuegod --elderfier-key` | Web UI + JSON-RPC + P2P relay | Free (funded by consensus rewards) |
| Personal node | `fuegod` | JSON-RPC + P2P relay + web UI (for personal use) | Self-hosted |
| User (standalone) | `swapxfg --efier <efier>` | Nothing — consumes data | Free |
| User (wallet) | `fire_wallet` → `swapxfg` | Nothing — consumes data | Free |

### 4.2 Privacy Model

- **Orderbook data is public.** All offers are P2P-gossiped to every node. Viewing the orderbook reveals nothing.
- **Offer submission goes through user's connected node.** If that's an EFier, the EFier sees the submission IP. Dandelion++ (v11) mitigates this by randomizing the apparent origin.
- **Wallet operations are local.** Signing and HTLC creation happen in `fire_wallet` on the user's machine. Keys never leave the wallet process.
- **MetaMask bridge is localhost-only.** No network exposure.
- **EFier web UI has no tracking.** No cookies, no analytics, no external fetches.

### 4.3 Dandelion++ (v11, future context)

Not part of this spec. For reference, the planned approach:
- **Stem phase:** New offers forwarded to 1 random peer for 2-4 hops before broadcast.
- **Fluff phase:** After stem, normal broadcast via `externalRelayNotifyToAll()`.
- **Result:** No single node can determine which node originated an offer.

See separate v11 spec when available.

---

## 5. Pair Configuration

| Pair ID | Display | Assets | XFG side | Counterparty side | Status |
|---------|---------|--------|----------|-------------------|--------|
| 0 | SOL/XFG | SOL ↔ XFG | Adaptor sig (normal KeyOutput) | Solana HTLC Program (Keccak-256) | Active |
| 1 | ETH/XFG | ETH ↔ XFG | Adaptor sig (normal KeyOutput) | HashedTimelock.sol (Keccak-256) | Active |
| 2 | XMR/XFG | XMR ↔ XFG | Adaptor sig (normal KeyOutput) | Adaptor sig (COMIT, both sides) | Active |
| 3 | HEAT/XFG | HEAT ↔ XFG | Adaptor sig (normal KeyOutput) | ERC-20 HTLC (Keccak-256) | Active |
| 4 | LUSD/XFG | LUSD ↔ XFG | Adaptor sig (normal KeyOutput) | ERC-20 HTLC (Keccak-256) | Active |

All XFG-side swap operations use Ed25519 adaptor signatures. Swap outputs are normal `KeyOutput` to Musig2 joint addresses — indistinguishable from regular transactions on-chain. HEAT and LUSD share the same ERC-20 HTLC contract on Ethereum. SOL uses a dedicated Anchor Program with Keccak-256 hashlock. XMR uses the COMIT protocol with adaptor sigs on both sides (no HTLC on either chain). Both Fuego and Solana use Ed25519 keys. BCH support (pair 2 slot, `HtlcScript.h/cpp`) preserved in codebase for potential future activation.

---

## 6. Project Structure

```
swapxfg/
├── main.go                 CLI entry, flag parsing
├── go.mod
├── app/
│   ├── config.go           Config struct, defaults, flag mapping
│   ├── app.go              Run() orchestrator: splash → TUI
│   ├── splash.go           Doom fire + SwapXFG ASCII art
│   ├── tui.go              Main TUI model (bubbletea)
│   ├── layout.go           Layout calculations, responsive sizing
│   ├── chart.go            Candlestick chart rendering
│   ├── orderbook.go        Order book component
│   ├── tape.go             Trade tape component
│   ├── ticker.go           Top market ticker bar
│   ├── input.go            Command input + processing
│   ├── swap_monitor.go     Adaptor sig swap state monitor view
│   ├── styles.go           lipgloss styles, color palette
│   ├── rpc.go              Fuego daemon RPC client
│   ├── wallet_rpc.go       Fuego wallet RPC client
│   ├── eth_bridge.go       MetaMask WebSocket bridge
│   ├── sol_bridge.go       Phantom WebSocket bridge (Solana)
│   ├── sol_rpc.go          Solana RPC client
│   └── xmr_rpc.go          Monero wallet RPC client
├── bridge/
│   ├── bridge.html         MetaMask bridge page (embedded)
│   └── sol-bridge.html     Phantom bridge page (embedded)
└── README.md
```

---

## 7. Color Palette

Fuego-branded, consistent across all pairs:

| Element | Color | Hex |
|---------|-------|-----|
| Background | Terminal default | — |
| Primary accent | Fuego orange | `#FF5500` |
| Bullish / buy | Green | `#00CC66` |
| Bearish / sell | Red | `#FF3344` |
| Spread line | Yellow | `#FFAA00` |
| Muted text | Gray | `#555555` |
| Active pair tab | White on orange | `#FFFFFF` on `#FF5500` |
| Inactive pair tab | Gray | `#777777` |
| Own offer marker | Cyan | `#00CCCC` |
| Swap active | Pulsing yellow | `#FFDD00` |
| Connection OK | Green dot | `#00FF00` |
| Connection lost | Red dot | `#FF0000` |

---

## 8. Milestones

### M1: Core TUI (standalone read-only)
- Single binary, connects to daemon RPC
- All 5 pairs displayed in ticker
- Candlestick chart for selected pair
- Orderbook + trade tape
- Splash screen with fire animation

### M2: Wallet integration + adaptor sig swaps
- Connect to wallet RPC
- Submit/cancel offers
- Initiate/complete/refund adaptor sig swaps on XFG side
- Balance display
- Swap state machine monitor (key exchange → lock → claim → complete)

### M3: MetaMask + Phantom bridges
- Localhost bridge servers (MetaMask for ETH/HEAT/LUSD, Phantom for SOL)
- ETH/HEAT/LUSD balance queries + ERC-20 HTLC contract interaction (counterparty side)
- SOL balance queries + Solana HTLC Program interaction (counterparty side)
- Complete counterparty-side swaps from TUI

### M4: XMR connection (COMIT protocol)
- Monero wallet RPC integration
- COMIT adaptor sig protocol (adaptor sigs on both XFG and XMR sides)
- XMR balance + transfer operations
- 2-of-2 joint address key exchange

### M5: EFier web UI
- Static SPA embedded in fuegod
- Charts, orderbook, trades, prices
- Swap explorer (active/completed swaps, anonymity set stats)
- Served by all EFier nodes automatically

### M6: Wallet `swap` command update
- Replace ncurses SwapTerminal with swapxfg subprocess
- Pass connection flags automatically
- Works for both fire_wallet and test_wallet
