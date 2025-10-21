#!/bin/bash
# FastLED Docker Build Script - Refactored for external argument passing
#
# This script orchestrates the Docker build layers for PlatformIO platform dependencies.
# It calls external Python utilities (docker_build_utils.py) instead of embedding Python code.
#
# LAYERS:
#   Layer 1: install-platform   - Install platform definitions only
#   Layer 2: install-packages    - Install platform packages (toolchains)
#   Layer 3: compile             - Warm up compilation cache
#
# Usage:
#   PLATFORMS=uno,esp32dev bash build.sh install-platform
#   PLATFORMS=uno,esp32dev bash build.sh install-packages
#   PLATFORMS=uno,esp32dev bash build.sh compile
#   PLATFORMS=uno,esp32dev bash build.sh legacy (default: do all)

set -e

# ==============================================================================
# CONFIGURATION
# ==============================================================================

PROJECT_ROOT="${PROJECT_ROOT:=/fastled}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Validate environment
if [ -z "$PLATFORMS" ]; then
    echo "Error: PLATFORMS environment variable not set"
    echo "Usage: PLATFORMS=uno,esp32dev bash build.sh [command]"
    exit 1
fi

# Extract first board as default
FIRST_PLATFORM=$(echo "$PLATFORMS" | cut -d',' -f1 | xargs)

# Parse command argument (default to "legacy")
COMMAND="${1:-legacy}"

# ==============================================================================
# HELPER FUNCTIONS
# ==============================================================================

# Call Python utility from docker_build_utils module
call_python_util() {
    local action=$1
    shift

    python3 -m ci.docker.docker_build_utils "$action" "$@"
}

# Generate platformio.ini from board configuration
generate_platformio_ini() {
    local board_name="${1:-$FIRST_PLATFORM}"
    echo "Generating platformio.ini from board: $board_name"

    python3 -m ci.docker.generate_platformio_ini "$board_name" "$PROJECT_ROOT"

    if [ $? -ne 0 ]; then
        echo "Error: Failed to generate platformio.ini"
        return 1
    fi

    echo "platformio.ini generated successfully"
    return 0
}

# Get platform from platformio.ini
get_platform_from_ini() {
    if [ -f "platformio.ini" ]; then
        grep -m1 "^platform" platformio.ini 2>/dev/null | sed 's/platform[[:space:]]*=[[:space:]]*//' | tr -d '\r' || echo ""
    fi
}

# Get platform_packages from platformio.ini
get_platform_packages_from_ini() {
    if [ -f "platformio.ini" ]; then
        grep -m1 "^platform_packages" platformio.ini 2>/dev/null | sed 's/platform_packages[[:space:]]*=[[:space:]]*//' | tr -d '\r' || echo ""
    fi
}

# Print colored warning banner
print_compilation_warning() {
    local board=$1
    echo ""
    echo -e "\033[1;31m╔════════════════════════════════════════════════════════════════╗\033[0m"
    echo -e "\033[1;31m║                    ⚠️  COMPILATION FAILED ⚠️                      \033[0m"
    echo -e "\033[1;31m╠════════════════════════════════════════════════════════════════╣\033[0m"
    echo -e "\033[1;31m║ Board: $board\033[0m"
    echo -e "\033[1;31m║ This board will be skipped and compilation will continue        \033[0m"
    echo -e "\033[1;31m╚════════════════════════════════════════════════════════════════╝\033[0m"
    echo ""
}

# Print compilation summary
print_compilation_summary() {
    local failed_count=$1
    shift
    local failed_boards=("$@")

    if [ $failed_count -gt 0 ]; then
        echo ""
        echo -e "\033[1;33m⚠️  Summary: $failed_count board(s) failed to compile:\033[0m"
        for board in "${failed_boards[@]}"; do
            echo -e "\033[1;31m  ✗ $board\033[0m"
        done
        echo ""
    fi
}

# ==============================================================================
# BUILD COMMANDS
# ==============================================================================

case "$COMMAND" in
    install-platform)
        echo "=== LAYER 1: Installing platform definitions for: $PLATFORMS ==="

        # Generate platformio.ini first
        generate_platformio_ini || exit 1

        # Get platform from platformio.ini
        PLATFORM=$(get_platform_from_ini)

        if [ -n "$PLATFORM" ]; then
            echo "Installing platform: $PLATFORM"
            pio pkg install --skip-dependencies --platform "$PLATFORM" || true
            echo "Platform definition installed"
        else
            echo "Error: Could not extract platform from platformio.ini"
            exit 1
        fi

        echo "Platform definition layer ready"
        ;;

    install-packages)
        echo "=== LAYER 2: Installing platform packages (toolchains) for: $PLATFORMS ==="

        # Generate platformio.ini if not present
        if [ ! -f "platformio.ini" ]; then
            generate_platformio_ini || exit 1
        fi

        # Get platform and custom packages from ini
        PLATFORM=$(get_platform_from_ini)
        PLATFORM_PACKAGES=$(get_platform_packages_from_ini)

        if [ -n "$PLATFORM" ]; then
            echo "Installing platform packages for: $PLATFORM"
            pio pkg install --platform "$PLATFORM" || echo "Platform packages may already be installed"

            # Install custom packages if specified
            if [ -n "$PLATFORM_PACKAGES" ]; then
                echo "Installing custom platform packages: $PLATFORM_PACKAGES"
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
            echo "Error: Could not extract platform from platformio.ini"
            exit 1
        fi

        echo "Platform packages layer ready"
        ;;

    compile)
        echo "=== LAYER 3: Warming up compilation for: $PLATFORMS ==="
        echo "Platforms/boards to compile: $PLATFORMS"

        # Compile each platform/board sequentially
        IFS=',' read -ra BOARDS_ARRAY <<< "$PLATFORMS"
        failed_boards=()

        for board in "${BOARDS_ARRAY[@]}"; do
            board=$(echo "$board" | xargs)  # Trim whitespace
            echo ""
            echo ">>> Pre-caching toolchain for board: $board"

            # Regenerate platformio.ini for each board
            generate_platformio_ini "$board" || {
                print_compilation_warning "$board"
                failed_boards+=("$board")
                continue
            }

            # Compile the board
            NO_PARALLEL=1 bash compile "$board" Blink || {
                print_compilation_warning "$board"
                failed_boards+=("$board")
                continue
            }
        done

        echo "Compilation layer ready - cached ${#BOARDS_ARRAY[@]} board(s)"

        # Print summary if any boards failed
        if [ ${#failed_boards[@]} -gt 0 ]; then
            print_compilation_summary ${#failed_boards[@]} "${failed_boards[@]}"
        fi
        ;;

    legacy|*)
        # Legacy behavior: do everything in one shot (backwards compatible)
        echo "=== LEGACY MODE: Installing all dependencies for: $PLATFORMS ==="

        # Generate platformio.ini
        generate_platformio_ini || exit 1

        # Compile for each board
        IFS=',' read -ra BOARDS_ARRAY <<< "$PLATFORMS"
        failed_boards=()

        for board in "${BOARDS_ARRAY[@]}"; do
            board=$(echo "$board" | xargs)  # Trim whitespace
            echo ""
            echo ">>> Compiling board: $board"
            bash compile "$board" Blink || {
                print_compilation_warning "$board"
                failed_boards+=("$board")
                continue
            }
        done

        # Print summary if any boards failed
        if [ ${#failed_boards[@]} -gt 0 ]; then
            print_compilation_summary ${#failed_boards[@]} "${failed_boards[@]}"
        fi
        ;;
esac

echo "Build step '$COMMAND' completed successfully"
