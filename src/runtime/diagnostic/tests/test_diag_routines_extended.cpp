#include <gtest/gtest.h>
#include "../routines/diag_routines.h"
#include "mocks/battery_mock.h"
#include "mocks/memory_mock.h"
#include "mocks/network_mock.h"

class DiagRoutinesExtendedTest : public ::testing::Test {
protected:
    void SetUp() override {
        Battery_Mock_Init();
        Memory_Mock_Init();
        Network_Mock_Init();
    }

    void TearDown() override {
        Battery_Mock_Deinit();
        Memory_Mock_Deinit();
        Network_Mock_Deinit();
    }
};

TEST_F(DiagRoutinesExtendedTest, MemoryCheckTest) {
    // Test start with valid parameters
    uint32_t start_address = 0x20000000;
    uint32_t size = 0x1000;
    uint8_t start_data[8];
    memcpy(start_data, &start_address, 4);
    memcpy(&start_data[4], &size, 4);
    
    ASSERT_TRUE(memory_check_start(start_data, sizeof(start_data)));
    
    // Set mock results
    Memory_Mock_SetCheckStatus(0x01);
    Memory_Mock_SetChecksum(0x12345678);
    Memory_Mock_SetCheckComplete(true);
    
    RoutineResult result;
    ASSERT_TRUE(memory_check_get_result(&result));
    ASSERT_EQ(result.result_code, 0);
    ASSERT_EQ(result.length, 6);
    
    // Verify result data
    ASSERT_EQ(result.data[0], 0x01); // Check complete
    ASSERT_EQ(result.data[1], 0x01); // Check status
    uint32_t checksum;
    memcpy(&checksum, &result.data[2], 4);
    ASSERT_EQ(checksum, 0x12345678);
    
    // Test stop
    ASSERT_TRUE(memory_check_stop());
}

TEST_F(DiagRoutinesExtendedTest, NetworkTestComprehensive) {
    // Test start with multiple nodes
    uint8_t start_data[] = {0x05}; // Test 5 nodes
    ASSERT_TRUE(network_test_start(start_data, sizeof(start_data)));
    
    // Set mock network test results
    Network_Mock_SetActiveNodeCount(4);
    uint16_t response_times[] = {100, 150, 200, 125, 0};
    uint8_t error_counts[] = {0, 1, 0, 0, 255};
    Network_Mock_SetResponseTimes(response_times);
    Network_Mock_SetErrorCounts(error_counts);
    Network_Mock_SetTestComplete(true);
    
    RoutineResult result;
    ASSERT_TRUE(network_test_get_result(&result));
    ASSERT_EQ(result.result_code, 0);
    
    // Verify result data
    ASSERT_EQ(result.data[0], 0x01); // Test complete
    ASSERT_EQ(result.data[1], 0x04); // Active nodes
    
    // Verify response times
    uint16_t result_times[5];
    memcpy(result_times, &result.data[2], 10);
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(result_times[i], response_times[i]);
    }
    
    // Verify error counts
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(result.data[12 + i], error_counts[i]);
    }
    
    // Test stop
    ASSERT_TRUE(network_test_stop());
}

TEST_F(DiagRoutinesExtendedTest, BatteryTestComprehensive) {
    // Test different battery test types
    struct TestCase {
        uint8_t test_type;
        float expected_voltage;
        float expected_current;
        float expected_temp;
        uint8_t expected_health;
    } test_cases[] = {
        {0x01, 12.6f, 5.0f, 25.0f, 0x01},  // Load test
        {0x02, 14.2f, -2.5f, 30.0f, 0x02}, // Charging test
        {0x03, 12.8f, 0.1f, 22.0f, 0x03}   // Health check
    };
    
    for (const auto& tc : test_cases) {
        uint8_t start_data[] = {tc.test_type};
        ASSERT_TRUE(battery_test_start(start_data, sizeof(start_data)));
        
        Battery_Mock_SetVoltage(tc.expected_voltage);
        Battery_Mock_SetCurrent(tc.expected_current);
        Battery_Mock_SetTemperature(tc.expected_temp);
        Battery_Mock_SetHealthStatus(tc.expected_health);
        Battery_Mock_SetTestComplete(true);
        
        RoutineResult result;
        ASSERT_TRUE(battery_test_get_result(&result));
        ASSERT_EQ(result.result_code, 0);
        
        float voltage, current, temperature;
        memcpy(&voltage, &result.data[1], 4);
        memcpy(&current, &result.data[5], 4);
        memcpy(&temperature, &result.data[9], 4);
        
        ASSERT_FLOAT_EQ(voltage, tc.expected_voltage);
        ASSERT_FLOAT_EQ(current, tc.expected_current);
        ASSERT_FLOAT_EQ(temperature, tc.expected_temp);
        ASSERT_EQ(result.data[13], tc.expected_health);
        
        ASSERT_TRUE(battery_test_stop());
    }
} 