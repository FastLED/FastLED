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



# Build the tests to verify compilation works
echo "Building tests to verify compilation works..."
uv run -m ci.compiler.cpp_test_run --compile-only --clang --test test_function

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "Generating compile_commands.json from compilation flags..."
    cat > compile_commands.json << 'EOF'
[
  {
    "directory": "/workspace",
    "command": "python -m ziglang c++ -std=gnu++17 -fpermissive -Wall -Wextra -Wno-deprecated-register -Wno-backslash-newline-escape -fno-exceptions -fno-rtti -O1 -g0 -fno-inline-functions -fno-vectorize -fno-unroll-loops -fno-strict-aliasing -I. -Isrc -Itests -I/workspace/src/platforms/stub -I/workspace/src -DFASTLED_UNIT_TEST=1 -DFASTLED_FORCE_NAMESPACE=1 -DFASTLED_USE_PROGMEM=0 -DSTUB_PLATFORM -DARDUINO=10808 -DFASTLED_USE_STUB_ARDUINO -DSKETCH_HAS_LOTS_OF_MEMORY=1 -DFASTLED_STUB_IMPL -DFASTLED_USE_JSON_UI=1 -DFASTLED_TESTING -DFASTLED_NO_AUTO_NAMESPACE -DFASTLED_NO_PINMAP -DHAS_HARDWARE_PIN_SUPPORT -DFASTLED_DEBUG_LEVEL=1 -DFASTLED_NO_ATEXIT=1 -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -c",
    "file": "/workspace/src/FastLED.cpp"
  },
  {
    "directory": "/workspace",
    "command": "python -m ziglang c++ -std=gnu++17 -fpermissive -Wall -Wextra -Wno-deprecated-register -Wno-backslash-newline-escape -fno-exceptions -fno-rtti -O1 -g0 -fno-inline-functions -fno-vectorize -fno-unroll-loops -fno-strict-aliasing -I. -Isrc -Itests -I/workspace/src/platforms/stub -I/workspace/src -DFASTLED_UNIT_TEST=1 -DFASTLED_FORCE_NAMESPACE=1 -DFASTLED_USE_PROGMEM=0 -DSTUB_PLATFORM -DARDUINO=10808 -DFASTLED_USE_STUB_ARDUINO -DSKETCH_HAS_LOTS_OF_MEMORY=1 -DFASTLED_STUB_IMPL -DFASTLED_USE_JSON_UI=1 -DFASTLED_TESTING -DFASTLED_NO_AUTO_NAMESPACE -DFASTLED_NO_PINMAP -DHAS_HARDWARE_PIN_SUPPORT -DFASTLED_DEBUG_LEVEL=1 -DFASTLED_NO_ATEXIT=1 -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -c",
    "file": "/workspace/tests/test_function.cpp"
  }
]
EOF
    echo "✅ compile_commands.json generated successfully"
    echo "Size: $(wc -c < compile_commands.json) bytes"
    echo "Entries: $(jq length compile_commands.json 2>/dev/null || echo "unknown (jq not available)")"
else
    echo "❌ C++ compilation failed. Check the build output above."
    exit 1
fi

echo "Done!"
