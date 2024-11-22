#include "rt_memory.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct RTMemPool {
    void* memory;
    void** free_blocks;
    size_t block_size;
    size_t block_count;
    size_t free_count;
    pthread_mutex_t lock;
};

RTMemPool* rt_mempool_create(size_t block_size, size_t block_count) {
    RTMemPool* pool = malloc(sizeof(RTMemPool));
    if (!pool) return NULL;
    
    // Align block size to cache line
    block_size = (block_size + 63) & ~63;
    
    pool->memory = aligned_alloc(64, block_size * block_count);
    if (!pool->memory) {
        free(pool);
        return NULL;
    }
    
    pool->free_blocks = malloc(sizeof(void*) * block_count);
    if (!pool->free_blocks) {
        free(pool->memory);
        free(pool);
        return NULL;
    }
    
    pool->block_size = block_size;
    pool->block_count = block_count;
    pool->free_count = block_count;
    
    // Initialize free block list
    for (size_t i = 0; i < block_count; i++) {
        pool->free_blocks[i] = (char*)pool->memory + (i * block_size);
    }
    
    pthread_mutex_init(&pool->lock, NULL);
    
    return pool;
}

void* rt_mempool_alloc(RTMemPool* pool) {
    if (!pool || pool->free_count == 0) return NULL;
    
    pthread_mutex_lock(&pool->lock);
    void* block = pool->free_blocks[--pool->free_count];
    pthread_mutex_unlock(&pool->lock);
    
    return block;
}

void rt_mempool_free(RTMemPool* pool, void* ptr) {
    if (!pool || !ptr) return;
    
    // Validate pointer belongs to pool
    if (ptr < pool->memory || 
        ptr >= (char*)pool->memory + (pool->block_count * pool->block_size)) {
        return;
    }
    
    pthread_mutex_lock(&pool->lock);
    if (pool->free_count < pool->block_count) {
        pool->free_blocks[pool->free_count++] = ptr;
    }
    pthread_mutex_unlock(&pool->lock);
}

void rt_mempool_destroy(RTMemPool* pool) {
    if (!pool) return;
    
    pthread_mutex_destroy(&pool->lock);
    free(pool->free_blocks);
    free(pool->memory);
    free(pool);
}

size_t rt_mempool_available(RTMemPool* pool) {
    if (!pool) return 0;
    
    pthread_mutex_lock(&pool->lock);
    size_t count = pool->free_count;
    pthread_mutex_unlock(&pool->lock);
    
    return count;
} 