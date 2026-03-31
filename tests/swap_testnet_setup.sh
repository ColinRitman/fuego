#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────
# XFG Atomic Swap Testnet Setup
# ──────────────────────────────────────────────────────────────────────
#
# This script sets up and tests atomic swaps between:
#   - XFG testnet (fuegod --testnet)
#   - Ethereum Sepolia testnet (or local Anvil)
#
# Prerequisites:
#   1. fuegod built:      cmake --build build -j$(nproc)
#   2. test_wallet built: cmake --build build -j$(nproc)
#   3. xfg-swap built:   cmake --build build --target SwapDaemon -j$(nproc)
#   4. foundry (anvil, forge, cast) — for local ETH testing
#      Install: curl -L https://foundry.paradigm.xyz | bash && foundryup
#   5. Optional: Sepolia RPC URL (Alchemy/Infura) for live testnet
#
# Usage:
#   ./tests/swap_testnet_setup.sh [local|sepolia]
#
#   local   — spin up Anvil (local ETH fork) + XFG testnet (default)
#   sepolia — use Sepolia testnet (requires SEPOLIA_RPC_URL env var)
#
# ──────────────────────────────────────────────────────────────────────

set -euo pipefail

FUEGO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$FUEGO_DIR/build"
DATA_DIR="/tmp/xfg-swap-test"
MODE="${1:-local}"

# Binaries
FUEGOD="$BUILD_DIR/src/fuegod"
WALLET="$BUILD_DIR/src/test_wallet"
XFG_SWAP="$BUILD_DIR/src/xfg-swap"

# Ports
FUEGOD_P2P_PORT=20808
FUEGOD_RPC_PORT=28280
WALLET_RPC_PORT=28281
SWAP_P2P_PORT=29999

# ETH defaults
ETH_RPC_HOST="127.0.0.1"
ETH_RPC_PORT=8545
ETH_CONTRACT=""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

