#ifndef CANT_SAFETY_DATA_H
#define CANT_SAFETY_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Safety Data Types
typedef enum {
    SAFETY_DATA_INT8,
    SAFETY_DATA_INT16,
    SAFETY_DATA_INT32,
    SAFETY_DATA_UINT8,
    SAFETY_DATA_UINT16,
    SAFETY_DATA_UINT32,
    SAFETY_DATA_FLOAT,
    SAFETY_DATA_DOUBLE,
    SAFETY_DATA_BOOL,
    SAFETY_DATA_CUSTOM
} SafetyDataType;

// Safety Level
typedef enum {
    SAFETY_LEVEL_QM,     // Quality Management
    SAFETY_LEVEL_ASIL_A,
    SAFETY_LEVEL_ASIL_B,
    SAFETY_LEVEL_ASIL_C,
    SAFETY_LEVEL_ASIL_D
} SafetyLevel;

// Data Protection Method
typedef enum {
    PROTECTION_CRC,
    PROTECTION_REDUNDANT,
    PROTECTION_INVERSE,
    PROTECTION_CHECKSUM,
    PROTECTION_E2E
} ProtectionMethod;

// Safety Data Configuration
typedef struct {
    void* data_ptr;
    void* redundant_ptr;
    size_t data_size;
    SafetyDataType type;
    SafetyLevel level;
    ProtectionMethod protection;
    struct {
        float min_value;
        float max_value;
        float default_value;
        float tolerance;
    } limits;
    bool enable_logging;
    void (*validation_callback)(void* data, bool valid);
} SafetyDataConfig;

// Safety Data API
bool Safety_Data_Init(SafetyDataConfig* configs, uint32_t count);
void Safety_Data_DeInit(void);
bool Safety_Data_Write(uint32_t data_id, const void* value);
bool Safety_Data_Read(uint32_t data_id, void* value);
bool Safety_Data_IsValid(uint32_t data_id);
void Safety_Data_Reset(uint32_t data_id);
bool Safety_Data_Verify(uint32_t data_id);
uint32_t Safety_Data_GetErrorCount(uint32_t data_id);
void Safety_Data_GetStatus(uint32_t data_id, bool* valid, uint32_t* error_count);
bool Safety_Data_Backup(uint32_t data_id);
bool Safety_Data_Restore(uint32_t data_id);

#endif // CANT_SAFETY_DATA_H 