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

