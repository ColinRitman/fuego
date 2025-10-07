#!/bin/bash

# Linux Shared Library Bundling Script for Fuego
# This script collects all shared library dependencies and creates a portable package

set -e

# Configuration
BUILD_DIR="${1:-build}"
OUTPUT_DIR="${2:-fuego-linux-bundle}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if required tools are available
check_dependencies() {
    log_info "Checking dependencies..."
    
    local missing_deps=()
    
    if ! command -v ldd &> /dev/null; then
        missing_deps+=("ldd")
    fi
    
    if ! command -v readelf &> /dev/null; then
        missing_deps+=("readelf")
    fi
    
    if ! command -v objdump &> /dev/null; then
        missing_deps+=("objdump")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "Missing required tools: ${missing_deps[*]}"
        log_error "Please install: sudo apt-get install binutils"
        exit 1
    fi
    
    log_success "All dependencies found"
}

# Find all executable files
find_executables() {
    local exec_files=()
    
    # Look for the main executables
    local main_execs=("fuegod" "fuego-wallet-cli" "walletd" "optimizer")
    
    for exec in "${main_execs[@]}"; do
        local path="$BUILD_DIR/src/$exec"
        if [ -f "$path" ] && [ -x "$path" ]; then
            exec_files+=("$path")
        fi
    done
    
    if [ ${#exec_files[@]} -eq 0 ]; then
        return 1
    fi
    
    printf '%s\n' "${exec_files[@]}"
}

# Get shared library dependencies for a file
get_shared_libs() {
    local file="$1"
    local libs=()
    
    # Use ldd to get shared library dependencies
    while IFS= read -r line; do
        # Skip lines that don't contain library paths
        if [[ $line == *"=>"* ]]; then
            local lib_path=$(echo "$line" | awk '{print $3}')
            if [ -n "$lib_path" ] && [ -f "$lib_path" ]; then
                libs+=("$lib_path")
            fi
        elif [[ $line == *".so"* ]] && [[ $line != *"not found"* ]]; then
            # Handle libraries without path (system libraries)
            local lib_name=$(echo "$line" | awk '{print $1}')
            if [ -n "$lib_name" ]; then
                libs+=("$lib_name")
            fi
        fi
    done < <(ldd "$file" 2>/dev/null || true)
    
    printf '%s\n' "${libs[@]}"
}

# Copy shared library to bundle directory
copy_shared_lib() {
    local lib_path="$1"
    local dest_dir="$2"
    
    if [ -f "$lib_path" ]; then
        local lib_name=$(basename "$lib_path")
        local dest_path="$dest_dir/$lib_name"
        
        # Create symlink if it doesn't exist
        if [ ! -e "$dest_path" ]; then
            cp -L "$lib_path" "$dest_path"
            log_info "Copied: $lib_name"
        fi
        
        # Also copy any versioned symlinks
        local lib_dir=$(dirname "$lib_path")
        for link in "$lib_dir"/"${lib_name%.so}"*.so*; do
            if [ -L "$link" ] && [ -f "$link" ]; then
                local link_name=$(basename "$link")
                local link_dest="$dest_dir/$link_name"
                if [ ! -e "$link_dest" ]; then
                    cp -L "$link" "$link_dest"
                    log_info "Copied symlink: $link_name"
                fi
            fi
        done
    fi
}

# Create a wrapper script for an executable
create_wrapper_script() {
    local exec_path="$1"
    local exec_name=$(basename "$exec_path")
    local wrapper_path="$2"
    
    cat > "$wrapper_path" << EOF
#!/bin/bash

# Wrapper script for $exec_name
# This script sets up the library path and runs the executable

# Get the directory where this script is located
SCRIPT_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"

# Set library path to include bundled libraries
export LD_LIBRARY_PATH="\${SCRIPT_DIR}/lib:\${LD_LIBRARY_PATH}"

# Run the actual executable from bin directory
exec "\${SCRIPT_DIR}/bin/$exec_name" "\$@"
EOF
    
    chmod +x "$wrapper_path"
    log_info "Created wrapper script: $(basename "$wrapper_path")"
}

# Main bundling function
bundle_linux() {
    log_info "Starting Linux bundling process..."
    
    # Check dependencies
    check_dependencies
    
    # Change to project root
    cd "$PROJECT_ROOT"
    
    # Create output directory
    log_info "Creating output directory: $OUTPUT_DIR"
    rm -rf "$OUTPUT_DIR"
    mkdir -p "$OUTPUT_DIR"/{bin,lib}
    
    # Find all executables
    log_info "Finding executable files..."
    local exec_files
    if ! exec_files=($(find_executables)); then
        log_error "No executables found in $BUILD_DIR/src/"
        exit 1
    fi
    
    for exec_file in "${exec_files[@]}"; do
        log_info "Found executable: $exec_file"
    done
    
    # Collect all unique shared libraries
    log_info "Collecting shared library dependencies..."
    local all_libs=()
    
    for exec_file in "${exec_files[@]}"; do
        log_info "Analyzing dependencies for: $(basename "$exec_file")"
        local libs
        libs=($(get_shared_libs "$exec_file"))
        
        for lib in "${libs[@]}"; do
            if [ -n "$lib" ]; then
                all_libs+=("$lib")
            fi
        done
    done
    
    # Remove duplicates and sort
    unique_libs=($(printf '%s\n' "${all_libs[@]}" | sort -u))
    
    log_info "Found ${#unique_libs[@]} unique shared libraries"
    
    # Copy shared libraries
    for lib in "${unique_libs[@]}"; do
        if [ -f "$lib" ]; then
            copy_shared_lib "$lib" "$OUTPUT_DIR/lib"
        else
            log_warning "Library not found: $lib"
        fi
    done
    
    # Copy executables and create wrapper scripts
    log_info "Copying executables and creating wrapper scripts..."
    for exec_file in "${exec_files[@]}"; do
        local exec_name=$(basename "$exec_file")
        local dest_exec="$OUTPUT_DIR/bin/$exec_name"
        local wrapper_script="$OUTPUT_DIR/$exec_name"
        
        # Copy the executable
        cp "$exec_file" "$dest_exec"
        log_info "Copied executable: $exec_name"
        
        # Create wrapper script in root of bundle
        create_wrapper_script "$exec_name" "$wrapper_script"
    done
    
    # Create a README file
    create_readme
    
    # Create a simple installation script
    create_install_script
    
    log_success "Linux bundle created successfully in: $OUTPUT_DIR"
    log_info "Bundle contents:"
    find "$OUTPUT_DIR" -type f | sort
}

# Create README file
create_readme() {
    cat > "$OUTPUT_DIR/README.md" << 'EOF'
# Fuego Linux Bundle

This is a portable Linux bundle of Fuego that includes all necessary shared libraries.

## Contents

- `bin/` - Fuego executables
- `lib/` - Shared libraries required by the executables
- `fuegod`, `fuego-wallet-cli`, `walletd`, `optimizer` - Wrapper scripts that set up the library path

## Usage

Run any of the wrapper scripts directly:

```bash
./fuegod --help
./fuego-wallet-cli --help
./walletd --help
./optimizer --help
```

## Installation

Run the install script to copy files to system directories:

```bash
sudo ./install.sh
```

## Manual Installation

If you prefer to install manually:

1. Copy the executables to `/usr/local/bin/`:
   ```bash
   sudo cp bin/* /usr/local/bin/
   ```

2. Copy the libraries to `/usr/local/lib/fuego/`:
   ```bash
   sudo mkdir -p /usr/local/lib/fuego
   sudo cp lib/* /usr/local/lib/fuego/
   ```

3. Update the library cache:
   ```bash
   sudo ldconfig
   ```

## Troubleshooting

If you encounter library loading issues:

1. Check that all libraries are present in the `lib/` directory
2. Verify that the wrapper scripts are executable
3. Check the library path: `echo $LD_LIBRARY_PATH`

## System Requirements

- Linux x86_64
- glibc 2.17 or later
- No additional dependencies required (all bundled)
EOF
}

# Create installation script
create_install_script() {
    cat > "$OUTPUT_DIR/install.sh" << 'EOF'
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
EOF
    
    chmod +x "$OUTPUT_DIR/install.sh"
    log_info "Created installation script: install.sh"
}

# Main execution
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    bundle_linux "$@"
fi