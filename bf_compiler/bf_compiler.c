#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TAPE_SIZE 30000

// Global variables to store metadata for `$` optimizations
typedef struct {
    size_t position;  // Position of '$' in the Brainfuck code
    int shift_value;  // Net shift value (e.g., -4 for [<<<<], +4 for [>>>>])
} LoopInfo;

// Declare a global array to store loop information and an index to track it
LoopInfo loop_info_array[256];  // Store metadata for up to 256 loops
int loop_info_index = 0;        // Index to track the number of loops

#define MAX_OFFSETS 100  // Maximum offsets to track within a loop
#define MAX_LOOPS 256    // Maximum number of simple loops

typedef struct {
    size_t position;      // Position of '#' in the Brainfuck code
    int changes[MAX_OFFSETS];  // Net changes at each offset (index = offset, value = net change)
    int totalOffsets;     // Total number of unique offsets modified in the loop
} SimpleLoopInfo;

SimpleLoopInfo simple_loop_info_array[MAX_LOOPS];  // Array to store simple loop information
int simple_loop_info_index = 0;  // Index to track the number of simple loops


// Function to read the Brainfuck source code from file, filtering out invalid characters
char* read_bf_file(const char *filename, size_t *size) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open Brainfuck file");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);  // Get the size of the file
    fseek(file, 0, SEEK_SET);

    char *source = (char *)malloc(file_size + 1);  // Allocate memory for the file contents
    if (!source) {
        perror("Failed to allocate memory for source");
        exit(1);
    }

    fread(source, 1, file_size, file);  // Read the entire file
    fclose(file);

    // Filter out any non-Brainfuck characters
    char *filtered_source = (char *)malloc(file_size + 1);  // Allocate memory for the filtered source
    size_t j = 0;
    for (size_t i = 0; i < file_size; ++i) {
        char c = source[i];
        if (c == '>' || c == '<' || c == '+' || c == '-' || c == '.' || c == ',' || c == '[' || c == ']') {
            filtered_source[j++] = c;
        }
    }
    filtered_source[j] = '\0';  // Null terminate the filtered source

    *size = j;  // Update the size to reflect the size of the filtered source
    free(source);  // Free the original unfiltered source
    return filtered_source;  // Return the filtered Brainfuck source
}

// Function to create and populate the jump map for matching '[' and ']' brackets
int* create_jump_map(const char *bf_source, size_t bf_size) {
    int *jump_map = malloc(bf_size * sizeof(int));
    if (!jump_map) {
        perror("Failed to allocate memory for jump map");
        return NULL;
    }

    int stack[256];  // Stack to handle matching brackets
    int stack_ptr = 0;

    for (size_t i = 0; i < bf_size; ++i) {
        if (bf_source[i] == '[') {
            stack[stack_ptr++] = i;  // Push the position of '[' onto the stack
            if (stack_ptr >= 256) {
                fprintf(stderr, "Error: Bracket nesting exceeds maximum stack size\n");
                free(jump_map);
                return NULL;
            }
        } else if (bf_source[i] == ']') {
            if (stack_ptr == 0) {
                fprintf(stderr, "Error: Unmatched ']' at position %zu\n", i);
                free(jump_map);
                return NULL;
            }
            int open = stack[--stack_ptr];  // Pop the position of the matching '[' from the stack
            jump_map[open] = i;  // Map the opening '[' to the closing ']'
            jump_map[i] = open;  // Map the closing ']' to the opening '['
        }
    }

    if (stack_ptr != 0) {
        fprintf(stderr, "Error: Unmatched '[' in input\n");
        free(jump_map);
        return NULL;
    }

    return jump_map;
}


void normalize_bf_code(char *buffer, size_t *input_length) {
    size_t j = 0;
    for (size_t i = 0; i < *input_length; ++i) {
        char c = buffer[i];
        if (c == '>' || c == '<' || c == '+' || c == '-' || c == '.' || c == ',' || c == '[' || c == ']' || c == '#' || c == '$') {
            buffer[j++] = c;
        }
    }
    buffer[j] = '\0'; // Null terminate the normalized code
    *input_length = j; // Update the input length after removing unnecessary characters
    // Print the normalized Brainfuck code after each normalization pass
    //printf("Updated BF program: %s\n", buffer);
}






