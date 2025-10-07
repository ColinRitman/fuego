#!/bin/bash

# Fuego Linux Bundle Installation Script

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    log_error "This script must be run as root (use sudo)"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

log_info "Installing Fuego Linux Bundle..."

# Create directories
log_info "Creating system directories..."
mkdir -p /usr/local/bin
mkdir -p /usr/local/lib/fuego

# Install executables
log_info "Installing executables..."
cp "$SCRIPT_DIR/bin"/* /usr/local/bin/
chmod +x /usr/local/bin/fuegod
chmod +x /usr/local/bin/fuego-wallet-cli
chmod +x /usr/local/bin/walletd
chmod +x /usr/local/bin/optimizer

# Install libraries
log_info "Installing shared libraries..."
cp "$SCRIPT_DIR/lib"/* /usr/local/lib/fuego/

# Update library cache
log_info "Updating library cache..."
ldconfig

# Create symlinks for easy access
log_info "Creating symlinks..."
ln -sf /usr/local/bin/fuegod /usr/local/bin/fuego-daemon
ln -sf /usr/local/bin/fuego-wallet-cli /usr/local/bin/fuego-wallet

log_success "Installation completed successfully!"
log_info "You can now run:"
log_info "  fuegod --help"
log_info "  fuego-wallet-cli --help"
log_info "  walletd --help"
log_info "  optimizer --help"
