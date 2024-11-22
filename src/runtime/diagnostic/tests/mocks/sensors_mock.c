#include "sensors_mock.h"
#include <string.h>

#define MAX_SENSORS 16

typedef struct {
    float raw_value;
    float calibrated_value;
    uint8_t calibration_status;
    bool calibration_complete;
} SensorData;

static struct {
    SensorData sensors[MAX_SENSORS];
    float temperatures[8];
} sensor_mock_data;

void Sensors_Mock_Init(void) {
    memset(&sensor_mock_data, 0, sizeof(sensor_mock_data));
}

void Sensors_Mock_Deinit(void) {
    memset(&sensor_mock_data, 0, sizeof(sensor_mock_data));
}

void Sensor_Mock_SetRawValue(uint16_t sensor_id, float value) {
    if (sensor_id < MAX_SENSORS) {
        sensor_mock_data.sensors[sensor_id].raw_value = value;
    }
}

