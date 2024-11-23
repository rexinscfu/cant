#include "memory_manager.h"
#include "../diagnostic/logging/diag_logger.h"
#include "../diagnostic/os/critical.h"
#include <string.h>
#include <stdlib.h>

#define GUARD_PATTERN     0xDEADBEEF
#define ALIGNMENT_MASK    (~(sizeof(void*) - 1))
#define MIN_BLOCK_SIZE    (sizeof(MemBlock) + sizeof(MemFooter))
#define BLOCK_OVERHEAD    (sizeof(MemBlock) + sizeof(MemFooter))
#define MAX_POOLS         16
#define POOL_MIN_BLOCKS   8

typedef struct MemoryPool {
    uint8_t* pool_memory;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t free_blocks;
    uint32_t* free_list;
    bool* block_used;
} MemoryPool;

typedef struct {
    MemConfig config;
    uint8_t* heap_memory;
    MemBlock* first_block;
    MemBlock* last_block;
    MemoryPool pools[MAX_POOLS];
    uint32_t pool_count;
    MemStats stats;
    bool initialized;
} MemoryManager;

static MemoryManager mem_mgr;

static bool is_valid_block(const MemBlock* block) {
    if (!block) return false;
    if (block < (MemBlock*)mem_mgr.heap_memory) return false;
    if ((uint8_t*)block >= mem_mgr.heap_memory + mem_mgr.config.heap_size) return false;
    if (block->size == 0) return false;
    if (block->size > mem_mgr.config.heap_size - BLOCK_OVERHEAD) return false;
    return true;
}

static void setup_block_guards(MemBlock* block) {
    if (!mem_mgr.config.enable_guards || !block) return;
    
    block->guard_front = GUARD_PATTERN;
    MemFooter* footer = (MemFooter*)((uint8_t*)(block + 1) + block->size);
    footer->guard_back = GUARD_PATTERN;
}

static bool check_block_guards(const MemBlock* block) {
    if (!mem_mgr.config.enable_guards || !block) return false;
    
    if (block->guard_front != GUARD_PATTERN) {
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Front guard corrupted at %p", block);
        return false;
    }
    
    const MemFooter* footer = (const MemFooter*)((const uint8_t*)(block + 1) + block->size);
    if (footer->guard_back != GUARD_PATTERN) {
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Back guard corrupted at %p", block);
        return false;
    }
    
    return true;
}

static MemoryPool* find_suitable_pool(uint32_t size) {
    for (uint32_t i = 0; i < mem_mgr.pool_count; i++) {
        if (mem_mgr.pools[i].block_size >= size && mem_mgr.pools[i].free_blocks > 0) {
            return &mem_mgr.pools[i];
        }
    }
    return NULL;
}

static void* allocate_from_pool(MemoryPool* pool, const char* file, uint32_t line) {
    if (!pool->free_blocks) return NULL;
    
    uint32_t block_index = pool->free_list[--pool->free_blocks];
    pool->block_used[block_index] = true;
    
    void* ptr = pool->pool_memory + (block_index * pool->block_size);
    MemBlock* block = (MemBlock*)ptr;
    block->size = pool->block_size - BLOCK_OVERHEAD;
    block->flags = MEM_BLOCK_POOL | MEM_BLOCK_USED;
    block->file = file;
    block->line = line;
    setup_block_guards(block);
    
    return block + 1;
}

static bool free_to_pool(MemoryPool* pool, void* ptr) {
    uint8_t* block_ptr = (uint8_t*)ptr - sizeof(MemBlock);
    uint32_t block_index = (block_ptr - pool->pool_memory) / pool->block_size;
    
    if (block_index >= pool->block_count) return false;
    if (!pool->block_used[block_index]) return false;
    
    pool->block_used[block_index] = false;
    pool->free_list[pool->free_blocks++] = block_index;
    return true;
}

static MemBlock* find_best_fit(uint32_t size) {
    MemBlock* best_block = NULL;
    uint32_t best_size = UINT32_MAX;
    
    for (MemBlock* block = mem_mgr.first_block; block != NULL; block = block->next) {
        if (!(block->flags & MEM_BLOCK_USED) && 
            block->size >= size && 
            block->size < best_size) {
            best_block = block;
            best_size = block->size;
            
            // If we find an exact match, use it immediately
            if (block->size == size) {
                break;
            }
        }
    }
    
    return best_block;
}

