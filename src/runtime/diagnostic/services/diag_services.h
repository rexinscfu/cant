#ifndef CANT_DIAG_SERVICES_H
#define CANT_DIAG_SERVICES_H

#include "../diag_system.h"
#include "../service_router.h"

// Service IDs
#define UDS_SID_DIAGNOSTIC_SESSION_CONTROL      0x10
#define UDS_SID_ECU_RESET                      0x11
#define UDS_SID_CLEAR_DTC                      0x14
#define UDS_SID_READ_DTC                       0x19
#define UDS_SID_READ_DATA_BY_ID                0x22
#define UDS_SID_READ_MEMORY_BY_ADDRESS         0x23
#define UDS_SID_READ_SCALING_BY_ID             0x24
#define UDS_SID_SECURITY_ACCESS                0x27
#define UDS_SID_COMMUNICATION_CONTROL          0x28
#define UDS_SID_READ_DATA_PERIODIC             0x2A
#define UDS_SID_DEFINE_DATA_ID                 0x2C
#define UDS_SID_WRITE_DATA_BY_ID               0x2E
#define UDS_SID_IO_CONTROL                     0x2F
#define UDS_SID_ROUTINE_CONTROL                0x31
#define UDS_SID_REQUEST_DOWNLOAD               0x34
#define UDS_SID_REQUEST_UPLOAD                 0x35
#define UDS_SID_TRANSFER_DATA                  0x36
#define UDS_SID_REQUEST_TRANSFER_EXIT          0x37
#define UDS_SID_REQUEST_FILE_TRANSFER          0x38
#define UDS_SID_WRITE_MEMORY_BY_ADDRESS        0x3D
#define UDS_SID_TESTER_PRESENT                 0x3E
#define UDS_SID_ACCESS_TIMING_PARAMS           0x83
#define UDS_SID_SECURED_DATA_TRANSMISSION      0x84
#define UDS_SID_CONTROL_DTC_SETTINGS          0x85
#define UDS_SID_RESPONSE_ON_EVENT              0x86
#define UDS_SID_LINK_CONTROL                   0x87

// Session Types
#define UDS_SESSION_DEFAULT                    0x01
#define UDS_SESSION_PROGRAMMING                0x02
#define UDS_SESSION_EXTENDED                   0x03
#define UDS_SESSION_SAFETY                     0x04

// Security Levels
#define SECURITY_LEVEL_UNLOCKED                0x00
#define SECURITY_LEVEL_PROGRAMMING             0x01
#define SECURITY_LEVEL_EXTENDED                0x03
#define SECURITY_LEVEL_SAFETY                  0x05

// Routine IDs
#define ROUTINE_BATTERY_TEST                   0x0100
#define ROUTINE_SENSOR_CALIBRATION             0x0200
#define ROUTINE_ACTUATOR_TEST                  0x0300
#define ROUTINE_MEMORY_CHECK                   0x0400
#define ROUTINE_NETWORK_TEST                   0x0500

// Data Identifiers
#define DID_VEHICLE_INFO                       0xF190
#define DID_ECU_INFO                          0xF191
#define DID_BOOT_INFO                         0xF192
#define DID_BATTERY_INFO                      0xF193
#define DID_SENSOR_DATA                       0xF194
#define DID_NETWORK_CONFIG                    0xF195

// Service handler declarations
UdsResponseCode handle_diagnostic_session_control(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_ecu_reset(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_security_access(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_communication_control(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_tester_present(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_read_data_by_id(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_write_data_by_id(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_routine_control(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_request_download(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_transfer_data(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_request_transfer_exit(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_read_memory_by_address(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_write_memory_by_address(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_clear_dtc(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_read_dtc(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_io_control(const UdsMessage* request, UdsMessage* response);
UdsResponseCode handle_read_scaling_by_id(const UdsMessage* request, UdsMessage* response);

// System control functions
bool System_PerformHardReset(void);
bool System_PerformKeyReset(void);
bool System_PerformSoftReset(void);
bool System_EnableRapidPowerdown(void);
bool System_DisableRapidPowerdown(void);

// Service helper functions
bool validate_memory_range(uint32_t address, uint32_t size);
bool validate_data_identifier(uint16_t did);
bool validate_routine_id(uint16_t rid);
bool validate_io_parameters(uint8_t control_type, const uint8_t* params, uint16_t length);

#endif 