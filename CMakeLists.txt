cmake_minimum_required(VERSION 3.12)
project(DiagnosticSystem C)

# Configuration options
option(DIAG_ENABLE_TESTS "Enable diagnostic system tests" ON)
option(DIAG_ENABLE_LOGGING "Enable diagnostic system logging" ON)
option(DIAG_DEVELOPMENT_BUILD "Enable development features" OFF)
option(DIAG_ENABLE_RECORDING "Enable diagnostic recording" ON)
option(DIAG_ENABLE_MONITORING "Enable diagnostic monitoring" ON)
option(DIAG_MAX_RECORD_ENTRIES "Maximum number of record entries" "1000")

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
    src/runtime/diagnostic/diag_logger.c
    src/runtime/diagnostic/diag_monitor.c
    src/runtime/diagnostic/diag_router.c
    src/runtime/diagnostic/diag_filter.c
    src/runtime/network/message_handler.c
    src/hardware/timer_hw.c
    src/hardware/watchdog.c
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
        tests/diagnostic/test_diag_logger.c
        tests/diagnostic/test_diag_monitor.c
        tests/diagnostic/test_diag_integration_advanced.c
        tests/diagnostic/test_diag_router_advanced.c
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

# Configure features
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/diag_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/diag_config.h
)

# Hardware specific stuff
if(STM32_TARGET)
    add_definitions(-DUSE_HAL_DRIVER)
    add_definitions(-DSTM32F407xx)
endif()

# Update include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/hardware
    ${CMAKE_BINARY_DIR}/include
)

# Add performance test executable
add_executable(test_network_perf
    tests/performance/test_network_perf.c
    ${COMMON_SOURCES}
)

target_link_libraries(test_network_perf
    diagnostic
    unity
)

# Add new source files to diagnostic library
target_sources(diagnostic
    PRIVATE
        src/runtime/network/buffer_manager.c
        src/runtime/network/message_handler.c
        src/runtime/memory/mem_pool.c
        src/runtime/diagnostic/route_table.c
        src/runtime/diagnostic/perf_monitor.c
        src/runtime/core/critical_section.c
        src/runtime/core/sys_monitor.c
        src/hardware/interrupt_handler.c
        src/hardware/dma_config.c
)

# Hardware specific configurations
if(STM32_TARGET)
    target_compile_definitions(diagnostic
        PRIVATE
            USE_HAL_DRIVER
            STM32F407xx
            USE_DMA
            USE_WATCHDOG
    )
endif()

# Add test coverage
add_test(NAME test_network_perf
         COMMAND test_network_perf)

set_tests_properties(test_network_perf
    PROPERTIES
        TIMEOUT 120
        LABELS "performance"
)