static void split_block(MemBlock* block, uint32_t size) {
    uint32_t remaining = block->size - size - BLOCK_OVERHEAD;
    if (remaining < MIN_BLOCK_SIZE) return;
    
    MemBlock* new_block = (MemBlock*)((uint8_t*)(block + 1) + size + sizeof(MemFooter));
    new_block->size = remaining - BLOCK_OVERHEAD;
    new_block->flags = MEM_BLOCK_FREE;
    new_block->file = NULL;
    new_block->line = 0;
    new_block->next = block->next;
    new_block->prev = block;
    
    if (block->next) {
        block->next->prev = new_block;
    } else {
        mem_mgr.last_block = new_block;
    }
    
    block->next = new_block;
    block->size = size;
}

bool Memory_Init(const MemConfig* config) {
    if (!config || config->heap_size < MIN_BLOCK_SIZE) {
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Invalid configuration");
        return false;
    }
    
    memset(&mem_mgr, 0, sizeof(MemoryManager));
    memcpy(&mem_mgr.config, config, sizeof(MemConfig));
    
    // Allocate heap memory
    mem_mgr.heap_memory = (uint8_t*)malloc(config->heap_size);
    if (!mem_mgr.heap_memory) {
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Failed to allocate heap");
        return false;
    }
    
    // Initialize first block
    mem_mgr.first_block = (MemBlock*)mem_mgr.heap_memory;
    mem_mgr.first_block->size = config->heap_size - BLOCK_OVERHEAD;
    mem_mgr.first_block->flags = MEM_BLOCK_FREE;
    mem_mgr.first_block->next = NULL;
    mem_mgr.first_block->prev = NULL;
    mem_mgr.last_block = mem_mgr.first_block;
    
    // Initialize memory pools if configured
    if (config->pool_sizes && config->pool_counts) {
        for (uint32_t i = 0; i < config->pool_count && i < MAX_POOLS; i++) {
            MemoryPool* pool = &mem_mgr.pools[mem_mgr.pool_count];
            uint32_t total_size = config->pool_sizes[i] * config->pool_counts[i];
            
            pool->pool_memory = (uint8_t*)malloc(total_size);
            if (!pool->pool_memory) continue;
            
            pool->block_size = config->pool_sizes[i];
            pool->block_count = config->pool_counts[i];
            pool->free_blocks = pool->block_count;
            
            pool->free_list = (uint32_t*)malloc(pool->block_count * sizeof(uint32_t));
            pool->block_used = (bool*)malloc(pool->block_count * sizeof(bool));
            
            if (!pool->free_list || !pool->block_used) {
                free(pool->pool_memory);
                free(pool->free_list);
                free(pool->block_used);
                continue;
            }
            
            // Initialize free list
            for (uint32_t j = 0; j < pool->block_count; j++) {
                pool->free_list[j] = j;
                pool->block_used[j] = false;
            }
            
            mem_mgr.pool_count++;
        }
    }
    
    mem_mgr.initialized = true;
    Logger_Log(LOG_LEVEL_INFO, "MEMORY", "Memory manager initialized with %u bytes heap", 
               config->heap_size);
    return true;
} 

void* Memory_Alloc(uint32_t size, const char* file, uint32_t line) {
    if (!mem_mgr.initialized || size == 0) {
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Invalid allocation request");
        return NULL;
    }
    
    enter_critical();
    
    // Try pool allocation first for small sizes
    MemoryPool* pool = find_suitable_pool(size + BLOCK_OVERHEAD);
    if (pool) {
        void* ptr = allocate_from_pool(pool, file, line);
        if (ptr) {
            if (mem_mgr.config.enable_stats) {
                mem_mgr.stats.total_allocated += size;
                mem_mgr.stats.current_usage += size;
                mem_mgr.stats.allocation_count++;
                mem_mgr.stats.pool_allocations++;
                mem_mgr.stats.peak_usage = (mem_mgr.stats.current_usage > mem_mgr.stats.peak_usage) ? 
                                          mem_mgr.stats.current_usage : mem_mgr.stats.peak_usage;
            }
            exit_critical();
            return ptr;
        }
    }
    
    // Align size to word boundary
    size = (size + sizeof(void*) - 1) & ALIGNMENT_MASK;
    
    // Find best fitting block
    MemBlock* block = find_best_fit(size);
    if (!block) {
        exit_critical();
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "No suitable block found for size %u", size);
        return NULL;
    }
    
    // Split block if possible
    split_block(block, size);
    
    // Setup block
    block->flags = MEM_BLOCK_USED;
    block->file = file;
    block->line = line;
    setup_block_guards(block);
    
    // Update statistics
    if (mem_mgr.config.enable_stats) {
        mem_mgr.stats.total_allocated += size;
        mem_mgr.stats.current_usage += size;
        mem_mgr.stats.allocation_count++;
        mem_mgr.stats.heap_allocations++;
        mem_mgr.stats.peak_usage = (mem_mgr.stats.current_usage > mem_mgr.stats.peak_usage) ? 
                                   mem_mgr.stats.current_usage : mem_mgr.stats.peak_usage;
    }
    
    void* ptr = block + 1;
    exit_critical();
    return ptr;
}

