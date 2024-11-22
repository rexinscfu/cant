#ifndef CANT_DIAG_MEMORY_MANAGER_H
#define CANT_DIAG_MEMORY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// Memory Area Types
typedef enum {
    MEMORY_AREA_RAM,
    MEMORY_AREA_FLASH,
    MEMORY_AREA_EEPROM,
    MEMORY_AREA_ROM,
    MEMORY_AREA_MIRROR
} MemoryAreaType;

// Memory Access Rights
typedef enum {
    MEMORY_ACCESS_READ = 0x01,
    MEMORY_ACCESS_WRITE = 0x02,
    MEMORY_ACCESS_EXECUTE = 0x04
} MemoryAccessRight;

// Memory Area Definition
typedef struct {
    uint32_t start_address;
    uint32_t size;
    MemoryAreaType type;
    uint8_t access_rights;
    uint8_t security_level;
} MemoryAreaDef;

// Memory Operation Result
typedef enum {
    MEMORY_RESULT_SUCCESS,
    MEMORY_RESULT_INVALID_ADDRESS,
    MEMORY_RESULT_ACCESS_DENIED,
    MEMORY_RESULT_WRITE_FAILED,
    MEMORY_RESULT_READ_FAILED,
    MEMORY_RESULT_VERIFY_FAILED,
    MEMORY_RESULT_BUSY,
    MEMORY_RESULT_ERROR
} MemoryResult;

// Memory Manager Configuration
typedef struct {
    MemoryAreaDef* memory_areas;
    uint32_t area_count;
    uint32_t write_timeout_ms;
    uint32_t erase_timeout_ms;
    void (*operation_complete_callback)(MemoryResult result);
} MemoryManagerConfig;

// Memory Manager API
bool Memory_Manager_Init(const MemoryManagerConfig* config);
void Memory_Manager_DeInit(void);
MemoryResult Memory_Manager_Read(uint32_t address, uint8_t* data, uint32_t length);
MemoryResult Memory_Manager_Write(uint32_t address, const uint8_t* data, uint32_t length);
MemoryResult Memory_Manager_Erase(uint32_t address, uint32_t length);
MemoryResult Memory_Manager_Verify(uint32_t address, const uint8_t* data, uint32_t length);
bool Memory_Manager_IsAddressValid(uint32_t address, uint32_t length);
bool Memory_Manager_HasAccess(uint32_t address, uint32_t length, uint8_t access_right);
MemoryAreaType Memory_Manager_GetAreaType(uint32_t address);
bool Memory_Manager_IsBusy(void);
void Memory_Manager_AbortOperation(void);
MemoryResult Memory_Manager_GetLastResult(void);
uint32_t Memory_Manager_GetProgress(void);

#endif // CANT_DIAG_MEMORY_MANAGER_H 