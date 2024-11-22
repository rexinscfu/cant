#include <gtest/gtest.h>
#include "../diag_system.h"
#include "../service_router.h"
#include "mocks/ecu_mock.h"
#include "mocks/battery_mock.h"
#include "mocks/sensors_mock.h"
#include "mocks/network_mock.h"

class DiagIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        ECU_Mock_Init();
        Battery_Mock_Init();
        Sensors_Mock_Init();
        Network_Mock_Init();
        
        DiagSystemConfig config = {
            .transport_config = {
                .protocol = DIAG_PROTOCOL_UDS,
                .max_message_length = 4096,
                .p2_timeout_ms = 50,
                .p2_star_timeout_ms = 5000
            },
            .session_config = {
                .default_p2_timeout_ms = 50,
                .extended_p2_timeout_ms = 5000,
                .s3_timeout_ms = 5000,
                .enable_session_lock = true
            },
            .security_config = {
                .default_delay_time_ms = 10000,
                .default_max_attempts = 3
            }
        };
        
        ASSERT_TRUE(Diag_System_Init(&config));
    }

    void TearDown() override {
        Diag_System_Deinit();
        ECU_Mock_Deinit();
        Battery_Mock_Deinit();
        Sensors_Mock_Deinit();
        Network_Mock_Deinit();
    }
};

TEST_F(DiagIntegrationTest, CompleteSessionFlow) {
    // Test diagnostic session control
    uint8_t session_request[] = {0x10, 0x03}; // Request extended session
    UdsMessage response;
    ASSERT_EQ(Diag_System_HandleRequest(session_request, sizeof(session_request), &response),
              UDS_RESPONSE_OK);
    ASSERT_EQ(response.data[0], 0x03);

    // Test security access
    uint8_t seed_request[] = {0x27, 0x01}; // Request seed
    ASSERT_EQ(Diag_System_HandleRequest(seed_request, sizeof(seed_request), &response),
              UDS_RESPONSE_OK);
    ASSERT_EQ(response.length, 5); // 1 byte subfunction + 4 bytes seed

    // Test routine control
    uint8_t routine_request[] = {0x31, 0x01, 0x01, 0x00}; // Start battery test
    ASSERT_EQ(Diag_System_HandleRequest(routine_request, sizeof(routine_request), &response),
              UDS_RESPONSE_OK);

    // Test read data by identifier
    uint8_t read_did_request[] = {0x22, 0xF1, 0x90}; // Read VIN
    ASSERT_EQ(Diag_System_HandleRequest(read_did_request, sizeof(read_did_request), &response),
              UDS_RESPONSE_OK);
}

TEST_F(DiagIntegrationTest, ErrorHandling) {
    // Test invalid service ID
    uint8_t invalid_service[] = {0xFF, 0x00};
    UdsMessage response;
    ASSERT_EQ(Diag_System_HandleRequest(invalid_service, sizeof(invalid_service), &response),
              UDS_RESPONSE_SERVICE_NOT_SUPPORTED);

    // Test invalid DID
    uint8_t invalid_did[] = {0x22, 0xFF, 0xFF};
    ASSERT_EQ(Diag_System_HandleRequest(invalid_did, sizeof(invalid_did), &response),
              UDS_RESPONSE_REQUEST_OUT_OF_RANGE);

    // Test security access without proper session
    uint8_t security_request[] = {0x27, 0x01};
    ASSERT_EQ(Diag_System_HandleRequest(security_request, sizeof(security_request), &response),
              UDS_RESPONSE_CONDITIONS_NOT_CORRECT);
}

