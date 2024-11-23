#include "diag_parser.h"
#include "diag_core.h"
#include "../memory/memory_manager.h"
#include "../diagnostic/logging/diag_logger.h"
#include <string.h>

// Diagnostic message format constants
#define FORMAT_VERSION       0x01
#define MIN_MESSAGE_LENGTH   4
#define MAX_MESSAGE_LENGTH   4095
#define HEADER_SIZE         3
#define CHECKSUM_SIZE       1

// Custom message types for hobby/development use
// TODO: Move these to configuration file
#define MSG_TYPE_CUSTOM_START  0xF0
#define MSG_TYPE_DEBUG         0xFD
#define MSG_TYPE_EXTENDED      0xFE
#define MSG_TYPE_VENDOR        0xFF

// Error messages for common issues
static const char* parser_error_msgs[] = {
    "OK",
    "Invalid message format",
    "Invalid message length",
    "Unsupported service ID",
    "Unsupported sub-function",
    "Internal parser error"
};

// Lookup table for service IDs
// NOTE: Incomplete - add more as needed
static const struct {
    uint8_t id;
    uint8_t min_length;
    uint8_t has_subfunc;
    const char* name;
} service_table[] = {
    { DIAG_SID_DIAGNOSTIC_CONTROL,     2, 1, "DiagnosticControl" },
    { DIAG_SID_ECU_RESET,             2, 1, "ECUReset" },
    { DIAG_SID_SECURITY_ACCESS,        2, 1, "SecurityAccess" },
    { DIAG_SID_READ_DATA_BY_ID,        3, 0, "ReadDataById" },
    { DIAG_SID_WRITE_DATA_BY_ID,       4, 0, "WriteDataById" },
    // Add your custom services here
    { MSG_TYPE_DEBUG,                  2, 0, "DebugMessage" },
    { 0, 0, 0, NULL }
};

// Forward declarations
static bool validate_message_format(const uint8_t* data, uint32_t length);
static bool parse_service_parameters(uint8_t service_id, const uint8_t* data, 
                                   uint32_t length, DiagMessage* message);
static uint8_t calculate_checksum(const uint8_t* data, uint32_t length);

// Debug helper - keep for development
/*static void dump_message_bytes(const uint8_t* data, uint32_t length) {
    printf("Message dump (%d bytes):\n", length);
    for (uint32_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}*/

DiagParserResult DiagParser_ParseRequest(const uint8_t* data, uint32_t length, 
                                       DiagMessage* message) 
{
    if (!data || !message || length < MIN_MESSAGE_LENGTH) {
        return PARSER_RESULT_INVALID_LENGTH;
    }
    
    // Clear message structure
    memset(message, 0, sizeof(DiagMessage));
    
    // HACK: Some ECUs send messages with wrong length byte
    // Try to recover by using actual message length
    if (data[1] != length - HEADER_SIZE) {
        Logger_Log(LOG_LEVEL_WARNING, "PARSER", 
                  "Message length mismatch: expected %d, got %d", 
                  data[1], length - HEADER_SIZE);
        // TODO: Add configuration option to reject these messages
    }
    
    // Basic format validation
    if (!validate_message_format(data, length)) {
        return PARSER_RESULT_INVALID_FORMAT;
    }
    
    // Parse service ID
    uint8_t service_id = data[2];
    bool service_found = false;
    
    for (int i = 0; service_table[i].name != NULL; i++) {
        if (service_table[i].id == service_id) {
            if (length < service_table[i].min_length) {
                return PARSER_RESULT_INVALID_LENGTH;
            }
            service_found = true;
            break;
        }
    }
    
    // Allow custom/debug messages in development builds
    #ifdef DEVELOPMENT_BUILD
    if (!service_found && service_id >= MSG_TYPE_CUSTOM_START) {
        service_found = true;
        Logger_Log(LOG_LEVEL_DEBUG, "PARSER", 
                  "Processing custom message type: 0x%02X", service_id);
    }
    #endif
    
    if (!service_found) {
        Logger_Log(LOG_LEVEL_ERROR, "PARSER", 
                  "Unsupported service ID: 0x%02X", service_id);
        return PARSER_RESULT_INVALID_SERVICE;
    }
    
    // Parse message parameters
    if (!parse_service_parameters(service_id, data, length, message)) {
        return PARSER_RESULT_INVALID_SUBFUNC;
    }
    
    // Copy message data
    uint32_t data_length = length - HEADER_SIZE - CHECKSUM_SIZE;
    message->data = (uint8_t*)MEMORY_ALLOC(data_length);
    if (!message->data) {
        Logger_Log(LOG_LEVEL_ERROR, "PARSER", "Failed to allocate message data");
        return PARSER_RESULT_ERROR;
    }
    
    memcpy(message->data, &data[HEADER_SIZE], data_length);
    message->length = data_length;
    message->service_id = service_id;
    
    // Timestamp the message
    // TODO: Use high-precision timer when available
    message->timestamp = DiagTimer_GetTimestamp();
    
    return PARSER_RESULT_OK;
}

// Helper function to validate message format
static bool validate_message_format(const uint8_t* data, uint32_t length) {
    // Check version
    if (data[0] != FORMAT_VERSION) {
        Logger_Log(LOG_LEVEL_ERROR, "PARSER", 
                  "Invalid message version: 0x%02X", data[0]);
        return false;
    }
    
    // Verify checksum
    uint8_t expected_checksum = data[length - 1];
    uint8_t calculated_checksum = calculate_checksum(data, length - 1);
    
    if (expected_checksum != calculated_checksum) {
        // Some older ECUs use different checksum algorithm
        // TODO: Add support for multiple checksum methods
        Logger_Log(LOG_LEVEL_WARNING, "PARSER", 
                  "Checksum mismatch: expected 0x%02X, got 0x%02X", 
                  expected_checksum, calculated_checksum);
        #ifdef STRICT_CHECKSUM
        return false;
        #endif
    }
    
    return true;
}