void Memory_Free(void* ptr) {
    if (!mem_mgr.initialized || !ptr) {
        return;
    }
    
    enter_critical();
    
    MemBlock* block = ((MemBlock*)ptr) - 1;
    
    // Check if this is a pool allocation
    if (block->flags & MEM_BLOCK_POOL) {
        for (uint32_t i = 0; i < mem_mgr.pool_count; i++) {
            if (free_to_pool(&mem_mgr.pools[i], ptr)) {
                if (mem_mgr.config.enable_stats) {
                    mem_mgr.stats.total_freed += block->size;
                    mem_mgr.stats.current_usage -= block->size;
                    mem_mgr.stats.free_count++;
                }
                exit_critical();
                return;
            }
        }
    }
    
    if (!is_valid_block(block)) {
        exit_critical();
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Invalid block pointer in free: %p", ptr);
        return;
    }
    
    if (!(block->flags & MEM_BLOCK_USED)) {
        exit_critical();
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Double free detected at %p", ptr);
        return;
    }
    
    if (!check_block_guards(block)) {
        exit_critical();
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Memory corruption detected at %p", ptr);
        return;
    }
    
    // Update statistics
    if (mem_mgr.config.enable_stats) {
        mem_mgr.stats.total_freed += block->size;
        mem_mgr.stats.current_usage -= block->size;
        mem_mgr.stats.free_count++;
    }
    
    // Clear block data
    memset(ptr, 0, block->size);
    block->flags = MEM_BLOCK_FREE;
    block->file = NULL;
    block->line = 0;
    
    // Merge with next block if free
    if (block->next && !(block->next->flags & MEM_BLOCK_USED)) {
        block->size += block->next->size + BLOCK_OVERHEAD;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        } else {
            mem_mgr.last_block = block;
        }
    }
    
    // Merge with previous block if free
    if (block->prev && !(block->prev->flags & MEM_BLOCK_USED)) {
        block->prev->size += block->size + BLOCK_OVERHEAD;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        } else {
            mem_mgr.last_block = block->prev;
        }
    }
    
    exit_critical();
}

void* Memory_Calloc(uint32_t count, uint32_t size, const char* file, uint32_t line) {
    if (count == 0 || size == 0) {
        return NULL;
    }
    
    // Check for multiplication overflow
    uint64_t total = (uint64_t)count * (uint64_t)size;
    if (total > UINT32_MAX) {
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Calloc size overflow");
        return NULL;
    }
    
    void* ptr = Memory_Alloc((uint32_t)total, file, line);
    if (ptr) {
        memset(ptr, 0, (uint32_t)total);
    }
    return ptr;
}

void* Memory_AllocAligned(uint32_t size, uint32_t alignment, const char* file, uint32_t line) {
    if (!mem_mgr.initialized || size == 0 || alignment == 0) {
        return NULL;
    }
    
    // Alignment must be power of 2
    if ((alignment & (alignment - 1)) != 0) {
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Invalid alignment: %u", alignment);
        return NULL;
    }
    
    // Calculate total size needed including alignment overhead
    uint32_t total_size = size + alignment + sizeof(void*);
    
    // Allocate memory with extra space for alignment
    uint8_t* raw = (uint8_t*)Memory_Alloc(total_size, file, line);
    if (!raw) {
        return NULL;
    }
    
    // Calculate aligned address
    void* aligned = (void*)(((uintptr_t)raw + sizeof(void*) + alignment - 1) & ~(alignment - 1));
    
    // Store original address before aligned pointer
    *((void**)aligned - 1) = raw;
    
    return aligned;
}

