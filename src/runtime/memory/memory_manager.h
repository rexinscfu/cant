#ifndef CANT_MEMORY_MANAGER_H
#define CANT_MEMORY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// Memory block flags
#define MEM_BLOCK_FREE     0x00
#define MEM_BLOCK_USED     0x01
#define MEM_BLOCK_POOL     0x02
#define MEM_BLOCK_GUARD    0x04
#define MEM_BLOCK_ALIGNED  0x08

// Memory configuration
typedef struct {
    uint32_t heap_size;
    uint32_t* pool_sizes;    // Array of pool block sizes
    uint32_t* pool_counts;   // Array of blocks per pool
    uint32_t pool_count;     // Number of pools
    bool enable_guards;      // Enable memory guards
    bool enable_tracking;    // Enable allocation tracking
    bool enable_stats;       // Enable memory statistics
} MemConfig;

// Memory statistics
typedef struct {
    uint32_t total_allocated;
    uint32_t total_freed;
    uint32_t current_usage;
    uint32_t peak_usage;
    uint32_t allocation_count;
    uint32_t free_count;
    uint32_t pool_allocations;
    uint32_t heap_allocations;
    uint32_t fragmentation;
} MemStats;

// Memory block header
typedef struct MemBlock {
    uint32_t size;
    uint32_t flags;
    const char* file;
    uint32_t line;
    struct MemBlock* next;
    struct MemBlock* prev;
    uint32_t guard_front;
} MemBlock;

// Memory block footer
typedef struct {
    uint32_t guard_back;
} MemFooter;

// Memory initialization and cleanup
bool Memory_Init(const MemConfig* config);
void Memory_Deinit(void);

// Memory allocation functions
void* Memory_Alloc(uint32_t size, const char* file, uint32_t line);
void* Memory_AllocAligned(uint32_t size, uint32_t alignment, const char* file, uint32_t line);
void* Memory_Calloc(uint32_t count, uint32_t size, const char* file, uint32_t line);
void* Memory_Realloc(void* ptr, uint32_t size, const char* file, uint32_t line);
void Memory_Free(void* ptr);

// Memory statistics and diagnostics
void Memory_GetStats(MemStats* stats);
bool Memory_CheckIntegrity(void);
void Memory_DumpStats(void);
void Memory_Defragment(void);

// Memory leak detection
typedef void (*MemoryLeakCallback)(const MemBlock* block);
void Memory_TrackLeaks(MemoryLeakCallback callback);

// Convenience macros
#define MEMORY_ALLOC(size)           Memory_Alloc(size, __FILE__, __LINE__)
#define MEMORY_ALLOC_ALIGNED(s, a)   Memory_AllocAligned(s, a, __FILE__, __LINE__)
#define MEMORY_CALLOC(count, size)   Memory_Calloc(count, size, __FILE__, __LINE__)
#define MEMORY_REALLOC(ptr, size)    Memory_Realloc(ptr, size, __FILE__, __LINE__)
#define MEMORY_FREE(ptr)             Memory_Free(ptr)

#endif // CANT_MEMORY_MANAGER_H 