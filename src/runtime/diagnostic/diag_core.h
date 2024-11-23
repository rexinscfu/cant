#ifndef CANT_DIAG_CORE_H
#define DIAG_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "../network/net_protocol.h"

// Diagnostic service IDs
#define DIAG_SID_DIAGNOSTIC_CONTROL        0x10
#define DIAG_SID_ECU_RESET                0x11
#define DIAG_SID_SECURITY_ACCESS          0x27
#define DIAG_SID_COMMUNICATION_CONTROL    0x28
#define DIAG_SID_TESTER_PRESENT          0x3E
#define DIAG_SID_ACCESS_TIMING_PARAMS    0x83
#define DIAG_SID_SECURED_DATA_TRANS      0x84
#define DIAG_SID_CONTROL_DTC_SETTING     0x85
#define DIAG_SID_RESPONSE_ON_EVENT       0x86
#define DIAG_SID_LINK_CONTROL            0x87
#define DIAG_SID_READ_DATA_BY_ID         0x22
#define DIAG_SID_READ_MEM_BY_ADDR        0x23
#define DIAG_SID_READ_SCALING_BY_ID      0x24
#define DIAG_SID_READ_DATA_BY_ID_PERIOD  0x2A
#define DIAG_SID_WRITE_DATA_BY_ID        0x2E
#define DIAG_SID_WRITE_MEM_BY_ADDR       0x3D

// Diagnostic response codes
#define DIAG_RESP_POSITIVE               0x40
#define DIAG_RESP_GENERAL_REJECT         0x10
#define DIAG_RESP_SERVICE_NOT_SUPPORTED  0x11
#define DIAG_RESP_SUBFUNC_NOT_SUPPORTED  0x12
#define DIAG_RESP_BUSY                   0x21
#define DIAG_RESP_CONDITIONS_NOT_CORRECT 0x22
#define DIAG_RESP_REQUEST_SEQ_ERROR      0x24
#define DIAG_RESP_SECURITY_ACCESS_DENIED 0x33
#define DIAG_RESP_INVALID_KEY            0x35
#define DIAG_RESP_EXCEED_NUMBER_ATTEMPTS 0x36
#define DIAG_RESP_REQUIRED_TIME_DELAY    0x37

// Diagnostic session types
typedef enum {
    DIAG_SESSION_DEFAULT = 0x01,
    DIAG_SESSION_PROGRAMMING = 0x02,
    DIAG_SESSION_EXTENDED = 0x03,
    DIAG_SESSION_SAFETY = 0x04
} DiagSessionType;

// Diagnostic security levels
typedef enum {
    DIAG_SEC_LOCKED = 0x00,
    DIAG_SEC_LEVEL1 = 0x01,
    DIAG_SEC_LEVEL2 = 0x02,
    DIAG_SEC_LEVEL3 = 0x03,
    DIAG_SEC_LEVEL4 = 0x04
} DiagSecurityLevel;

// Diagnostic message structure
typedef struct {
    uint32_t id;
    uint8_t service_id;
    uint8_t sub_function;
    uint8_t* data;
    uint32_t length;
    uint32_t timestamp;
    DiagSessionType session;
    DiagSecurityLevel security;
} DiagMessage;

// Diagnostic response structure
typedef struct {
    uint32_t id;
    uint8_t service_id;
    uint8_t response_code;
    uint8_t* data;
    uint32_t length;
    uint32_t timestamp;
    bool success;
} DiagResponse;

// Diagnostic configuration
typedef struct {
    uint32_t request_timeout_ms;
    uint32_t session_timeout_ms;
    uint32_t security_timeout_ms;
    uint32_t max_request_attempts;
    bool enable_security;
    bool enable_session_control;
    bool enable_timing_params;
    NetProtocolType protocol;
} DiagConfig;

// Diagnostic callback types
typedef void (*DiagResponseCallback)(const DiagResponse* response, void* context);
typedef void (*DiagEventCallback)(uint32_t event_id, const uint8_t* data, uint32_t length, void* context);
typedef void (*DiagErrorCallback)(uint32_t error_code, const char* message, void* context);

// Core diagnostic functions
bool Diag_Init(const DiagConfig* config);
void Diag_Deinit(void);

bool Diag_StartSession(DiagSessionType session_type);
bool Diag_EndSession(void);
DiagSessionType Diag_GetCurrentSession(void);

bool Diag_SecurityAccess(DiagSecurityLevel level, const uint8_t* key, uint32_t key_length);
DiagSecurityLevel Diag_GetSecurityLevel(void);

bool Diag_SendRequest(const DiagMessage* request, DiagResponseCallback callback, void* context);
bool Diag_SendRequestSync(const DiagMessage* request, DiagResponse* response);

void Diag_RegisterEventCallback(DiagEventCallback callback, void* context);
void Diag_RegisterErrorCallback(DiagErrorCallback callback, void* context);

bool Diag_IsSessionActive(void);
bool Diag_IsSecurityActive(void);

uint32_t Diag_GetLastError(void);
const char* Diag_GetErrorString(uint32_t error_code);

#endif // CANT_DIAG_CORE_H 