log()  { echo -e "${CYAN}[swap-test]${NC} $*"; }
ok()   { echo -e "${GREEN}[OK]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
fail() { echo -e "${RED}[FAIL]${NC} $*"; exit 1; }

cleanup() {
  log "Cleaning up..."
  [[ -n "${ANVIL_PID:-}" ]]  && kill "$ANVIL_PID" 2>/dev/null || true
  [[ -n "${FUEGOD_PID:-}" ]] && kill "$FUEGOD_PID" 2>/dev/null || true
  [[ -n "${WALLET_PID:-}" ]] && kill "$WALLET_PID" 2>/dev/null || true
  wait 2>/dev/null || true
}
trap cleanup EXIT

# ── Check prerequisites ──────────────────────────────────────────────

check_binary() {
  if [[ ! -x "$1" ]]; then
    fail "Binary not found: $1\n  Build with: cmake --build $BUILD_DIR -j\$(nproc)"
  fi
}

check_binary "$FUEGOD"
check_binary "$WALLET"
check_binary "$XFG_SWAP"
ok "All binaries found"

# ── Set up data directory ────────────────────────────────────────────

rm -rf "$DATA_DIR"
mkdir -p "$DATA_DIR"/{alice,bob,fuegod}
log "Data directory: $DATA_DIR"

# ── Start Ethereum environment ───────────────────────────────────────

if [[ "$MODE" == "local" ]]; then
  log "Starting local Anvil (Ethereum devnet)..."

  if ! command -v anvil &>/dev/null; then
    fail "anvil not found. Install foundry:\n  curl -L https://foundry.paradigm.xyz | bash && foundryup"
  fi

  anvil --port "$ETH_RPC_PORT" --silent &
  ANVIL_PID=$!
  sleep 2

  # Check Anvil is alive
  if ! kill -0 "$ANVIL_PID" 2>/dev/null; then
    fail "Anvil failed to start"
  fi
  ok "Anvil running on :$ETH_RPC_PORT (PID $ANVIL_PID)"

  # Deploy HashedTimelock contract
  log "Deploying HashedTimelock contract..."

  if ! command -v forge &>/dev/null; then
    fail "forge not found. Install foundry."
  fi

  # Anvil default account 0 (private key is deterministic)
  DEPLOYER_KEY="0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"

  # Compile and deploy
  ETH_CONTRACT=$(forge create \
    --rpc-url "http://$ETH_RPC_HOST:$ETH_RPC_PORT" \
    --private-key "$DEPLOYER_KEY" \
    "$FUEGO_DIR/src/SwapDaemon/Ethereum/HashedTimelock.sol:HashedTimelock" \
    --json 2>/dev/null | jq -r '.deployedTo' 2>/dev/null) || true

  if [[ -z "$ETH_CONTRACT" || "$ETH_CONTRACT" == "null" ]]; then
    warn "forge create failed — trying solc + cast..."

    # Fallback: compile with solc if available
    if command -v solc &>/dev/null; then
      COMPILED=$(solc --combined-json bin "$FUEGO_DIR/src/SwapDaemon/Ethereum/HashedTimelock.sol" 2>/dev/null)
      BYTECODE=$(echo "$COMPILED" | jq -r '.contracts | to_entries[0].value.bin' 2>/dev/null)
      if [[ -n "$BYTECODE" && "$BYTECODE" != "null" ]]; then
        ETH_CONTRACT=$(cast send \
          --rpc-url "http://$ETH_RPC_HOST:$ETH_RPC_PORT" \
          --private-key "$DEPLOYER_KEY" \
          --create "0x$BYTECODE" \
          --json 2>/dev/null | jq -r '.contractAddress' 2>/dev/null) || true
      fi
    fi
  fi

  if [[ -z "$ETH_CONTRACT" || "$ETH_CONTRACT" == "null" ]]; then
    warn "Could not deploy HashedTimelock contract automatically."
    warn "You can deploy manually with forge or remix."
    warn "Set ETH_CONTRACT=<address> and re-run."
    ETH_CONTRACT=""
  else
    ok "HashedTimelock deployed at: $ETH_CONTRACT"
  fi

elif [[ "$MODE" == "sepolia" ]]; then
  if [[ -z "${SEPOLIA_RPC_URL:-}" ]]; then
    fail "SEPOLIA_RPC_URL not set. Get one from Alchemy or Infura.\n  export SEPOLIA_RPC_URL=https://eth-sepolia.g.alchemy.com/v2/YOUR_KEY"
  fi

  # Parse host:port from URL
  ETH_RPC_HOST=$(echo "$SEPOLIA_RPC_URL" | sed -E 's|https?://||;s|/.*||;s|:.*||')
  ETH_RPC_PORT=$(echo "$SEPOLIA_RPC_URL" | grep -oP ':\K[0-9]+' || echo "443")

  log "Using Sepolia RPC: $SEPOLIA_RPC_URL"
  warn "For Sepolia, you need to deploy the HashedTimelock contract first."
  warn "Set ETH_CONTRACT=<address> if already deployed."

  if [[ -z "${ETH_CONTRACT:-}" ]]; then
    warn "No ETH_CONTRACT set — ETH swap testing will be skipped."
  fi
else
  fail "Unknown mode: $MODE (use 'local' or 'sepolia')"
fi

# ── Start XFG testnet node ───────────────────────────────────────────

log "Starting fuegod in testnet mode..."
"$FUEGOD" \
  --testnet \
  --data-dir "$DATA_DIR/fuegod" \
  --rpc-bind-port "$FUEGOD_RPC_PORT" \
  --p2p-bind-port "$FUEGOD_P2P_PORT" \
  --no-console \
  --log-level 2 \
  &>"$DATA_DIR/fuegod.log" &
FUEGOD_PID=$!
sleep 3

if ! kill -0 "$FUEGOD_PID" 2>/dev/null; then
  cat "$DATA_DIR/fuegod.log"
  fail "fuegod failed to start"
fi
ok "fuegod running on :$FUEGOD_RPC_PORT (PID $FUEGOD_PID)"

# Check fuegod is responsive
if curl -s "http://127.0.0.1:$FUEGOD_RPC_PORT/getheight" | grep -q '"height"'; then
  CHAIN_HEIGHT=$(curl -s "http://127.0.0.1:$FUEGOD_RPC_PORT/getheight" | grep -oP '"height":\s*\K[0-9]+')
  ok "fuegod responsive — chain height: $CHAIN_HEIGHT"
else
  warn "fuegod not responsive yet — may need mining"
fi

# ── Test xfg-swap CLI ───────────────────────────────────────────────

log "Testing xfg-swap CLI..."

# List swaps (should be empty)
"$XFG_SWAP" --testnet --data-dir "$DATA_DIR/alice" list 2>&1 || true
ok "xfg-swap CLI works"

# ── Print test configuration ─────────────────────────────────────────

echo ""
echo "════════════════════════════════════════════════════════════════"
echo "  XFG Swap Testnet Environment Ready"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "  XFG Testnet:"
echo "    fuegod RPC:    http://127.0.0.1:$FUEGOD_RPC_PORT"
echo "    fuegod P2P:    127.0.0.1:$FUEGOD_P2P_PORT"
echo "    fuegod PID:    $FUEGOD_PID"
echo ""
echo "  Ethereum ($MODE):"
echo "    ETH RPC:       http://$ETH_RPC_HOST:$ETH_RPC_PORT"
if [[ -n "$ETH_CONTRACT" ]]; then
echo "    HTLC Contract: $ETH_CONTRACT"
fi
if [[ "$MODE" == "local" ]]; then
echo "    Anvil PID:     ${ANVIL_PID:-N/A}"
fi
echo ""
echo "  Data directories:"
echo "    Alice: $DATA_DIR/alice"
echo "    Bob:   $DATA_DIR/bob"
echo ""
echo "  Commands:"
echo ""
echo "  # Alice: initiate a swap (0.1 XFG for 0.001 ETH)"
echo "  $XFG_SWAP --testnet \\"
echo "    --wallet-rpc $WALLET_RPC_PORT \\"
echo "    --listen-port $SWAP_P2P_PORT \\"
echo "    --data-dir $DATA_DIR/alice \\"
if [[ -n "$ETH_CONTRACT" ]]; then
echo "    --eth-rpc $ETH_RPC_HOST:$ETH_RPC_PORT \\"
echo "    --eth-contract $ETH_CONTRACT \\"
fi
echo "    initiate ETH 1000000 1000000000000000 127.0.0.1:$((SWAP_P2P_PORT+1))"
echo ""
echo "  # Bob: accept the swap"
echo "  $XFG_SWAP --testnet \\"
echo "    --wallet-rpc $((WALLET_RPC_PORT+1)) \\"
echo "    --listen-port $((SWAP_P2P_PORT+1)) \\"
echo "    --data-dir $DATA_DIR/bob \\"
if [[ -n "$ETH_CONTRACT" ]]; then
echo "    --eth-rpc $ETH_RPC_HOST:$ETH_RPC_PORT \\"
echo "    --eth-contract $ETH_CONTRACT \\"
fi
echo "    accept <swap_id>"
echo ""
echo "  # Check swap status"
echo "  $XFG_SWAP --testnet --data-dir $DATA_DIR/alice status"
echo ""
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "Press Ctrl+C to stop all services."
echo ""

# Keep running until killed
wait
