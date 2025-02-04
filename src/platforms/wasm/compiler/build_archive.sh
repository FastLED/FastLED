#!/bin/bash


# if src/ does not exist, then do the copy
if [ ! -d "src" ]; then
    echo "Copying src/ directory"
    python compile.py --keep-files
fi



# Create build directory
mkdir -p build
cd build

# Set build mode (QUICK, DEBUG, or RELEASE)
export BUILD_MODE=${1:-QUICK}

    # cmake_configure_command_list: list[str] = [
    #     "cmake",
    #     "-S",
    #     str(PROJECT_ROOT / "tests"),
    #     "-B",
    #     str(BUILD_DIR),
    #     "-G",
    #     "Ninja",
    #     "-DCMAKE_VERBOSE_MAKEFILE=ON",
    # ]

# Configure with CMake
emcmake cmake ..

# Build
emmake make -j$(nproc)
