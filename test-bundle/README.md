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
