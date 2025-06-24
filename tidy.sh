#!/bin/bash

set -e

# Unset VIRTUAL_ENV to avoid warnings about mismatched paths
unset VIRTUAL_ENV

export CLANG_FORMAT_STYLE="{SortIncludes: false}"

# Lint targets
folders=(
    "src/fl"
    #"src"
)

# Optionally run clang-format
# for folder in "${folders[@]}"; do
#     echo "Running clang-format on $folder"
#     uv run ci/run-clang-format.py -i -r "$folder" || uv run ci/run-clang-format.py -i -r "$folder"
# done

# Make sure compile_commands.json exists
COMPILE_DB="tests/.build/compile_commands.json"
if [ ! -f "$COMPILE_DB" ]; then
    echo "Error: Missing $COMPILE_DB. Run ./test --cpp to generate it"
    exit 1
fi

# Run clang-tidy
for folder in "${folders[@]}"; do
    echo "Running clang-tidy on $folder"
    find "$folder" -name '*.cpp' ! -path "*/third_party/*" | while read -r file; do
        echo "Processing $file"
        clang-tidy "$file" -p tests/.build \
        -extra-arg=-std=c++17 \
        -checks='clang-analyzer-core.*,clang-analyzer-deadcode.*,clang-analyzer-cplusplus.*' \
        --quiet
    done
done
