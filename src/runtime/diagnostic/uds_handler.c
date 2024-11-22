#include "uds_handler.h"
#include <string.h>
#include "../utils/timer.h"
#include "../os/critical.h"
#include "dtc_manager.h"
#include "event_handler.h"

// Internal state structure
typedef struct {
    UdsConfig config;
    struct {
        UdsSessionType current_session;
        Timer session_timer;
        Timer security_delay_timer;
        uint8_t security_attempt_count;
        uint8_t security_level;
        bool security_locked;
        bool initialized;
    } state;
    CriticalSection critical;
} UdsHandler;

static UdsHandler uds_handler;

// Security access seed
static uint32_t security_seed;
static uint32_t expected_key;

// Internal helper functions
static bool validate_service_access(UdsServiceId service_id, UdsSessionType session) {
    // Service access matrix
    static const struct {
        UdsServiceId sid;
        uint8_t allowed_sessions;  // Bitmap of allowed sessions
    } service_access[] = {
        {UDS_SID_DIAGNOSTIC_SESSION_CONTROL,     0x0F}, // All sessions
        {UDS_SID_ECU_RESET,                     0x0E}, // All except default
        {UDS_SID_SECURITY_ACCESS,               0x0E}, // All except default
        {UDS_SID_COMMUNICATION_CONTROL,         0x0C}, // Extended and safety
        {UDS_SID_TESTER_PRESENT,               0x0F}, // All sessions
        {UDS_SID_READ_DATA_BY_IDENTIFIER,      0x0F}, // All sessions
        {UDS_SID_WRITE_DATA_BY_IDENTIFIER,     0x0C}, // Extended and safety
        {UDS_SID_ROUTINE_CONTROL,              0x0E}, // All except default
        {UDS_SID_REQUEST_DOWNLOAD,             0x02}, // Programming only
        {UDS_SID_TRANSFER_DATA,                0x02}, // Programming only
        {UDS_SID_REQUEST_TRANSFER_EXIT,        0x02}, // Programming only
    };

    for (size_t i = 0; i < sizeof(service_access)/sizeof(service_access[0]); i++) {
        if (service_access[i].sid == service_id) {
            return (service_access[i].allowed_sessions & (1 << (session - 1))) != 0;
        }
    }
    return false;
}

static uint32_t calculate_key(uint32_t seed) {
    // Example key calculation algorithm (should be more complex in production)
    return ((seed ^ 0x55AA55AA) + 0x12345678) ^ 0xAA55AA55;
}

static bool process_diagnostic_session_control(const UdsMessage* request, UdsMessage* response) {
    if (request->length != 2) {
        return false;
    }

    UdsSessionType new_session = (UdsSessionType)request->sub_function;
    if (new_session < UDS_SESSION_DEFAULT || new_session > UDS_SESSION_SAFETY_SYSTEM) {
        return false;
    }

    UdsSessionType old_session = uds_handler.state.current_session;
    uds_handler.state.current_session = new_session;
    
    // Reset security on session change
    uds_handler.state.security_level = 0;
    uds_handler.state.security_locked = false;
    uds_handler.state.security_attempt_count = 0;

    if (uds_handler.config.session_change_callback) {
        uds_handler.config.session_change_callback(old_session, new_session);
    }

    // Prepare positive response
    response->service_id = request->service_id + 0x40;
    response->sub_function = request->sub_function;
    response->length = 2;

    return true;
}

