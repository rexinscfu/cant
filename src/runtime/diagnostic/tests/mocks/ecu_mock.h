#ifndef CANT_ECU_MOCK_H
#define CANT_ECU_MOCK_H

#include <stdint.h>
#include <stdbool.h>

void ECU_Mock_Init(void);
void ECU_Mock_Deinit(void);

// Setters for mock values
void ECU_Mock_SetSerialNumber(const char* serial);
void ECU_Mock_SetHardwareVersion(uint16_t version);
void ECU_Mock_SetSoftwareVersion(uint16_t version);
void ECU_Mock_SetManufacturingDate(const char* date);
void ECU_Mock_SetSupplierId(uint16_t id);
void ECU_Mock_SetSystemVoltage(float voltage);
void ECU_Mock_SetEngineSpeed(uint16_t rpm);
void ECU_Mock_SetVehicleSpeed(float speed);
void ECU_Mock_SetTotalDistance(uint32_t distance);
void ECU_Mock_SetTotalRuntime(uint32_t runtime);
void ECU_Mock_SetErrorCount(uint16_t count);
void ECU_Mock_SetLastErrorCode(uint16_t code);
void ECU_Mock_SetBootCount(uint16_t count);

// Getters for verification
const char* ECU_Mock_GetSerialNumber(void);
uint16_t ECU_Mock_GetHardwareVersion(void);
uint16_t ECU_Mock_GetSoftwareVersion(void);
const char* ECU_Mock_GetManufacturingDate(void);
uint16_t ECU_Mock_GetSupplierId(void);
float ECU_Mock_GetSystemVoltage(void);
uint16_t ECU_Mock_GetEngineSpeed(void);
float ECU_Mock_GetVehicleSpeed(void);
uint32_t ECU_Mock_GetTotalDistance(void);
uint32_t ECU_Mock_GetTotalRuntime(void);
uint16_t ECU_Mock_GetErrorCount(void);
uint16_t ECU_Mock_GetLastErrorCode(void);
uint16_t ECU_Mock_GetBootCount(void);

#endif // CANT_ECU_MOCK_H 