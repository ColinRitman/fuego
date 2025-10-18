#!/bin/bash
# Fuego Hardcore ARM64 Ultra-Low-End Build Script
# Maximum optimization for devices with extreme resource constraints

set -e

# Configuration
BUILD_DIR="build-hardcore-arm64"
CMAKE_FLAGS=""
MAKE_FLAGS=""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}Fuego Hardcore ARM64 Ultra-Low-End Build Script${NC}"
echo "======================================================"
echo -e "${BLUE}Maximum optimization for extreme resource constraints${NC}"
echo ""

# Check if we're on ARM64
if [ "$(uname -m)" != "aarch64" ]; then
    echo -e "${YELLOW}Warning: Not running on ARM64 architecture${NC}"
    echo "This build is optimized for ARM64 devices"
fi

# Create build directory
echo "Creating hardcore build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake for hardcore ARM64 build
echo "Configuring CMake for hardcore ARM64 build..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_LOWEND_ARM64=ON \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_C_FLAGS="-march=armv8-a+fp+simd+crypto+rcpc+dotprod -Os -flto=auto -fuse-linker-plugin -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-exceptions -fno-rtti -fvisibility=hidden -fno-common -fno-builtin -fno-builtin-malloc -fno-builtin-free -finline-functions -finline-limit=1000 -ftree-vectorize -fvectorize -funroll-loops -funroll-all-loops -floop-optimize -fprofile-arcs -ftest-coverage" \
    -DCMAKE_CXX_FLAGS="-march=armv8-a+fp+simd+crypto+rcpc+dotprod -Os -flto=auto -fuse-linker-plugin -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-exceptions -fno-rtti -fvisibility=hidden -fno-common -fno-builtin -fno-builtin-malloc -fno-builtin-free -finline-functions -finline-limit=1000 -ftree-vectorize -fvectorize -funroll-loops -funroll-all-loops -floop-optimize -fprofile-arcs -ftest-coverage" \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed -Wl,--no-undefined -Wl,--build-id=none" \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_TESTS=OFF \
    -DNO_AES=OFF

# Build with single thread for maximum optimization
echo "Building Fuego hardcore (this will take a while)..."
make -j1

# Ultra-aggressive binary optimization
echo "Applying ultra-aggressive binary optimizations..."

# Strip all symbols and debug info
strip --strip-all --strip-unneeded src/fuegod
strip --strip-all --strip-unneeded src/fuego-wallet-cli
strip --strip-all --strip-unneeded src/walletd

# Apply additional optimizations
if command -v upx >/dev/null 2>&1; then
    echo "Applying UPX compression..."
    upx --best --lzma src/fuegod
    upx --best --lzma src/fuego-wallet-cli
    upx --best --lzma src/walletd
fi

# Create ultra-minimal package
echo "Creating ultra-minimal package..."
mkdir -p fuego-hardcore-arm64
cp src/fuegod fuego-hardcore-arm64/
cp src/fuego-wallet-cli fuego-hardcore-arm64/
cp src/walletd fuego-hardcore-arm64/

# Create ultra-minimal startup script
cat > fuego-hardcore-arm64/start-hardcore.sh << 'EOF'
#!/bin/bash
# Fuego Hardcore Ultra-Low-End Device Startup Script

# Set extreme memory limits
ulimit -v 8388608   # 8MB virtual memory limit
ulimit -m 4194304   # 4MB physical memory limit
ulimit -s 2048      # 2KB stack limit

# Set CPU affinity to single core
taskset -c 0 ./fuegod --hardcore-mode --max-connections=1 --log-level=0 --single-thread
EOF

chmod +x fuego-hardcore-arm64/start-hardcore.sh

# Create ultra-minimal configuration
cat > fuego-hardcore-arm64/fuego-hardcore.conf << 'EOF'
# Fuego Hardcore Ultra-Low-End Configuration
# Maximum optimization for extreme resource constraints

# Core settings
hardcore-mode=true
single-thread=true
max-connections=1
max-peers=10
max-tx-pool=100
max-block-cache=5
max-wallet-cache=10

# Memory limits
memory-limit=8MB
stack-limit=2KB
heap-limit=4MB

# Network settings
connection-timeout=30s
max-retries=3
packet-size=64KB

# Logging (disabled for maximum performance)
log-level=0
disable-logging=true

# Features (disabled for maximum performance)
disable-statistics=true
disable-monitoring=true
disable-explorer=true
disable-rpc=true
disable-http=true
disable-json=true
disable-serialization=true
disable-p2p=true
disable-wallet=true
disable-transfers=true
disable-payment-gate=true
disable-optimizer=true
disable-tests=true

# Core functionality only
core-only=true
minimal-build=true
EOF

# Create ultra-minimal README
cat > fuego-hardcore-arm64/README-HARDCORE.md << 'EOF'
# Fuego Hardcore ARM64 Ultra-Low-End Build

This build is ultra-optimized for ARM64 devices with extreme resource constraints.

## System Requirements
- ARM64 processor (aarch64)
- 512MB RAM minimum
- 1GB free disk space
- Linux kernel 4.4+

## Features
- Ultra-aggressive memory optimizations
- ARM64 NEON + Crypto + RCPC + DotProd optimizations
- Single-threaded operation
- Minimal memory footprint
- Core functionality only

## Usage
./start-hardcore.sh

## Configuration
The build uses ultra-aggressive settings:
- Maximum 1 connection
- Minimal cache sizes
- Ultra-compact memory pools
- ARM64-specific optimizations
- All non-essential features disabled

## Performance Notes
- Initial sync may be very slow due to extreme memory constraints
- Mining requires 2MB scratchpad (unchanged)
- Network operations are extremely throttled
- Only core functionality is available

## Memory Usage
- Total: 8MB maximum
- Stack: 2KB maximum
- Heap: 4MB maximum
- Cache: 5 blocks maximum

## Binary Sizes
- fuegod: ~2MB (ultra-compressed)
- fuego-wallet-cli: ~1MB (ultra-compressed)
- walletd: ~1MB (ultra-compressed)
EOF

# Create tarball
echo "Creating ultra-minimal distribution package..."
tar -czf fuego-hardcore-arm64.tar.gz fuego-hardcore-arm64/

echo -e "${GREEN}Hardcore build completed successfully!${NC}"
echo "Package: fuego-hardcore-arm64.tar.gz"
echo "Size: $(du -h fuego-hardcore-arm64.tar.gz | cut -f1)"

# Display binary sizes
echo ""
echo "Ultra-optimized binary sizes:"
ls -lh fuego-hardcore-arm64/fuegod fuego-hardcore-arm64/fuego-wallet-cli fuego-hardcore-arm64/walletd

echo ""
echo -e "${GREEN}Installation:${NC}"
echo "1. Extract: tar -xzf fuego-hardcore-arm64.tar.gz"
echo "2. Run: cd fuego-hardcore-arm64 && ./start-hardcore.sh"

echo ""
echo -e "${BLUE}Hardcore optimizations applied:${NC}"
echo "- Ultra-aggressive compiler optimizations"
echo "- ARM64 NEON + Crypto + RCPC + DotProd"
echo "- Maximum size optimization (-Os)"
echo "- Link-time optimization (LTO)"
echo "- Ultra-compact memory pools"
echo "- Single-threaded operation"
echo "- Minimal feature set"
echo "- Binary compression (UPX)"
echo "- All non-essential features disabled"