static bool process_security_access(const UdsMessage* request, UdsMessage* response) {
    if (request->length < 2) {
        return false;
    }

    uint8_t security_level = request->sub_function;
    bool is_request_seed = (security_level & 0x01) == 1;

    if (uds_handler.state.security_locked) {
        if (!timer_expired(&uds_handler.state.security_delay_timer)) {
            return false;
        }
        uds_handler.state.security_locked = false;
        uds_handler.state.security_attempt_count = 0;
    }

    if (is_request_seed) {
        // Generate seed
        Timer timer;
        timer_init();
        security_seed = (uint32_t)timer.start_time ^ 0xA5A5A5A5;
        expected_key = calculate_key(security_seed);

        response->service_id = request->service_id + 0x40;
        response->sub_function = request->sub_function;
        response->data = (uint8_t*)&security_seed;
        response->length = 6;  // SubFunction + 4 bytes seed
        return true;
    } else {
        // Key verification
        if (request->length != 6) {  // SubFunction + 4 bytes key
            return false;
        }

        uint32_t received_key;
        memcpy(&received_key, &request->data[2], 4);

        if (received_key == expected_key) {
            uds_handler.state.security_level = security_level - 1;
            uds_handler.state.security_attempt_count = 0;

            if (uds_handler.config.security_callback) {
                uds_handler.config.security_callback(uds_handler.state.security_level, true);
            }

            response->service_id = request->service_id + 0x40;
            response->sub_function = request->sub_function;
            response->length = 2;
            return true;
        } else {
            uds_handler.state.security_attempt_count++;
            if (uds_handler.state.security_attempt_count >= uds_handler.config.security_attempt_limit) {
                uds_handler.state.security_locked = true;
                timer_start(&uds_handler.state.security_delay_timer, 
                           uds_handler.config.security_delay_ms);
            }

            if (uds_handler.config.security_callback) {
                uds_handler.config.security_callback(security_level - 1, false);
            }

            return false;
        }
    }
}

static bool process_ecu_reset(const UdsMessage* request, UdsMessage* response) {
    if (request->length != 2) {
        return false;
    }

    response->service_id = request->service_id + 0x40;
    response->sub_function = request->sub_function;
    response->length = 2;

    // Schedule reset after response is sent
    switch (request->sub_function) {
        case 0x01:  // Hard reset
            // Schedule hard reset
            break;
        case 0x02:  // Key off/on reset
            // Schedule key cycle reset
            break;
        case 0x03:  // Soft reset
            // Schedule soft reset
            break;
        default:
            return false;
    }

    return true;
}

static bool process_read_data_by_identifier(const UdsMessage* request, UdsMessage* response) {
    if (request->length < 3) {
        return false;
    }

    uint16_t data_identifier;
    memcpy(&data_identifier, &request->data[1], 2);

    // Buffer for read data (adjust size as needed)
    static uint8_t read_buffer[256];
    uint16_t read_length = 0;

    // Read data based on identifier
    switch (data_identifier) {
        case 0xF190:  // VIN
            // Read VIN
            strcpy((char*)read_buffer, "SAMPLE1234567890");
            read_length = 17;
            break;
            
        case 0xF19E:  // ECU Serial Number
            // Read ECU Serial
            strcpy((char*)read_buffer, "ECU123456");
            read_length = 10;
            break;
            
        case 0xF197:  // System Supplier ECU Hardware Number
            // Read Hardware Number
            strcpy((char*)read_buffer, "HW-V1.0");
            read_length = 7;
            break;
            
        case 0xF195:  // System Supplier ECU Software Number
            // Read Software Number
            strcpy((char*)read_buffer, "SW-V1.0");
            read_length = 7;
            break;
            
        default:
            return false;
    }

    response->service_id = request->service_id + 0x40;
    memcpy(response->data, &data_identifier, 2);
    memcpy(&response->data[2], read_buffer, read_length);
    response->length = 2 + read_length;

    return true;
}

bool UDS_Handler_Init(const UdsConfig* config) {
    if (!config) {
        return false;
    }

    enter_critical(&uds_handler.critical);

    memcpy(&uds_handler.config, config, sizeof(UdsConfig));
    
    uds_handler.state.current_session = UDS_SESSION_DEFAULT;
    uds_handler.state.security_level = 0;
    uds_handler.state.security_locked = false;
    uds_handler.state.security_attempt_count = 0;
    
    timer_init();
    init_critical(&uds_handler.critical);
    
    if (config->enable_session_timeout) {
        timer_start(&uds_handler.state.session_timer, config->s3_client_ms);
    }

    uds_handler.state.initialized = true;

    exit_critical(&uds_handler.critical);
    return true;
}

