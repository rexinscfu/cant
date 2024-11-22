#include "isotp.h"
#include <stdlib.h>
#include <string.h>
#include "../os/critical.h"
#include "../utils/timer.h"

#define ISOTP_MAX_PAYLOAD 4095
#define ISOTP_MAX_FRAME_SIZE 8

struct ISOTP {
    CANDriver* can_driver;
    ISOTPConfig config;
    
    // Transmission state
    struct {
        uint8_t buffer[ISOTP_MAX_PAYLOAD];
        size_t length;
        size_t offset;
        uint8_t sequence;
        Timer timer;
        bool waiting_fc;
        uint8_t block_counter;
    } tx_state;
    
    // Reception state
    struct {
        uint8_t buffer[ISOTP_MAX_PAYLOAD];
        size_t length;
        size_t offset;
        uint8_t sequence;
        Timer timer;
        bool receiving_multi;
    } rx_state;
    
    CriticalSection critical;
};

static bool send_single_frame(ISOTP* isotp, const uint8_t* data, size_t length) {
    CANFrame frame = {
        .id = isotp->config.tx_id,
        .is_extended = false,
        .dlc = length + 1
    };
    
    frame.data[0] = (ISOTP_SINGLE_FRAME << 4) | length;
    memcpy(&frame.data[1], data, length);
    
    return can_transmit(isotp->can_driver, &frame, isotp->config.timeout_ms);
}

static bool send_first_frame(ISOTP* isotp, const uint8_t* data, size_t length) {
    CANFrame frame = {
        .id = isotp->config.tx_id,
        .is_extended = false,
        .dlc = 8
    };
    
    frame.data[0] = (ISOTP_FIRST_FRAME << 4) | ((length >> 8) & 0x0F);
    frame.data[1] = length & 0xFF;
    memcpy(&frame.data[2], data, 6);
    
    isotp->tx_state.length = length;
    isotp->tx_state.offset = 6;
    isotp->tx_state.sequence = 1;
    isotp->tx_state.waiting_fc = true;
    timer_start(&isotp->tx_state.timer, isotp->config.timeout_ms);
    
    return can_transmit(isotp->can_driver, &frame, isotp->config.timeout_ms);
}

static bool send_consecutive_frame(ISOTP* isotp) {
    size_t remaining = isotp->tx_state.length - isotp->tx_state.offset;
    size_t segment_size = remaining > 7 ? 7 : remaining;
    
    CANFrame frame = {
        .id = isotp->config.tx_id,
        .is_extended = false,
        .dlc = segment_size + 1
    };
    
    frame.data[0] = (ISOTP_CONSECUTIVE_FRAME << 4) | isotp->tx_state.sequence;
    memcpy(&frame.data[1], 
           &isotp->tx_state.buffer[isotp->tx_state.offset],
           segment_size);
    
    isotp->tx_state.offset += segment_size;
    isotp->tx_state.sequence = (isotp->tx_state.sequence + 1) & 0x0F;
    
    return can_transmit(isotp->can_driver, &frame, isotp->config.timeout_ms);
}

static void process_single_frame(ISOTP* isotp, const CANFrame* frame) {
    size_t length = frame->data[0] & 0x0F;
    if (length > 0 && length <= 7) {
        memcpy(isotp->rx_state.buffer, &frame->data[1], length);
        isotp->rx_state.length = length;
        isotp->rx_state.receiving_multi = false;
    }
}

static void process_first_frame(ISOTP* isotp, const CANFrame* frame) {
    size_t length = ((frame->data[0] & 0x0F) << 8) | frame->data[1];
    if (length > 0 && length <= ISOTP_MAX_PAYLOAD) {
        memcpy(isotp->rx_state.buffer, &frame->data[2], 6);
        isotp->rx_state.length = length;
        isotp->rx_state.offset = 6;
        isotp->rx_state.sequence = 1;
        isotp->rx_state.receiving_multi = true;
        
        // Send flow control
        CANFrame fc = {
            .id = isotp->config.tx_id,
            .is_extended = false,
            .dlc = 3
        };
        fc.data[0] = ISOTP_FLOW_CONTROL << 4;
        fc.data[1] = isotp->config.blocksize;
        fc.data[2] = isotp->config.stmin;
        
        can_transmit(isotp->can_driver, &fc, isotp->config.timeout_ms);
    }
}