void* Memory_Realloc(void* ptr, uint32_t size, const char* file, uint32_t line) {
    if (!mem_mgr.initialized) {
        return NULL;
    }
    
    if (!ptr) {
        return Memory_Alloc(size, file, line);
    }
    
    if (size == 0) {
        Memory_Free(ptr);
        return NULL;
    }
    
    enter_critical();
    
    MemBlock* block = ((MemBlock*)ptr) - 1;
    
    // Handle pool allocations
    if (block->flags & MEM_BLOCK_POOL) {
        // For pool allocations, always allocate new block and copy
        exit_critical();
        void* new_ptr = Memory_Alloc(size, file, line);
        if (new_ptr) {
            memcpy(new_ptr, ptr, block->size < size ? block->size : size);
            Memory_Free(ptr);
        }
        return new_ptr;
    }
    
    if (!is_valid_block(block)) {
        exit_critical();
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Invalid block in realloc: %p", ptr);
        return NULL;
    }
    
    if (!check_block_guards(block)) {
        exit_critical();
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Memory corruption in realloc at %p", ptr);
        return NULL;
    }
    
    // Align size
    size = (size + sizeof(void*) - 1) & ALIGNMENT_MASK;
    
    // If new size fits in current block
    if (size <= block->size) {
        if (size < block->size) {
            split_block(block, size);
        }
        exit_critical();
        return ptr;
    }
    
    // Try to merge with next block if free
    if (block->next && !(block->next->flags & MEM_BLOCK_USED) && 
        (block->size + block->next->size + BLOCK_OVERHEAD) >= size) {
        
        block->size += block->next->size + BLOCK_OVERHEAD;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        } else {
            mem_mgr.last_block = block;
        }
        
        split_block(block, size);
        exit_critical();
        return ptr;
    }
    
    // Allocate new block
    exit_critical();
    void* new_ptr = Memory_Alloc(size, file, line);
    if (!new_ptr) {
        return NULL;
    }
    
    // Copy data and free old block
    memcpy(new_ptr, ptr, block->size);
    Memory_Free(ptr);
    
    return new_ptr;
}

void Memory_GetStats(MemStats* stats) {
    if (!mem_mgr.initialized || !stats) {
        return;
    }
    
    enter_critical();
    memcpy(stats, &mem_mgr.stats, sizeof(MemStats));
    
    // Calculate fragmentation
    uint32_t total_free = 0;
    uint32_t largest_free = 0;
    uint32_t free_blocks = 0;
    
    for (MemBlock* block = mem_mgr.first_block; block != NULL; block = block->next) {
        if (!(block->flags & MEM_BLOCK_USED)) {
            total_free += block->size;
            free_blocks++;
            if (block->size > largest_free) {
                largest_free = block->size;
            }
        }
    }
    
    if (total_free > 0) {
        stats->fragmentation = (uint32_t)(100 - (largest_free * 100ULL / total_free));
    } else {
        stats->fragmentation = 0;
    }
    
    exit_critical();
}

void Memory_DumpStats(void) {
    if (!mem_mgr.initialized) {
        return;
    }
    
    MemStats stats;
    Memory_GetStats(&stats);
    
    Logger_Log(LOG_LEVEL_INFO, "MEMORY", "Memory Statistics:");
    Logger_Log(LOG_LEVEL_INFO, "MEMORY", "  Total Allocated: %u bytes", stats.total_allocated);
    Logger_Log(LOG_LEVEL_INFO, "MEMORY", "  Total Freed: %u bytes", stats.total_freed);
    Logger_Log(LOG_LEVEL_INFO, "MEMORY", "  Current Usage: %u bytes", stats.current_usage);
    Logger_Log(LOG_LEVEL_INFO, "MEMORY", "  Peak Usage: %u bytes", stats.peak_usage);
    Logger_Log(LOG_LEVEL_INFO, "MEMORY", "  Heap Allocations: %u", stats.heap_allocations);
    Logger_Log(LOG_LEVEL_INFO, "MEMORY", "  Pool Allocations: %u", stats.pool_allocations);
    Logger_Log(LOG_LEVEL_INFO, "MEMORY", "  Total Frees: %u", stats.free_count);
    Logger_Log(LOG_LEVEL_INFO, "MEMORY", "  Fragmentation: %u%%", stats.fragmentation);
}

