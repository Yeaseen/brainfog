# Brainfuck Compiler Project

This project provides a Brainfuck compiler implemented in C++ using LLVM. It compiles Brainfuck code to LLVM IR, which can then be converted to machine code and executed. This document outlines how to build the compiler and run Brainfuck programs using the built compiler.

## Prerequisites

Before building and running the compiler, ensure you have the following installed on your system:

- CMake (version 3.13 or higher)
- LLVM (version 16.0 or compatible)
- Clang
- GCC or another compatible C++ compiler

## Building the Compiler

Follow these steps to build the Brainfuck compiler built atop LLVM (16.0.\*):

1. **Create a Build Directory**:
   This directory will contain all build-related files.

   ```bash
   mkdir build
   cd build
   ```

2. **Run CMake**:
   Generate makefiles and configure the project.

   ```bash
   cmake ..
   ```

3. **Compile the Project**:
   Build the project using the generated makefiles.

   ```bash
   make
   ```

   This will create the `bf_compiler` executable within the `build` directory.

## Running Brainfuck Programs

Once you have built the compiler, you can run Brainfuck programs as follows:

1. **Place the Bash Script**:
   Ensure that the `run_bf.sh` script is in the root directory of the project and is executable.

   ```bash
   chmod +x run_bf.sh
   ```

2. **Run a Brainfuck Program**:
   Use the script to compile and run your Brainfuck program. Provide the path to the Brainfuck file as an argument to the script.

   ```bash
   ./run_bf.sh path/to/your/program.b
   ```

   The script performs the following steps:

   - Compiles the Brainfuck file to LLVM IR using `bf_compiler`.
   - Uses `llc` to compile the IR to an object file with position-independent code.
   - Links the object file using `clang` to create an executable.
   - Runs the executable.

## Example

To run a sample "Hello, World!" Brainfuck program, you might use:

```bash
./run_bf.sh ../becnches/mandel.b
```

## Troubleshooting

- **Build Errors**: Ensure all prerequisites are installed and paths in CMakeLists.txt are correctly set to match your LLVM installation.
- **Runtime Errors**: Check the Brainfuck code for errors. Ensure the Bash script is correctly handling paths and file names.

For more help, raise an issue on the project repository or contact the project maintainers.
