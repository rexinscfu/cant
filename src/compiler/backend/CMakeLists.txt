# Find LLVM package
find_package(LLVM 15.0 REQUIRED CONFIG)

# Add LLVM definitions and includes
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

# Add backend library
add_library(cant_backend
    llvm_generator.c
    llvm_optimizations.c
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