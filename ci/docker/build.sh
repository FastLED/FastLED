#!/bin/bash
# Pre-cache PlatformIO platform dependencies with granular control
# This script supports multiple commands for layered Docker caching:
#   - install-platform: Install platform definition only
#   - install-packages: Install platform packages (toolchains)
#   - compile: Run compilation to warm up and validate

set -e

cd /fastled

# Check if PLATFORM_NAME is set
if [ -z "$PLATFORM_NAME" ]; then
    echo "Error: PLATFORM_NAME not set"
    exit 1
fi

# Parse command argument (default to legacy "compile all" behavior)
COMMAND="${1:-legacy}"

# Generate platformio.ini from PLATFORM_NAME using board configuration
# This is the source of truth - platformio.ini is derived, not canonical
generate_platformio_ini() {
    echo "Generating platformio.ini from platform: $PLATFORM_NAME"

    # Use Python to generate platformio.ini from board configuration
    python3 -c "
import sys
sys.path.insert(0, '/fastled')
from ci.boards import create_board
from pathlib import Path

try:
    board = create_board('$PLATFORM_NAME')
    project_root = '/fastled'
    ini_content = board.to_platformio_ini(
        include_platformio_section=True,
        project_root=project_root,
        additional_libs=['FastLED']
    )

    # Write to platformio.ini
    with open('platformio.ini', 'w') as f:
        f.write(ini_content)

    print('Successfully generated platformio.ini')
except Exception as e:
    print(f'Error generating platformio.ini: {e}', file=sys.stderr)
    sys.exit(1)
"

    if [ $? -ne 0 ]; then
        echo "Error: Failed to generate platformio.ini"
        exit 1
    fi

    echo "platformio.ini generated successfully"
}

# Helper function to extract platform from platformio.ini
get_platform_from_ini() {
    if [ -f "platformio.ini" ]; then
        # Extract platform value from first environment
        grep -m1 "^platform" platformio.ini | sed 's/platform[[:space:]]*=[[:space:]]*//' | tr -d '\r'
    fi
}

# Helper function to extract platform_packages from platformio.ini
get_platform_packages_from_ini() {
    if [ -f "platformio.ini" ]; then
        # Extract platform_packages value from first environment
        grep -m1 "^platform_packages" platformio.ini | sed 's/platform_packages[[:space:]]*=[[:space:]]*//' | tr -d '\r'
    fi
}

case "$COMMAND" in
    install-platform)
        echo "=== LAYER 1: Installing platform definition for: $PLATFORM_NAME ==="

        # Generate platformio.ini first
        generate_platformio_ini

        # Get platform from platformio.ini
        PLATFORM=$(get_platform_from_ini)

        if [ -n "$PLATFORM" ]; then
            echo "Installing platform: $PLATFORM"
            # Install platform without packages first (just the platform definition)
            pio pkg install --skip-dependencies --platform "$PLATFORM" || true
            echo "Platform definition installed"
        else
            echo "Warning: Could not extract platform from platformio.ini"
        fi

        echo "Platform definition layer ready"
        ;;

    install-packages)
        echo "=== LAYER 2: Installing platform packages (toolchains) for: $PLATFORM_NAME ==="

        # Generate platformio.ini if not already present
        if [ ! -f "platformio.ini" ]; then
            generate_platformio_ini
        fi

        # Get platform and platform_packages from platformio.ini
        PLATFORM=$(get_platform_from_ini)
        PLATFORM_PACKAGES=$(get_platform_packages_from_ini)

        if [ -n "$PLATFORM" ]; then
            # Install the platform WITH its dependencies (toolchains, etc.)
            echo "Installing platform packages for: $PLATFORM"
            pio pkg install --platform "$PLATFORM" || echo "Platform packages may already be installed"

            # Install any custom platform packages if specified
            if [ -n "$PLATFORM_PACKAGES" ]; then
                echo "Installing custom platform packages: $PLATFORM_PACKAGES"
                # Split by comma and install each package
                IFS=',' read -ra PACKAGES <<< "$PLATFORM_PACKAGES"
                for pkg in "${PACKAGES[@]}"; do
                    pkg=$(echo "$pkg" | xargs)  # Trim whitespace
                    if [ -n "$pkg" ]; then
                        echo "Installing: $pkg"
                        pio pkg install -g --platform "$pkg" || true
                    fi
                done
            fi

            echo "Platform packages installed"
        else
            echo "Warning: Could not extract platform from platformio.ini"
        fi

        echo "Platform packages layer ready"
        ;;

    compile)
        echo "=== LAYER 3: Warming up compilation for: $PLATFORM_NAME ==="

        # Generate platformio.ini if not already present
        if [ ! -f "platformio.ini" ]; then
            generate_platformio_ini
        fi

        # Run compile command to:
        # 1. Download any remaining framework files
        # 2. Validate that everything works
        # 3. Warm up the build cache
        bash compile "$PLATFORM_NAME" Blink
        echo "Compilation layer ready"
        ;;

    legacy|*)
        # Legacy behavior: do everything in one shot (backwards compatible)
        echo "=== LEGACY MODE: Installing all dependencies for: $PLATFORM_NAME ==="

        # Generate platformio.ini first
        generate_platformio_ini

        bash compile "$PLATFORM_NAME" Blink
        ;;
esac

echo "Build step '$COMMAND' completed successfully"