bool Memory_CheckIntegrity(void) {
    if (!mem_mgr.initialized) {
        return false;
    }
    
    enter_critical();
    bool integrity_ok = true;
    
    // Check heap blocks
    MemBlock* block = mem_mgr.first_block;
    uint32_t total_size = 0;
    
    while (block && integrity_ok) {
        // Basic block validation
        if (!is_valid_block(block)) {
            Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Invalid block at %p", block);
            integrity_ok = false;
            break;
        }
        
        // Check guards for used blocks
        if ((block->flags & MEM_BLOCK_USED) && !check_block_guards(block)) {
            Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Guard check failed at %p", block);
            integrity_ok = false;
            break;
        }
        
        // Check links
        if (block->next) {
            if (block->next->prev != block) {
                Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Broken linked list at %p", block);
                integrity_ok = false;
                break;
            }
        } else if (block != mem_mgr.last_block) {
            Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Last block mismatch at %p", block);
            integrity_ok = false;
            break;
        }
        
        total_size += block->size + BLOCK_OVERHEAD;
        block = block->next;
    }
    
    // Check total size
    if (integrity_ok && total_size > mem_mgr.config.heap_size) {
        Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Heap size overflow: %u > %u", 
                  total_size, mem_mgr.config.heap_size);
        integrity_ok = false;
    }
    
    // Check memory pools
    for (uint32_t i = 0; i < mem_mgr.pool_count && integrity_ok; i++) {
        MemoryPool* pool = &mem_mgr.pools[i];
        
        // Check pool blocks
        for (uint32_t j = 0; j < pool->block_count; j++) {
            if (pool->block_used[j]) {
                MemBlock* block = (MemBlock*)(pool->pool_memory + (j * pool->block_size));
                if (!check_block_guards(block)) {
                    Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Pool guard check failed at %p", block);
                    integrity_ok = false;
                    break;
                }
            }
        }
        
        // Verify free list
        for (uint32_t j = 0; j < pool->free_blocks && integrity_ok; j++) {
            if (pool->free_list[j] >= pool->block_count) {
                Logger_Log(LOG_LEVEL_ERROR, "MEMORY", "Invalid pool free list entry: %u", 
                          pool->free_list[j]);
                integrity_ok = false;
                break;
            }
        }
    }
    
    exit_critical();
    return integrity_ok;
}

void Memory_Defragment(void) {
    if (!mem_mgr.initialized) {
        return;
    }
    
    enter_critical();
    
    bool merged;
    do {
        merged = false;
        MemBlock* block = mem_mgr.first_block;
        
        while (block && block->next) {
            if (!(block->flags & MEM_BLOCK_USED) && !(block->next->flags & MEM_BLOCK_USED)) {
                // Merge adjacent free blocks
                block->size += block->next->size + BLOCK_OVERHEAD;
                block->next = block->next->next;
                if (block->next) {
                    block->next->prev = block;
                } else {
                    mem_mgr.last_block = block;
                }
                merged = true;
            } else {
                block = block->next;
            }
        }
    } while (merged);
    
    exit_critical();
    Logger_Log(LOG_LEVEL_INFO, "MEMORY", "Memory defragmentation completed");
}

void Memory_TrackLeaks(MemoryLeakCallback callback) {
    if (!mem_mgr.initialized || !callback) {
        return;
    }
    
    enter_critical();
    
    // Check heap allocations
    MemBlock* block = mem_mgr.first_block;
    uint32_t leak_count = 0;
    uint32_t leaked_bytes = 0;
    
    while (block) {
        if (block->flags & MEM_BLOCK_USED) {
            leak_count++;
            leaked_bytes += block->size;
            callback(block);
        }
        block = block->next;
    }
    
    // Check pool allocations
    for (uint32_t i = 0; i < mem_mgr.pool_count; i++) {
        MemoryPool* pool = &mem_mgr.pools[i];
        for (uint32_t j = 0; j < pool->block_count; j++) {
            if (pool->block_used[j]) {
                MemBlock* block = (MemBlock*)(pool->pool_memory + (j * pool->block_size));
                leak_count++;
                leaked_bytes += block->size;
                callback(block);
            }
        }
    }
    
    if (leak_count > 0) {
        Logger_Log(LOG_LEVEL_WARNING, "MEMORY", "Detected %u memory leaks totaling %u bytes", 
                  leak_count, leaked_bytes);
    }
    
    exit_critical();
}

void Memory_Deinit(void) {
    if (!mem_mgr.initialized) {
        return;
    }
    
    enter_critical();
    
    // Report memory leaks if tracking is enabled
    if (mem_mgr.config.enable_tracking) {
        Memory_TrackLeaks([](const MemBlock* block) {
            Logger_Log(LOG_LEVEL_WARNING, "MEMORY", 
                      "Memory leak: %u bytes at %p (allocated in %s:%u)",
                      block->size, block + 1, block->file, block->line);
        });
    }
    
    // Free memory pools
    for (uint32_t i = 0; i < mem_mgr.pool_count; i++) {
        free(mem_mgr.pools[i].pool_memory);
        free(mem_mgr.pools[i].free_list);
        free(mem_mgr.pools[i].block_used);
    }
    
    // Free heap memory
    free(mem_mgr.heap_memory);
    
    // Clear manager state
    memset(&mem_mgr, 0, sizeof(MemoryManager));
    
    Logger_Log(LOG_LEVEL_INFO, "MEMORY", "Memory manager deinitialized");
    
    exit_critical();
}
