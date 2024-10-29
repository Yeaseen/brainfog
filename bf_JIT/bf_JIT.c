#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#define TAPE_SIZE 30000

// Global variables to store metadata for `$` optimizations
typedef struct {
    size_t position;  // Position of '$' in the Brainfuck code
    int shift_value;  // Net shift value (e.g., -4 for [<<<<], +4 for [>>>>])
} LoopInfo;

// Declare a global array to store loop information and an index to track it
LoopInfo loop_info_array[256];  // Store metadata for up to 256 loops
int loop_info_index = 0;        // Index to track the number of loops

#define MAX_SIMPLE_LOOPS 1000
#define MAX_OFFSETS 50  // Maximum number of offsets we'll track for a simple loop

// Struct to hold an offset and its corresponding net value change
typedef struct {
    int offset;      // Relative position from the starting pointer position
    int net_change;  // Net change in value at this offset
} OffsetChange;

// Struct to hold simple loop information
typedef struct {
    int position;            // Starting position of the loop in the code
    int totalOffsets;        // Number of unique offsets in the loop
    OffsetChange changes[MAX_OFFSETS];  // Array to hold changes at different offsets
} SimpleLoopInfo;

// Array to store all detected simple loops
SimpleLoopInfo simple_loop_info_array[MAX_SIMPLE_LOOPS];
int simple_loop_info_index = 0;


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

    // Initialize jump map to -1 to indicate unassigned positions
    for (size_t i = 0; i < bf_size; ++i) {
        jump_map[i] = -1;
    }

    // Stack to handle matching brackets
    int stack[256];
    int stack_ptr = 0;

    for (size_t i = 0; i < bf_size; ++i) {
        if (bf_source[i] == '[') {
            if (stack_ptr >= 256) {
                fprintf(stderr, "Error: Stack overflow - bracket nesting too deep at position %zu\n", i);
                free(jump_map);
                return NULL;
            }
            stack[stack_ptr++] = i;  // Push the position of '[' onto the stack
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

    // If stack_ptr is not 0, then there are unmatched '[' brackets
    if (stack_ptr != 0) {
        fprintf(stderr, "Error: Unmatched '[' in input\n");
        free(jump_map);
        return NULL;
    }

    // Print and validate the jump map for correctness
    for (size_t i = 0; i < bf_size; ++i) {
        if (bf_source[i] == '[' || bf_source[i] == ']') {
            if (jump_map[i] == -1 || jump_map[i] < 0 || jump_map[i] >= bf_size) {
                fprintf(stderr, "Error: Invalid jump map entry at index %zu. Jump map value: %d\n", i, jump_map[i]);
                free(jump_map);
                return NULL;
            }
            //printf("Index %zu: %c maps to %d\n", i, bf_source[i], jump_map[i]);
        }
    }

    return jump_map;
}



// Function to find or create an entry for a given offset in a SimpleLoopInfo struct
void add_or_update_offset(SimpleLoopInfo *loop_info, int pointerPosition) {
    int found = 0;
    for (int k = 0; k < loop_info->totalOffsets; ++k) {
        if (loop_info->changes[k].offset == pointerPosition) {
            found = 1;
            break;
        }
    }
    // If not found, add a new offset entry with a net change of 0 (default value)
    if (!found) {
        loop_info->changes[loop_info->totalOffsets].offset = pointerPosition;
        loop_info->changes[loop_info->totalOffsets].net_change = 0;
        loop_info->totalOffsets++;
    }
}

// Function to optimize simple loops by replacing them with '#'
void optimize_simple_loops(char *buffer, int *jump_map, size_t *input_length) {
    for (size_t i = 0; i < *input_length; ++i) {
        if (buffer[i] == '[') {  // Start of a loop
            int loop_end = jump_map[i];

            // Initialize variables to track loop properties
            int pointerPosition = 0;  
            int netChangeAtStart = 0;  
            int isSimple = 1;  
            int contains_io = 0;  
            int contains_inner_loop = 0;

            SimpleLoopInfo newSimpleLoopInfo = { .position = i, .totalOffsets = 0 };

            // Analyze the loop to determine its behavior
            for (int j = i + 1; j < loop_end; ++j) {
                if (buffer[j] == '>') {
                    pointerPosition++;
                    add_or_update_offset(&newSimpleLoopInfo, pointerPosition);  // Track the offset
                } else if (buffer[j] == '<') {
                    pointerPosition--;
                    add_or_update_offset(&newSimpleLoopInfo, pointerPosition);  // Track the offset
                } else if (buffer[j] == '+') {
                    add_or_update_offset(&newSimpleLoopInfo, pointerPosition);  // Ensure offset exists
                    for (int k = 0; k < newSimpleLoopInfo.totalOffsets; ++k) {
                        if (newSimpleLoopInfo.changes[k].offset == pointerPosition) {
                            newSimpleLoopInfo.changes[k].net_change++;
                            break;
                        }
                    }
                    if (pointerPosition == 0) netChangeAtStart++;
                } else if (buffer[j] == '-') {
                    add_or_update_offset(&newSimpleLoopInfo, pointerPosition);  // Ensure offset exists
                    for (int k = 0; k < newSimpleLoopInfo.totalOffsets; ++k) {
                        if (newSimpleLoopInfo.changes[k].offset == pointerPosition) {
                            newSimpleLoopInfo.changes[k].net_change--;
                            break;
                        }
                    }
                    if (pointerPosition == 0) netChangeAtStart--;
                } else if (buffer[j] == '.' || buffer[j] == ',') {
                    contains_io = 1;  // I/O disqualifies the loop from being simple
                    break;
                } else if (buffer[j] == '[') {
                    contains_inner_loop = 1;  // Inner loop found
                    break;
                }
            }

            // Skip loops that contain I/O or inner loops
            if (contains_io || contains_inner_loop) {
                printf("Skipping complex loop starting at %zu due to I/O or nested loops.\n", i);
                continue;
            }

            // Check if the loop is simple:
            // - No I/O
            // - Pointer returns to starting position (pointerPosition == 0)
            // - The net change at the starting position is exactly +1 or -1
            if (pointerPosition == 0 && (netChangeAtStart == 1 || netChangeAtStart == -1)) {
                //printf("Simple loop detected at position %zu with %d total offsets.\n", i, newSimpleLoopInfo.totalOffsets);
                //for (int offset = 0; offset < newSimpleLoopInfo.totalOffsets; offset++) {
                //    printf("  Offset %d -> Net Change: %d\n", newSimpleLoopInfo.changes[offset].offset, newSimpleLoopInfo.changes[offset].net_change);
                //}

                // Save the simple loop info before modifying the buffer
                simple_loop_info_array[simple_loop_info_index++] = newSimpleLoopInfo;

                // Replace the entire loop with '#'
                for (int k = i; k <= loop_end; k++) {
                    buffer[k] = ' ';
                }
                buffer[i] = '#';  // Mark the start of the optimized loop
                
                // Skip to the end of the loop after optimization to avoid re-processing
                //i = loop_end;
                printf("Simple loop optimized at position %zu with %d total offsets.\n", i, newSimpleLoopInfo.totalOffsets);
            }
        }
    }
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
                //printf("NOT GETTING THE DOST??");
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
            //printf("Have we got a #\n");
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

            // Store the initial value at the current position in %cl
            

            // Apply the stored net changes to each unique offset, skipping the offset 0
            for (int index = 0; index < sli.totalOffsets; index++) {
                int offset = sli.changes[index].offset;
                int net_change = sli.changes[index].net_change;

                if (offset == 0 || net_change == 0) {
                    // Skip offset 0 or if there's no net change to apply
                    continue;
                }
                fprintf(out, "movb (%%rsi), %%cl\n");
                // Handle positive and negative offsets relative to %rsi
                fprintf(out, "movq %%rsi, %%rax\n");  // Copy the current pointer position to %rax
                if (offset > 0) {
                    fprintf(out, "addq $%d, %%rax\n", offset);  // Move to the positive offset position
                } else if (offset < 0) {
                    fprintf(out, "subq $%d, %%rax\n", -offset);  // Move to the negative offset position
                }

                fprintf(out, "movb (%%rax), %%dl\n");  // Load the byte at the target offset into %dl
                fprintf(out, "movb %%cl, %%al\n");  // Move the original value into %al

                if (net_change > 0) {  // Positive net changes
                    fprintf(out, "mov $%d, %%bl\n", net_change);  // Load the multiplier into %bl
                    fprintf(out, "PositiveMultiplyLoop_%lu_%d:\n", i, index);  // Start of the multiplication loop
                    fprintf(out, "addb %%al, %%dl\n");  // Add the original value to %dl
                    fprintf(out, "dec %%bl\n");  // Decrement the multiplier
                    fprintf(out, "jnz PositiveMultiplyLoop_%lu_%d\n", i, index);  // Repeat until %bl is zero
                } else {  // Negative net changes
                    int abs_value = -net_change;  // Convert to positive value for the loop
                    fprintf(out, "mov $%d, %%bl\n", abs_value);  // Load the absolute multiplier into %bl
                    fprintf(out, "NegativeMultiplyLoop_%lu_%d:\n", i, index);  // Start of the subtraction loop
                    fprintf(out, "subb %%al, %%dl\n");  // Subtract the original value from %dl
                    fprintf(out, "dec %%bl\n");  // Decrement the multiplier
                    fprintf(out, "jnz NegativeMultiplyLoop_%lu_%d\n", i, index);  // Repeat until %bl is zero
                }

                fprintf(out, "movq %%rsi, %%rax\n");  // Copy the current pointer position to %rax
                if (offset > 0) {
                    fprintf(out, "addq $%d, %%rax\n", offset);  // Move to the positive offset position
                } else if (offset < 0) {
                    fprintf(out, "subq $%d, %%rax\n", -offset);  // Move to the negative offset position
                }
                // Store the updated value back at the offset
                fprintf(out, "movb %%dl, (%%rax)\n");
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
// Assemble the generated assembly code into an object file
int assemble_code(const char *assembly_file, const char *object_file) {
    char command[256];
    snprintf(command, sizeof(command), "gcc -c -o %s %s", object_file, assembly_file);
    int result = system(command);
    if (result != 0) {
        fprintf(stderr, "Error: Assembly failed\n");
        return 1;
    }
    return 0;
}

// Load the object file into executable memory
void* load_object_code(const char *object_file, size_t *code_size) {
    // Open the object file
    FILE *file = fopen(object_file, "rb");
    if (!file) {
        perror("Failed to open object file");
        return NULL;
    }

    // Get the size of the object file
    fseek(file, 0, SEEK_END);
    *code_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate executable memory for the object code
    void* exec_memory = mmap(NULL, *code_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (exec_memory == MAP_FAILED) {
        perror("mmap");
        fclose(file);
        return NULL;
    }

    // Read the object file into the allocated memory
    fread(exec_memory, 1, *code_size, file);
    fclose(file);

    return exec_memory;
}

// Execute the JIT-compiled code
void execute_jit_code(void* exec_memory) {
    // Cast the executable memory to a function pointer and execute it
    void (*jit_function)() = (void (*)())exec_memory;
    jit_function();
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.bf>\n", argv[0]);
        return 1;
    }

    // Read the Brainfuck source code from file
    size_t bf_size;
    char *bf_source = read_bf_file(argv[1], &bf_size);

    

    // Create jump map
    int *jump_map = create_jump_map(bf_source, bf_size);
    if (!jump_map) {
        free(bf_source);
        return 1;
    }

    
    //optimize_simple_loops(bf_source, jump_map, &bf_size);

    //Optimize non-simple loops using global variables for loop info
    //optimize_non_simple_loops(bf_source, jump_map, &bf_size);

    // Generate assembly code

     // Step 2: Create a temporary file for the assembly code
    char assembly_filename[] = "/tmp/bf_XXXXXX.s";
    int assembly_fd = mkstemps(assembly_filename, 2);  // Create temporary assembly file with '.s' suffix
    if (assembly_fd == -1) {
        perror("Failed to create temporary assembly file");
        free(bf_source);
        return 1;
    }
    FILE *assembly_file = fdopen(assembly_fd, "w");
    if (!assembly_file) {
        perror("Failed to open temporary assembly file");
        free(bf_source);
        return 1;
    }

    // Step 3: Generate assembly code
    generate_assembly(bf_source, bf_size, assembly_file);
    fclose(assembly_file);  // Close the assembly file
    free(bf_source);  // Free the Brainfuck source code
    free(jump_map);

    // Step 4: Assemble the generated assembly code into an object file
    char object_filename[] = "/tmp/bf_XXXXXX.o";
    int object_fd = mkstemps(object_filename, 2);  // Create temporary object file with '.o' suffix
    close(object_fd);  // We don't need the file descriptor after creating the file
    if (assemble_code(assembly_filename, object_filename) != 0) {
        fprintf(stderr, "Failed to assemble the code\n");
        return 1;
    }

    // Step 5: Load the object code into executable memory
    size_t code_size;
    void* exec_memory = load_object_code(object_filename, &code_size);
    if (!exec_memory) {
        return 1;
    }

    // Step 6: Execute the JIT-compiled code
    execute_jit_code(exec_memory);

    // Clean up memory and remove temporary files
    munmap(exec_memory, code_size);
    unlink(assembly_filename);  // Remove the temporary assembly file
    unlink(object_filename);    // Remove the temporary object file

    return 0;
}