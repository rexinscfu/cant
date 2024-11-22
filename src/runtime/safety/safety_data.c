#include "safety_data.h"
#include <string.h>
#include "../utils/crc.h"
#include "../os/critical.h"

#define MAX_SAFETY_DATA 64
#define INVERSE_PATTERN 0xFFFFFFFF

// Internal data structure
typedef struct {
    SafetyDataConfig config;
    struct {
        uint32_t crc;
        uint32_t error_count;
        bool valid;
        bool initialized;
    } status;
} SafetyDataInstance;

// Internal state
static struct {
    SafetyDataInstance data[MAX_SAFETY_DATA];
    uint32_t data_count;
    bool initialized;
    CriticalSection critical;
} safety_data;

// Internal helper functions
static bool validate_data_id(uint32_t data_id) {
    return data_id < safety_data.data_count;
}

static uint32_t calculate_data_crc(const SafetyDataInstance* instance) {
    if (!instance->config.data_ptr) return 0;
    
    return calculate_crc32(instance->config.data_ptr, 
                          instance->config.data_size);
}

static bool validate_value_range(const SafetyDataConfig* config, const void* value) {
    if (!config || !value) return false;
    
    float float_value;
    
    switch (config->type) {
        case SAFETY_DATA_INT8:
            float_value = (float)(*(int8_t*)value);
            break;
        case SAFETY_DATA_INT16:
            float_value = (float)(*(int16_t*)value);
            break;
        case SAFETY_DATA_INT32:
            float_value = (float)(*(int32_t*)value);
            break;
        case SAFETY_DATA_UINT8:
            float_value = (float)(*(uint8_t*)value);
            break;
        case SAFETY_DATA_UINT16:
            float_value = (float)(*(uint16_t*)value);
            break;
        case SAFETY_DATA_UINT32:
            float_value = (float)(*(uint32_t*)value);
            break;
        case SAFETY_DATA_FLOAT:
            float_value = *(float*)value;
            break;
        case SAFETY_DATA_DOUBLE:
            float_value = (float)(*(double*)value);
            break;
        case SAFETY_DATA_BOOL:
            return true;  // Boolean values don't need range checking
        case SAFETY_DATA_CUSTOM:
            return true;  // Custom types need their own validation
        default:
            return false;
    }
    
    return (float_value >= config->limits.min_value && 
            float_value <= config->limits.max_value);
}

static void update_redundant_copy(SafetyDataInstance* instance) {
    if (!instance->config.redundant_ptr) return;
    
    switch (instance->config.protection) {
        case PROTECTION_REDUNDANT:
            memcpy(instance->config.redundant_ptr, 
                   instance->config.data_ptr, 
                   instance->config.data_size);
            break;
            
        case PROTECTION_INVERSE:
            for (size_t i = 0; i < instance->config.data_size; i++) {
                ((uint8_t*)instance->config.redundant_ptr)[i] = 
                    ~((uint8_t*)instance->config.data_ptr)[i];
            }
            break;
            
        default:
            break;
    }
}

static bool verify_redundant_copy(const SafetyDataInstance* instance) {
    if (!instance->config.redundant_ptr) return true;
    
    switch (instance->config.protection) {
        case PROTECTION_REDUNDANT:
            return memcmp(instance->config.data_ptr, 
                         instance->config.redundant_ptr, 
                         instance->config.data_size) == 0;
            
        case PROTECTION_INVERSE:
            for (size_t i = 0; i < instance->config.data_size; i++) {
                if ((((uint8_t*)instance->config.data_ptr)[i] ^ 
                     ((uint8_t*)instance->config.redundant_ptr)[i]) != 0xFF) {
                    return false;
                }
            }
            return true;
            
        default:
            return true;
    }
}

