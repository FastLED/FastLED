#!/bin/bash
# Script to generate compile_commands.json using compiledb
set -e

echo "Generating compile_commands.json using compiledb..."

# Activate virtual environment if it exists
if [ -d .venv ]; then
    source .venv/bin/activate
else
    echo "Virtual environment not found. Please run ./install first."
    exit 1
fi

# Check if compiledb is installed
if ! command -v compiledb &> /dev/null; then
    echo "Installing compiledb..."
    pip install compiledb
fi

# Build the tests to ensure compile_commands.json is generated in tests/.build/
echo "Building tests to generate compile_commands.json..."
uv run ci/compiler/cpp_test_run.py --compile-only --clang --test test_function

# Copy the generated compile_commands.json from tests/.build/
if [ -f tests/.build/compile_commands.json ]; then
    echo "Copying compile_commands.json from tests/.build/ to project root..."
    cp tests/.build/compile_commands.json compile_commands.json
    echo "✅ compile_commands.json generated successfully"
    echo "Size: $(wc -c < compile_commands.json) bytes"
    echo "Entries: $(jq length compile_commands.json 2>/dev/null || echo "unknown (jq not available)")"
else
    echo "❌ No compile_commands.json found in tests/.build/"
    echo "The C++ compilation may have failed. Check the build output above."
    exit 1
fi

echo "Done!"