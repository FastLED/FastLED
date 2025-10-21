#!/bin/bash
# Pre-cache PlatformIO platform dependencies with granular control
# This script supports multiple commands for layered Docker caching:
#   - install-platform: Install platform definition only
#   - install-packages: Install platform packages (toolchains)
#   - compile: Run compilation to warm up and validate

set -e

cd /fastled

# Check if PLATFORMS is set
if [ -z "$PLATFORMS" ]; then
    echo "Error: PLATFORMS not set"
    exit 1
fi

# Extract first board from PLATFORMS list as default for generate_platformio_ini
FIRST_PLATFORM=$(echo "$PLATFORMS" | cut -d',' -f1 | xargs)

# Parse command argument (default to legacy "compile all" behavior)
COMMAND="${1:-legacy}"

# Generate platformio.ini from a board using board configuration
# This is the source of truth - platformio.ini is derived, not canonical
# Usage: generate_platformio_ini [board_name]
# If board_name is omitted, defaults to first board in PLATFORMS list
generate_platformio_ini() {
    local board_name="${1:-$FIRST_PLATFORM}"
    echo "Generating platformio.ini from board: $board_name"

    # Use Python to generate platformio.ini from board configuration
    python3 -c "
import sys
sys.path.insert(0, '/fastled')
from ci.boards import create_board
from pathlib import Path

try:
    board = create_board('$board_name')
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
        echo "=== LAYER 1: Installing platform definition for: $PLATFORMS ==="

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
        echo "=== LAYER 2: Installing platform packages (toolchains) for: $PLATFORMS ==="

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
        echo "=== LAYER 3: Warming up compilation for: $PLATFORMS ==="

        echo "Platforms/boards to compile: $PLATFORMS"

        # Run compile command for each platform/board to:
        # 1. Download any remaining framework files
        # 2. Pre-cache platform-specific toolchains
        # 3. Validate that everything works
        # NOTE: Boards are compiled sequentially (one at a time) to ensure each completes before the next starts
        IFS=',' read -ra BOARDS_ARRAY <<< "$PLATFORMS"
        failed_boards=()
        for board in "${BOARDS_ARRAY[@]}"; do
            board=$(echo "$board" | xargs)  # Trim whitespace
            echo ""
            echo ">>> Pre-caching toolchain for board: $board"
            # Regenerate platformio.ini for each board to ensure correct configuration
            generate_platformio_ini "$board"
            NO_PARALLEL=1 bash compile "$board" Blink
            if [ $? -ne 0 ]; then
                failed_boards+=("$board")
                # Print red warning banner
                echo ""
                echo -e "\033[1;31m╔════════════════════════════════════════════════════════════════╗\033[0m"
                echo -e "\033[1;31m║                    ⚠️  COMPILATION FAILED ⚠️                      \033[0m"
                echo -e "\033[1;31m╠════════════════════════════════════════════════════════════════╣\033[0m"
                echo -e "\033[1;31m║ Board: $board\033[0m"
                echo -e "\033[1;31m║ This board will be skipped and compilation will continue        \033[0m"
                echo -e "\033[1;31m╚════════════════════════════════════════════════════════════════╝\033[0m"
                echo ""
            fi
        done

        echo "Compilation layer ready - cached ${#BOARDS_ARRAY[@]} board(s)"

        # Print summary if any boards failed
        if [ ${#failed_boards[@]} -gt 0 ]; then
            echo ""
            echo -e "\033[1;33m⚠️  Summary: ${#failed_boards[@]} board(s) failed to compile:\033[0m"
            for failed_board in "${failed_boards[@]}"; do
                echo -e "\033[1;31m  ✗ $failed_board\033[0m"
            done
            echo ""
        fi
        ;;

    legacy|*)
        # Legacy behavior: do everything in one shot (backwards compatible)
        echo "=== LEGACY MODE: Installing all dependencies for: $PLATFORMS ==="

        # Generate platformio.ini first
        generate_platformio_ini

        # Compile for each board
        IFS=',' read -ra BOARDS_ARRAY <<< "$PLATFORMS"
        failed_boards=()
        for board in "${BOARDS_ARRAY[@]}"; do
            board=$(echo "$board" | xargs)  # Trim whitespace
            echo ""
            echo ">>> Compiling board: $board"
            bash compile "$board" Blink
            if [ $? -ne 0 ]; then
                failed_boards+=("$board")
                # Print red warning banner
                echo ""
                echo -e "\033[1;31m╔════════════════════════════════════════════════════════════════╗\033[0m"
                echo -e "\033[1;31m║                    ⚠️  COMPILATION FAILED ⚠️                      \033[0m"
                echo -e "\033[1;31m╠════════════════════════════════════════════════════════════════╣\033[0m"
                echo -e "\033[1;31m║ Board: $board\033[0m"
                echo -e "\033[1;31m║ This board will be skipped and compilation will continue        \033[0m"
                echo -e "\033[1;31m╚════════════════════════════════════════════════════════════════╝\033[0m"
                echo ""
            fi
        done

        # Print summary if any boards failed
        if [ ${#failed_boards[@]} -gt 0 ]; then
            echo ""
            echo -e "\033[1;33m⚠️  Summary: ${#failed_boards[@]} board(s) failed to compile:\033[0m"
            for failed_board in "${failed_boards[@]}"; do
                echo -e "\033[1;31m  ✗ $failed_board\033[0m"
            done
            echo ""
        fi
        ;;
esac

echo "Build step '$COMMAND' completed successfully"