bool Safety_Data_Init(SafetyDataConfig* configs, uint32_t count) {
    if (!configs || count == 0 || count > MAX_SAFETY_DATA) {
        return false;
    }
    
    enter_critical(&safety_data.critical);
    
    memset(&safety_data, 0, sizeof(safety_data));
    safety_data.data_count = count;
    
    // Initialize each safety data instance
    for (uint32_t i = 0; i < count; i++) {
        SafetyDataInstance* instance = &safety_data.data[i];
        memcpy(&instance->config, &configs[i], sizeof(SafetyDataConfig));
        
        // Initialize with default value
        if (instance->config.data_ptr) {
            switch (instance->config.type) {
                case SAFETY_DATA_INT8:
                    *(int8_t*)instance->config.data_ptr = 
                        (int8_t)instance->config.limits.default_value;
                    break;
                case SAFETY_DATA_INT16:
                    *(int16_t*)instance->config.data_ptr = 
                        (int16_t)instance->config.limits.default_value;
                    break;
                case SAFETY_DATA_INT32:
                    *(int32_t*)instance->config.data_ptr = 
                        (int32_t)instance->config.limits.default_value;
                    break;
                case SAFETY_DATA_UINT8:
                    *(uint8_t*)instance->config.data_ptr = 
                        (uint8_t)instance->config.limits.default_value;
                    break;
                case SAFETY_DATA_UINT16:
                    *(uint16_t*)instance->config.data_ptr = 
                        (uint16_t)instance->config.limits.default_value;
                    break;
                case SAFETY_DATA_UINT32:
                    *(uint32_t*)instance->config.data_ptr = 
                        (uint32_t)instance->config.limits.default_value;
                    break;
                case SAFETY_DATA_FLOAT:
                    *(float*)instance->config.data_ptr = 
                        (float)instance->config.limits.default_value;
                    break;
                case SAFETY_DATA_DOUBLE:
                    *(double*)instance->config.data_ptr = 
                        instance->config.limits.default_value;
                    break;
                case SAFETY_DATA_BOOL:
                    *(bool*)instance->config.data_ptr = false;
                    break;
                default:
                    break;
            }
            
            update_redundant_copy(instance);
            instance->status.crc = calculate_data_crc(instance);
            instance->status.valid = true;
            instance->status.initialized = true;
        }
    }
    
    safety_data.initialized = true;
    
    exit_critical(&safety_data.critical);
    return true;
}

void Safety_Data_DeInit(void) {
    enter_critical(&safety_data.critical);
    safety_data.initialized = false;
    memset(&safety_data, 0, sizeof(safety_data));
    exit_critical(&safety_data.critical);
}

bool Safety_Data_Write(uint32_t data_id, const void* value) {
    if (!safety_data.initialized || !validate_data_id(data_id) || !value) {
        return false;
    }
    
    SafetyDataInstance* instance = &safety_data.data[data_id];
    if (!instance->status.initialized || !instance->config.data_ptr) {
        return false;
    }
    
    enter_critical(&safety_data.critical);
    
    // Validate value range
    if (!validate_value_range(&instance->config, value)) {
        instance->status.error_count++;
        exit_critical(&safety_data.critical);
        return false;
    }
    
    // Update main data
    memcpy(instance->config.data_ptr, value, instance->config.data_size);
    
    // Update protection
    update_redundant_copy(instance);
    instance->status.crc = calculate_data_crc(instance);
    
    instance->status.valid = true;
    
    if (instance->config.validation_callback) {
        instance->config.validation_callback(instance->config.data_ptr, true);
    }
    
    exit_critical(&safety_data.critical);
    return true;
}

bool Safety_Data_Read(uint32_t data_id, void* value) {
    if (!safety_data.initialized || !validate_data_id(data_id) || !value) {
        return false;
    }
    
    SafetyDataInstance* instance = &safety_data.data[data_id];
    if (!instance->status.initialized || !instance->config.data_ptr) {
        return false;
    }
    
    enter_critical(&safety_data.critical);
    
    // Verify data integrity
    bool valid = Safety_Data_Verify(data_id);
    if (!valid) {
        exit_critical(&safety_data.critical);
        return false;
    }
    
    memcpy(value, instance->config.data_ptr, instance->config.data_size);
    
    exit_critical(&safety_data.critical);
    return true;
}

bool Safety_Data_IsValid(uint32_t data_id) {
    if (!safety_data.initialized || !validate_data_id(data_id)) {
        return false;
    }
    
    return safety_data.data[data_id].status.valid;
}

