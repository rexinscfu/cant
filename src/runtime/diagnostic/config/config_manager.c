#include "config_manager.h"
#include "../logging/diag_logger.h"
#include "../os/critical.h"
#include "../os/timer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_CONFIG_ITEMS 256
#define MAX_STRING_LENGTH 256
#define MAX_BLOB_SIZE 1024

typedef struct {
    ConfigItem item;
    uint8_t current_value[MAX_BLOB_SIZE];
    bool modified;
} ConfigEntry;

typedef struct {
    ConfigManagerConfig config;
    ConfigEntry items[MAX_CONFIG_ITEMS];
    uint32_t item_count;
    uint32_t last_save_time;
    bool initialized;
} ConfigManager;

static ConfigManager config_mgr;

static bool validate_config_size(ConfigType type, uint32_t size) {
    switch (type) {
        case CONFIG_TYPE_BOOL:
            return size == sizeof(bool);
        case CONFIG_TYPE_INT32:
        case CONFIG_TYPE_UINT32:
        case CONFIG_TYPE_FLOAT:
            return size == sizeof(uint32_t);
        case CONFIG_TYPE_STRING:
            return size <= MAX_STRING_LENGTH;
        case CONFIG_TYPE_BLOB:
            return size <= MAX_BLOB_SIZE;
        default:
            return false;
    }
}

bool Config_Init(const ConfigManagerConfig* config) {
    if (!config || config->max_items > MAX_CONFIG_ITEMS) {
        return false;
    }

    memset(&config_mgr, 0, sizeof(ConfigManager));
    memcpy(&config_mgr.config, config, sizeof(ConfigManagerConfig));

    config_mgr.last_save_time = Timer_GetMilliseconds();
    config_mgr.initialized = true;

    Logger_Log(LOG_LEVEL_INFO, "CONFIG", "Configuration manager initialized");
    return true;
}

void Config_Deinit(void) {
    if (config_mgr.initialized && config_mgr.config.auto_save) {
        Config_SaveToFile(config_mgr.config.storage_path);
    }
    
    Logger_Log(LOG_LEVEL_INFO, "CONFIG", "Configuration manager deinitialized");
    memset(&config_mgr, 0, sizeof(ConfigManager));
}

bool Config_RegisterItem(const ConfigItem* item) {
    if (!config_mgr.initialized || !item || !item->name || 
        config_mgr.item_count >= config_mgr.config.max_items) {
        return false;
    }

    enter_critical();

    // Check for duplicate ID or name
    for (uint32_t i = 0; i < config_mgr.item_count; i++) {
        if (config_mgr.items[i].item.id == item->id ||
            strcmp(config_mgr.items[i].item.name, item->name) == 0) {
            exit_critical();
            Logger_Log(LOG_LEVEL_ERROR, "CONFIG", 
                      "Duplicate config item ID or name: %s", item->name);
            return false;
        }
    }

    // Validate size for type
    if (!validate_config_size(item->type, item->size)) {
        exit_critical();
        Logger_Log(LOG_LEVEL_ERROR, "CONFIG", 
                  "Invalid size for config type: %s", item->name);
        return false;
    }

    // Add new item
    ConfigEntry* entry = &config_mgr.items[config_mgr.item_count];
    memcpy(&entry->item, item, sizeof(ConfigItem));
    
    // Set default value
    if (item->default_value) {
        memcpy(entry->current_value, item->default_value, item->size);
    }

    config_mgr.item_count++;

    Logger_Log(LOG_LEVEL_INFO, "CONFIG", 
              "Registered config item: %s (ID=0x%08X)", 
              item->name, item->id);

    exit_critical();
    return true;
}

