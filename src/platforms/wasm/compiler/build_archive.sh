#!/bin/bash
set -e  # Exit immediately if a command exits with a non-zero status
set -x


# if /include does not exist
if [ ! -d "/precompiled/include" ]; then
   echo "include directory does not exist, copying header include tree"
   # echo "include directory does not exist, copying header include tree"
   # copy *.h,*.hpp files from fastled/src/** to /include
   mkdir -p /precompiled/include
   cd fastled/src
   find . -name "*.h*" -exec cp --parents {} /precompiled/include \;
   cd ../../
fi

# if /precompiled/libfastled.a does not exist
if [ ! -f "/precompiled/libfastled.a" ]; then
   # echo "libfastled.a does not exist, compiling static library"
   echo "libfastled.a does not exist, compiling static library"
   cd fastled/src/platforms/wasm/compiler/lib
   mkdir -p build
   cd build
   emcmake cmake -DCMAKE_VERBOSE_MAKEFILE=ON ..
   emmake cmake --build . -v -j
   cp fastled/libfastled.a /precompiled/libfastled.a
   cd ../../../../../
fi

# first compile if build doesn't exist
first_compile=false

# if build directory doesn't exist
if [ ! -d "build" ]; then
    # Create the build directory if it doesn't exist and mark as first compile.
    mkdir -p build
    # if /js/src/CMakeCache.txt exists then delete it
    if [ -f "/js/fastled/src/CMakeCache.txt" ]; then
        rm -rf /js/fastled/src/CMakeCache.txt
    fi
    first_compile=true
fi

cd build

# Set build mode: QUICK, DEBUG, or RELEASE (default is QUICK)
export BUILD_MODE=${1:-QUICK}

# Check for an optional second parameter to activate PROFILE
# PROFILE_FLAG="-DPROFILE=ON"


if [ "$first_compile" = true ]; then
    # Configure with CMake
    # If you want verbose build output, you can enable CMAKE_VERBOSE_MAKEFILE.
    echo "Configuring build with CMake..."
    emcmake cmake -DCMAKE_VERBOSE_MAKEFILE=ON  ..
else
    # If the build directory already exists, reconfigure with CMake.
    echo "Skipping CMake configuration as build directory already exists."
fi

# Build the project using CMake
emmake cmake --build . -v -j