TEST_F(DiagIntegrationTest, CompleteDataFlow) {
    // Setup mock data
    ECU_Mock_SetVehicleSpeed(60.5f);
    Battery_Mock_SetVoltage(12.8f);
    Sensor_Mock_SetTemperature(SENSOR_ENGINE_TEMP, 85.0f);

    // Test multiple DIDs in one request
    uint8_t multi_did_request[] = {0x22, 0xF3, 0x03, 0xF3, 0x00, 0xF3, 0x04};
    UdsMessage response;
    ASSERT_EQ(Diag_System_HandleRequest(multi_did_request, sizeof(multi_did_request), &response),
              UDS_RESPONSE_OK);

    // Verify response format and values
    float vehicle_speed, battery_voltage, engine_temp;
    uint32_t offset = 1;
    memcpy(&vehicle_speed, &response.data[offset], sizeof(float));
    offset += sizeof(float);
    memcpy(&battery_voltage, &response.data[offset], sizeof(float));
    offset += sizeof(float);
    memcpy(&engine_temp, &response.data[offset], sizeof(float));

    ASSERT_FLOAT_EQ(vehicle_speed, 60.5f);
    ASSERT_FLOAT_EQ(battery_voltage, 12.8f);
    ASSERT_FLOAT_EQ(engine_temp, 85.0f);
}

TEST_F(DiagIntegrationTest, RoutineSequence) {
    // Start diagnostic session
    uint8_t session_request[] = {0x10, 0x02}; // Programming session
    UdsMessage response;
    ASSERT_EQ(Diag_System_HandleRequest(session_request, sizeof(session_request), &response),
              UDS_RESPONSE_OK);

    // Security access sequence
    uint8_t seed_request[] = {0x27, 0x01};
    ASSERT_EQ(Diag_System_HandleRequest(seed_request, sizeof(seed_request), &response),
              UDS_RESPONSE_OK);
    uint32_t seed;
    memcpy(&seed, &response.data[1], 4);

    // Calculate key (mock security algorithm)
    uint32_t key = ~seed; // Simple inverse for testing
    uint8_t key_request[] = {0x27, 0x02, 0x00, 0x00, 0x00, 0x00};
    memcpy(&key_request[2], &key, 4);
    ASSERT_EQ(Diag_System_HandleRequest(key_request, sizeof(key_request), &response),
              UDS_RESPONSE_OK);

    // Start memory check routine
    uint8_t memory_check_request[] = {0x31, 0x01, 0x04, 0x00,
                                    0x20, 0x00, 0x00, 0x00, // Start address
                                    0x00, 0x10, 0x00, 0x00}; // Size
    ASSERT_EQ(Diag_System_HandleRequest(memory_check_request, sizeof(memory_check_request), &response),
              UDS_RESPONSE_OK);

    // Poll routine results
    uint8_t result_request[] = {0x31, 0x03, 0x04, 0x00};
    Memory_Mock_SetCheckComplete(true);
    Memory_Mock_SetChecksum(0x12345678);
    ASSERT_EQ(Diag_System_HandleRequest(result_request, sizeof(result_request), &response),
              UDS_RESPONSE_OK);
}

TEST_F(DiagIntegrationTest, ConcurrentOperations) {
    // Start multiple routines
    uint8_t battery_test_request[] = {0x31, 0x01, 0x01, 0x00, 0x01};
    uint8_t sensor_cal_request[] = {0x31, 0x01, 0x02, 0x00, 0x01, 0x00};
    UdsMessage response;

    ASSERT_EQ(Diag_System_HandleRequest(battery_test_request, sizeof(battery_test_request), &response),
              UDS_RESPONSE_OK);
    ASSERT_EQ(Diag_System_HandleRequest(sensor_cal_request, sizeof(sensor_cal_request), &response),
              UDS_RESPONSE_OK);

    // Read status while routines are running
    uint8_t status_request[] = {0x22, 0xF3, 0x00, 0xF3, 0x01};
    ASSERT_EQ(Diag_System_HandleRequest(status_request, sizeof(status_request), &response),
              UDS_RESPONSE_OK);

    // Verify routines can be stopped
    uint8_t stop_battery_request[] = {0x31, 0x02, 0x01, 0x00};
    uint8_t stop_sensor_request[] = {0x31, 0x02, 0x02, 0x00};
    ASSERT_EQ(Diag_System_HandleRequest(stop_battery_request, sizeof(stop_battery_request), &response),
              UDS_RESPONSE_OK);
    ASSERT_EQ(Diag_System_HandleRequest(stop_sensor_request, sizeof(stop_sensor_request), &response),
              UDS_RESPONSE_OK);
} 