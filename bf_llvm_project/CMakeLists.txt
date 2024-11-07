cmake_minimum_required(VERSION 3.13)
project(BFCompiler)

# Specify the path to the installed LLVM
set(LLVM_DIR "/usr/lib/llvm-16/lib/cmake/llvm")

# Find the LLVM package
find_package(LLVM 16.0 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Add the LLVM header paths
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Define the executable that will use your bf_llvm.cpp source
add_executable(bf_compiler bf_llvm.cpp)

# Find the libraries that correspond to the LLVM components
# that we wish to use. LLVM_AVAILABLE_LIBS or llvm_map_components_to_libnames
# can be used to map component names to library names.
llvm_map_components_to_libnames(llvm_libs support core irreader)

# Link against LLVM libraries
target_link_libraries(bf_compiler ${llvm_libs})
