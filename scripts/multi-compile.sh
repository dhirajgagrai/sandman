#!/bin/bash

# Exit immediately if any command fails
set -e

PASS_PLUGIN="./build/pass/SandmanPlugin.so"

OUTPUT_FILE="build.out"

if [ $# -eq 0 ]; then
    echo "Error: No C files specified."
    echo "Usage: $0 file1.c file2.c file3.c"
    exit 1
fi

if [ ! -f "$PASS_PLUGIN" ]; then
    echo "Error: Pass plugin not found at $PASS_PLUGIN"
    echo "Did you build your pass?"
    exit 1
fi

for source_file in "$@"; do
    if [ ! -f "$source_file" ]; then
        echo "Warning: File not found, skipping: $source_file"
        continue
    fi
    
    output_bc="${source_file%.c}.bc"

    clang -emit-llvm -c "$source_file" -o "$output_bc"
    
    bitcode_files+=("$output_bc")
done

if [ ${#bitcode_files[@]} -eq 0 ]; then
    echo "Error: No valid C files were compiled."
    exit 1
fi

llvm-link "${bitcode_files[@]}" -o combined.bc

clang -fpass-plugin=./build/pass/SandmanPlugin.so combined.bc -o "$OUTPUT_FILE"

rm "${bitcode_files[@]}" combined.bc