void UDS_Handler_DeInit(void) {
    enter_critical(&uds_handler.critical);
    memset(&uds_handler, 0, sizeof(UdsHandler));
    exit_critical(&uds_handler.critical);
}

UdsResponseCode UDS_Handler_ProcessRequest(const UdsMessage* request, UdsMessage* response) {
    if (!uds_handler.state.initialized || !request || !response) {
        return UDS_RESPONSE_GENERAL_REJECT;
    }

    enter_critical(&uds_handler.critical);

    // Update session timer
    if (uds_handler.config.enable_session_timeout) {
        timer_start(&uds_handler.state.session_timer, uds_handler.config.s3_client_ms);
    }

    // Validate service access
    if (!validate_service_access(request->service_id, uds_handler.state.current_session)) {
        exit_critical(&uds_handler.critical);
        return UDS_RESPONSE_SERVICE_NOT_SUPPORTED;
    }

    bool result = false;
    switch (request->service_id) {
        case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:
            result = process_diagnostic_session_control(request, response);
            break;
            
        case UDS_SID_SECURITY_ACCESS:
            result = process_security_access(request, response);
            break;
            
        case UDS_SID_ECU_RESET:
            result = process_ecu_reset(request, response);
            break;
            
        case UDS_SID_READ_DATA_BY_IDENTIFIER:
            result = process_read_data_by_identifier(request, response);
            break;
            
        case UDS_SID_TESTER_PRESENT:
            response->service_id = request->service_id + 0x40;
            response->sub_function = request->sub_function;
            response->length = 2;
            result = true;
            break;
            
        default:
            result = false;
            break;
    }

    exit_critical(&uds_handler.critical);
    return result ? UDS_RESPONSE_POSITIVE : UDS_RESPONSE_SERVICE_NOT_SUPPORTED;
}

bool UDS_Handler_IsSessionActive(UdsSessionType session) {
    if (!uds_handler.state.initialized) {
        return false;
    }

    enter_critical(&uds_handler.critical);
    bool is_active = (uds_handler.state.current_session == session);
    
    if (is_active && uds_handler.config.enable_session_timeout) {
        is_active = !timer_expired(&uds_handler.state.session_timer);
    }
    
    exit_critical(&uds_handler.critical);
    return is_active;
}

bool UDS_Handler_IsSecurityUnlocked(uint8_t level) {
    if (!uds_handler.state.initialized) {
        return false;
    }

    enter_critical(&uds_handler.critical);
    bool is_unlocked = (uds_handler.state.security_level == level && !uds_handler.state.security_locked);
    exit_critical(&uds_handler.critical);
    
    return is_unlocked;
}

void UDS_Handler_ProcessTimeout(void) {
    if (!uds_handler.state.initialized) {
        return;
    }

    enter_critical(&uds_handler.critical);

    if (uds_handler.config.enable_session_timeout) {
        if (timer_expired(&uds_handler.state.session_timer) && 
            uds_handler.state.current_session != UDS_SESSION_DEFAULT) {
            
            UdsSessionType old_session = uds_handler.state.current_session;
            uds_handler.state.current_session = UDS_SESSION_DEFAULT;
            uds_handler.state.security_level = 0;
            
            if (uds_handler.config.session_change_callback) {
                uds_handler.config.session_change_callback(old_session, UDS_SESSION_DEFAULT);
            }
        }
    }

    exit_critical(&uds_handler.critical);
}

