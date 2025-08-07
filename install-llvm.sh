#!/bin/bash

# Script to install LLVM toolchain using llvm-installer
# Artifacts will be installed to .cache/cc

set -e  # Exit on any error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CACHE_DIR="$SCRIPT_DIR/.cache/cc"

echo "Installing LLVM toolchain to $CACHE_DIR"

# Create cache directory if it doesn't exist
mkdir -p "$CACHE_DIR"

# Make sure we have the required dependencies
echo "Installing dependencies..."
uv add llvm-installer sys-detection

# Run the installer
echo "Running LLVM installer..."
uv run python3 "$SCRIPT_DIR/install_llvm.py" "$CACHE_DIR"

echo "✅ LLVM installation complete!"
echo "Installation directory: $CACHE_DIR"

# List what was installed
if [ -d "$CACHE_DIR" ]; then
    echo "Installed files:"
    find "$CACHE_DIR" -name "clang*" -type f -executable | head -5
    echo ""
    echo "To use the installed clang compiler:"
    echo "  export PATH=\"$CACHE_DIR/*/bin:\$PATH\""
    echo "  clang --version"
fi