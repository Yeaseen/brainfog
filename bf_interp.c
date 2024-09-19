#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAPE_SIZE 30000
#define OUTPUT_BUFFER_SIZE 8192

// Parse command-line arguments for profiling option
void parse_arguments(int argc, char *argv[], int *profiling_enabled) {
    for (int j = 1; j < argc; j++) {
        if (strcmp(argv[j], "-p") == 0) {
            *profiling_enabled = 1;
        }
    }
}

// Flush output buffer to stdout
void flush_output(char *buffer, int *index) {
    fwrite(buffer, 1, *index, stdout);
    *index = 0;
}

// Add character to output buffer, flush if full
void buffered_put(char c, char *buffer, int *index) {
    buffer[(*index)++] = c;
    if (*index == OUTPUT_BUFFER_SIZE) {
        flush_output(buffer, index);
    }
}

// Count instruction executions
void count_instructions(int *instruction_counts, char instruction) {
    instruction_counts[(int)instruction]++;
}

// Helper function to extract loop content as a string
char *get_loop_content(char *buffer, int start, int end) {
    int length = end - start + 1;
    char *loop_content = (char *)malloc((length + 1) * sizeof(char));
    strncpy(loop_content, &buffer[start], length);
    loop_content[length] = '\0'; // Null terminate the string
    return loop_content;
}

// Analyze loops and classify them as simple or non-simple, and store their content
void analyze_loops(char *buffer, int input_length, int *jump_map, int *loop_counts, int *simple_loop_count, int *non_simple_loop_count, char **simple_loops, char **non_simple_loops) {
    for (int i = 0; i < input_length; ++i) {
        if (buffer[i] == '[') {
            int loop_end = jump_map[i];
            int net_pointer_change = 0;
            int change_to_p0 = 0;
            int is_simple = 1;

            for (int j = i + 1; j < loop_end; ++j) {
                if (buffer[j] == '>') net_pointer_change++;
                else if (buffer[j] == '<') net_pointer_change--;
                else if (buffer[j] == '+') change_to_p0++;
                else if (buffer[j] == '-') change_to_p0--;
                else if (buffer[j] == '.' || buffer[j] == ',') {
                    is_simple = 0;
                }
            }

            if (net_pointer_change != 0 || abs(change_to_p0) != 1) {
                is_simple = 0;
            }

            char *loop_content = get_loop_content(buffer, i, loop_end);

            if (is_simple) {
                simple_loop_count[i] = loop_counts[i];
                simple_loops[i] = loop_content;  // Store the loop content
            } else {
                non_simple_loop_count[i] = loop_counts[i];
                non_simple_loops[i] = loop_content;  // Store the loop content
            }
        }
    }
}

// Print profiling results
void print_profiling_results(int *instruction_counts, int input_length, char **simple_loops, int *simple_loop_count, char **non_simple_loops, int *non_simple_loop_count) {
    // Print instruction execution counts
    printf("\n\nInstruction execution counts:\n");
    for (int i = 0; i < 256; i++) {
        if (instruction_counts[i] > 0) {
            printf("%c: %d\n", i, instruction_counts[i]);
        }
    }

    // Print simple loops and their counts
    printf("\nSimple loops execution counts:\n");
    for (int i = 0; i < input_length; ++i) {
        if (simple_loop_count[i] > 0 && simple_loops[i] != NULL) {
            printf("%s : %d executions\n", simple_loops[i], simple_loop_count[i]);
            free(simple_loops[i]);  // Free memory once printed
        }
    }

    // Print non-simple loops and their counts
    printf("\nNon-simple loops execution counts:\n");
    for (int i = 0; i < input_length; ++i) {
        if (non_simple_loop_count[i] > 0 && non_simple_loops[i] != NULL) {
            printf("%s : %d executions\n", non_simple_loops[i], non_simple_loop_count[i]);
            free(non_simple_loops[i]);  // Free memory once printed
        }
    }
}

int main(int argc, char *argv[]) {
    int profiling_enabled = 0;
    parse_arguments(argc, argv, &profiling_enabled);

    static unsigned char tape[TAPE_SIZE] = {0}; 
    unsigned char *ptr = tape;

    char output_buffer[OUTPUT_BUFFER_SIZE];
    int output_index = 0;

    char *buffer = NULL;
    size_t bufsize = 0;
    size_t input_length = getdelim(&buffer, &bufsize, EOF, stdin);

    if (!buffer) {
        perror("Failed to read input");
        return 1;
    }

    int *jump_map = malloc(input_length * sizeof(int));
    if (!jump_map) {
        perror("Failed to allocate memory for jump map");
        free(buffer);
        return 1;
    }

    memset(jump_map, -1, input_length * sizeof(int));

    int stack[TAPE_SIZE], stack_ptr = 0;
    for (int i = 0; i < input_length; ++i) {
        if (buffer[i] == '[') {
            stack[stack_ptr++] = i;
        } else if (buffer[i] == ']') {
            int open = stack[--stack_ptr];
            int close = i;
            jump_map[open] = close;
            jump_map[close] = open;
        }
    }

    // Profiling arrays, initialized only if profiling is enabled
    int *instruction_counts = NULL;
    int *loop_counts = NULL;
    int *simple_loop_count = NULL;
    int *non_simple_loop_count = NULL;
    char **simple_loops = NULL;
    char **non_simple_loops = NULL;

    if (profiling_enabled) {
        instruction_counts = calloc(256, sizeof(int));
        loop_counts = calloc(TAPE_SIZE, sizeof(int));
        simple_loop_count = calloc(TAPE_SIZE, sizeof(int));
        non_simple_loop_count = calloc(TAPE_SIZE, sizeof(int));
        simple_loops = calloc(TAPE_SIZE, sizeof(char *));  // Store loop patterns
        non_simple_loops = calloc(TAPE_SIZE, sizeof(char *));  // Store loop patterns
    }

    // Modify the loop that processes Brainfuck instructions
    for (int i = 0; i < input_length; ++i) {
        char instruction = buffer[i];

        // Skip any character that is not a valid Brainfuck instruction
        if (instruction != '>' && instruction != '<' &&
            instruction != '+' && instruction != '-' &&
            instruction != '.' && instruction != ',' &&
            instruction != '[' && instruction != ']') {
            continue; // Skip non-instruction characters (comments, spaces, etc.)
        }

        // If profiling is enabled, count the instructions
        if (profiling_enabled) {
            count_instructions(instruction_counts, instruction);
        }

        // Execute valid Brainfuck instructions
        switch (instruction) {
            case '>': ++ptr; break;
            case '<': --ptr; break;
            case '+': ++*ptr; break;
            case '-': --*ptr; break;
            case '.':
                buffered_put(*ptr, output_buffer, &output_index);
                break;
            case ',':
                *ptr = getchar();
                break;
            case '[':
                if (!*ptr) i = jump_map[i];
                else if (profiling_enabled) loop_counts[i]++;
                break;
            case ']':
                if (*ptr) i = jump_map[i];
                break;
        }
    }

    flush_output(output_buffer, &output_index);

    if (profiling_enabled) {
        analyze_loops(buffer, input_length, jump_map, loop_counts, simple_loop_count, non_simple_loop_count, simple_loops, non_simple_loops);
        print_profiling_results(instruction_counts, input_length, simple_loops, simple_loop_count, non_simple_loops, non_simple_loop_count);
        free(instruction_counts);
        free(loop_counts);
        free(simple_loop_count);
        free(non_simple_loop_count);
        free(simple_loops);
        free(non_simple_loops);
    }

    free(buffer);
    free(jump_map);
    return 0;
}
