#!/bin/bash
set -e  # Exit immediately if a command exits with a non-zero status

# first compile if build doesn't exist
first_compile=false

# if build directory doesn't exist
if [ ! -d "build" ]; then
    # Create the build directory if it doesn't exist and mark as first compile.
    mkdir -p build
    first_compile=true
fi

cd build

# Set build mode: QUICK, DEBUG, or RELEASE (default is QUICK)
export BUILD_MODE=${1:-QUICK}

# Check for an optional second parameter to activate PROFILE
PROFILE_FLAG="-DPROFILE=ON"


if [ "$first_compile" = true ]; then
    # Configure with CMake using the Ninja generator.
    # If you want verbose build output, you can enable CMAKE_VERBOSE_MAKEFILE.
    echo "Configuring build with CMake..."
    emcmake cmake -G Ninja -DCMAKE_VERBOSE_MAKEFILE=ON ${PROFILE_FLAG} ..
    # emcmake cmake -G Ninja
else
    # If the build directory already exists, reconfigure with CMake.
    echo "Skipping CMake configuration as build directory already exists."
fi

# Build the project using Ninja.
emmake ninja -d stats -v
