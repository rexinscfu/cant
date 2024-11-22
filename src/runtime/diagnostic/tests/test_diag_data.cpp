#include <gtest/gtest.h>
#include "../data/diag_data.h"
#include "../../hw/ecu_mock.h"
#include "../../hw/battery_mock.h"
#include "../../hw/sensors_mock.h"
#include "../../hw/network_mock.h"
#include "../../utils/nvram_mock.h"

class DiagDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize mocks
        ECU_Mock_Init();
        Battery_Mock_Init();
        Sensors_Mock_Init();
        Network_Mock_Init();
        NVRAM_Mock_Init();
        
        // Initialize diagnostic data
        Diag_Data_Init();
    }

    void TearDown() override {
        ECU_Mock_Deinit();
        Battery_Mock_Deinit();
        Sensors_Mock_Deinit();
        Network_Mock_Deinit();
        NVRAM_Mock_Deinit();
    }
};

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

TEST_F(DiagDataTest, WriteVehicleInfo) {
    const char* test_vin = "WDD2030461A789012";
    ASSERT_TRUE(write_vehicle_info(DID_VIN, (uint8_t*)test_vin, strlen(test_vin)));
    
    uint8_t stored_vin[32];
    uint16_t stored_length;
    NVRAM_Mock_GetData(NVRAM_ADDR_VIN, stored_vin, &stored_length);
    ASSERT_EQ(stored_length, strlen(test_vin));
    ASSERT_EQ(memcmp(stored_vin, test_vin, stored_length), 0);
}

TEST_F(DiagDataTest, ReadSystemStatus) {
    uint8_t data[32];
    uint16_t length;
    
    // Set mock values
    Battery_Mock_SetVoltage(12.6f);
    ECU_Mock_SetEngineSpeed(1500);
    Sensor_Mock_SetTemperature(SENSOR_ENGINE_TEMP, 90.5f);
    
    ASSERT_TRUE(read_system_status(DID_BATTERY_VOLTAGE, data, &length));
    float battery_voltage;
    memcpy(&battery_voltage, data, sizeof(float));
    ASSERT_FLOAT_EQ(battery_voltage, 12.6f);
    
    ASSERT_TRUE(read_system_status(DID_ENGINE_SPEED, data, &length));
    uint16_t engine_speed;
    memcpy(&engine_speed, data, sizeof(uint16_t));
    ASSERT_EQ(engine_speed, 1500);
    
    ASSERT_TRUE(read_system_status(DID_ENGINE_TEMP, data, &length));
    float engine_temp;
    memcpy(&engine_temp, data, sizeof(float));
    ASSERT_FLOAT_EQ(engine_temp, 90.5f);
}

TEST_F(DiagDataTest, ReadDiagnosticData) {
    uint8_t data[32];
    uint16_t length;
    
    // Set mock diagnostic values
    ECU_Mock_SetTotalRuntime(3600);
    ECU_Mock_SetErrorCount(5);
    ECU_Mock_SetLastErrorCode(0x1234);
    
    ASSERT_TRUE(read_diagnostic_data(DID_TOTAL_RUNTIME, data, &length));
    uint32_t total_runtime;
    memcpy(&total_runtime, data, sizeof(uint32_t));
    ASSERT_EQ(total_runtime, 3600);
    
    ASSERT_TRUE(read_diagnostic_data(DID_ERROR_COUNT, data, &length));
    uint16_t error_count;
    memcpy(&error_count, data, sizeof(uint16_t));
    ASSERT_EQ(error_count, 5);
    
    ASSERT_TRUE(read_diagnostic_data(DID_LAST_ERROR_CODE, data, &length));
    uint16_t last_error;
    memcpy(&last_error, data, sizeof(uint16_t));
    ASSERT_EQ(last_error, 0x1234);
}

TEST_F(DiagDataTest, WriteConfiguration) {
    uint32_t test_baudrate = 500000;
    ASSERT_TRUE(write_configuration(DID_CAN_BAUDRATE, (uint8_t*)&test_baudrate, sizeof(uint32_t)));
    
    uint8_t stored_data[32];
    uint16_t stored_length;
    NVRAM_Mock_GetData(NVRAM_ADDR_CAN_BAUDRATE, stored_data, &stored_length);
    uint32_t stored_baudrate;
    memcpy(&stored_baudrate, stored_data, sizeof(uint32_t));
    ASSERT_EQ(stored_baudrate, test_baudrate);
    
    // Verify network configuration was applied
    ASSERT_EQ(Network_Mock_GetBaudrate(), test_baudrate);
} 