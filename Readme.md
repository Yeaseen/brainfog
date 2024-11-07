## Brainfuck Interpreter in C

The Brainfuck interpreter is designed to execute Brainfuck programs, which is a minimalist esoteric programming language created by Urban Müller.

### Prerequisites

To compile and run the Brainfuck interpreter, you will need:

- GCC (GNU Compiler Collection) or another compatible C compiler
- CMake (version 3.13 or higher)
- LLVM (version 16.0 or compatible)

### Compiling the Interpreter

To compile the Brainfuck interpreter, follow these steps:

1. Open your terminal.
2. Navigate to the directory containing the source code file `bf_interp.c`.
3. Run the following command to compile the interpreter:

```bash
gcc -O3 bf_interp.c -o bf_interp
```

This command will create an executable named `bf_interp` in the same directory.

### Running a Brainfuck Program using the interpreter

To run a Brainfuck program using the compiled interpreter, you can use the following command:

```bash
./bf_interp < path/to/your/brainfuck_program.b
```

Replace path/to/your/brainfuck_program.b with the path to the Brainfuck file you want to execute.

To enable the profiler, run the following command:

```bash
./bf_interp -p < path/to/your/brainfuck_program.b
```

For timer

```bash
time ./bf_interp < benches/mandel.b > /dev/null
```

## Usage of the Compiler for bf program on a x86-64 machine

```bash
chmod +x bf_compiler/run_compiler.sh

./bf_compiler/run_compiler.sh path/to/bf/file
```

## Usage of the JIT compiler for BF PL

```bash
cd bf_JIT

gcc -O3 bf_JIT.c -o bf_JIT

./bf_JIT <path/to/bf/file>

```

## Usage of the bf built with llvm

You will file a readme inside the folder `bf_llvm_project`.
Below is a quick building+running approach.

```bash
git clone https://github.com/Yeaseen/brainfog.git
cd brainfog/bf_llvm_project
mkdir build
cd build
cmake ..
make
cd ..
chmod +x bf_run.sh
./bf_run.sh ../benches/mandel.b
```
