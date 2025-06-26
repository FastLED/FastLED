#!/bin/bash

set -e

echo "Converting FastLED to allsrc build approach..."
echo "Converting all src/fl/*.cpp files to src/fl/*.hpp files"

# Get all .cpp files in src/fl directory
cpp_files=$(find src/fl -name "*.cpp" | sort)

echo "Found $(echo "$cpp_files" | wc -l) .cpp files to convert:"
echo "$cpp_files"
echo

# Convert each .cpp file to .hpp
for cpp_file in $cpp_files; do
    hpp_file="${cpp_file%.cpp}.hpp"
    echo "Converting: $cpp_file -> $hpp_file"
    mv "$cpp_file" "$hpp_file"
done

echo
echo "All .cpp files converted to .hpp files successfully!"
echo "Next step: Updating _fl_compile.cpp to include .hpp files unconditionally..."

# Get the list of converted files (now .hpp) for updating _fl_compile.cpp
hpp_files=$(find src/fl -name "*.hpp" | sort)

echo "Found $(echo "$hpp_files" | wc -l) .hpp files to include in _fl_compile.cpp"
echo "Script completed successfully!"
