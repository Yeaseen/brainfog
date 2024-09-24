#!/bin/bash

# Check if a .b file argument is provided
if [ -z "$1" ]; then
    exit 1
fi

# Set the input Brainfuck file and output files
BF_FILE=$1
ASM_FILE="output.s"
OBJ_FILE="output.o"
EXEC_FILE="output_program"

# Compile the bf_compiler if it's not already compiled
if [ ! -f bf_compiler ]; then
    gcc bf_compiler.c -o bf_compiler
    if [ $? -ne 0 ]; then
        exit 1
    fi
fi

# Generate the assembly file from the Brainfuck file
./bf_compiler "$BF_FILE" -o "$ASM_FILE"
if [ $? -ne 0 ]; then
    exit 1
fi

# Assemble the assembly file
as -o "$OBJ_FILE" "$ASM_FILE"
if [ $? -ne 0 ]; then
    exit 1
fi

# Link the object file to create the executable
ld -o "$EXEC_FILE" "$OBJ_FILE" -lc --dynamic-linker /lib64/ld-linux-x86-64.so.2
if [ $? -ne 0 ]; then
    exit 1
fi

# Run the compiled Brainfuck program
./"$EXEC_FILE"

# Exit
exit 0