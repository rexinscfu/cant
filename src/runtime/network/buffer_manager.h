#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

void BufferManager_Init(void);
uint8_t* BufferManager_Alloc(void);
void BufferManager_Free(uint8_t* buffer);
void BufferManager_Process(void);
uint32_t BufferManager_GetUsage(void);

// Buffer statistics
typedef struct {
    uint32_t total_allocs;
    uint32_t total_frees;
    uint32_t current_usage;
    uint32_t peak_usage;
} BufferStats;

void BufferManager_GetStats(BufferStats* stats);

#endif 