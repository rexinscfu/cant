cmake_minimum_required(VERSION 3.20)
project(cant VERSION 0.0.1 LANGUAGES C)

# Require C23 support
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find LLVM package
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Add LLVM definitions
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

# Configure compiler flags
if(CMAKE_C_COMPILER_ID

# Add diagnostic system
add_subdirectory(src/runtime/diagnostic)

# Update main target dependencies
target_link_libraries(cant_runtime
    PRIVATE
        os
        utils
        diagnostic
)

# Add examples if requested
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
