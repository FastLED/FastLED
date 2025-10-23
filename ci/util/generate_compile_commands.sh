#!/bin/bash
# Script to generate compile_commands.json using Meson
set -e

echo "Generating compile_commands.json using Meson..."

# Check if virtual environment exists
if [ -d .venv ]; then
    source .venv/bin/activate
else
    echo "Virtual environment not found. Please run ./install first."
    exit 1
fi

# Check if Meson is available
if ! command -v meson &> /dev/null; then
    echo "❌ Error: Meson is not installed"
    echo "Installing Meson and Ninja..."
    uv pip install meson ninja
fi

# Check if build directory exists
if [ ! -d ".build/meson" ]; then
    echo "Setting up Meson build directory..."
    mkdir -p .build/meson
    cd .build/meson
    meson setup . ../..
    cd ../..
else
    echo "Meson build directory already configured."
fi

# Compile to ensure compile_commands.json is up-to-date
echo "Building with Meson to generate compile_commands.json..."
meson compile -C .build/meson

# Check if compile_commands.json was generated
if [ -f ".build/meson/compile_commands.json" ]; then
    echo "✅ compile_commands.json generated successfully"
    cp .build/meson/compile_commands.json .
    echo "Size: $(wc -c < compile_commands.json) bytes"
    # Count entries (number of lines starting with {)
    echo "Entries: $(grep -c '"file":' compile_commands.json 2>/dev/null || echo "unknown")"
else
    echo "❌ compile_commands.json was not generated. Check the build output above."
    exit 1
fi

echo "Done!"
