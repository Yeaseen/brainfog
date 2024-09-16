## Brainfuck Interpreter in C

The Brainfuck interpreter is designed to execute Brainfuck programs, which is a minimalist esoteric programming language created by Urban Müller.

### Prerequisites

To compile and run the Brainfuck interpreter, you will need:

- GCC (GNU Compiler Collection) or another compatible C compiler

### Compiling the Interpreter

To compile the Brainfuck interpreter, follow these steps:

1. Open your terminal.
2. Navigate to the directory containing the source code file `bf_interp.c`.
3. Run the following command to compile the interpreter:

```bash
gcc -O3 bf_interp.c -o bf_interp
```

This command will create an executable named `bf_interp` in the same directory.

### Running a Brainfuck Program

To run a Brainfuck program using the compiled interpreter, you can use the following command:

```bash
./bf_interp < path/to/your/brainfuck_program.b
```

Replace path/to/your/brainfuck_program.b with the path to the Brainfuck file you want to execute.

For timer

```bash
time ./bf_interp < mandel.b > /dev/null
```
