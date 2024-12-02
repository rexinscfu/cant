#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

// Optimized values based on performance testing
#define BUFFER_POOL_SIZE 64
#define MESSAGE_QUEUE_SIZE 128
#define CACHE_LINE_SIZE 32
#define DMA_THRESHOLD 64
#define MAX_RETRY_COUNT 3
#define CLEANUP_INTERVAL_MS 1000
#define MEMORY_POOL_SIZE (32 * 1024)

// Performance monitoring
#define ENABLE_PERFORMANCE_MONITORING
#define PERFORMANCE_HISTORY_SIZE 60
#define METRIC_UPDATE_INTERVAL_MS 100

// Memory management
#define USE_MEMORY_POOL
#define ENABLE_GARBAGE_COLLECTION
#define GC_THRESHOLD_PERCENT 75

// Hardware specific
#define USE_DMA_ACCELERATION
#define ENABLE_CACHE_OPTIMIZATION
#define USE_HARDWARE_CRC

#endif 