void optimize_simple_loops(char *buffer, int *jump_map, size_t *input_length) {
    for (size_t i = 0; i < *input_length; ++i) {
        if (buffer[i] == '[') {  // Start of a loop
            int loop_end = jump_map[i];
            int pointerPosition = 0;  // Track pointer movements
            int netChangeAtStart = 0;  // Track changes at starting position
            int isSimple = 1;  // Assume loop is simple until proven otherwise

            SimpleLoopInfo newSimpleLoopInfo = { .position = i, .totalOffsets = 1 };  // Start with initial position as one offset
            memset(newSimpleLoopInfo.changes, 0, sizeof(newSimpleLoopInfo.changes));  // Initialize net changes to zero

            for (int j = i + 1; j < loop_end; ++j) {
                char c = buffer[j];
                switch (c) {
                    case '>':  // Increment pointer position and track new offset
                        pointerPosition++;
                        if (pointerPosition >= newSimpleLoopInfo.totalOffsets) {
                            newSimpleLoopInfo.totalOffsets++;  // Increment total offsets only when moving right and increasing unique offsets
                        }
                        break;
                    case '<':  // Decrement pointer position without changing unique offset count
                        pointerPosition--;
                        break;
                    case '+':  // Increment value at current position
                        newSimpleLoopInfo.changes[pointerPosition]++;
                        if (pointerPosition == 0) netChangeAtStart++;
                        break;
                    case '-':  // Decrement value at current position
                        newSimpleLoopInfo.changes[pointerPosition]--;
                        if (pointerPosition == 0) netChangeAtStart--;
                        break;
                    case '[':
                    case ']':
                    case '.':
                    case ',':
                        isSimple = 0;  // Disqualify the loop if it contains nested loops or I/O
                        break;
                }
            }

            // Check if the loop is simple based on criteria
            if (isSimple && pointerPosition == 0 && (netChangeAtStart == 1 || netChangeAtStart == -1)) {

                printf("Simple loop detected at position %zu with %d total offsets.\n", i, newSimpleLoopInfo.totalOffsets);
                for (int offset = 0; offset < newSimpleLoopInfo.totalOffsets; offset++) {
                    
                        printf("  Offset %d -> Net Change: %d\n", offset, newSimpleLoopInfo.changes[offset]);
                    
                }
                // Replace the entire loop with '#'
                for (int k = i; k <= loop_end; k++) {
                    buffer[k] = ' ';
                }
                buffer[i] = '#';  // Mark the start of the optimized loop
                simple_loop_info_array[simple_loop_info_index++] = newSimpleLoopInfo;
                printf("Simple loop optimized at position %zu with %d total offsets.\n", i, newSimpleLoopInfo.totalOffsets);
            }
        }
    }

    printf("Updated BF program: %s\n", buffer);
}