bool Config_UnregisterItem(uint32_t id) {
    if (!config_mgr.initialized) {
        return false;
    }

    enter_critical();

    for (uint32_t i = 0; i < config_mgr.item_count; i++) {
        if (config_mgr.items[i].item.id == id) {
            // Move last item to this position if not last
            if (i < config_mgr.item_count - 1) {
                memcpy(&config_mgr.items[i], 
                       &config_mgr.items[config_mgr.item_count - 1],
                       sizeof(ConfigEntry));
            }
            config_mgr.item_count--;
            
            Logger_Log(LOG_LEVEL_INFO, "CONFIG", 
                      "Unregistered config item: ID=0x%08X", id);
            
            exit_critical();
            return true;
        }
    }

    exit_critical();
    return false;
}

bool Config_SetValue(uint32_t id, const void* value) {
    if (!config_mgr.initialized || !value) {
        return false;
    }

    enter_critical();

    for (uint32_t i = 0; i < config_mgr.item_count; i++) {
        ConfigEntry* entry = &config_mgr.items[i];
        if (entry->item.id == id) {
            // Validate if enabled
            if (config_mgr.config.enable_validation && entry->item.validator) {
                if (!entry->item.validator(value)) {
                    exit_critical();
                    Logger_Log(LOG_LEVEL_ERROR, "CONFIG", 
                              "Validation failed for %s", entry->item.name);
                    return false;
                }
            }

            // Store old value for callback
            uint8_t old_value[MAX_BLOB_SIZE];
            if (config_mgr.config.enable_callbacks && entry->item.callback) {
                memcpy(old_value, entry->current_value, entry->item.size);
            }

            // Update value
            memcpy(entry->current_value, value, entry->item.size);
            entry->modified = true;

            // Call callback if enabled
            if (config_mgr.config.enable_callbacks && entry->item.callback) {
                entry->item.callback(old_value, value);
            }

            Logger_Log(LOG_LEVEL_DEBUG, "CONFIG", 
                      "Updated config item: %s", entry->item.name);
            
            exit_critical();
            return true;
        }
    }

    exit_critical();
    return false;
}

bool Config_GetValue(uint32_t id, void* value) {
    if (!config_mgr.initialized || !value) {
        return false;
    }

    enter_critical();

    for (uint32_t i = 0; i < config_mgr.item_count; i++) {
        ConfigEntry* entry = &config_mgr.items[i];
        if (entry->item.id == id) {
            memcpy(value, entry->current_value, entry->item.size);
            exit_critical();
            return true;
        }
    }

    exit_critical();
    return false;
}

bool Config_ResetToDefault(uint32_t id) {
    if (!config_mgr.initialized) {
        return false;
    }

    enter_critical();

    for (uint32_t i = 0; i < config_mgr.item_count; i++) {
        ConfigEntry* entry = &config_mgr.items[i];
        if (entry->item.id == id && entry->item.default_value) {
            memcpy(entry->current_value, entry->item.default_value, 
                   entry->item.size);
            entry->modified = true;
            
            Logger_Log(LOG_LEVEL_INFO, "CONFIG", 
                      "Reset config item to default: %s", entry->item.name);
            
            exit_critical();
            return true;
        }
    }

    exit_critical();
    return false;
}

void Config_ProcessAutoSave(void) {
    if (!config_mgr.initialized || !config_mgr.config.auto_save || 
        !config_mgr.config.storage_path) {
        return;
    }

    uint32_t current_time = Timer_GetMilliseconds();
    if (current_time - config_mgr.last_save_time >= 
        config_mgr.config.auto_save_interval_ms) {
        
        bool need_save = false;
        for (uint32_t i = 0; i < config_mgr.item_count; i++) {
            if (config_mgr.items[i].modified && 
                config_mgr.items[i].item.persistent) {
                need_save = true;
                break;
            }
        }

        if (need_save) {
            if (Config_SaveToFile(config_mgr.config.storage_path)) {
                // Clear modified flags
                for (uint32_t i = 0; i < config_mgr.item_count; i++) {
                    config_mgr.items[i].modified = false;
                }
            }
        }

        config_mgr.last_save_time = current_time;
    }
}

