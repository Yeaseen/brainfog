#!/bin/bash

# Check if the path to the Brainfuck file is provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 path/to/bf_file"
    exit 1
fi

BF_FILE=$1

# Ensure the Brainfuck file exists
if [ ! -f "$BF_FILE" ]; then
    echo "Error: Brainfuck file does not exist at '$BF_FILE'"
    exit 1
fi

# Path to the bf_compiler (assuming it's in the 'build' directory)
BF_COMPILER="./build/bf_compiler"

# Compile the Brainfuck file with your custom bf_compiler
$BF_COMPILER "$BF_FILE"

# Check if the LLVM IR was successfully generated
if [ ! -f output.ll ]; then
    echo "Error: Failed to generate LLVM IR (output.ll missing)."
    exit 1
fi

# Generate the object file using llc with PIC enabled
llc -filetype=obj -relocation-model=pic output.ll -o output.o

# Check if the object file was successfully generated
if [ ! -f output.o ]; then
    echo "Error: Failed to generate object file (output.o missing)."
    exit 1
fi

# Link the object file to create the executable
clang -o bf_output output.o

# Check if the executable was successfully created
if [ ! -f bf_output ]; then
    echo "Error: Failed to link executable (bf_output missing)."
    exit 1
fi

# Run the executable
./bf_output
