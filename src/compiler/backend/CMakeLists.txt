# Find LLVM package
find_package(LLVM REQUIRED CONFIG)

# Get LLVM definitions and include paths
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

# Add compiler backend library
add_library(compiler_backend STATIC
    llvm_gen.c
    llvm_gen.h
    llvm_generator.c
    llvm_generator.h
    llvm_optimizations.c
    llvm_optimizations.h
)

# Link against LLVM libraries
llvm_map_components_to_libnames(llvm_libs
    core
    analysis
    target
    mcjit
    native
    vectorize
    scalaropts
)

target_link_libraries(compiler_backend
    PRIVATE
        ${llvm_libs}
        compiler_frontend
)

target_include_directories(compiler_backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/compiler/frontend
        ${CMAKE_SOURCE_DIR}/src/compiler/middle
)

# Add optimization flags
target_compile_options(compiler_backend
    PRIVATE
        -O3
        -march=native
        -ffast-math
)

# Add backend library
add_library(cant_backend
    target/s32k3.c
    target/tda4vm.c
)

# Link against LLVM libraries
target_link_libraries(cant_backend
    LLVM
    cant_middle
    cant_utils
)

# Set compiler flags
target_compile_options(cant_backend PRIVATE
    -Wall
    -Wextra
    -Werror
    -fPIC
)

# Install backend library
install(TARGETS cant_backend
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
) 