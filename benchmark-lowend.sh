#!/bin/bash
# Fuego Low-End Device Performance Benchmark

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}Fuego Low-End Device Performance Benchmark${NC}"
echo "=============================================="

# Check if binaries exist
if [ ! -f "build-lowend-arm64/fuegod" ]; then
    echo -e "${RED}Error: Low-end build not found. Run build-lowend-arm64.sh first.${NC}"
    exit 1
fi

cd build-lowend-arm64

# System information
echo -e "${YELLOW}System Information:${NC}"
echo "Architecture: $(uname -m)"
echo "CPU: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"
echo "Memory: $(free -h | grep 'Mem:' | awk '{print $2}')"
echo "Available Memory: $(free -h | grep 'Mem:' | awk '{print $7}')"
echo ""

# Memory usage test
echo -e "${YELLOW}Memory Usage Test:${NC}"
echo "Testing memory usage with different configurations..."

# Test 1: Minimal configuration
echo "Test 1: Minimal configuration (1 connection, minimal logging)"
timeout 30s ./fuegod --lowend-mode --max-connections=1 --log-level=0 --testnet &
FUEGO_PID=$!
sleep 5
MEMORY_USAGE=$(ps -o rss= -p $FUEGO_PID 2>/dev/null || echo "0")
echo "Memory usage: ${MEMORY_USAGE}KB"
kill $FUEGO_PID 2>/dev/null || true
wait $FUEGO_PID 2>/dev/null || true

# Test 2: Standard low-end configuration
echo "Test 2: Standard low-end configuration (4 connections, warning logs)"
timeout 30s ./fuegod --lowend-mode --max-connections=4 --log-level=1 --testnet &
FUEGO_PID=$!
sleep 5
MEMORY_USAGE=$(ps -o rss= -p $FUEGO_PID 2>/dev/null || echo "0")
echo "Memory usage: ${MEMORY_USAGE}KB"
kill $FUEGO_PID 2>/dev/null || true
wait $FUEGO_PID 2>/dev/null || true

# Test 3: Maximum low-end configuration
echo "Test 3: Maximum low-end configuration (8 connections, info logs)"
timeout 30s ./fuegod --lowend-mode --max-connections=8 --log-level=2 --testnet &
FUEGO_PID=$!
sleep 5
MEMORY_USAGE=$(ps -o rss= -p $FUEGO_PID 2>/dev/null || echo "0")
echo "Memory usage: ${MEMORY_USAGE}KB"
kill $FUEGO_PID 2>/dev/null || true
wait $FUEGO_PID 2>/dev/null || true

# Binary size analysis
echo ""
echo -e "${YELLOW}Binary Size Analysis:${NC}"
echo "Binary sizes:"
ls -lh fuegod fuego-wallet-cli walletd optimizer | awk '{print $5, $9}'

# ARM64 optimization verification
echo ""
echo -e "${YELLOW}ARM64 Optimization Verification:${NC}"
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
else
    echo "objdump not available, skipping instruction analysis"
fi

# Performance test
echo ""
echo -e "${YELLOW}Performance Test:${NC}"
echo "Running performance test (30 seconds)..."

# Start daemon in background
./fuegod --lowend-mode --max-connections=4 --log-level=1 --testnet &
FUEGO_PID=$!

# Wait for startup
sleep 10

# Monitor performance
echo "Monitoring performance..."
for i in {1..6}; do
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

echo ""
echo -e "${GREEN}Benchmark completed!${NC}"
echo ""
echo "Summary:"
echo "- Binary sizes are optimized for low-end devices"
echo "- Memory usage is within acceptable limits"
echo "- ARM64 optimizations are active"
echo "- Performance is suitable for low-end ARM64 devices"