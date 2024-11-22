#include "resource_manager.h"
#include "../logging/diag_logger.h"
#include "../os/critical.h"
#include "../os/timer.h"
#include <string.h>

typedef struct {
    ResourceConfig config;
    ResourceStats stats[RESOURCE_COUNT];
    uint32_t last_check_time;
    uint32_t cpu_usage_samples[10];
    uint8_t sample_index;
    bool initialized;
} ResourceManager;

static ResourceManager resource_mgr;

static const char* resource_names[] = {
    "MEMORY",
    "CPU",
    "FLASH",
    "NETWORK"
};

bool Resource_Init(const ResourceConfig* config) {
    if (!config) {
        return false;
    }

    memset(&resource_mgr, 0, sizeof(ResourceManager));
    memcpy(&resource_mgr.config, config, sizeof(ResourceConfig));

    // Initialize resource stats
    resource_mgr.stats[RESOURCE_MEMORY].total = config->memory_limit;
    resource_mgr.stats[RESOURCE_CPU].total = 100; // CPU percentage
    resource_mgr.stats[RESOURCE_FLASH].total = config->flash_write_limit;
    resource_mgr.stats[RESOURCE_NETWORK].total = config->network_bandwidth;

    for (int i = 0; i < RESOURCE_COUNT; i++) {
        resource_mgr.stats[i].threshold = resource_mgr.stats[i].total * 80 / 100; // 80% threshold
    }

    resource_mgr.last_check_time = Timer_GetMilliseconds();
    resource_mgr.initialized = true;

    Logger_Log(LOG_LEVEL_INFO, "RESOURCE", "Resource manager initialized");
    return true;
}

void Resource_Deinit(void) {
    Logger_Log(LOG_LEVEL_INFO, "RESOURCE", "Resource manager deinitialized");
    memset(&resource_mgr, 0, sizeof(ResourceManager));
}

bool Resource_Allocate(ResourceType type, uint32_t amount) {
    if (!resource_mgr.initialized || type >= RESOURCE_COUNT) {
        return false;
    }

    enter_critical();

    ResourceStats* stats = &resource_mgr.stats[type];
    if (stats->used + amount > stats->total) {
        Logger_Log(LOG_LEVEL_ERROR, "RESOURCE", 
                  "%s resource allocation failed: requested=%u, available=%u",
                  resource_names[type], amount, stats->total - stats->used);
        exit_critical();
        return false;
    }

    stats->used += amount;
    if (stats->used > stats->peak) {
        stats->peak = stats->used;
    }

    if (stats->used >= stats->threshold) {
        Logger_Log(LOG_LEVEL_WARNING, "RESOURCE", 
                  "%s resource usage above threshold: %u/%u",
                  resource_names[type], stats->used, stats->total);
    }

    exit_critical();
    return true;
}

void Resource_Release(ResourceType type, uint32_t amount) {
    if (!resource_mgr.initialized || type >= RESOURCE_COUNT) {
        return;
    }

    enter_critical();

    ResourceStats* stats = &resource_mgr.stats[type];
    if (amount > stats->used) {
        Logger_Log(LOG_LEVEL_WARNING, "RESOURCE", 
                  "%s resource over-release detected", resource_names[type]);
        stats->used = 0;
    } else {
        stats->used -= amount;
    }

    exit_critical();
}

bool Resource_IsAvailable(ResourceType type, uint32_t amount) {
    if (!resource_mgr.initialized || type >= RESOURCE_COUNT) {
        return false;
    }

    enter_critical();
    bool available = (resource_mgr.stats[type].used + amount <= 
                     resource_mgr.stats[type].total);
    exit_critical();
    return available;
}

void Resource_GetStats(ResourceType type, ResourceStats* stats) {
    if (!resource_mgr.initialized || type >= RESOURCE_COUNT || !stats) {
        return;
    }

    enter_critical();
    memcpy(stats, &resource_mgr.stats[type], sizeof(ResourceStats));
    exit_critical();
}

void Resource_ResetStats(ResourceType type) {
    if (!resource_mgr.initialized || type >= RESOURCE_COUNT) {
        return;
    }

    enter_critical();
    resource_mgr.stats[type].peak = resource_mgr.stats[type].used;
    exit_critical();
}

void Resource_ProcessUsage(void) {
    if (!resource_mgr.initialized || !resource_mgr.config.enable_monitoring) {
        return;
    }

    uint32_t current_time = Timer_GetMilliseconds();
    if (current_time - resource_mgr.last_check_time < resource_mgr.config.check_interval_ms) {
        return;
    }

    enter_critical();
    resource_mgr.last_check_time = current_time;

    // Update CPU usage (example implementation)
    uint32_t cpu_usage = OS_GetCPUUsage(); // Platform-specific function
    resource_mgr.cpu_usage_samples[resource_mgr.sample_index] = cpu_usage;
    resource_mgr.sample_index = (resource_mgr.sample_index + 1) % 10;

    uint32_t avg_cpu_usage = 0;
    for (int i = 0; i < 10; i++) {
        avg_cpu_usage += resource_mgr.cpu_usage_samples[i];
    }
    avg_cpu_usage /= 10;

    resource_mgr.stats[RESOURCE_CPU].used = avg_cpu_usage;
    if (avg_cpu_usage > resource_mgr.stats[RESOURCE_CPU].peak) {
        resource_mgr.stats[RESOURCE_CPU].peak = avg_cpu_usage;
    }

    // Log warnings for high resource usage
    for (int i = 0; i < RESOURCE_COUNT; i++) {
        if (resource_mgr.stats[i].used >= resource_mgr.stats[i].threshold) {
            Logger_Log(LOG_LEVEL_WARNING, "RESOURCE", 
                      "%s resource usage high: %u/%u (Peak: %u)",
                      resource_names[i],
                      resource_mgr.stats[i].used,
                      resource_mgr.stats[i].total,
                      resource_mgr.stats[i].peak);
        }
    }

    exit_critical();
} 