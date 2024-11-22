#ifndef CANT_CONFIG_MANAGER_H
#define CANT_CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_INT32,
    CONFIG_TYPE_UINT32,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_BLOB
} ConfigType;

typedef struct {
    uint32_t id;
    ConfigType type;
    const char* name;
    const char* description;
    void* default_value;
    uint32_t size;
    bool persistent;
    bool (*validator)(const void* value);
    void (*callback)(const void* old_value, const void* new_value);
} ConfigItem;

typedef struct {
    uint32_t max_items;
    const char* storage_path;
    bool enable_validation;
    bool enable_callbacks;
    bool auto_save;
    uint32_t auto_save_interval_ms;
} ConfigManagerConfig;

bool Config_Init(const ConfigManagerConfig* config);
void Config_Deinit(void);

bool Config_RegisterItem(const ConfigItem* item);
bool Config_UnregisterItem(uint32_t id);

bool Config_SetValue(uint32_t id, const void* value);
bool Config_GetValue(uint32_t id, void* value);
bool Config_ResetToDefault(uint32_t id);

bool Config_LoadFromFile(const char* filename);
bool Config_SaveToFile(const char* filename);
void Config_ProcessAutoSave(void);

#endif // CANT_CONFIG_MANAGER_H 