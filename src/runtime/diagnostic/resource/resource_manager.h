#ifndef CANT_RESOURCE_MANAGER_H
#define CANT_RESOURCE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    RESOURCE_MEMORY = 0,
    RESOURCE_CPU,
    RESOURCE_FLASH,
    RESOURCE_NETWORK,
    RESOURCE_COUNT
} ResourceType;

typedef struct {
    uint32_t total;
    uint32_t used;
    uint32_t peak;
    uint32_t threshold;
} ResourceStats;

typedef struct {
    uint32_t memory_limit;
    uint32_t cpu_threshold;
    uint32_t flash_write_limit;
    uint32_t network_bandwidth;
    bool enable_monitoring;
    uint32_t check_interval_ms;
} ResourceConfig;

bool Resource_Init(const ResourceConfig* config);
void Resource_Deinit(void);

bool Resource_Allocate(ResourceType type, uint32_t amount);
void Resource_Release(ResourceType type, uint32_t amount);
bool Resource_IsAvailable(ResourceType type, uint32_t amount);

void Resource_GetStats(ResourceType type, ResourceStats* stats);
void Resource_ResetStats(ResourceType type);
void Resource_ProcessUsage(void);

#endif // CANT_RESOURCE_MANAGER_H 