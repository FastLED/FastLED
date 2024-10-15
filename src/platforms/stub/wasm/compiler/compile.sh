#!/bin/bash
set -e
set -x

<<<<<<< HEAD


compile() {
    cp Arduino.h src/Arduino.h
=======
_compile() {
>>>>>>> 87bb1c69 (commit)
    # sometimes the compilation fails, attempt to compile multiple times
    local max_attempts=2
    local attempt=1
    
    while [ $attempt -le $max_attempts ]; do
        if pio run; then
            echo "Compilation successful on attempt $attempt"
            return 0
        else
            echo "Compilation failed on attempt $attempt"
            if [ $attempt -eq $max_attempts ]; then
                echo "Max attempts reached. Compilation failed."
                return 1
            fi
            echo "Retrying..."
            ((attempt++))
        fi
    done
}

compile() {
    local prev_dir=$(pwd)
    echo "current directory: $(pwd)"
    local PROJECT_DIR_NAME="src"
    #cp Arduino.h "src/$PROJECT_DIR_NAME/Arduino.h"
    # handle this in a separate process so that we don't change the current directory
    # _compile "$PROJECT_DIR_NAME"
    cd $PROJECT_DIR_NAME
    _compile
    cd $prev_dir
}

# Function to insert the header
insert_header() {
    local file="$1"
    sed -i '/#include "platforms\/stub\/wasm\/js.h"/d' "$file"
    sed -i '1i#include "platforms/stub/wasm/js.h"' "$file"
    echo "Processed: $file"
}

# Copy the contents of the hosts mapped directory to the container
mkdir -p /js/src


# error if there is more than one file/dir in mapped
if [ $(ls /mapped | wc -l) -gt 1 ]; then
    echo "Error: More than one file or directory in /mapped"
    exit 1
fi

# get the directory in mapped/
PROJECT_DIR_NAME=$(ls /mapped)
PROJECT_DIR="/js/src/"
mkdir -p $PROJECT_DIR/src

# copy files
cp -r /mapped/$PROJECT_DIR_NAME/* $PROJECT_DIR
cp ./platformio.ini $PROJECT_DIR
cp ./Arduino.h $PROJECT_DIR/src/Arduino.h
cp -r ./fastled  $PROJECT_DIR/fastled
cp wasm.py $PROJECT_DIR/wasm.py

include_deps() {

    
    # If there is an .ino file, rename it and handle main.cpp/main2.hpp logic
    if [ -f "$PROJECT_DIR"/*.ino ]; then
        # Check if main.cpp already exists
        if [ -f "$PROJECT_DIR/main.cpp" ]; then
            mv "$PROJECT_DIR/main.cpp" "$PROJECT_DIR/main2.hpp"
        fi
        mv "$PROJECT_DIR"/*.ino "$PROJECT_DIR/src/generated_main.cpp"
        
        # If main2.hpp exists, append it to main.cpp
        if [ -f "$PROJECT_DIR/main2.hpp" ]; then
            echo '#include "main2.hpp"' >> "$PROJECT_DIR/main.cpp"
        fi
    fi
    
    # Find all .ino, .h, .hpp, and .cpp files and process them
    find "$PROJECT_DIR" -type f \( -name "*.ino" -o -name "*.h" -o -name "*.hpp" -o -name "*.cpp" \) ! -path "*fastled*" | while read -r file; do
        insert_header "$file"
    done
}

# Remove the .pio directory copy, if it exists because this could contain build
# artifacts from a previous build
cd /js


# We only want to run this compile pre-step once. Leave a breadcrumb to indicate
# that we have already done this.

include_deps

#############################################
compile "$PROJECT_DIR_NAME"
#############################################


# Ensure the directory exists
mkdir -p /mapped/fastled_js


# Copy (overwrite if exists) the files
cp -f ./.pio/build/*/fastled.js /mapped/fastled_js/fastled.js
cp -f ./.pio/build/*/fastled.wasm /mapped/fastled_js/fastled.wasm
cp -f ./index.html /mapped/fastled_js/index.html

# now open the files and write them a second time just to be safe
cat ./.pio/build/*/fastled.js > /mapped/fastled_js/fastled.js
cat ./.pio/build/*/fastled.wasm > /mapped/fastled_js/fastled.wasm
cat ./index.html > /mapped/fastled_js/index.html


# Initialize KEEP_FILES to false
KEEP_FILES=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --keep-files)
            KEEP_FILES=true
            shift
            ;;
        *)
            shift
            ;;
    esac
done

# If KEEP_FILES is false, remove the .pio directory
if [ "$KEEP_FILES" = false ]; then
   rm -rf /js/src/*
fi


