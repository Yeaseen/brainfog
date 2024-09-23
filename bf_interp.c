#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TAPE_SIZE 30000
#define OUTPUT_BUFFER_SIZE 8192

// Structure to hold loop information
typedef struct {
    char *loop_content;
    int executions;
} loop_info_t;

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

// Normalize loop content by removing all whitespace characters
void normalize_loop_content(char *loop_content) {
    char *dst = loop_content;
    char *src = loop_content;

    while (*src) {
        if (!isspace((unsigned char)*src)) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';  // Null terminate the normalized string
}

// Find if a loop pattern already exists in the list and return its index, otherwise -1
int find_loop_pattern(loop_info_t *loops, int loop_count, const char *pattern) {
    for (int i = 0; i < loop_count; i++) {
        if (strcmp(loops[i].loop_content, pattern) == 0) {
            return i;  // Found the pattern, return its index
        }
    }
    return -1;  // Pattern not found
}

// Function to sort loops by execution count (descending order)
int compare_loops(const void *a, const void *b) {
    loop_info_t *loop_a = (loop_info_t *)a;
    loop_info_t *loop_b = (loop_info_t *)b;
    return loop_b->executions - loop_a->executions;  // Sort in decreasing order
}

void analyze_loops(char *buffer, int input_length, int *jump_map, int *loop_counts, 
                   loop_info_t **simple_loops, loop_info_t **non_simple_loops, 
                   int *simple_loop_count, int *non_simple_loop_count) {
    for (int i = 0; i < input_length; ++i) {
        if (buffer[i] == '[') {
            int loop_end = jump_map[i];
            int net_pointer_change = 0;
            int p0_change = 0;
            int is_simple = 1;
            int contains_io = 0;
            int contains_inner_loop = 0;
            int pointer_pos = 0; // Track the pointer position relative to p[0]

            // Analyze the loop body
            for (int j = i + 1; j < loop_end; ++j) {
                if (buffer[j] == '>') {
                    pointer_pos++;
                } else if (buffer[j] == '<') {
                    pointer_pos--;
                } else if (buffer[j] == '+') {
                    if (pointer_pos == 0) p0_change++;
                } else if (buffer[j] == '-') {
                    if (pointer_pos == 0) p0_change--;
                } else if (buffer[j] == '.' || buffer[j] == ',') {
                    contains_io = 1;  // I/O disqualifies the loop from being simple
                } else if (buffer[j] == '[') {
                    contains_inner_loop = 1;  // Inner loop found
                }
            }

            // Skip loops that contain inner loops
            if (contains_inner_loop) continue;

            // Check if the loop is simple:
            // - No I/O
            // - Pointer returns to p[0] (pointer_pos == 0)
            // - p[0] changes by exactly +1 or -1
            if (contains_io || pointer_pos != 0 || abs(p0_change) != 1) {
                is_simple = 0;
            }

            char *loop_content = get_loop_content(buffer, i, loop_end);
            normalize_loop_content(loop_content);  // Normalize loop content to remove whitespace
            int index;

            if (is_simple) {
                // Check if the loop pattern already exists in the simple loops list
                index = find_loop_pattern(*simple_loops, *simple_loop_count, loop_content);
                if (index != -1) {
                    (*simple_loops)[index].executions += loop_counts[i];  // Aggregate execution count
                    free(loop_content);  // Free memory as it's not needed (we're aggregating)
                } else {
                    (*simple_loops)[*simple_loop_count].loop_content = loop_content;
                    (*simple_loops)[*simple_loop_count].executions = loop_counts[i];
                    (*simple_loop_count)++;
                }
            } else {
                // Check if the loop pattern already exists in the non-simple loops list
                index = find_loop_pattern(*non_simple_loops, *non_simple_loop_count, loop_content);
                if (index != -1) {
                    (*non_simple_loops)[index].executions += loop_counts[i];  // Aggregate execution count
                    free(loop_content);  // Free memory as it's not needed (we're aggregating)
                } else {
                    (*non_simple_loops)[*non_simple_loop_count].loop_content = loop_content;
                    (*non_simple_loops)[*non_simple_loop_count].executions = loop_counts[i];
                    (*non_simple_loop_count)++;
                }
            }
        }
    }

    // Sort both simple and non-simple loops by execution count in decreasing order
    qsort(*simple_loops, *simple_loop_count, sizeof(loop_info_t), compare_loops);
    qsort(*non_simple_loops, *non_simple_loop_count, sizeof(loop_info_t), compare_loops);
}


// Print profiling results
void print_profiling_results(loop_info_t *simple_loops, int simple_loop_count, loop_info_t *non_simple_loops, int non_simple_loop_count, int total_instructions) {
    // Print simple loops and their counts
    printf("\nSimple loops:\n");
    for (int i = 0; i < simple_loop_count; ++i) {
        if (simple_loops[i].executions > 0) {  // Filter out zero-execution loops
            printf("  %s : %d\n", simple_loops[i].loop_content, simple_loops[i].executions);
        }
        free(simple_loops[i].loop_content);  // Free memory once printed
    }

    // Print non-simple loops and their counts
    printf("\nNon-simple loops:\n");
    for (int i = 0; i < non_simple_loop_count; ++i) {
        if (non_simple_loops[i].executions > 0) {  // Filter out zero-execution loops
            printf("  %s : %d\n", non_simple_loops[i].loop_content, non_simple_loops[i].executions);
        }
        free(non_simple_loops[i].loop_content);  // Free memory once printed
    }

    // Print total instructions executed
    printf("\nNormal termination after %d instructions.\n", total_instructions);
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
    loop_info_t *simple_loops = NULL;
    loop_info_t *non_simple_loops = NULL;
    int *loop_counts = NULL;
    int simple_loop_count = 0;
    int non_simple_loop_count = 0;
    int total_instructions = 0;

    if (profiling_enabled) {
        simple_loops = calloc(TAPE_SIZE, sizeof(loop_info_t));  // Store simple loop patterns
        non_simple_loops = calloc(TAPE_SIZE, sizeof(loop_info_t));  // Store non-simple loop patterns
        loop_counts = calloc(TAPE_SIZE, sizeof(int));  // Count how many times each loop is executed
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

        // Count the total number of instructions executed
        total_instructions++;

        // If profiling is enabled, count the loop executions
        if (profiling_enabled && (instruction == '[' || instruction == ']')) {
            loop_counts[i]++;
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
                break;
            case ']':
                if (*ptr) i = jump_map[i];
                break;
        }
    }

    flush_output(output_buffer, &output_index);

    if (profiling_enabled) {
        analyze_loops(buffer, input_length, jump_map, loop_counts, &simple_loops, &non_simple_loops, &simple_loop_count, &non_simple_loop_count);
        print_profiling_results(simple_loops, simple_loop_count, non_simple_loops, non_simple_loop_count, total_instructions);
        free(loop_counts);
        free(simple_loops);
        free(non_simple_loops);
    }

    free(buffer);
    free(jump_map);
    return 0;
}
