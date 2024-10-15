#!/bin/bash

# Source the Emscripten environment to ensure Clang is available
source /emsdk_portable/emsdk_env.sh

# Add Clang from the Emscripten SDK to the local PATH
export PATH="/emsdk_portable/upstream/bin:$PATH"

# Optionally print Clang version to verify it's available
clang --version

# Check if a file was passed as an argument
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <source-file.cpp>"
    exit 1
fi

# Input source file
INPUT_FILE=$1

# Ensure the file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: File '$INPUT_FILE' not found!"
    exit 1
fi



# Dump the AST and filter function prototypes
clang -Xclang -ast-dump -fsyntax-only -nostdinc "$INPUT_FILE"
