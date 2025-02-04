#!/bin/bash
set -e  # Exit immediately if a command exits with a non-zero status

# static_lib_generated=false
# # if lib/build directory doesn't exist then enter it so that we can run cmake
# if [ ! -d "fastled/src/platforms/wasm/compiler/lib/build" ]; then
#     static_lib_generated=false
#     # echo "lib/build directory doesn't exist. Entering lib directory to run cmake."
#     # mkdir -p lib/build
#     # cd lib/build
#     # echo "Running cmake in lib/build directory."
#     # emcmake cmake -DCMAKE_VERBOSE_MAKEFILE=ON .
#     # cmake --build . --target fastled
#     cd fastled/src/platforms/wasm/compiler/lib
#     mkdir -p build
#     cd build
#     # emcmake cmake -DCMAKE_VERBOSE_MAKEFILE=ON ..
#     emcmake cmake -DCMAKE_VERBOSE_MAKEFILE=ON ..
# fi

# exit 0

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
    # Configure with CMake
    # If you want verbose build output, you can enable CMAKE_VERBOSE_MAKEFILE.
    echo "Configuring build with CMake..."
    emcmake cmake -DCMAKE_VERBOSE_MAKEFILE=ON ${PROFILE_FLAG} ..
else
    # If the build directory already exists, reconfigure with CMake.
    echo "Skipping CMake configuration as build directory already exists."
fi

# Build the project using CMake
emmake cmake --build . -v
