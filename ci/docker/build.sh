#!/bin/bash
# Pre-cache PlatformIO platform dependencies
# This script runs initial compilation to download and cache:
# - Platform toolchains (e.g., xtensa-esp32-elf-gcc)
# - Framework files (e.g., Arduino core)
# - Required libraries (e.g., FastLED)

set -e

# Run PlatformIO compilation to trigger dependency downloads
# The cache is stored in ~/.platformio/packages and ~/.platformio/platforms
pio run || {
    echo "Initial compilation may have warnings, continuing..."
    exit 0
}
