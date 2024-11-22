#include <gtest/gtest.h>
#include "../routines/diag_routines.h"
#include "../../hw/battery_mock.h"
#include "../../hw/sensors_mock.h"
#include "../../hw/actuators_mock.h"
#include "../../hw/memory_mock.h"
#include "../../hw/network_mock.h"

class DiagRoutinesTest : public ::testing::Test {
protected:
    void SetUp() override {
        Battery_Mock_Init();
        Sensors_Mock_Init();
        Actuators_Mock_Init();
        Memory_Mock_Init();
        Network_Mock_Init();
    }

    void TearDown() override {
        Battery_Mock_Deinit();
        Sensors_Mock_Deinit();
        Actuators_Mock_Deinit();
        Memory_Mock_Deinit();
        Network_Mock_Deinit();
    }
};

TEST_F(DiagRoutinesTest, BatteryTest) {
    // Test start
    uint8_t start_data[] = {0x01}; // Load test
    ASSERT_TRUE(battery_test_start(start_data, sizeof(start_data)));
    ASSERT_TRUE(Battery_Mock_IsTestLoadEnabled());
    
    // Test results
    Battery_Mock_SetVoltage(12.6f);
    Battery_Mock_SetCurrent(5.0f);
    Battery_Mock_SetTemperature(25.0f);
    Battery_Mock_SetHealthStatus(0x01);
    Battery_Mock_SetTestComplete(true);
    
    RoutineResult result;
    ASSERT_TRUE(battery_test_get_result(&result));
    ASSERT_EQ(result.result_code, 0);
    ASSERT_EQ(result.length, 14);
    
    // Verify result data
    ASSERT_EQ(result.data[0], 0x01); // Test complete
    float voltage, current, temperature;
    memcpy(&voltage, &result.data[1], 4);
    memcpy(&current, &result.data[5], 4);
    memcpy(&temperature, &result.data[9], 4);
    ASSERT_FLOAT_EQ(voltage, 12.6f);
    ASSERT_FLOAT_EQ(current, 5.0f);
    ASSERT_FLOAT_EQ(temperature, 25.0f);
    ASSERT_EQ(result.data[13], 0x01); // Health status
    
    // Test stop
    ASSERT_TRUE(battery_test_stop());
    ASSERT_FALSE(Battery_Mock_IsTestLoadEnabled());
}

TEST_F(DiagRoutinesTest, SensorCalibration) {
    uint16_t sensor_id = 0x0001;
    uint8_t start_data[] = {0x00, 0x01, 0x01}; // Sensor 1, Zero calibration
    ASSERT_TRUE(sensor_calibration_start(start_data, sizeof(start_data)));
    
    Sensor_Mock_SetRawValue(sensor_id, 0.5f);
    Sensor_Mock_SetCalibratedValue(sensor_id, 0.0f);
    Sensor_Mock_SetCalibrationStatus(sensor_id, 0x01);
    Sensor_Mock_SetCalibrationComplete(sensor_id, true);
    
    RoutineResult result;
    ASSERT_TRUE(sensor_calibration_get_result(&result));
    ASSERT_EQ(result.result_code, 0);
    ASSERT_EQ(result.length, 10);
    
    float raw_value, calibrated_value;
    memcpy(&raw_value, &result.data[1], 4);
    memcpy(&calibrated_value, &result.data[5], 4);
    ASSERT_FLOAT_EQ(raw_value, 0.5f);
    ASSERT_FLOAT_EQ(calibrated_value, 0.0f);
    ASSERT_EQ(result.data[9], 0x01);
}

TEST_F(DiagRoutinesTest, ActuatorTest) {
    uint16_t actuator_id = 0x0001;
    float target_position = 45.0f;
    uint8_t start_data[] = {0x00, 0x01, 0x02, 0x05}; // Actuator 1, Step test, 5s duration
    memcpy(&start_data[4], &target_position, 4);
    ASSERT_TRUE(actuator_test_start(start_data, sizeof(start_data)));
    
    Actuator_Mock_SetTestStatus(actuator_id, 0x01);
    Actuator_Mock_SetTestComplete(actuator_id, true);
    
    float settling_time = 0.5f;
    float overshoot = 5.0f;
    float steady_state_error = 0.1f;
    Actuator_Mock_SetStepResults(actuator_id, settling_time, overshoot, steady_state_error);
    
    RoutineResult result;
    ASSERT_TRUE(actuator_test_get_result(&result));
    ASSERT_EQ(result.result_code, 0);
    
    float result_settling_time, result_overshoot, result_error;
    memcpy(&result_settling_time, &result.data[2], 4);
    memcpy(&result_overshoot, &result.data[6], 4);
    memcpy(&result_error, &result.data[10], 4);
    ASSERT_FLOAT_EQ(result_settling_time, settling_time);
    ASSERT_FLOAT_EQ(result_overshoot, overshoot);
    ASSERT_FLOAT_EQ(result_error, steady_state_error);
}
