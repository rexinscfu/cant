#include "diag_protocol.h"
#include "diag_timer.h"
#include "../network/net_protocol.h"
#include "../memory/memory_manager.h"
#include "../diagnostic/logging/diag_logger.h"
#include <string.h>

#define MAX_PROTOCOL_HANDLERS 4
#define MAX_MESSAGE_SIZE 4096

typedef struct {
    NetProtocolType type;
    DiagProtocolHandler handler;
    bool active;
} ProtocolEntry;

typedef struct {
    ProtocolEntry handlers[MAX_PROTOCOL_HANDLERS];
    uint32_t handler_count;
    NetProtocolType current_protocol;
    uint8_t* buffer;
    bool initialized;
} ProtocolManager;

static ProtocolManager proto_mgr;

// CAN Protocol Handler Implementation
static bool can_init(void) {
    return Net_InitCAN();
}

static void can_deinit(void) {
    Net_DeinitCAN();
}

static bool can_send_message(const DiagMessage* message) {
    if (!message || !message->data) {
        return false;
    }
    
    NetCANFrame frame = {
        .id = message->id,
        .length = message->length,
        .data = message->data
    };
    
    return Net_SendCAN(&frame);
}

static bool can_receive_message(DiagMessage* message) {
    if (!message) {
        return false;
    }
    
    NetCANFrame frame;
    if (!Net_ReceiveCAN(&frame)) {
        return false;
    }
    
    message->id = frame.id;
    message->length = frame.length;
    memcpy(message->data, frame.data, frame.length);
    
    return true;
}

static bool can_start_session(DiagSessionType session) {
    uint8_t data[2] = {DIAG_SID_DIAGNOSTIC_CONTROL, session};
    DiagMessage msg = {
        .id = 0x7DF,
        .service_id = DIAG_SID_DIAGNOSTIC_CONTROL,
        .sub_function = session,
        .data = data,
        .length = 2
    };
    
    return can_send_message(&msg);
}

static bool can_end_session(void) {
    uint8_t data[2] = {DIAG_SID_DIAGNOSTIC_CONTROL, DIAG_SESSION_DEFAULT};
    DiagMessage msg = {
        .id = 0x7DF,
        .service_id = DIAG_SID_DIAGNOSTIC_CONTROL,
        .sub_function = DIAG_SESSION_DEFAULT,
        .data = data,
        .length = 2
    };
    
    return can_send_message(&msg);
}

static bool can_security_access(DiagSecurityLevel level, const uint8_t* key, uint32_t length) {
    if (!key || length == 0) {
        return false;
    }
    
    uint8_t* data = MEMORY_ALLOC(length + 2);
    if (!data) {
        return false;
    }
    
    data[0] = DIAG_SID_SECURITY_ACCESS;
    data[1] = level;
    memcpy(data + 2, key, length);
    
    DiagMessage msg = {
        .id = 0x7DF,
        .service_id = DIAG_SID_SECURITY_ACCESS,
        .sub_function = level,
        .data = data,
        .length = length + 2
    };
    
    bool result = can_send_message(&msg);
    MEMORY_FREE(data);
    
    return result;
}

static bool can_tester_present(void) {
    uint8_t data[1] = {DIAG_SID_TESTER_PRESENT};
    DiagMessage msg = {
        .id = 0x7DF,
        .service_id = DIAG_SID_TESTER_PRESENT,
        .data = data,
        .length = 1
    };
    
    return can_send_message(&msg);
}

static void can_handle_timeout(void) {
    // Reset CAN controller if needed
    Net_ResetCAN();
}

// Protocol Manager Implementation
bool DiagProtocol_Init(NetProtocolType protocol) {
    if (proto_mgr.initialized) {
        return false;
    }
    
    memset(&proto_mgr, 0, sizeof(ProtocolManager));
    
    // Allocate message buffer
    proto_mgr.buffer = MEMORY_ALLOC(MAX_MESSAGE_SIZE);
    if (!proto_mgr.buffer) {
        return false;
    }
    
    // Register CAN protocol handler
    DiagProtocolHandler can_handler = {
        .init = can_init,
        .deinit = can_deinit,
        .send_message = can_send_message,
        .receive_message = can_receive_message,
        .start_session = can_start_session,
        .end_session = can_end_session,
        .security_access = can_security_access,
        .tester_present = can_tester_present,
        .handle_timeout = can_handle_timeout
    };
    
    proto_mgr.handlers[proto_mgr.handler_count].type = NET_PROTOCOL_CAN;
    proto_mgr.handlers[proto_mgr.handler_count].handler = can_handler;
    proto_mgr.handler_count++;
    
    // Initialize requested protocol
    for (uint32_t i = 0; i < proto_mgr.handler_count; i++) {
        if (proto_mgr.handlers[i].type == protocol) {
            if (!proto_mgr.handlers[i].handler.init()) {
                MEMORY_FREE(proto_mgr.buffer);
                return false;
            }
            proto_mgr.handlers[i].active = true;
            proto_mgr.current_protocol = protocol;
            break;
        }
    }
    
    proto_mgr.initialized = true;
    return true;
} 