#ifndef CANT_MEMORY_TEST_H
#define CANT_MEMORY_TEST_H

#include <stdint.h>
#include <stdbool.h>

// Memory Test Types
typedef enum {
    MEM_TEST_MARCH_C,
    MEM_TEST_CHECKERBOARD,
    MEM_TEST_WALKING_1,
    MEM_TEST_WALKING_0,
    MEM_TEST_ADDRESS_FAULT,
    MEM_TEST_FLASH_CRC,
    MEM_TEST_RAM_PATTERN
} MemoryTestType;

// Memory Test Results
typedef enum {
    MEM_TEST_OK,
    MEM_TEST_FAILED_WRITE,
    MEM_TEST_FAILED_READ,
    MEM_TEST_FAILED_PATTERN,
    MEM_TEST_FAILED_ADDRESS,
    MEM_TEST_FAILED_CRC,
    MEM_TEST_FAILED_TIMEOUT
} MemoryTestResult;

// Memory Region Types
typedef enum {
    MEM_REGION_RAM,
    MEM_REGION_ROM,
    MEM_REGION_FLASH,
    MEM_REGION_EEPROM
} MemoryRegionType;

// Memory Region Configuration
typedef struct {
    uint32_t start_address;
    uint32_t size;
    MemoryRegionType type;
    bool is_executable;
    bool is_writable;
    bool run_background_test;
} MemoryRegionConfig;

// Memory Test Configuration
typedef struct {
    MemoryRegionConfig* regions;
    uint32_t region_count;
    uint32_t test_interval_ms;
    uint32_t pattern_count;
    uint32_t* test_patterns;
    void (*error_callback)(MemoryTestType, MemoryTestResult, uint32_t address);
} MemoryTestConfig;

// Memory Test API
bool Memory_Test_Init(const MemoryTestConfig* config);
void Memory_Test_DeInit(void);
void Memory_Test_Process(void);
MemoryTestResult Memory_Test_RunTest(MemoryTestType test, uint32_t region_index);
bool Memory_Test_IsRegionHealthy(uint32_t region_index);
uint32_t Memory_Test_GetErrorCount(void);
void Memory_Test_ResetErrorCount(void);
void Memory_Test_GetStatus(MemoryTestResult* results, uint32_t* count);
bool Memory_Test_VerifyRegion(uint32_t region_index, uint32_t* failed_address);

#endif // CANT_MEMORY_TEST_H 