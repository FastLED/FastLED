#!/bin/bash
set -e

# Function to insert the header
insert_header() {
    local file="$1"
    # Remove any existing include of _exports.hpp
    sed -i '/#include "platforms\/stub\/wasm\/_exports.hpp"/d' "$file"
    # Add the include at the beginning of the file
    sed -i '1i#include "platforms/stub/wasm/_exports.hpp"' "$file"
    echo "Processed: $file"
}

# Copy the contents of the hosts mapped directory to the container
mkdir -p /js/src
cp -r /mapped/* /js/src

# if there is an ino file in the src directory, then rename it to main.cpp
if [ -f /js/src/*.ino ]; then
    mv /js/src/*.ino /js/src/main.cpp
fi

# Remove the .pio directory copy, if it exists because this could contain build
# artifacts from a previous build
rm -rf /js/.pio
cp -r /wasm/* /js/
cd /js

# Find all .ino, .h, .hpp, and .cpp files recursively and process them
find . -type f \( -name "*.ino" -o -name "*.h" -o -name "*.hpp" -o -name "*.cpp" \) | while read -r file; do
    insert_header "$file"
done

pio run
rm -rf /mapped/fastled_js/*
mkdir -p /mapped/fastled_js
cp ./.pio/build/*/fastled.js /mapped/fastled_js/fastled.js
cp ./.pio/build/*/fastled.wasm /mapped/fastled_js/fastled.wasm
cp ./index.html /mapped/fastled_js/index.html
