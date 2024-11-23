cmake_minimum_required(VERSION 3.12)
project(CANT VERSION 1.0.0 LANGUAGES C CXX)

# Enable testing
enable_testing()

# Find required packages
find_package(GTest REQUIRED)
find_package(GMock REQUIRED)

# Set compiler flags
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Source files
set(DIAG_SOURCES
    src/runtime/diagnostic/diag_system.c
    src/runtime/diagnostic/service_router.c
    src/runtime/diagnostic/session_manager.c
    src/runtime/diagnostic/security_manager.c
    src/runtime/diagnostic/memory_manager.c
    src/runtime/diagnostic/data_manager.c
    src/runtime/diagnostic/routine_manager.c
    src/runtime/diagnostic/services/diag_services.c
    src/runtime/diagnostic/routines/diag_routines.c
    src/runtime/diagnostic/data/diag_data.c
)

# Test files
set(TEST_SOURCES
    src/runtime/diagnostic/tests/test_diag_data.cpp
    src/runtime/diagnostic/tests/test_diag_routines.cpp
    src/runtime/diagnostic/tests/test_diag_routines_extended.cpp
    src/runtime/diagnostic/tests/test_diag_integration.cpp
    src/runtime/diagnostic/tests/mocks/ecu_mock.c
    src/runtime/diagnostic/tests/mocks/battery_mock.c
    src/runtime/diagnostic/tests/mocks/sensors_mock.c
    src/runtime/diagnostic/tests/mocks/network_mock.c
)

# Create library
add_library(diagnostic STATIC ${DIAG_SOURCES})
target_include_directories(diagnostic PUBLIC src)

# Create test executable
add_executable(diagnostic_tests ${TEST_SOURCES})
target_link_libraries(diagnostic_tests
    diagnostic
    GTest::GTest
    GTest::Main
    GMock::GMock
)

# Add tests
add_test(NAME DiagnosticTests COMMAND diagnostic_tests)

# Add network components to runtime library
set(NETWORK_SOURCES
    src/runtime/network/net_core.c
    src/runtime/network/net_buffer.c
    src/runtime/network/net_protocol.c
    src/runtime/network/net_interface.c
    src/runtime/platform/hardware/ethernet.c
    src/runtime/platform/hardware/wifi.c
    src/runtime/platform/hardware/cellular.c
    src/runtime/platform/hardware/can.c
)

set(NETWORK_HEADERS
    src/runtime/network/net_core.h
    src/runtime/network/net_buffer.h
    src/runtime/network/net_protocol.h
    src/runtime/network/net_interface.h
    src/runtime/platform/hardware/ethernet.h
    src/runtime/platform/hardware/wifi.h
    src/runtime/platform/hardware/cellular.h
    src/runtime/platform/hardware/can.h
)

target_sources(cant_runtime
    PRIVATE
        ${NETWORK_SOURCES}
    PUBLIC
        ${NETWORK_HEADERS}
)

# Add network tests
add_executable(network_tests
    test/runtime/network/test_net_core.c
    test/runtime/network/test_net_buffer.c
    test/runtime/network/test_net_protocol.c
    test/runtime/network/test_net_interface.c
)

target_link_libraries(network_tests
    PRIVATE
        cant_runtime
        unity
)

# Add memory management components
set(MEMORY_SOURCES
    src/runtime/memory/memory_manager.c
)

set(MEMORY_HEADERS
    src/runtime/memory/memory_manager.h
)

target_sources(cant_runtime
    PRIVATE
        ${MEMORY_SOURCES}
    PUBLIC
        ${MEMORY_HEADERS}
)

# Add memory tests
add_executable(memory_tests
    test/runtime/memory/test_memory_manager.c
)

target_link_libraries(memory_tests
    PRIVATE
        cant_runtime
        unity
)
