cmake_minimum_required(VERSION 3.15)
project(DiagnosticSystem C)

# Compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Optimization flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU")
    set(OPT_FLAGS "-O3 -flto -ffunction-sections -fdata-sections")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OPT_FLAGS} -Wall -Wextra")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
endif()

# STM32 specific settings
if(STM32_TARGET)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
    add_definitions(-DUSE_HAL_DRIVER -DSTM32F407xx)
endif()

# Source files
set(CORE_SOURCES
    src/runtime/core/sys_monitor.c
    src/runtime/core/critical_section.c
    src/runtime/core/cache_opt.c
)

set(MEMORY_SOURCES
    src/runtime/memory/mem_pool.c
)

set(NETWORK_SOURCES
    src/runtime/network/buffer_manager.c
    src/runtime/network/message_handler.c
    src/runtime/network/network_handler.c
)

set(DIAGNOSTIC_SOURCES
    src/runtime/diagnostic/diag_router.c
    src/runtime/diagnostic/diag_filter.c
    src/runtime/diagnostic/perf_monitor.c
    src/runtime/diagnostic/route_table.c
)

set(HARDWARE_SOURCES
    src/hardware/timer_hw.c
    src/hardware/timer_opt.c
    src/hardware/dma_config.c
    src/hardware/interrupt_handler.c
    src/hardware/watchdog.c
)

# Test sources
set(TEST_SOURCES
    tests/final/system_integration_test.c
    tests/final/hardware_test.c
    tests/performance/test_network_perf.c
    tests/performance/test_system_perf.c
)

# Main library
add_library(diagnostic_core STATIC
    ${CORE_SOURCES}
    ${MEMORY_SOURCES}
    ${NETWORK_SOURCES}
    ${DIAGNOSTIC_SOURCES}
    ${HARDWARE_SOURCES}
)

target_include_directories(diagnostic_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hardware
)

# Unity test framework
add_library(unity STATIC
    external/unity/unity.c
)

target_include_directories(unity PUBLIC
    external/unity
)

# Test executables
foreach(test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} 
        diagnostic_core
        unity
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Performance test settings
set_tests_properties(test_network_perf test_system_perf
    PROPERTIES
        TIMEOUT 300
        LABELS "performance"
)

# Integration test settings
set_tests_properties(system_integration_test hardware_test
    PROPERTIES
        TIMEOUT 600
        LABELS "integration"
)

# Install targets
install(TARGETS diagnostic_core
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY src/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating documentation with Doxygen"
        VERBATIM
    )
endif()

# Build type specific settings
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG -g3 -Og")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG ${OPT_FLAGS}")

# Cache settings for better build performance
set_directory_properties(PROPERTIES
    CLEAN_NO_CUSTOM TRUE
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DNDEBUG)
    add_definitions(-DRELEASE_BUILD)
endif()

