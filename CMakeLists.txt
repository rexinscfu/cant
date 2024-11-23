cmake_minimum_required(VERSION 3.12)
project(DiagnosticSystem C)

# Configuration options
option(DIAG_ENABLE_TESTS "Enable diagnostic system tests" ON)
option(DIAG_ENABLE_LOGGING "Enable diagnostic system logging" ON)
option(DIAG_DEVELOPMENT_BUILD "Enable development features" OFF)

# Compiler settings
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add compile definitions based on options
if(DIAG_ENABLE_LOGGING)
    add_definitions(-DDIAG_LOGGING_ENABLED)
endif()

if(DIAG_DEVELOPMENT_BUILD)
    add_definitions(-DDEVELOPMENT_BUILD)
endif()

# Source files
set(DIAG_SOURCES
    src/runtime/diagnostic/diag_core.c
    src/runtime/diagnostic/diag_session.c
    src/runtime/diagnostic/diag_security.c
    src/runtime/diagnostic/diag_parser.c
    src/runtime/diagnostic/diag_state.c
    src/runtime/diagnostic/diag_timer.c
    src/runtime/diagnostic/diag_error.c
)

# Create diagnostic library
add_library(diagnostic STATIC ${DIAG_SOURCES})

target_include_directories(diagnostic PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Tests
if(DIAG_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(external/unity)
    
    # Test executables
    set(TEST_SOURCES
        tests/diagnostic/test_diag_core.c
        tests/diagnostic/test_diag_session.c
        tests/diagnostic/test_diag_security.c
        tests/diagnostic/test_diag_parser.c
        tests/diagnostic/test_diag_state.c
        tests/diagnostic/test_diag_timer.c
        tests/diagnostic/test_diag_error.c
        tests/diagnostic/test_diag_integration.c
    )
    
    foreach(test_source ${TEST_SOURCES})
        get_filename_component(test_name ${test_source} NAME_WE)
        add_executable(${test_name} ${test_source})
        target_link_libraries(${test_name} diagnostic unity)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()

# Installation
install(TARGETS diagnostic
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
