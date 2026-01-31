#!/bin/bash
# Script to generate compile_commands.json using Meson
set -e

echo "Generating compile_commands.json using Meson..."

# CRITICAL: Always use venv meson to avoid version incompatibility
# Meson 1.9.x and 1.10.x create INCOMPATIBLE build directories!
# System meson cannot interact with venv meson build directories and vice versa.

# Determine the correct meson executable
if [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "win32" ]]; then
    # Windows (Git Bash / MSYS2)
    MESON_EXE=".venv/Scripts/meson"
else
    # Linux / macOS
    MESON_EXE=".venv/bin/meson"
fi

# Check if venv meson exists
if [ ! -f "$MESON_EXE" ]; then
    echo "❌ Error: Venv meson not found at $MESON_EXE"
    echo "Please run ./install first to set up the virtual environment."
    echo "Or use: uv pip install meson ninja"
    exit 1
fi

echo "Using meson: $MESON_EXE ($($MESON_EXE --version))"

# Note: Build system uses mode-specific directories (.build/meson-quick, .build/meson-debug, etc.)
# We use the default "quick" mode for IDE integration
BUILD_DIR=".build/meson-quick"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Setting up Meson build directory..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    "../../$MESON_EXE" setup . ../..
    cd ../..
else
    echo "Meson build directory already configured."
fi

# Compile to ensure compile_commands.json is up-to-date
echo "Building with Meson to generate compile_commands.json..."
"$MESON_EXE" compile -C "$BUILD_DIR"

# Check if compile_commands.json was generated
if [ -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "✅ compile_commands.json generated successfully"
    cp "$BUILD_DIR/compile_commands.json" .
    echo "Size: $(wc -c < compile_commands.json) bytes"
    # Count entries (number of lines starting with {)
    echo "Entries: $(grep -c '"file":' compile_commands.json 2>/dev/null || echo "unknown")"
else
    echo "❌ compile_commands.json was not generated. Check the build output above."
    exit 1
fi

echo "Done!"
