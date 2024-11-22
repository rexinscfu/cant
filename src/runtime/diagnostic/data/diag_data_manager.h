#ifndef CANT_DIAG_DATA_MANAGER_H
#define CANT_DIAG_DATA_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    DATA_TYPE_UINT8,
    DATA_TYPE_UINT16,
    DATA_TYPE_UINT32,
    DATA_TYPE_INT8,
    DATA_TYPE_INT16,
    DATA_TYPE_INT32,
    DATA_TYPE_FLOAT,
    DATA_TYPE_STRING
} DataType;

typedef struct {
    uint16_t id;
    DataType type;
    void* data;
    uint16_t size;
    bool (*validator)(const void* data, uint16_t size);
    void (*callback)(const void* old_data, const void* new_data);
} DiagDataItem;

typedef struct {
    uint32_t max_items;
    bool enable_validation;
    bool enable_callbacks;
    bool enable_caching;
    uint32_t cache_timeout_ms;
} DiagDataConfig;

bool DiagData_Init(const DiagDataConfig* config);
void DiagData_Deinit(void);

bool DiagData_RegisterItem(const DiagDataItem* item);
bool DiagData_UnregisterItem(uint16_t id);

bool DiagData_ReadItem(uint16_t id, void* data, uint16_t* size);
bool DiagData_WriteItem(uint16_t id, const void* data, uint16_t size);

void DiagData_ProcessCache(void);

#endif // CANT_DIAG_DATA_MANAGER_H 