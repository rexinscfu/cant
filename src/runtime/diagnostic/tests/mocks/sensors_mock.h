#ifndef CANT_SENSORS_MOCK_H
#define CANT_SENSORS_MOCK_H

#include <stdint.h>
#include <stdbool.h>

void Sensors_Mock_Init(void);
void Sensors_Mock_Deinit(void);

void Sensor_Mock_SetRawValue(uint16_t sensor_id, float value);
void Sensor_Mock_SetCalibratedValue(uint16_t sensor_id, float value);
void Sensor_Mock_SetCalibrationStatus(uint16_t sensor_id, uint8_t status);
void Sensor_Mock_SetCalibrationComplete(uint16_t sensor_id, bool complete);
void Sensor_Mock_SetTemperature(uint8_t sensor_type, float temp);

float Sensor_Mock_GetRawValue(uint16_t sensor_id);
float Sensor_Mock_GetCalibratedValue(uint16_t sensor_id);
uint8_t Sensor_Mock_GetCalibrationStatus(uint16_t sensor_id);
bool Sensor_Mock_IsCalibrationComplete(uint16_t sensor_id);
float Sensor_Mock_GetTemperature(uint8_t sensor_type);

#endif // CANT_SENSORS_MOCK_H 