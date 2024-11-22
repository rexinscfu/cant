#ifndef CANT_RT_MEMORY_H
#define CANT_RT_MEMORY_H

#include <stddef.h>
#include <stdbool.h>

typedef struct RTMemPool RTMemPool;

RTMemPool* rt_mempool_create(size_t block_size, size_t block_count);
void* rt_mempool_alloc(RTMemPool* pool);
void rt_mempool_free(RTMemPool* pool, void* ptr);
void rt_mempool_destroy(RTMemPool* pool);
size_t rt_mempool_available(RTMemPool* pool);

#endif // CANT_RT_MEMORY_H 