// Function to optimize non-simple loops that contain only < or > commands
void optimize_non_simple_loops(char *buffer, int *jump_map, size_t *input_length) {
    //int optimized = 0;  // Flag to track if any non-simple loops were optimized

    for (size_t i = 0; i < *input_length; ++i) {
        if (buffer[i] == '[') {
            int loop_end = jump_map[i];  // Find the matching ']'
            
            // Check if the loop contains only < or > instructions
            char loop_type = '\0';  // Store if it is '<' or '>'
            int net_shift = 0;

            int valid = 1;  // Flag to check if loop contains only < or >
            for (size_t j = i + 1; j < loop_end; ++j) {
                if (buffer[j] == '>' || buffer[j] == '<') {
                    if (loop_type == '\0') {
                        loop_type = buffer[j];  // Set loop type on first encounter
                    }
                    if (buffer[j] == loop_type) {
                        net_shift += (loop_type == '>') ? 1 : -1;
                    } else {
                        // Loop has mixed '<' and '>' instructions; not a candidate for optimization
                        valid = 0;
                        break;
                    }
                } else {
                    // **Ignore loops containing operations other than < or >**
                    valid = 0;
                    break;  // Loop has other instructions (e.g., +, -, ., etc.), so it's not just pointer movement
                }
            }

            // Debug: Print the detected pattern and the net shift
            //printf("Detected loop from %zu to %d, net shift: %d\n", i, loop_end, net_shift);

            // If the loop is valid for optimization and net shift is a power of 2
            // Condition updated to handle both positive and negative shifts
            if (valid && net_shift != 0 && (net_shift == -1) && ((net_shift & (net_shift - 1)) == 0 || ((-net_shift) & (-net_shift - 1)) == 0)) {
                //printf("Optimizing loop from %zu to %d with shift value %d\n", i, loop_end, net_shift);

                // Replace the entire loop with '$'
                buffer[i] = '$';
                for (size_t j = i + 1; j <= loop_end; ++j) {
                    buffer[j] = ' ';  // Clear out the loop contents
                }

                // Store position and net shift value in the global metadata array
                loop_info_array[loop_info_index].position = i;
                loop_info_array[loop_info_index].shift_value = net_shift;
                loop_info_index++;

            }
        }
    }

    
}


