#ifndef CANT_MEMORY_MOCK_H
#define CANT_MEMORY_MOCK_H

#include <stdint.h>
#include <stdbool.h>

void Memory_Mock_Init(void);
void Memory_Mock_Deinit(void);

void Memory_Mock_SetCheckStatus(uint8_t status);
void Memory_Mock_SetChecksum(uint32_t checksum);
void Memory_Mock_SetCheckComplete(bool complete);
void Memory_Mock_SetMemoryContent(uint32_t address, const uint8_t* data, uint32_t size);
void Memory_Mock_SetProtectedRange(uint32_t start, uint32_t end);

uint8_t Memory_Mock_GetCheckStatus(void);
uint32_t Memory_Mock_GetChecksum(void);
bool Memory_Mock_IsCheckComplete(void);
bool Memory_Mock_ReadMemory(uint32_t address, uint8_t* data, uint32_t size);
bool Memory_Mock_IsAddressProtected(uint32_t address, uint32_t size);

#endif // CANT_MEMORY_MOCK_H 