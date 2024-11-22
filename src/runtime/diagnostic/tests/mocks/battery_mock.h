#ifndef CANT_BATTERY_MOCK_H
#define CANT_BATTERY_MOCK_H

#include <stdint.h>
#include <stdbool.h>

void Battery_Mock_Init(void);
void Battery_Mock_Deinit(void);

void Battery_Mock_SetVoltage(float voltage);
void Battery_Mock_SetCurrent(float current);
void Battery_Mock_SetTemperature(float temp);
void Battery_Mock_SetHealthStatus(uint8_t status);
void Battery_Mock_SetTestComplete(bool complete);
void Battery_Mock_SetTestLoadEnabled(bool enabled);
void Battery_Mock_SetChargingEnabled(bool enabled);

float Battery_Mock_GetVoltage(void);
float Battery_Mock_GetCurrent(void);
float Battery_Mock_GetTemperature(void);
uint8_t Battery_Mock_GetHealthStatus(void);
bool Battery_Mock_IsTestComplete(void);
bool Battery_Mock_IsTestLoadEnabled(void);
bool Battery_Mock_IsChargingEnabled(void);

#endif // CANT_BATTERY_MOCK_H 