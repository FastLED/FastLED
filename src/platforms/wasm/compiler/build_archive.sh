#!/bin/bash


# if src/ does not exist, then do the copy
if [ ! -d "src" ]; then
    echo "Copying src/ directory"
    python compile.py --only-copy
    mv src/wasm.ino src/wasm.ino.cpp
fi



# Create build directory
mkdir -p build
cd build

# Set build mode (QUICK, DEBUG, or RELEASE)
export BUILD_MODE=${1:-QUICK}

# Configure with CMake
emcmake cmake ..

# Build
emmake make -j$(nproc)
