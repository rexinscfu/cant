#include "mem_pool.h"
#include "../hardware/timer_hw.h"
#include <string.h>

static MemBlock blocks[POOL_NUM_BLOCKS] __attribute__((aligned(8)));
static uint32_t free_blocks = POOL_NUM_BLOCKS;
static uint32_t peak_usage = 0;

void MemPool_Init(void) {
    memset(blocks, 0, sizeof(blocks));
    free_blocks = POOL_NUM_BLOCKS;
    peak_usage = 0;
}

void* MemPool_Alloc(void) {
    static uint32_t last_allocated = 0;
    uint32_t start = last_allocated;
    
    // Try to find next free block starting from last allocation
    do {
        if(!blocks[last_allocated].used) {
            blocks[last_allocated].used = true;
            blocks[last_allocated].timestamp = TIMER_GetMs();
            free_blocks--;
            return blocks[last_allocated].data;
        }
        last_allocated = (last_allocated + 1) % POOL_NUM_BLOCKS;
    } while(last_allocated != start);
    
    return NULL;
}

void MemPool_Free(void* ptr) {
    for(uint32_t i = 0; i < POOL_NUM_BLOCKS; i++) {
        if(blocks[i].data == ptr) {
            blocks[i].used = false;
            free_blocks++;
            return;
        }
    }
}

uint32_t MemPool_GetFreeBlocks(void) {
    return free_blocks;
}

void MemPool_GarbageCollect(void) {
    uint32_t current_time = TIMER_GetMs();
    uint32_t freed = 0;
    
    for(uint32_t i = 0; i < POOL_NUM_BLOCKS; i++) {
        if(blocks[i].used) {
            if(current_time - blocks[i].timestamp > 5000) {
                blocks[i].used = false;
                free_blocks++;
                freed++;
            }
        }
    }
    
    if(freed > 0) {
        // Compact blocks if significant memory was freed
        compact_blocks();
    }
}

static void compact_blocks(void) {
    uint32_t dest = 0;
    uint32_t src = 0;
    
    while(src < POOL_NUM_BLOCKS) {
        if(blocks[src].used) {
            if(src != dest) {
                memcpy(blocks[dest].data, blocks[src].data, POOL_BLOCK_SIZE);
                blocks[dest].used = true;
                blocks[dest].timestamp = blocks[src].timestamp;
                blocks[src].used = false;
            }
            dest++;
        }
        src++;
    }
}

static uint32_t alloc_count = 0;
static uint32_t free_count = 0; 