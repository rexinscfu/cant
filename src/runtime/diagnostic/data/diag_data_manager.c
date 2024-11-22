#include "diag_data_manager.h"
#include "../logging/diag_logger.h"
#include "../os/critical.h"
#include "../os/timer.h"
#include <string.h>

#define MAX_DATA_ITEMS 256

typedef struct {
    DiagDataItem item;
    uint32_t last_update_time;
    bool is_cached;
    uint8_t cache_data[64];
} DataItemEntry;

typedef struct {
    DiagDataConfig config;
    DataItemEntry items[MAX_DATA_ITEMS];
    uint32_t item_count;
    uint32_t last_cache_check;
    bool initialized;
} DiagDataManager;

static DiagDataManager data_mgr;

static bool validate_data_size(DataType type, uint16_t size) {
    switch (type) {
        case DATA_TYPE_UINT8:
        case DATA_TYPE_INT8:
            return size == 1;
        case DATA_TYPE_UINT16:
        case DATA_TYPE_INT16:
            return size == 2;
        case DATA_TYPE_UINT32:
        case DATA_TYPE_INT32:
        case DATA_TYPE_FLOAT:
            return size == 4;
        case DATA_TYPE_STRING:
            return size > 0 && size <= 64;
        default:
            return false;
    }
}

bool DiagData_Init(const DiagDataConfig* config) {
    if (!config || config->max_items > MAX_DATA_ITEMS) {
        return false;
    }

    memset(&data_mgr, 0, sizeof(DiagDataManager));
    memcpy(&data_mgr.config, config, sizeof(DiagDataConfig));

    data_mgr.last_cache_check = Timer_GetMilliseconds();
    data_mgr.initialized = true;

    Logger_Log(LOG_LEVEL_INFO, "DATA", "Diagnostic data manager initialized");
    return true;
}

void DiagData_Deinit(void) {
    Logger_Log(LOG_LEVEL_INFO, "DATA", "Diagnostic data manager deinitialized");
    memset(&data_mgr, 0, sizeof(DiagDataManager));
}

bool DiagData_RegisterItem(const DiagDataItem* item) {
    if (!data_mgr.initialized || !item || !item->data || 
        data_mgr.item_count >= data_mgr.config.max_items) {
        return false;
    }

    enter_critical();

    // Check for duplicate ID
    for (uint32_t i = 0; i < data_mgr.item_count; i++) {
        if (data_mgr.items[i].item.id == item->id) {
            exit_critical();
            Logger_Log(LOG_LEVEL_ERROR, "DATA", 
                      "Duplicate data item ID: 0x%04X", item->id);
            return false;
        }
    }

    // Validate size for type
    if (!validate_data_size(item->type, item->size)) {
        exit_critical();
        Logger_Log(LOG_LEVEL_ERROR, "DATA", 
                  "Invalid size for data type: ID=0x%04X", item->id);
        return false;
    }

    // Add new item
    DataItemEntry* entry = &data_mgr.items[data_mgr.item_count];
    memcpy(&entry->item, item, sizeof(DiagDataItem));
    entry->last_update_time = Timer_GetMilliseconds();
    entry->is_cached = false;

    data_mgr.item_count++;

    Logger_Log(LOG_LEVEL_INFO, "DATA", 
              "Registered data item: ID=0x%04X, Type=%d, Size=%d",
              item->id, item->type, item->size);

    exit_critical();
    return true;
}

bool DiagData_UnregisterItem(uint16_t id) {
    if (!data_mgr.initialized) {
        return false;
    }

    enter_critical();

    for (uint32_t i = 0; i < data_mgr.item_count; i++) {
        if (data_mgr.items[i].item.id == id) {
            // Move last item to this position if not last
            if (i < data_mgr.item_count - 1) {
                memcpy(&data_mgr.items[i], 
                       &data_mgr.items[data_mgr.item_count - 1],
                       sizeof(DataItemEntry));
            }
            data_mgr.item_count--;
            
            Logger_Log(LOG_LEVEL_INFO, "DATA", 
                      "Unregistered data item: ID=0x%04X", id);
            
            exit_critical();
            return true;
        }
    }

    exit_critical();
    return false;
}

bool DiagData_ReadItem(uint16_t id, void* data, uint16_t* size) {
    if (!data_mgr.initialized || !data || !size) {
        return false;
    }

    enter_critical();

    for (uint32_t i = 0; i < data_mgr.item_count; i++) {
        DataItemEntry* entry = &data_mgr.items[i];
        if (entry->item.id == id) {
            if (entry->is_cached && data_mgr.config.enable_caching) {
                memcpy(data, entry->cache_data, entry->item.size);
            } else {
                memcpy(data, entry->item.data, entry->item.size);
            }
            *size = entry->item.size;
            
            exit_critical();
            return true;
        }
    }

    exit_critical();
    return false;
}

bool DiagData_WriteItem(uint16_t id, const void* data, uint16_t size) {
    if (!data_mgr.initialized || !data) {
        return false;
    }

    enter_critical();

    for (uint32_t i = 0; i < data_mgr.item_count; i++) {
        DataItemEntry* entry = &data_mgr.items[i];
        if (entry->item.id == id) {
            if (size != entry->item.size) {
                exit_critical();
                Logger_Log(LOG_LEVEL_ERROR, "DATA", 
                          "Size mismatch for ID=0x%04X: expected=%d, got=%d",
                          id, entry->item.size, size);
                return false;
            }

            // Validate if enabled
            if (data_mgr.config.enable_validation && entry->item.validator) {
                if (!entry->item.validator(data, size)) {
                    exit_critical();
                    Logger_Log(LOG_LEVEL_ERROR, "DATA", 
                              "Validation failed for ID=0x%04X", id);
                    return false;
                }
            }

            // Store old data for callback
            uint8_t old_data[64];
            if (data_mgr.config.enable_callbacks && entry->item.callback) {
                memcpy(old_data, entry->item.data, size);
            }

            // Update data
            memcpy(entry->item.data, data, size);
            entry->last_update_time = Timer_GetMilliseconds();
            entry->is_cached = false;

            // Call callback if enabled
            if (data_mgr.config.enable_callbacks && entry->item.callback) {
                entry->item.callback(old_data, data);
            }

            Logger_Log(LOG_LEVEL_DEBUG, "DATA", 
                      "Updated data item: ID=0x%04X", id);
            
            exit_critical();
            return true;
        }
    }

    exit_critical();
    return false;
}

void DiagData_ProcessCache(void) {
    if (!data_mgr.initialized || !data_mgr.config.enable_caching) {
        return;
    }

    uint32_t current_time = Timer_GetMilliseconds();
    if (current_time - data_mgr.last_cache_check < 100) { // Check every 100ms
        return;
    }

    enter_critical();
    data_mgr.last_cache_check = current_time;

    for (uint32_t i = 0; i < data_mgr.item_count; i++) {
        DataItemEntry* entry = &data_mgr.items[i];
        
        // Update cache if needed
        if (!entry->is_cached || 
            (current_time - entry->last_update_time >= data_mgr.config.cache_timeout_ms)) {
            memcpy(entry->cache_data, entry->item.data, entry->item.size);
            entry->is_cached = true;
            entry->last_update_time = current_time;
        }
    }

    exit_critical();
} 