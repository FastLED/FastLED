#!/bin/bash
set -e

# Function to find Python executable
find_python() {
    if command -v python3 &> /dev/null; then
        echo "python3"
    elif command -v python &> /dev/null; then
        echo "python"
    else
        echo "Python not found. Please install Python 3."
        exit 1
    fi
}

# Check if uv is installed, if not, install it
if ! command -v uv &> /dev/null; then
    echo "uv command not found. Installing uv..."
    PYTHON=$(find_python)
    $PYTHON -m pip install uv
fi

cd "$(dirname "$0")"

# Skip .venv setup if running inside Docker (system packages are pre-installed)
if [ -z "$FASTLED_DOCKER" ]; then
    # if .venv not found
    if [ ! -d .venv ]; then
        # create virtual environment
        ./install
    fi
fi

# Show usage if no arguments
if [ $# -eq 0 ]; then
    echo "Usage: bash profile <function> [options]"
    echo ""
    echo "Options:"
    echo "  --docker              Run in Docker (consistent environment)"
    echo "  --iterations N        Number of benchmark iterations (default: 20)"
    echo "  --build-mode MODE     Build mode: quick, debug, release, profile (default: release)"
    echo "  --no-generate         Skip test generation (use existing profiler)"
    echo "  --callgrind           Run callgrind analysis (slower)"
    echo ""
    echo "Examples:"
    echo "  bash profile sincos16 --docker"
    echo "  bash profile sincos16 --iterations 50 --build-mode release"
    echo "  bash profile 'fl::Perlin::pnoise2d' --docker --callgrind"
    exit 0
fi

# Simple wrapper - all logic handled in Python
echo -e "\nRunning uv run ci/profile_runner.py $@\n"
uv run ci/profile_runner.py "$@"
