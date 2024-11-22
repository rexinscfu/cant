#ifndef CANT_UDS_SERVICES_H
#define CANT_UDS_SERVICES_H

#include <stdint.h>
#include <stdbool.h>
#include "../protocols/isotp.h"

// UDS Service IDs
typedef enum {
    UDS_DIAGNOSTIC_SESSION_CONTROL = 0x10,
    UDS_ECU_RESET = 0x11,
    UDS_SECURITY_ACCESS = 0x27,
    UDS_COMMUNICATION_CONTROL = 0x28,
    UDS_TESTER_PRESENT = 0x3E,
    UDS_READ_DATA_BY_ID = 0x22,
    UDS_WRITE_DATA_BY_ID = 0x2E,
    UDS_READ_MEMORY_BY_ADDRESS = 0x23,
    UDS_WRITE_MEMORY_BY_ADDRESS = 0x3D,
    UDS_CLEAR_DTC = 0x14,
    UDS_READ_DTC = 0x19,
    UDS_ROUTINE_CONTROL = 0x31
} UDSServiceID;

// UDS Session types
typedef enum {
    UDS_SESSION_DEFAULT = 0x01,
    UDS_SESSION_PROGRAMMING = 0x02,
    UDS_SESSION_EXTENDED = 0x03,
    UDS_SESSION_SAFETY = 0x04
} UDSSessionType;

// UDS Configuration
typedef struct {
    uint16_t vendor_id;
    uint16_t ecu_id;
    uint32_t session_timeout_ms;
    bool security_enabled;
    uint8_t max_security_level;
} UDSConfig;

typedef struct UDSHandler UDSHandler;

// UDS API
UDSHandler* uds_create(ISOTP* isotp, const UDSConfig* config);
void uds_destroy(UDSHandler* handler);
void uds_process(UDSHandler* handler);
bool uds_send_response(UDSHandler* handler, const uint8_t* data, size_t length);
void uds_set_session_timeout_callback(UDSHandler* handler, void (*callback)(void));

#endif // CANT_UDS_SERVICES_H 