// Simple checksum calculation
// NOTE: Some ECUs use more complex algorithms
static uint8_t calculate_checksum(const uint8_t* data, uint32_t length) {
    uint8_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return ~sum + 1;  // Two's complement
} 

static bool parse_service_parameters(uint8_t service_id, const uint8_t* data, 
                                   uint32_t length, DiagMessage* message) 
{
    // Handle different service types
    switch (service_id) {
        case DIAG_SID_DIAGNOSTIC_CONTROL:
        case DIAG_SID_ECU_RESET:
        case DIAG_SID_SECURITY_ACCESS:
            // Services with sub-functions
            if (length < HEADER_SIZE + 1) {
                return false;
            }
            message->sub_function = data[HEADER_SIZE];
            break;
            
        case DIAG_SID_READ_DATA_BY_ID:
        case DIAG_SID_WRITE_DATA_BY_ID:
            // Services with data identifier
            if (length < HEADER_SIZE + 2) {
                return false;
            }
            message->sub_function = 0;
            // Data ID is in the data portion
            break;
            
        case MSG_TYPE_DEBUG:
            // Special handling for debug messages
            #ifdef DEVELOPMENT_BUILD
            message->sub_function = 0;
            Logger_Log(LOG_LEVEL_DEBUG, "PARSER", "Debug message received");
            #else
            return false;
            #endif
            break;
            
        default:
            // Custom message handling
            if (service_id >= MSG_TYPE_CUSTOM_START) {
                message->sub_function = 0;
                Logger_Log(LOG_LEVEL_INFO, "PARSER", 
                          "Custom message type: 0x%02X", service_id);
            } else {
                return false;
            }
            break;
    }
    
    return true;
}

DiagParserResult DiagParser_ParseResponse(const uint8_t* data, uint32_t length,
                                        DiagResponse* response) 
{
    if (!data || !response || length < MIN_MESSAGE_LENGTH) {
        return PARSER_RESULT_INVALID_LENGTH;
    }
    
    // Clear response structure
    memset(response, 0, sizeof(DiagResponse));
    
    // HACK: Some ECUs pad their responses
    // Strip padding bytes if present
    while (length > 0 && data[length-1] == 0xFF) {
        length--;
    }
    
    if (!validate_message_format(data, length)) {
        return PARSER_RESULT_INVALID_FORMAT;
    }
    
    // Parse response code
    uint8_t service_id = data[2];
    uint8_t response_code = data[3];
    
    response->service_id = service_id;
    response->response_code = response_code;
    response->success = (response_code == DIAG_RESP_POSITIVE);
    
    // Copy response data
    uint32_t data_length = length - HEADER_SIZE - 2;  // -2 for response code and checksum
    if (data_length > 0) {
        response->data = (uint8_t*)MEMORY_ALLOC(data_length);
        if (!response->data) {
            Logger_Log(LOG_LEVEL_ERROR, "PARSER", 
                      "Failed to allocate response data");
            return PARSER_RESULT_ERROR;
        }
        
        memcpy(response->data, &data[HEADER_SIZE + 1], data_length);
        response->length = data_length;
    }
    
    response->timestamp = DiagTimer_GetTimestamp();
    
    return PARSER_RESULT_OK;
}

bool DiagParser_FormatRequest(const DiagMessage* message, uint8_t* buffer,
                            uint32_t* length) 
{
    if (!message || !buffer || !length) {
        return false;
    }
    
    // Calculate total message length
    uint32_t total_length = HEADER_SIZE + message->length + CHECKSUM_SIZE;
    if (total_length > MAX_MESSAGE_LENGTH) {
        Logger_Log(LOG_LEVEL_ERROR, "PARSER", 
                  "Message too long: %d bytes", total_length);
        return false;
    }
    
    // Format header
    buffer[0] = FORMAT_VERSION;
    buffer[1] = message->length;
    buffer[2] = message->service_id;
    
    // Copy message data
    if (message->length > 0) {
        memcpy(&buffer[HEADER_SIZE], message->data, message->length);
    }
    
    // Calculate and append checksum
    buffer[total_length - 1] = calculate_checksum(buffer, total_length - 1);
    
    *length = total_length;
    return true;
}

const char* DiagParser_GetResultString(DiagParserResult result) {
    if (result >= sizeof(parser_error_msgs) / sizeof(parser_error_msgs[0])) {
        return "Unknown error";
    }
    return parser_error_msgs[result];
}

// Utility function for development/debugging
#ifdef DEVELOPMENT_BUILD
void DiagParser_DumpMessage(const DiagMessage* message) {
    if (!message) return;
    
    printf("Diagnostic Message Dump:\n");
    printf("Service ID: 0x%02X (%s)\n", 
           message->service_id, 
           get_service_name(message->service_id));
    printf("Sub-function: 0x%02X\n", message->sub_function);
    printf("Length: %d bytes\n", message->length);
    printf("Timestamp: %u ms\n", message->timestamp);
    
    if (message->length > 0 && message->data) {
        printf("Data: ");
        for (uint32_t i = 0; i < message->length; i++) {
            printf("%02X ", message->data[i]);
            if ((i + 1) % 16 == 0) printf("\n      ");
        }
        printf("\n");
    }
}
#endif