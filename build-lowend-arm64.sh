#!/bin/bash
# Fuego ARM64 Low-End Device Build Script
# Optimized for devices with limited memory and processing power

set -e

# Configuration
BUILD_DIR="build-lowend-arm64"
CMAKE_FLAGS=""
MAKE_FLAGS=""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Fuego ARM64 Low-End Device Build Script${NC}"
echo "=============================================="

# Check if we're on ARM64
if [ "$(uname -m)" != "aarch64" ]; then
    echo -e "${YELLOW}Warning: Not running on ARM64 architecture${NC}"
    echo "This build is optimized for ARM64 devices"
fi

# Create build directory
echo "Creating build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake for low-end ARM64 build
echo "Configuring CMake for low-end ARM64 build..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_LOWEND_ARM64=ON \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_C_FLAGS="-march=armv8-a+fp+simd+crypto -Os -flto" \
    -DCMAKE_CXX_FLAGS="-march=armv8-a+fp+simd+crypto -Os -flto" \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,--gc-sections -Wl,--strip-all" \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_TESTS=OFF \
    -DNO_AES=OFF

# Build with limited parallelism for low-end devices
echo "Building Fuego (this may take a while on low-end devices)..."
make -j2

# Strip debug symbols and optimize binary size
echo "Optimizing binary size..."
strip --strip-all src/fuegod
strip --strip-all src/fuego-wallet-cli
strip --strip-all src/walletd

# Create optimized package
echo "Creating optimized package..."
mkdir -p fuego-lowend-arm64
cp src/fuegod fuego-lowend-arm64/
cp src/fuego-wallet-cli fuego-lowend-arm64/
cp src/walletd fuego-lowend-arm64/
cp src/optimizer fuego-lowend-arm64/

# Create startup script
cat > fuego-lowend-arm64/start-lowend.sh << 'EOF'
#!/bin/bash
# Fuego Low-End Device Startup Script

# Set memory limits
ulimit -v 1048576  # 1GB virtual memory limit
ulimit -m 524288   # 512MB physical memory limit

# Set CPU affinity to single core for low-end devices
taskset -c 0 ./fuegod --lowend-mode --max-connections=4 --log-level=2
EOF

chmod +x fuego-lowend-arm64/start-lowend.sh

# Create README for low-end devices
cat > fuego-lowend-arm64/README-LOWEND.md << 'EOF'
# Fuego Low-End ARM64 Build

This build is optimized for ARM64 devices with limited memory and processing power.

## System Requirements
- ARM64 processor (aarch64)
- 1GB RAM minimum
- 2GB free disk space
- Linux kernel 4.4+

## Features
- Memory-optimized data structures
- ARM64 NEON optimizations
- Reduced memory footprint
- Single-threaded operation mode
- Minimal logging

## Usage
./start-lowend.sh

## Configuration
The build uses optimized settings for low-end devices:
- Maximum 4 connections
- Reduced cache sizes
- Memory pooling
- ARM64-specific optimizations

## Performance Notes
- Initial sync may be slower due to memory constraints
- Mining requires 2MB scratchpad (unchanged)
- Network operations are throttled for stability
EOF

# Create tarball
echo "Creating distribution package..."
tar -czf fuego-lowend-arm64.tar.gz fuego-lowend-arm64/

echo -e "${GREEN}Build completed successfully!${NC}"
echo "Package: fuego-lowend-arm64.tar.gz"
echo "Size: $(du -h fuego-lowend-arm64.tar.gz | cut -f1)"

# Display binary sizes
echo ""
echo "Binary sizes:"
ls -lh fuego-lowend-arm64/fuegod fuego-lowend-arm64/fuego-wallet-cli fuego-lowend-arm64/walletd

echo ""
echo -e "${GREEN}Installation:${NC}"
echo "1. Extract: tar -xzf fuego-lowend-arm64.tar.gz"
echo "2. Run: cd fuego-lowend-arm64 && ./start-lowend.sh"