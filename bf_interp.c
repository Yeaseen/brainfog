#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAPE_SIZE 30000
#define OUTPUT_BUFFER_SIZE 8192


static inline void flush_output(char *buffer, int *index) {
    fwrite(buffer, 1, *index, stdout);
    *index = 0;
}

static inline void buffered_put(char c, char *buffer, int *index) {
    buffer[(*index)++] = c;
    if (*index == OUTPUT_BUFFER_SIZE) {
        flush_output(buffer, index);
    }
}

int main() {
    static unsigned char tape[TAPE_SIZE] = {0}; 
    register unsigned char *ptr = tape;

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

    for (int i = 0; i < input_length; ++i) {
        switch (buffer[i]) {
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

    free(buffer);
    free(jump_map);
    return 0;
}

