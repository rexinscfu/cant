#include "memory_mock.h"
#include <string.h>

#define MOCK_MEMORY_SIZE (256 * 1024)  // 256KB mock memory
#define MAX_PROTECTED_RANGES 8

typedef struct {
    uint32_t start;
    uint32_t end;
} ProtectedRange;

static struct {
    uint8_t memory[MOCK_MEMORY_SIZE];
    uint8_t check_status;
    uint32_t checksum;
    bool check_complete;
    ProtectedRange protected_ranges[MAX_PROTECTED_RANGES];
    uint8_t protected_range_count;
} memory_mock_data;

void Memory_Mock_Init(void) {
    memset(&memory_mock_data, 0, sizeof(memory_mock_data));
}

void Memory_Mock_Deinit(void) {
    memset(&memory_mock_data, 0, sizeof(memory_mock_data));
}

void Memory_Mock_SetCheckStatus(uint8_t status) {
    memory_mock_data.check_status = status;
}

void Memory_Mock_SetChecksum(uint32_t checksum) {
    memory_mock_data.checksum = checksum;
}

void Memory_Mock_SetCheckComplete(bool complete) {
    memory_mock_data.check_complete = complete;
}

void Memory_Mock_SetMemoryContent(uint32_t address, const uint8_t* data, uint32_t size) {
    if (address + size <= MOCK_MEMORY_SIZE) {
        memcpy(&memory_mock_data.memory[address], data, size);
    }
}

void Memory_Mock_SetProtectedRange(uint32_t start, uint32_t end) {
    if (memory_mock_data.protected_range_count < MAX_PROTECTED_RANGES) {
        memory_mock_data.protected_ranges[memory_mock_data.protected_range_count].start = start;
        memory_mock_data.protected_ranges[memory_mock_data.protected_range_count].end = end;
        memory_mock_data.protected_range_count++;
    }
}

uint8_t Memory_Mock_GetCheckStatus(void) {
    return memory_mock_data.check_status;
}

uint32_t Memory_Mock_GetChecksum(void) {
    return memory_mock_data.checksum;
}

bool Memory_Mock_IsCheckComplete(void) {
    return memory_mock_data.check_complete;
}

bool Memory_Mock_ReadMemory(uint32_t address, uint8_t* data, uint32_t size) {
    if (address + size <= MOCK_MEMORY_SIZE) {
        memcpy(data, &memory_mock_data.memory[address], size);
        return true;
    }
    return false;
}

bool Memory_Mock_IsAddressProtected(uint32_t address, uint32_t size) {
    for (uint8_t i = 0; i < memory_mock_data.protected_range_count; i++) {
        if (address >= memory_mock_data.protected_ranges[i].start &&
            address + size <= memory_mock_data.protected_ranges[i].end) {
            return true;
        }
    }
    return false;
} 