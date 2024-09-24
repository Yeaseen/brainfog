#include <stdio.h>
#include <stdlib.h>

#define TAPE_SIZE 30000

// Function to read the Brainfuck source code from file
char* read_bf_file(const char *filename, size_t *size) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open Brainfuck file");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *source = (char *)malloc(*size + 1);
    fread(source, 1, *size, file);
    source[*size] = '\0';  // Null terminate the file contents

    fclose(file);
    return source;
}

// Generate assembly for Brainfuck code
void generate_assembly(const char *bf_source, size_t bf_size, FILE *out) {
    // Start of assembly code
    fprintf(out, ".global _start\n");
    fprintf(out, ".section .bss\n");
    fprintf(out, "tape: .space %d\n", TAPE_SIZE);  // Reserve space for the tape
    fprintf(out, ".section .text\n");
    fprintf(out, "_start:\n");

    // Load the tape base address into rsi (pointer to tape)
    fprintf(out, "lea tape(%%rip), %%rsi\n");

    // Brainfuck instruction translation
    int loop_counter = 0;  // Label counter for loops
    int stack[256];  // Stack to handle nested loops
    int stack_ptr = 0;

    for (size_t i = 0; i < bf_size; i++) {
        char c = bf_source[i];

        switch (c) {
            case '>':  // Move pointer right
                fprintf(out, "addq $1, %%rsi\n");
                break;
            case '<':  // Move pointer left
                fprintf(out, "subq $1, %%rsi\n");
                break;
            case '+':  // Increment the value at the pointer
                fprintf(out, "movb (%%rsi), %%al\n");
                fprintf(out, "addb $1, %%al\n");
                fprintf(out, "movb %%al, (%%rsi)\n");
                break;
            case '-':  // Decrement the value at the pointer
                fprintf(out, "movb (%%rsi), %%al\n");
                fprintf(out, "subb $1, %%al\n");
                fprintf(out, "movb %%al, (%%rsi)\n");
                break;
            case '.':  // Output the byte at the pointer using write system call
                fprintf(out, "mov $1, %%rax\n");  // syscall: write
                fprintf(out, "mov $1, %%rdi\n");  // stdout (file descriptor 1)
                fprintf(out, "mov %%rsi, %%rsi\n");  // address of the byte in memory
                fprintf(out, "mov $1, %%rdx\n");  // write 1 byte
                fprintf(out, "syscall\n");
                break;
            case ',':  // Input a byte using read system call
                fprintf(out, "mov $0, %%rax\n");  // syscall: read
                fprintf(out, "mov $0, %%rdi\n");  // stdin (file descriptor 0)
                fprintf(out, "mov %%rsi, %%rsi\n");  // address to store the input
                fprintf(out, "mov $1, %%rdx\n");  // read 1 byte
                fprintf(out, "syscall\n");
                break;
            case '[':  // Start of loop
                stack[stack_ptr++] = loop_counter;
                fprintf(out, "loop_start_%d:\n", loop_counter);
                fprintf(out, "movb (%%rsi), %%al\n");
                fprintf(out, "test %%al, %%al\n");
                fprintf(out, "jz loop_end_%d\n", loop_counter);
                loop_counter++;
                break;
            case ']':  // End of loop
                if (stack_ptr == 0) {
                    fprintf(stderr, "Error: unmatched ']' at position %zu\n", i);
                    exit(1);
                }
                stack_ptr--;
                int loop_id = stack[stack_ptr];
                fprintf(out, "movb (%%rsi), %%al\n");
                fprintf(out, "test %%al, %%al\n");
                fprintf(out, "jnz loop_start_%d\n", loop_id);
                fprintf(out, "loop_end_%d:\n", loop_id);
                break;
            default:
                // Ignore any non-Brainfuck character
                break;
        }
    }

    // Exit system call
    fprintf(out, "mov $60, %%rax\n");  // syscall: exit
    fprintf(out, "xor %%rdi, %%rdi\n"); // exit code 0
    fprintf(out, "syscall\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.bf> -o <output.s>\n", argv[0]);
        return 1;
    }

    // Read the Brainfuck source code from file
    size_t bf_size;
    char *bf_source = read_bf_file(argv[1], &bf_size);

    // Open the output assembly file for writing
    FILE *out = fopen(argv[3], "w");
    if (!out) {
        perror("Failed to open output assembly file");
        free(bf_source);
        return 1;
    }

    // Generate assembly code
    generate_assembly(bf_source, bf_size, out);

    // Clean up
    fclose(out);
    free(bf_source);

    //printf("Assembly generated and written to %s\n", argv[3]);

    return 0;
}

