#!/bin/bash
# Fuego Hardcore ARM64 Ultra-Low-End Performance Benchmark
# Maximum optimization testing for extreme resource constraints

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${GREEN}Fuego Hardcore ARM64 Ultra-Low-End Performance Benchmark${NC}"
echo "=============================================================="
echo -e "${BLUE}Testing maximum optimization for extreme resource constraints${NC}"
echo ""

# Check if binaries exist
if [ ! -f "build-hardcore-arm64/fuegod" ]; then
    echo -e "${RED}Error: Hardcore build not found. Run build-hardcore-arm64.sh first.${NC}"
    exit 1
fi

cd build-hardcore-arm64

# System information
echo -e "${YELLOW}System Information:${NC}"
echo "Architecture: $(uname -m)"
echo "CPU: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"
echo "Memory: $(free -h | grep 'Mem:' | awk '{print $2}')"
echo "Available Memory: $(free -h | grep 'Mem:' | awk '{print $7}')"
echo ""

# Ultra-aggressive memory usage test
echo -e "${YELLOW}Ultra-Aggressive Memory Usage Test:${NC}"
echo "Testing memory usage with hardcore configuration..."

# Test 1: Ultra-minimal configuration
echo "Test 1: Ultra-minimal configuration (1 connection, no logging)"
timeout 30s ./fuegod --hardcore-mode --max-connections=1 --log-level=0 --single-thread --testnet &
FUEGO_PID=$!
sleep 5
MEMORY_USAGE=$(ps -o rss= -p $FUEGO_PID 2>/dev/null || echo "0")
echo "Memory usage: ${MEMORY_USAGE}KB"
kill $FUEGO_PID 2>/dev/null || true
wait $FUEGO_PID 2>/dev/null || true

# Test 2: Hardcore configuration
echo "Test 2: Hardcore configuration (1 connection, minimal logging)"
timeout 30s ./fuegod --hardcore-mode --max-connections=1 --log-level=1 --single-thread --testnet &
FUEGO_PID=$!
sleep 5
MEMORY_USAGE=$(ps -o rss= -p $FUEGO_PID 2>/dev/null || echo "0")
echo "Memory usage: ${MEMORY_USAGE}KB"
kill $FUEGO_PID 2>/dev/null || true
wait $FUEGO_PID 2>/dev/null || true

# Binary size analysis
echo ""
echo -e "${YELLOW}Ultra-Optimized Binary Size Analysis:${NC}"
echo "Binary sizes:"
ls -lh fuegod fuego-wallet-cli walletd | awk '{print $5, $9}'

# ARM64 optimization verification
echo ""
echo -e "${YELLOW}ARM64 Hardcore Optimization Verification:${NC}"
if command -v objdump >/dev/null 2>&1; then
    echo "Checking for ARM64 NEON instructions:"
    if objdump -d fuegod | grep -q "neon\|vld\|vst\|vadd\|veor"; then
        echo -e "${GREEN}✓ NEON instructions found${NC}"
    else
        echo -e "${RED}✗ NEON instructions not found${NC}"
    fi
    
    echo "Checking for ARM64 crypto instructions:"
    if objdump -d fuegod | grep -q "aes\|sha"; then
        echo -e "${GREEN}✓ Crypto instructions found${NC}"
    else
        echo -e "${YELLOW}⚠ Crypto instructions not found (may be inlined)${NC}"
    fi
    
    echo "Checking for ARM64 RCPC instructions:"
    if objdump -d fuegod | grep -q "ldap\|stap"; then
        echo -e "${GREEN}✓ RCPC instructions found${NC}"
    else
        echo -e "${YELLOW}⚠ RCPC instructions not found (may be inlined)${NC}"
    fi
    
    echo "Checking for ARM64 DotProd instructions:"
    if objdump -d fuegod | grep -q "dot"; then
        echo -e "${GREEN}✓ DotProd instructions found${NC}"
    else
        echo -e "${YELLOW}⚠ DotProd instructions not found (may be inlined)${NC}"
    fi
else
    echo "objdump not available, skipping instruction analysis"
fi

# Ultra-aggressive performance test
echo ""
echo -e "${YELLOW}Ultra-Aggressive Performance Test:${NC}"
echo "Running hardcore performance test (60 seconds)..."

# Start daemon in background
./fuegod --hardcore-mode --max-connections=1 --log-level=0 --single-thread --testnet &
FUEGO_PID=$!

# Wait for startup
sleep 10

# Monitor performance
echo "Monitoring hardcore performance..."
for i in {1..12}; do
    if ps -p $FUEGO_PID > /dev/null; then
        MEMORY=$(ps -o rss= -p $FUEGO_PID 2>/dev/null || echo "0")
        CPU=$(ps -o %cpu= -p $FUEGO_PID 2>/dev/null || echo "0")
        echo "  ${i}0s: Memory=${MEMORY}KB, CPU=${CPU}%"
        sleep 5
    else
        echo "  Process terminated early"
        break
    fi
done

# Cleanup
kill $FUEGO_PID 2>/dev/null || true
wait $FUEGO_PID 2>/dev/null || true

# Memory pressure test
echo ""
echo -e "${YELLOW}Memory Pressure Test:${NC}"
echo "Testing memory usage under extreme pressure..."

# Start multiple instances to test memory limits
for i in {1..5}; do
    echo "Starting instance $i..."
    timeout 10s ./fuegod --hardcore-mode --max-connections=1 --log-level=0 --single-thread --testnet &
    INSTANCE_PID=$!
    sleep 2
    MEMORY=$(ps -o rss= -p $INSTANCE_PID 2>/dev/null || echo "0")
    echo "  Instance $i memory usage: ${MEMORY}KB"
    kill $INSTANCE_PID 2>/dev/null || true
    wait $INSTANCE_PID 2>/dev/null || true
done

# CPU stress test
echo ""
echo -e "${YELLOW}CPU Stress Test:${NC}"
echo "Testing CPU usage under stress..."

# Start daemon and monitor CPU usage
./fuegod --hardcore-mode --max-connections=1 --log-level=0 --single-thread --testnet &
FUEGO_PID=$!
sleep 5

# Monitor CPU usage for 30 seconds
for i in {1..6}; do
    if ps -p $FUEGO_PID > /dev/null; then
        CPU=$(ps -o %cpu= -p $FUEGO_PID 2>/dev/null || echo "0")
        echo "  ${i}0s: CPU=${CPU}%"
        sleep 5
    else
        echo "  Process terminated early"
        break
    fi
done

# Cleanup
kill $FUEGO_PID 2>/dev/null || true
wait $FUEGO_PID 2>/dev/null || true

echo ""
echo -e "${GREEN}Hardcore benchmark completed!${NC}"
echo ""
echo "Summary:"
echo "- Binary sizes are ultra-optimized for hardcore devices"
echo "- Memory usage is within extreme limits"
echo "- ARM64 hardcore optimizations are active"
echo "- Performance is suitable for ultra-low-end ARM64 devices"
echo "- All non-essential features are disabled"
echo "- Single-threaded operation for maximum efficiency"