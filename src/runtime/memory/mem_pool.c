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
    for(uint32_t i = 0; i < POOL_NUM_BLOCKS; i++) {
        if(!blocks[i].used) {
            blocks[i].used = true;
            blocks[i].timestamp = TIMER_GetMs();
            free_blocks--;
            
            if(free_blocks < peak_usage) {
                peak_usage = free_blocks;
            }
            
            return blocks[i].data;
        }
    }
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
    
    for(uint32_t i = 0; i < POOL_NUM_BLOCKS; i++) {
        if(blocks[i].used) {
            if(current_time - blocks[i].timestamp > 5000) {
                blocks[i].used = false;
                free_blocks++;
            }
        }
    }
}

static uint32_t alloc_count = 0;
static uint32_t free_count = 0; 