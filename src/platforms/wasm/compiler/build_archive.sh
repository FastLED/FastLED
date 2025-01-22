#!/bin/bash

# Create build directory
mkdir -p build
cd build

# Set build mode (QUICK, DEBUG, or RELEASE)
export BUILD_MODE=${1:-QUICK}

# Configure with CMake
emcmake cmake ..

# Build
emmake make -j$(nproc)
