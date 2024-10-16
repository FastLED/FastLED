#!/bin/bash

set -e
set -x

python compile.py --only-copy

# Find the .ino file in the src directory
ino_file=$(find src -name "*.ino" -type f)

if [ -z "$ino_file" ]; then
    echo "Error: No .ino file found in the src directory"
    exit 1
fi

# Extract the filename without extension
ino_filename=$(basename "$ino_file")
ino_name="${ino_filename%.*}"

# Copy and process the .ino file
cp "$ino_file" "src/${ino_name}.ino.cpp"
python process-ino.py "src/${ino_name}.ino.cpp"

# Remove the original .ino file
rm "$ino_file"

python compile.py --only-compile
