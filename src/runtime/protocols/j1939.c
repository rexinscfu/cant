#include "j1939.h"
#include <stdlib.h>
#include <string.h>
#include "../os/critical.h"
#include "../utils/timer.h"

#define J1939_MAX_PACKET_SIZE 1785
#define J1939_BROADCAST_ADDRESS 255

struct J1939Handler {
    CANDriver* can_driver;
    J1939Config config;
    
    // Address management
    struct {
        uint8_t current_address;
        bool address_claimed;
        Timer claim_timer;
        void (*address_changed_callback)(uint8_t);
    } address_mgmt;
    
    // Transport management
    struct {
        uint8_t buffer[J1939_MAX_PACKET_SIZE];
        size_t length;
        size_t offset;
        uint32_t pgn;
        uint8_t source;
        Timer timeout;
        bool in_progress;
    } transport;
    
    CriticalSection critical;
};

// Helper functions
static uint32_t compose_can_id(uint8_t priority, uint32_t pgn, uint8_t source) {
    return ((uint32_t)priority << 26) | (pgn << 8) | source;
}

static void decompose_can_id(uint32_t can_id, uint8_t* priority, 
                           uint32_t* pgn, uint8_t* source) {
    if (priority) *priority = (can_id >> 26) & 0x07;
    if (pgn) *pgn = (can_id >> 8) & 0x3FFFF;
    if (source) *source = can_id & 0xFF;
}

static bool send_address_claim(J1939Handler* handler) {
    J1939Message msg = {
        .pgn = J1939_PGN_ADDRESS_CLAIMED,
        .priority = 6,
        .source_address = handler->address_mgmt.current_address,
        .destination_address = J1939_BROADCAST_ADDRESS,
        .length = 8
    };
    
    memcpy(msg.data, handler->config.name, 8);
    return j1939_transmit(handler, &msg);
}

// Transport protocol handling
static bool start_transport_session(J1939Handler* handler, const J1939Message* message) {
    if (handler->transport.in_progress) return false;
    
    handler->transport.length = message->length;
    handler->transport.offset = 0;
    handler->transport.pgn = message->pgn;
    handler->transport.source = message->source_address;
    handler->transport.in_progress = true;
    timer_start(&handler->transport.timeout, handler->config.request_timeout_ms);
    
    return true;
}

static void process_transport_packet(J1939Handler* handler, const CANFrame* frame) {
    // Process transport protocol packets (BAM or RTS/CTS)
    uint8_t tp_cmd = frame->data[0];
    switch (tp_cmd) {
        case 0x20:  // BAM
            // Handle broadcast announce message
            break;
        case 0x21:  // RTS
            // Handle request to send
            break;
        case 0x23:  // CTS
            // Handle clear to send
            break;
        case 0xFF:  // Connection abort
            handler->transport.in_progress = false;
            break;
    }
}

J1939Handler* j1939_create(CANDriver* can_driver, const J1939Config* config) {
    if (!can_driver || !config) return NULL;
    
    J1939Handler* handler = calloc(1, sizeof(J1939Handler));
    if (!handler) return NULL;
    
    handler->can_driver = can_driver;
    memcpy(&handler->config, config, sizeof(J1939Config));
    
    handler->address_mgmt.current_address = config->address;
    init_critical(&handler->critical);
    
    return handler;
}

void j1939_destroy(J1939Handler* handler) {
    if (!handler) return;
    destroy_critical(&handler->critical);
    free(handler);
}

bool j1939_transmit(J1939Handler* handler, const J1939Message* message) {
    if (!handler || !message) return false;
    
    enter_critical(&handler->critical);
    
    bool result = false;
    if (message->length <= 8) {
        // Single frame transmission
        CANFrame frame = {
            .id = compose_can_id(message->priority, 
                               message->pgn, 
                               handler->address_mgmt.current_address),
            .is_extended = true,
            .dlc = message->length
        };
        memcpy(frame.data, message->data, message->length);
        result = can_transmit(handler->can_driver, &frame, 100);
    } else {
        // Start transport protocol
        result = start_transport_session(handler, message);
    }
    
    exit_critical(&handler->critical);
    return result;
}

bool j1939_receive(J1939Handler* handler, J1939Message* message, uint32_t timeout_ms) {
    if (!handler || !message) return false;
    
    CANFrame frame;
    if (!can_receive(handler->can_driver, &frame, timeout_ms)) {
        return false;
    }
    
    decompose_can_id(frame.id, 
                    &message->priority,
                    &message->pgn,
                    &message->source_address);
    
    message->length = frame.dlc;
    memcpy(message->data, frame.data, frame.dlc);
    
    return true;
}

void j1939_process(J1939Handler* handler) {
    if (!handler) return;
    
    enter_critical(&handler->critical);
    
    // Process address claiming
    if (handler->config.support_address_claim && 
        !handler->address_mgmt.address_claimed) {
        if (timer_expired(&handler->address_mgmt.claim_timer)) {
            send_address_claim(handler);
            timer_start(&handler->address_mgmt.claim_timer, 1250);  // 1.25s
        }
    }
    
    // Process received messages
    CANFrame frame;
    while (can_receive(handler->can_driver, &frame, 0)) {
        if ((frame.id >> 8) == J1939_PGN_ADDRESS_CLAIMED) {
            // Handle address claim messages
            if (frame.data[7] < handler->address_mgmt.current_address) {
                // Lost address claim
                handler->address_mgmt.current_address++;
                if (handler->address_mgmt.address_changed_callback) {
                    handler->address_mgmt.address_changed_callback(
                        handler->address_mgmt.current_address);
                }
                handler->address_mgmt.address_claimed = false;
            }
        } else if (frame.dlc == 8) {
            // Handle transport protocol
            process_transport_packet(handler, &frame);
        }
    }
    
    exit_critical(&handler->critical);
}

bool j1939_claim_address(J1939Handler* handler) {
    if (!handler) return false;
    
    enter_critical(&handler->critical);
    handler->address_mgmt.address_claimed = false;
    bool result = send_address_claim(handler);
    timer_start(&handler->address_mgmt.claim_timer, 1250);
    exit_critical(&handler->critical);
    
    return result;
}

bool j1939_request_pgn(J1939Handler* handler, uint32_t pgn, uint8_t destination) {
    if (!handler) return false;
    
    J1939Message msg = {
        .pgn = J1939_PGN_REQUEST,
        .priority = 6,
        .source_address = handler->address_mgmt.current_address,
        .destination_address = destination,
        .length = 3
    };
    
    msg.data[0] = pgn & 0xFF;
    msg.data[1] = (pgn >> 8) & 0xFF;
    msg.data[2] = (pgn >> 16) & 0xFF;
    
    return j1939_transmit(handler, &msg);
}

void j1939_set_address_changed_callback(J1939Handler* handler, 
                                      void (*callback)(uint8_t)) {
    if (handler) {
        handler->address_mgmt.address_changed_callback = callback;
    }
} 