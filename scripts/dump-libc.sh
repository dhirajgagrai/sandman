#!/bin/bash

C_FILE="./test/example.c"
TEMP_EXEC="a.out_temp"
OUTPUT_FILE="./pass/resources/libc_functions.txt"

if [ ! -f "$C_FILE" ]; then
    echo "Error: C file not found: $C_FILE"
    exit 1
fi

clang "$C_FILE" -o "$TEMP_EXEC"

if [ $? -ne 0 ]; then
    echo "Error: Compilation failed."
    exit 1
fi

LIBC_PATH=$(ldd "$TEMP_EXEC" | grep 'libc*\.so' | awk '{print $3}')

if [ -z "$LIBC_PATH" ]; then
    echo "Error: Could not automatically detect a path for 'libc*.so*'."
    rm "$TEMP_EXEC"
    exit 1
fi

mkdir -p "$(dirname "$OUTPUT_FILE")"

nm -D "$LIBC_PATH" | cut -d ' ' -f 3 | cut -d '@' -f 1 > "$OUTPUT_FILE"

rm "$TEMP_EXEC"