void Safety_Data_Reset(uint32_t data_id) {
    if (!safety_data.initialized || !validate_data_id(data_id)) {
        return;
    }
    
    SafetyDataInstance* instance = &safety_data.data[data_id];
    if (!instance->status.initialized) return;
    
    enter_critical(&safety_data.critical);
    
    // Reset to default value
    if (instance->config.data_ptr) {
        switch (instance->config.type) {
            case SAFETY_DATA_INT8:
                *(int8_t*)instance->config.data_ptr = 
                    (int8_t)instance->config.limits.default_value;
                break;
            case SAFETY_DATA_INT16:
                *(int16_t*)instance->config.data_ptr = 
                    (int16_t)instance->config.limits.default_value;
                break;
            case SAFETY_DATA_INT32:
                *(int32_t*)instance->config.data_ptr = 
                    (int32_t)instance->config.limits.default_value;
                break;
            case SAFETY_DATA_UINT8:
                *(uint8_t*)instance->config.data_ptr = 
                    (uint8_t)instance->config.limits.default_value;
                break;
            case SAFETY_DATA_UINT16:
                *(uint16_t*)instance->config.data_ptr = 
                    (uint16_t)instance->config.limits.default_value;
                break;
            case SAFETY_DATA_UINT32:
                *(uint32_t*)instance->config.data_ptr = 
                    (uint32_t)instance->config.limits.default_value;
                break;
            case SAFETY_DATA_FLOAT:
                *(float*)instance->config.data_ptr = 
                    (float)instance->config.limits.default_value;
                break;
            case SAFETY_DATA_DOUBLE:
                *(double*)instance->config.data_ptr = 
                    instance->config.limits.default_value;
                break;
            case SAFETY_DATA_BOOL:
                *(bool*)instance->config.data_ptr = false;
                break;
            default:
                break;
        }
        
        update_redundant_copy(instance);
        instance->status.crc = calculate_data_crc(instance);
        instance->status.valid = true;
        instance->status.error_count = 0;
        
        if (instance->config.validation_callback) {
            instance->config.validation_callback(instance->config.data_ptr, true);
        }
    }
    
    exit_critical(&safety_data.critical);
}

bool Safety_Data_Verify(uint32_t data_id) {
    if (!safety_data.initialized || !validate_data_id(data_id)) {
        return false;
    }
    
    SafetyDataInstance* instance = &safety_data.data[data_id];
    if (!instance->status.initialized) return false;
    
    bool valid = true;
    
    enter_critical(&safety_data.critical);
    
    // Check CRC
    if (instance->config.protection == PROTECTION_CRC) {
        uint32_t current_crc = calculate_data_crc(instance);
        if (current_crc != instance->status.crc) {
            valid = false;
        }
    }
    
    // Check redundant copy
    if (valid && (instance->config.protection == PROTECTION_REDUNDANT || 
                 instance->config.protection == PROTECTION_INVERSE)) {
        valid = verify_redundant_copy(instance);
    }
    
    if (!valid) {
        instance->status.error_count++;
        instance->status.valid = false;
        
        if (instance->config.validation_callback) {
            instance->config.validation_callback(instance->config.data_ptr, false);
        }
    }
    
    exit_critical(&safety_data.critical);
    return valid;
}

uint32_t Safety_Data_GetErrorCount(uint32_t data_id) {
    if (!safety_data.initialized || !validate_data_id(data_id)) {
        return 0;
    }
    
    return safety_data.data[data_id].status.error_count;
}

void Safety_Data_GetStatus(uint32_t data_id, bool* valid, uint32_t* error_count) {
    if (!safety_data.initialized || !validate_data_id(data_id) || 
        !valid || !error_count) {
        return;
    }
    
    SafetyDataInstance* instance = &safety_data.data[data_id];
    *valid = instance->status.valid;
    *error_count = instance->status.error_count;
}

bool Safety_Data_Backup(uint32_t data_id) {
    if (!safety_data.initialized || !validate_data_id(data_id)) {
        return false;
    }
    
    SafetyDataInstance* instance = &safety_data.data[data_id];
    if (!instance->status.initialized || !instance->config.redundant_ptr) {
        return false;
    }
    
    enter_critical(&safety_data.critical);
    update_redundant_copy(instance);
    exit_critical(&safety_data.critical);
    
    return true;
}

bool Safety_Data_Restore(uint32_t data_id) {
    if (!safety_data.initialized || !validate_data_id(data_id)) {
        return false;
    }
    
    SafetyDataInstance* instance = &safety_data.data[data_id];
    if (!instance->status.initialized || !instance->config.redundant_ptr) {
        return false;
    }
    
    enter_critical(&safety_data.critical);
    
    if (instance->config.protection == PROTECTION_REDUNDANT) {
        memcpy(instance->config.data_ptr, 
               instance->config.redundant_ptr, 
               instance->config.data_size);
    } else if (instance->config.protection == PROTECTION_INVERSE) {
        for (size_t i = 0; i < instance->config.data_size; i++) {
            ((uint8_t*)instance->config.data_ptr)[i] = 
                ~((uint8_t*)instance->config.redundant_ptr)[i];
        }
    }
    
    instance->status.crc = calculate_data_crc(instance);
    instance->status.valid = true;
    
    if (instance->config.validation_callback) {
        instance->config.validation_callback(instance->config.data_ptr, true);
    }
    
    exit_critical(&safety_data.critical);
    return true;
} 