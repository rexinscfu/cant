# Diagnostic System Testing Guide

## Overview
This document describes the testing strategy and provides examples for the diagnostic system implementation.

## Test Categories

### 1. Unit Tests
Individual components are tested in isolation using mock objects.

Example for testing data identifiers:
```cpp
TEST_F(DiagDataTest, ReadVehicleInfo) {
    uint8_t data[32];
    uint16_t length;
    
    // Set mock VIN
    const char* test_vin = "WDD2030461A123456";
    NVRAM_Mock_SetData(NVRAM_ADDR_VIN, (uint8_t*)test_vin, strlen(test_vin));
    
    ASSERT_TRUE(read_vehicle_info(DID_VIN, data, &length));
    ASSERT_EQ(length, 17);
    ASSERT_EQ(memcmp(data, test_vin, length), 0);
}
```

### 2. Integration Tests
Tests multiple components working together.

Example for testing a complete diagnostic session:
```cpp
TEST_F(DiagIntegrationTest, CompleteSessionFlow) {
    // Start session
    uint8_t session_request[] = {0x10, 0x03};
    UdsMessage response;
    ASSERT_EQ(Diag_System_HandleRequest(session_request, sizeof(session_request), &response),
        UDS_RESPONSE_OK);
        
    // Security access
    uint8_t seed_request[] = {0x27, 0x01};
    ASSERT_EQ(Diag_System_HandleRequest(seed_request, sizeof(seed_request), &response),
        UDS_RESPONSE_OK);
}
```

### 3. System Tests
Tests complete diagnostic workflows including timing and resource usage.

## Test Configuration
Tests can be configured using the `test_config.yaml` file:

- Transport settings
- Session timeouts
- Security parameters
- Routine configurations
- Data identifier definitions

## Running Tests
```bash
# Build and run all tests
cmake -B build
cmake --build build
cd build && ctest

# Run specific test categories
./diagnostic_tests --gtest_filter=DiagDataTest.*
./diagnostic_tests --gtest_filter=DiagIntegrationTest.*
```

## Mock Objects
The system uses several mock objects for testing:

- ECU Mock: Simulates ECU hardware and basic functions
- Battery Mock: Simulates battery measurements and tests
- Sensors Mock: Simulates various sensor inputs
- Network Mock: Simulates network communication

Example mock usage:
```cpp
// Setup mock data
Battery_Mock_SetVoltage(12.6f);
Battery_Mock_SetCurrent(5.0f);
Battery_Mock_SetTemperature(25.0f);

// Perform test
RoutineResult result;
ASSERT_TRUE(battery_test_get_result(&result));
```

## Test Coverage
The test suite aims to achieve:

- 100% function coverage
- 90% branch coverage
- 85% line coverage

Coverage reports can be generated using gcov/lcov tools.

## Best Practices

1. Always mock hardware dependencies
2. Test edge cases and error conditions
3. Verify timeout handling
4. Check memory usage during long operations
5. Test concurrent operations where applicable

## Troubleshooting

### Failed Tests
1. Check mock configurations
2. Verify test prerequisites
3. Check system resources
4. Review timeout settings

### Coverage Issues
1. Identify uncovered code paths
2. Add test cases for error conditions
3. Test timeout and interrupt handlers
4. Verify cleanup handlers

## Maintenance

- Regular test suite updates
- Mock object maintenance
- Configuration updates
- Coverage monitoring