#ifndef CANT_UDS_HANDLER_H
#define CANT_UDS_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

// UDS Service IDs
typedef enum {
    UDS_SID_DIAGNOSTIC_SESSION_CONTROL     = 0x10,
    UDS_SID_ECU_RESET                     = 0x11,
    UDS_SID_SECURITY_ACCESS               = 0x27,
    UDS_SID_COMMUNICATION_CONTROL         = 0x28,
    UDS_SID_TESTER_PRESENT               = 0x3E,
    UDS_SID_ACCESS_TIMING_PARAMETER      = 0x83,
    UDS_SID_SECURED_DATA_TRANSMISSION    = 0x84,
    UDS_SID_CONTROL_DTC_SETTING         = 0x85,
    UDS_SID_RESPONSE_ON_EVENT           = 0x86,
    UDS_SID_LINK_CONTROL                = 0x87,
    UDS_SID_READ_DATA_BY_IDENTIFIER     = 0x22,
    UDS_SID_READ_MEMORY_BY_ADDRESS      = 0x23,
    UDS_SID_READ_SCALING_DATA_BY_IDENTIFIER = 0x24,
    UDS_SID_READ_DATA_BY_PERIODIC_IDENTIFIER = 0x2A,
    UDS_SID_DYNAMICALLY_DEFINE_DATA_IDENTIFIER = 0x2C,
    UDS_SID_WRITE_DATA_BY_IDENTIFIER    = 0x2E,
    UDS_SID_WRITE_MEMORY_BY_ADDRESS     = 0x3D,
    UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION = 0x14,
    UDS_SID_READ_DTC_INFORMATION        = 0x19,
    UDS_SID_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER = 0x2F,
    UDS_SID_ROUTINE_CONTROL             = 0x31,
    UDS_SID_REQUEST_DOWNLOAD            = 0x34,
    UDS_SID_REQUEST_UPLOAD              = 0x35,
    UDS_SID_TRANSFER_DATA               = 0x36,
    UDS_SID_REQUEST_TRANSFER_EXIT       = 0x37
} UdsServiceId;

// UDS Session Types
typedef enum {
    UDS_SESSION_DEFAULT                 = 0x01,
    UDS_SESSION_PROGRAMMING            = 0x02,
    UDS_SESSION_EXTENDED_DIAGNOSTIC    = 0x03,
    UDS_SESSION_SAFETY_SYSTEM          = 0x04
} UdsSessionType;

// UDS Response Codes
typedef enum {
    UDS_RESPONSE_POSITIVE              = 0x00,
    UDS_RESPONSE_GENERAL_REJECT        = 0x10,
    UDS_RESPONSE_SERVICE_NOT_SUPPORTED = 0x11,
    UDS_RESPONSE_SUBFUNCTION_NOT_SUPPORTED = 0x12,
    UDS_RESPONSE_INCORRECT_LENGTH      = 0x13,
    UDS_RESPONSE_CONDITIONS_NOT_CORRECT = 0x22,
    UDS_RESPONSE_REQUEST_SEQUENCE_ERROR = 0x24,
    UDS_RESPONSE_REQUEST_OUT_OF_RANGE  = 0x31,
    UDS_RESPONSE_SECURITY_ACCESS_DENIED = 0x33,
    UDS_RESPONSE_INVALID_KEY           = 0x35,
    UDS_RESPONSE_EXCEEDED_NUMBER_OF_ATTEMPTS = 0x36,
    UDS_RESPONSE_REQUIRED_TIME_DELAY_NOT_EXPIRED = 0x37,
    UDS_RESPONSE_UPLOAD_DOWNLOAD_NOT_ACCEPTED = 0x70,
    UDS_RESPONSE_TRANSFER_DATA_SUSPENDED = 0x71,
    UDS_RESPONSE_GENERAL_PROGRAMMING_FAILURE = 0x72,
    UDS_RESPONSE_WRONG_BLOCK_SEQUENCE_COUNTER = 0x73,
    UDS_RESPONSE_RESPONSE_PENDING     = 0x78
} UdsResponseCode;

// UDS Message Structure
typedef struct {
    UdsServiceId service_id;
    uint8_t sub_function;
    uint8_t* data;
    uint16_t length;
} UdsMessage;

// UDS Configuration
typedef struct {
    uint32_t p2_server_max_ms;
    uint32_t p2_star_server_max_ms;
    uint32_t s3_client_ms;
    uint32_t security_delay_ms;
    uint8_t security_attempt_limit;
    bool enable_session_timeout;
    void (*session_change_callback)(UdsSessionType old_session, UdsSessionType new_session);
    void (*security_callback)(uint8_t level, bool access_granted);
} UdsConfig;

// UDS Handler API
bool UDS_Handler_Init(const UdsConfig* config);
void UDS_Handler_DeInit(void);
UdsResponseCode UDS_Handler_ProcessRequest(const UdsMessage* request, UdsMessage* response);
bool UDS_Handler_IsSessionActive(UdsSessionType session);
bool UDS_Handler_IsSecurityUnlocked(uint8_t level);
void UDS_Handler_ProcessTimeout(void);
bool UDS_Handler_SendResponse(const UdsMessage* response);
bool UDS_Handler_SendNegativeResponse(UdsServiceId service_id, UdsResponseCode response_code);
void UDS_Handler_ResetSession(void);
bool UDS_Handler_IsServiceAllowed(UdsServiceId service_id);
uint32_t UDS_Handler_GetSessionTimeout(void);
void UDS_Handler_UpdateS3Timer(void);

#endif // CANT_UDS_HANDLER_H 