// Generate assembly for Brainfuck code with vectorized scan support
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
            case '#':  // Optimized simple loop -> directly set *ptr = 0
            
            printf("Got a simple loop at position: %lu\n", i);

            // Retrieve the simple loop information corresponding to this position
            SimpleLoopInfo sli;
            int found = 0;

            // Find the SimpleLoopInfo corresponding to the current position
            for (int index = 0; index < simple_loop_info_index; index++) {
                if (simple_loop_info_array[index].position == i) {  // Match the BF position with stored position
                    sli = simple_loop_info_array[index];
                    found = 1;
                    break;
                }
            }

            if (!found) {
                printf("Error: Could not find SimpleLoopInfo for this loop position.\n");
                break;
            }

            // Debug: Print information about the simple loop
            printf("Processing Simple Loop at position %zu with %d total offsets.\n", sli.position, sli.totalOffsets);
            for (int k = 0; k < sli.totalOffsets; k++) {
                printf("  Offset %d -> Net Change: %d\n", k, sli.changes[k]);
            }

            // Store the initial value at the current position in %cl
            fprintf(out, "movb (%%rsi), %%cl\n");

            // Apply the stored net changes to each unique offset
            for (int offset = 1; offset < sli.totalOffsets; offset++) {
                printf("O---L------\n");
                if (sli.changes[offset] != 0) {  // Only update if there is a net change
                    fprintf(out, "movq %%rsi, %%rax\n");  // Copy the current pointer position
                    fprintf(out, "addq $%d, %%rax\n", offset);  // Move to the offset position
                    fprintf(out, "movb (%%rax), %%dl\n");  // Load the byte at the target offset into %dl
                    fprintf(out, "movb %%cl, %%al\n");  // Move the original value into %al

                    if (sli.changes[offset] > 0) {  // Positive net changes
                        fprintf(out, "mov $%d, %%bl\n", sli.changes[offset]);  // Load the multiplier into %bl
                        fprintf(out, "PositiveMultiplyLoop_%lu_%d:\n", i, offset);  // Start of the multiplication loop
                        fprintf(out, "addb %%al, %%dl\n");  // Add the original value to %dl
                        fprintf(out, "dec %%bl\n");  // Decrement the multiplier
                        fprintf(out, "jnz PositiveMultiplyLoop_%lu_%d\n", i, offset);  // Repeat until %bl is zero
                    } else if (sli.changes[offset] < 0) {  // Negative net changes
                        int abs_value = -sli.changes[offset];  // Convert to positive value for the loop
                        fprintf(out, "mov $%d, %%bl\n", abs_value);  // Load the absolute multiplier into %bl
                        fprintf(out, "NegativeMultiplyLoop_%lu_%d:\n", i, offset);  // Start of the subtraction loop
                        fprintf(out, "subb %%al, %%dl\n");  // Subtract the original value from %dl
                        fprintf(out, "dec %%bl\n");  // Decrement the multiplier
                        fprintf(out, "jnz NegativeMultiplyLoop_%lu_%d\n", i, offset);  // Repeat until %bl is zero
                    }

                    // Store the updated value back at the offset
                    fprintf(out, "movb %%dl, (%%rax)\n");
                }
            }
            // After all updates, set the value at the current position to zero as specified
            fprintf(out, "movb $0, (%%rsi)\n");  // Set the byte at the current position to zero
                
            break;
            
            case '$': {  // Optimized non-simple loop -> vectorized memory scan
                //printf("GOT A # at %lu\n",i);
                int shift_value = 0;
                for (int j = 0; j < loop_info_index; ++j) {
                    if (loop_info_array[j].position == i) {
                        shift_value = loop_info_array[j].shift_value;
                        //printf("GOT A SHIFT VALUE OF -1\n");
                        break;
                    }
                }

                if(shift_value == -1){
                    // For [<]-->#
                    fprintf(out, "loop_start_%zu:\n", i);
                    fprintf(out, "    mov %%rsi, %%rdi             # Copy current position to %%rdi for checking\n");
                    fprintf(out, "    sub $16, %%rdi              # Move back 16 bytes to check previous block\n");
                    fprintf(out, "    lea tape(%%rip), %%rax      # Load address of tape start into %%rax for comparison\n");
                    fprintf(out, "    cmp %%rdi, %%rax            # Compare if %%rdi is before the start of the tape\n");
                    fprintf(out, "    jae load_xmm0_%zu          # Jump to load xmm0 if %%rdi is after start of tape\n", i);
                    fprintf(out, "    lea tape(%%rip), %%rdi     # Adjust %%rdi to the start of tape if %%rdi is before start\n");

                    fprintf(out, "load_xmm0_%zu:\n", i);
                    fprintf(out, "    movdqu (%%rdi), %%xmm0      # Load 16 bytes into %%xmm0 from %%rdi\n");
                    fprintf(out, "    pxor %%xmm1, %%xmm1        # Set %%xmm1 to zeros for comparison\n");
                    fprintf(out, "    pcmpeqb %%xmm1, %%xmm0     # Compare each byte in %%xmm0 to zero\n");
                    fprintf(out, "    pmovmskb %%xmm0, %%eax     # Create a bitmask from comparison results\n");
                    fprintf(out, "    not %%eax                  # Invert the bitmask so zeros become ones\n");
                    fprintf(out, "    lzcntl %%eax, %%eax        # Count leading zeros to find the first set bit\n");
                    fprintf(out, "    test %%eax, %%eax          # Check if a zero byte was found\n");
                    fprintf(out, "    jz found_zero_%zu         # Exit loop if zero was found\n", i);
                    fprintf(out, "    sub $16, %%rsi             # Move %%rsi 16 bytes back for next loop iteration\n");
                    fprintf(out, "    jmp loop_start_%zu\n", i);

                    fprintf(out, "found_zero_%zu:\n", i);
                    fprintf(out, "    mov $31, %%ecx             # Load 31 into %%ecx\n");
                    fprintf(out, "    sub %%eax, %%ecx           # Calculate the index of the first zero bit\n");
                    fprintf(out, "    sub %%rax, %%rsi           # Adjust %%rsi to point to the first zero byte found\n");
                    fprintf(out, "    subq $1, %%rsi\n");
                }
                
                break;
            }


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

    // Create jump map
    int *jump_map = create_jump_map(bf_source, bf_size);
    if (!jump_map) {
        free(bf_source);
        fclose(out);
        return 1;
    }

    
    //optimize_simple_loops(bf_source, jump_map, &bf_size);

    //Optimize non-simple loops using global variables for loop info
    //optimize_non_simple_loops(bf_source, jump_map, &bf_size);

    // Generate assembly code
    generate_assembly(bf_source, bf_size, out);

    // Clean up
    fclose(out);
    free(bf_source);
    free(jump_map);

    return 0;
}