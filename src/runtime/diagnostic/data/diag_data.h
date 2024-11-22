#ifndef CANT_DIAG_DATA_H
#define CANT_DIAG_DATA_H

#include "../data_manager.h"
#include <stdint.h>
#include <stdbool.h>

// Vehicle Information DIDs
#define DID_VIN                     0xF190
#define DID_VEHICLE_MODEL           0xF191
#define DID_SYSTEM_NAME             0xF192
#define DID_REPAIR_SHOP_CODE        0xF193
#define DID_PROGRAMMING_DATE        0xF194

// ECU Information DIDs
#define DID_ECU_SERIAL             0xF200
#define DID_ECU_HW_VERSION         0xF201
#define DID_ECU_SW_VERSION         0xF202
#define DID_ECU_MFG_DATE          0xF203
#define DID_ECU_SUPPLIER_ID        0xF204

// System Status DIDs
#define DID_BATTERY_VOLTAGE        0xF300
#define DID_SYSTEM_VOLTAGE         0xF301
#define DID_ENGINE_SPEED           0xF302
#define DID_VEHICLE_SPEED          0xF303
#define DID_ENGINE_TEMP            0xF304
#define DID_AMBIENT_TEMP           0xF305

// Diagnostic Data DIDs
#define DID_TOTAL_DISTANCE         0xF400
#define DID_TOTAL_RUNTIME          0xF401
#define DID_LAST_ERROR_CODE        0xF402
#define DID_ERROR_COUNT            0xF403
#define DID_BOOT_COUNT             0xF404

// Configuration DIDs
#define DID_NETWORK_CONFIG         0xF500
#define DID_CAN_BAUDRATE          0xF501
#define DID_NODE_ADDRESS          0xF502
#define DID_SECURITY_CONFIG       0xF503
#define DID_ROUTINE_CONFIG        0xF504

// Data identifier handler declarations
bool read_vehicle_info(uint16_t did, uint8_t* data, uint16_t* length);
bool write_vehicle_info(uint16_t did, const uint8_t* data, uint16_t length);
bool read_ecu_info(uint16_t did, uint8_t* data, uint16_t* length);
bool write_ecu_info(uint16_t did, const uint8_t* data, uint16_t length);
bool read_system_status(uint16_t did, uint8_t* data, uint16_t* length);
bool read_diagnostic_data(uint16_t did, uint8_t* data, uint16_t* length);
bool read_configuration(uint16_t did, uint8_t* data, uint16_t* length);
bool write_configuration(uint16_t did, const uint8_t* data, uint16_t length);

#endif // CANT_DIAG_DATA_H 