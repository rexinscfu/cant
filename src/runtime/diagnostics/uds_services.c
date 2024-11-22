#include "uds_services.h"
#include <stdlib.h>
#include <string.h>
#include "../os/critical.h"
#include "../utils/timer.h"

#define UDS_MAX_DATA_LENGTH 4095
#define UDS_RESPONSE_OFFSET 0x40

struct UDSHandler {
    ISOTP* isotp;
    UDSConfig config;
    
    // Session management
    struct {
        UDSSessionType current;
        Timer timeout;
        void (*timeout_callback)(void);
    } session;
    
    // Security management
    struct {
        uint8_t level;
        bool locked;
        uint32_t seed;
    } security;
    
    // Response buffer
    struct {
        uint8_t data[UDS_MAX_DATA_LENGTH];
        size_t length;
    } response;
    
    CriticalSection critical;
};

// Service handler prototypes
static bool handle_session_control(UDSHandler* handler, const uint8_t* data, size_t length);
static bool handle_ecu_reset(UDSHandler* handler, const uint8_t* data, size_t length);
static bool handle_security_access(UDSHandler* handler, const uint8_t* data, size_t length);
static bool handle_tester_present(UDSHandler* handler, const uint8_t* data, size_t length);
static bool handle_read_data(UDSHandler* handler, const uint8_t* data, size_t length);
static bool handle_write_data(UDSHandler* handler, const uint8_t* data, size_t length);

// Helper functions
static void prepare_negative_response(UDSHandler* handler, uint8_t service_id, uint8_t nrc) {
    handler->response.data[0] = 0x7F;
    handler->response.data[1] = service_id;
    handler->response.data[2] = nrc;
    handler->response.length = 3;
}

static void prepare_positive_response(UDSHandler* handler, uint8_t service_id) {
    handler->response.data[0] = service_id + UDS_RESPONSE_OFFSET;
    handler->response.length = 1;
}

// Service implementations
static bool handle_session_control(UDSHandler* handler, const uint8_t* data, size_t length) {
    if (length != 2) {
        prepare_negative_response(handler, UDS_DIAGNOSTIC_SESSION_CONTROL, 0x13);
        return false;
    }
    
    UDSSessionType requested_session = data[1];
    switch (requested_session) {
        case UDS_SESSION_DEFAULT:
        case UDS_SESSION_PROGRAMMING:
        case UDS_SESSION_EXTENDED:
        case UDS_SESSION_SAFETY:
            handler->session.current = requested_session;
            timer_start(&handler->session.timeout, handler->config.session_timeout_ms);
            prepare_positive_response(handler, UDS_DIAGNOSTIC_SESSION_CONTROL);
            handler->response.data[1] = requested_session;
            handler->response.length = 2;
            return true;
            
        default:
            prepare_negative_response(handler, UDS_DIAGNOSTIC_SESSION_CONTROL, 0x12);
            return false;
    }
}

// ... (implement other service handlers)

UDSHandler* uds_create(ISOTP* isotp, const UDSConfig* config) {
    if (!isotp || !config) return NULL;
    
    UDSHandler* handler = calloc(1, sizeof(UDSHandler));
    if (!handler) return NULL;
    
    handler->isotp = isotp;
    memcpy(&handler->config, config, sizeof(UDSConfig));
    
    handler->session.current = UDS_SESSION_DEFAULT;
    init_critical(&handler->critical);
    
    return handler;
}

void uds_destroy(UDSHandler* handler) {
    if (!handler) return;
    destroy_critical(&handler->critical);
    free(handler);
}

void uds_process(UDSHandler* handler) {
    if (!handler) return;
    
    enter_critical(&handler->critical);
    
    // Check session timeout
    if (timer_expired(&handler->session.timeout) && 
        handler->session.current != UDS_SESSION_DEFAULT) {
        handler->session.current = UDS_SESSION_DEFAULT;
        if (handler->session.timeout_callback) {
            handler->session.timeout_callback();
        }
    }
    
    // Process received messages
    uint8_t buffer[UDS_MAX_DATA_LENGTH];
    size_t length;
    
    if (isotp_receive(handler->isotp, buffer, &length, 0)) {
        uint8_t service_id = buffer[0];
        bool handled = false;
        
        switch (service_id) {
            case UDS_DIAGNOSTIC_SESSION_CONTROL:
                handled = handle_session_control(handler, buffer, length);
                break;
            case UDS_ECU_RESET:
                handled = handle_ecu_reset(handler, buffer, length);
                break;
            case UDS_SECURITY_ACCESS:
                handled = handle_security_access(handler, buffer, length);
                break;
            case UDS_TESTER_PRESENT:
                handled = handle_tester_present(handler, buffer, length);
                break;
            case UDS_READ_DATA_BY_ID:
                handled = handle_read_data(handler, buffer, length);
                break;
            case UDS_WRITE_DATA_BY_ID:
                handled = handle_write_data(handler, buffer, length);
                break;
            default:
                prepare_negative_response(handler, service_id, 0x11);
                handled = false;
                break;
        }
        
        if (handler->response.length > 0) {
            isotp_transmit(handler->isotp, 
                         handler->response.data,
                         handler->response.length);
            handler->response.length = 0;
        }
    }
    
    exit_critical(&handler->critical);
}

bool uds_send_response(UDSHandler* handler, const uint8_t* data, size_t length) {
    if (!handler || !data || length == 0 || length > UDS_MAX_DATA_LENGTH) {
        return false;
    }
    
    return isotp_transmit(handler->isotp, data, length);
}

void uds_set_session_timeout_callback(UDSHandler* handler, void (*callback)(void)) {
    if (handler) {
        handler->session.timeout_callback = callback;
    }
} 