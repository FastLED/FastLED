#!/bin/bash
set -e  # Exit immediately if a command exits with a non-zero status

# If src/ does not exist, then copy (or generate) it.
if [ ! -d "src" ]; then
    echo "Copying src/ directory"
    python compile.py --keep-files
fi

# Create the build directory if it doesn't exist and switch to it.
mkdir -p build
cd build

# Set build mode: QUICK, DEBUG, or RELEASE (default is QUICK)
export BUILD_MODE=${1:-QUICK}

# Configure with CMake using the Ninja generator.
# If you want verbose build output, you can enable CMAKE_VERBOSE_MAKEFILE.
emcmake cmake -G Ninja -DCMAKE_VERBOSE_MAKEFILE=ON ..

# Build the project using Ninja.
emmake ninja
