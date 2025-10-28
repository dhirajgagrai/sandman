#!/bin/bash

# Exit immediately if any command fails
set -e

# --- Configuration ---
# Path to your compiled pass plugin
PASS_PLUGIN="./build/pass/SandmanPlugin.so"
# ---------------------

# 1. Check if an argument was provided
if [ $# -eq 0 ]; then
    echo "Error: No C file specified."
    echo "Usage: $0 path/to/yourfile.c"
    exit 1
fi

# 2. Get the C file path from the first argument
SOURCE_FILE="$1"

# 3. Check if the C file actually exists
if [ ! -f "$SOURCE_FILE" ]; then
    echo "Error: File not found: $SOURCE_FILE"
    exit 1
fi

# 4. Check if the pass plugin exists
if [ ! -f "$PASS_PLUGIN" ]; then
    echo "Error: Pass plugin not found at $PASS_PLUGIN"
    echo "Did you build your pass?"
    exit 1
fi

OUTPUT_FILE="${SOURCE_FILE%.c}.out"

# 6. Run the compile command with the variables
clang -fpass-plugin="$PASS_PLUGIN" "$SOURCE_FILE" -o "$OUTPUT_FILE"

