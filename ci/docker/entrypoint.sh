#!/bin/bash
# FastLED Docker entrypoint script
# Handles compilation and automatic copying of build artifacts to output directory

set -e

# Execute the command passed to the container
"$@"
EXIT_CODE=$?

# If /fastled/output is mounted and compilation succeeded, copy build artifacts
if [ -d "/fastled/output" ] && [ $EXIT_CODE -eq 0 ]; then
    echo ""
    echo "Copying build artifacts to output directory..."

    # Find and copy all build artifacts
    if [ -d "/fastled/.pio/build" ]; then
        # Copy all binary files
        find /fastled/.pio/build -type f \( -name "*.bin" -o -name "*.hex" -o -name "*.elf" -o -name "*.factory.bin" \) \
            -exec cp -v {} /fastled/output/ \; 2>/dev/null || true

        echo "Build artifacts copied to output directory"
    fi
fi

exit $EXIT_CODE