static void process_consecutive_frame(ISOTP* isotp, const CANFrame* frame) {
    if (!isotp->rx_state.receiving_multi) return;
    
    uint8_t sequence = frame->data[0] & 0x0F;
    if (sequence != isotp->rx_state.sequence) {
        // Sequence error
        isotp->rx_state.receiving_multi = false;
        return;
    }
    
    size_t remaining = isotp->rx_state.length - isotp->rx_state.offset;
    size_t segment_size = remaining > 7 ? 7 : remaining;
    
    memcpy(&isotp->rx_state.buffer[isotp->rx_state.offset],
           &frame->data[1], segment_size);
    
    isotp->rx_state.offset += segment_size;
    isotp->rx_state.sequence = (isotp->rx_state.sequence + 1) & 0x0F;
    
    if (isotp->rx_state.offset >= isotp->rx_state.length) {
        isotp->rx_state.receiving_multi = false;
    }
}

ISOTP* isotp_create(CANDriver* can_driver, const ISOTPConfig* config) {
    if (!can_driver || !config) return NULL;
    
    ISOTP* isotp = calloc(1, sizeof(ISOTP));
    if (!isotp) return NULL;
    
    isotp->can_driver = can_driver;
    memcpy(&isotp->config, config, sizeof(ISOTPConfig));
    
    init_critical(&isotp->critical);
    
    return isotp;
}

void isotp_destroy(ISOTP* isotp) {
    if (!isotp) return;
    destroy_critical(&isotp->critical);
    free(isotp);
}

bool isotp_transmit(ISOTP* isotp, const uint8_t* data, size_t length) {
    if (!isotp || !data || length == 0 || length > ISOTP_MAX_PAYLOAD) {
        return false;
    }
    
    enter_critical(&isotp->critical);
    
    bool result;
    if (length <= 7) {
        result = send_single_frame(isotp, data, length);
    } else {
        memcpy(isotp->tx_state.buffer, data, length);
        result = send_first_frame(isotp, data, length);
    }
    
    exit_critical(&isotp->critical);
    return result;
}

bool isotp_receive(ISOTP* isotp, uint8_t* data, size_t* length, uint32_t timeout_ms) {
    if (!isotp || !data || !length) return false;
    
    enter_critical(&isotp->critical);
    
    if (!isotp->rx_state.receiving_multi && isotp->rx_state.length > 0) {
        memcpy(data, isotp->rx_state.buffer, isotp->rx_state.length);
        *length = isotp->rx_state.length;
        isotp->rx_state.length = 0;
        exit_critical(&isotp->critical);
        return true;
    }
    
    exit_critical(&isotp->critical);
    return false;
}

void isotp_process(ISOTP* isotp) {
    if (!isotp) return;
    
    enter_critical(&isotp->critical);
    
    // Process received frames
    CANFrame frame;
    while (can_receive(isotp->can_driver, &frame, 0)) {
        if (frame.id == isotp->config.rx_id) {
            uint8_t pci = frame.data[0] >> 4;
            switch (pci) {
                case ISOTP_SINGLE_FRAME:
                    process_single_frame(isotp, &frame);
                    break;
                case ISOTP_FIRST_FRAME:
                    process_first_frame(isotp, &frame);
                    break;
                case ISOTP_CONSECUTIVE_FRAME:
                    process_consecutive_frame(isotp, &frame);
                    break;
                case ISOTP_FLOW_CONTROL:
                    // Handle flow control
                    break;
            }
        }
    }
    
    // Check for timeouts
    if (isotp->tx_state.waiting_fc && 
        timer_expired(&isotp->tx_state.timer)) {
        isotp->tx_state.waiting_fc = false;
    }
    
    exit_critical(&isotp->critical);
} 