#ifndef MEM_POOL_H
#define MEM_POOL_H

#include <stdint.h>
#include <stdbool.h>

#define POOL_BLOCK_SIZE 64
#define POOL_NUM_BLOCKS 32

typedef struct {
    uint8_t data[POOL_BLOCK_SIZE];
    bool used;
    uint32_t timestamp;
} MemBlock;

void MemPool_Init(void);
void* MemPool_Alloc(void);
void MemPool_Free(void* ptr);
uint32_t MemPool_GetFreeBlocks(void);

#endif 