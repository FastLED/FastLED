#!/bin/bash
# Pre-cache PlatformIO platform dependencies
# Runs: bash compile <platform> Blink
# This downloads and caches:
# - Platform toolchains (e.g., xtensa-esp32-elf-gcc)
# - Framework files (e.g., Arduino core)
# - FastLED library compilation

set -e

cd /fastled

# Check if PLATFORM_NAME is set
if [ -z "$PLATFORM_NAME" ]; then
    echo "Error: PLATFORM_NAME not set"
    exit 1
fi

echo "Warming up platform cache for: $PLATFORM_NAME"

# Run compile command to cache dependencies
# This is the same command users will run, ensuring perfect cache hit
# If compilation fails, the Docker build should fail
bash compile "$PLATFORM_NAME" Blink
