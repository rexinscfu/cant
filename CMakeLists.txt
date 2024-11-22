cmake_minimum_required(VERSION 3.12)
project(DiagnosticSystem)

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