bool UDS_Handler_SendResponse(const UdsMessage* response) {
    if (!uds_handler.state.initialized || !response) {
        return false;
    }

    // Implementation depends on the communication protocol (CAN, Ethernet, etc.)
    // Here's a basic example structure:
    
    uint8_t response_buffer[256];
    uint16_t total_length = 0;

    // Format response
    response_buffer[total_length++] = response->service_id;
    if (response->length > 1) {
        response_buffer[total_length++] = response->sub_function;
    }
    
    if (response->length > 2 && response->data) {
        memcpy(&response_buffer[total_length], response->data, response->length - 2);
        total_length += response->length - 2;
    }

    // Send response using platform-specific communication
    // return platform_send_diagnostic_response(response_buffer, total_length);
    return true; // Placeholder
}

bool UDS_Handler_SendNegativeResponse(UdsServiceId service_id, UdsResponseCode response_code) {
    if (!uds_handler.state.initialized) {
        return false;
    }

    uint8_t response_buffer[3];
    response_buffer[0] = 0x7F;  // Negative response ID
    response_buffer[1] = service_id;
    response_buffer[2] = response_code;

    // Send negative response using platform-specific communication
    // return platform_send_diagnostic_response(response_buffer, 3);
    return true; // Placeholder
}

void UDS_Handler_ResetSession(void) {
    if (!uds_handler.state.initialized) {
        return;
    }

    enter_critical(&uds_handler.critical);

    UdsSessionType old_session = uds_handler.state.current_session;
    uds_handler.state.current_session = UDS_SESSION_DEFAULT;
    uds_handler.state.security_level = 0;
    uds_handler.state.security_locked = false;
    uds_handler.state.security_attempt_count = 0;

    if (uds_handler.config.session_change_callback) {
        uds_handler.config.session_change_callback(old_session, UDS_SESSION_DEFAULT);
    }

    exit_critical(&uds_handler.critical);
}

bool UDS_Handler_IsServiceAllowed(UdsServiceId service_id) {
    if (!uds_handler.state.initialized) {
        return false;
    }

    enter_critical(&uds_handler.critical);
    bool is_allowed = validate_service_access(service_id, uds_handler.state.current_session);
    exit_critical(&uds_handler.critical);

    return is_allowed;
}

uint32_t UDS_Handler_GetSessionTimeout(void) {
    if (!uds_handler.state.initialized || !uds_handler.config.enable_session_timeout) {
        return 0;
    }

    enter_critical(&uds_handler.critical);
    uint32_t remaining = timer_remaining(&uds_handler.state.session_timer);
    exit_critical(&uds_handler.critical);

    return remaining;
}

void UDS_Handler_UpdateS3Timer(void) {
    if (!uds_handler.state.initialized || !uds_handler.config.enable_session_timeout) {
        return;
    }

    enter_critical(&uds_handler.critical);
    timer_start(&uds_handler.state.session_timer, uds_handler.config.s3_client_ms);
    exit_critical(&uds_handler.critical);
}

// Additional helper functions for internal use

static void reset_security_access(void) {
    uds_handler.state.security_level = 0;
    uds_handler.state.security_locked = false;
    uds_handler.state.security_attempt_count = 0;
    security_seed = 0;
    expected_key = 0;
}

static bool validate_timing_parameters(void) {
    return (uds_handler.config.p2_server_max_ms > 0 &&
            uds_handler.config.p2_star_server_max_ms > uds_handler.config.p2_server_max_ms &&
            uds_handler.config.s3_client_ms > 0);
}

static bool is_response_pending(void) {
    // Check if we need to send a pending response
    // This would be used for long-running operations
    return false; // Placeholder
}

static void handle_session_timeout(void) {
    if (uds_handler.state.current_session != UDS_SESSION_DEFAULT) {
        UdsSessionType old_session = uds_handler.state.current_session;
        uds_handler.state.current_session = UDS_SESSION_DEFAULT;
        reset_security_access();
        
        if (uds_handler.config.session_change_callback) {
            uds_handler.config.session_change_callback(old_session, UDS_SESSION_DEFAULT);
        }
    }
}

