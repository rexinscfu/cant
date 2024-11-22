#ifndef CANT_DATA_MANAGER_H
#define CANT_DATA_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// Data Identifier Types
typedef enum {
    DATA_TYPE_UINT8,
    DATA_TYPE_UINT16,
    DATA_TYPE_UINT32,
    DATA_TYPE_INT8,
    DATA_TYPE_INT16,
    DATA_TYPE_INT32,
    DATA_TYPE_FLOAT,
    DATA_TYPE_STRING,
    DATA_TYPE_RAW
} DataType;

// Scaling Method
typedef enum {
    SCALING_NONE,
    SCALING_LINEAR,
    SCALING_INVERSE,
    SCALING_FORMULA,
    SCALING_TABLE
} ScalingMethod;

// Data Access Rights
typedef enum {
    DATA_ACCESS_READ = 0x01,
    DATA_ACCESS_WRITE = 0x02,
    DATA_ACCESS_CONTROL = 0x04
} DataAccessRight;

// Data Identifier Definition
typedef struct {
    uint16_t did;
    DataType type;
    uint16_t length;
    uint8_t access_rights;
    uint8_t security_level;
    ScalingMethod scaling;
    void* data_ptr;
    bool (*read_handler)(uint16_t did, uint8_t* data, uint16_t* length);
    bool (*write_handler)(uint16_t did, const uint8_t* data, uint16_t length);
} DataIdentifier;

// Data Manager Configuration
typedef struct {
    DataIdentifier* identifiers;
    uint32_t identifier_count;
    void (*access_callback)(uint16_t did, DataAccessRight access, bool granted);
} DataManagerConfig;

// Data Manager API
bool Data_Manager_Init(const DataManagerConfig* config);
void Data_Manager_DeInit(void);
bool Data_Manager_ReadData(uint16_t did, uint8_t* data, uint16_t* length);
bool Data_Manager_WriteData(uint16_t did, const uint8_t* data, uint16_t length);
bool Data_Manager_AddIdentifier(const DataIdentifier* identifier);
bool Data_Manager_RemoveIdentifier(uint16_t did);
DataIdentifier* Data_Manager_GetIdentifier(uint16_t did);
bool Data_Manager_HasAccess(uint16_t did, DataAccessRight access);
uint32_t Data_Manager_GetIdentifierCount(void);

#endif // CANT_DATA_MANAGER_H 