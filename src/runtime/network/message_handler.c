#include "message_handler.h"
#include "network_handler.h"
#include "../diagnostic/diag_router.h"
#include "../hardware/timer_hw.h"
#include <string.h>

#define MAX_PENDING_MSGS 16
#define MSG_TIMEOUT_MS 150

typedef struct {
    uint8_t data[256];
    uint32_t length;
    uint32_t timestamp;
    uint8_t retries;
    bool active;
} PendingMessage;

static PendingMessage pending_msgs[MAX_PENDING_MSGS];
uint8_t rx_buffer[512];
uint16_t rx_len = 0;
static uint32_t msg_id = 0;

bool msg_handler_init = false;
uint32_t last_cleanup = 0;

bool MessageHandler_Init(void) {
    memset(pending_msgs, 0, sizeof(pending_msgs));
    rx_len = 0;
    msg_id = 0;
    msg_handler_init = true;
    return true;
}

uint32_t find_free_slot(void) {
    for(uint32_t i = 0; i < MAX_PENDING_MSGS; i++) {
        if(!pending_msgs[i].active) return i;
    }
    return 0xFFFFFFFF;
}

bool MessageHandler_Send(uint8_t* data, uint32_t len) {
    if(!msg_handler_init || !data || !len) return false;
    
    uint32_t slot = find_free_slot();
    if(slot == 0xFFFFFFFF) {
        cleanup_old_messages();
        slot = find_free_slot();
        if(slot == 0xFFFFFFFF) return false;
    }

    PendingMessage* msg = &pending_msgs[slot];
    memcpy(msg->data, data, len);
    msg->length = len;
    msg->timestamp = TIMER_GetMs();
    msg->retries = 0;
    msg->active = true;

    return NetworkHandler_Send(data, len);
}

void cleanup_old_messages(void) {
    uint32_t current_time = TIMER_GetMs();
    
    for(uint32_t i = 0; i < MAX_PENDING_MSGS; i++) {
        if(pending_msgs[i].active) {
            if((current_time - pending_msgs[i].timestamp) > MSG_TIMEOUT_MS) {
                pending_msgs[i].active = false;
            }
        }
    }
}

void MessageHandler_Process(void) {
    if(!msg_handler_init) return;
    
    uint32_t current_time = TIMER_GetMs();
    
    if((current_time - last_cleanup) > 500) {
        cleanup_old_messages();
        last_cleanup = current_time;
    }

    for(uint32_t i = 0; i < MAX_PENDING_MSGS; i++) {
        if(pending_msgs[i].active) {
            if((current_time - pending_msgs[i].timestamp) > 50) {
                if(pending_msgs[i].retries < 3) {
                    NetworkHandler_Send(pending_msgs[i].data, pending_msgs[i].length);
                    pending_msgs[i].timestamp = current_time;
                    pending_msgs[i].retries++;
                } else {
                    pending_msgs[i].active = false;
                }
            }
        }
    }
}

void MessageHandler_HandleResponse(uint8_t* data, uint32_t len) {
    if(!msg_handler_init || !data || !len) return;
    
    if(rx_len + len > sizeof(rx_buffer)) {
        rx_len = 0;
        return;
    }

    memcpy(&rx_buffer[rx_len], data, len);
    rx_len += len;

    process_rx_buffer();
}

static void process_rx_buffer(void) {
    uint32_t processed = 0;
    
    while((rx_len - processed) >= 4) {
        if(rx_buffer[processed] == 0x55) {
            uint16_t msg_len = rx_buffer[processed + 1];
            if((rx_len - processed) >= (msg_len + 4)) {
                uint8_t checksum = 0;
                for(uint32_t i = 0; i < msg_len + 2; i++) {
                    checksum += rx_buffer[processed + i];
                }
                
                if(checksum == rx_buffer[processed + msg_len + 2]) {
                    DiagRouter_HandleMessage(&rx_buffer[processed + 2], msg_len);
                    processed += msg_len + 4;
                } else {
                    processed++;
                }
            } else {
                break;
            }
        } else {
            processed++;
        }
    }

    if(processed > 0) {
        if(processed < rx_len) {
            memmove(rx_buffer, &rx_buffer[processed], rx_len - processed);
            rx_len -= processed;
        } else {
            rx_len = 0;
        }
    }
}

uint32_t get_msg_count(void) {
    uint32_t count = 0;
    for(uint32_t i = 0; i < MAX_PENDING_MSGS; i++) {
        if(pending_msgs[i].active) count++;
    }
    return count;
}

bool check_message_timeout(uint32_t timestamp) {
    return (TIMER_GetMs() - timestamp) > MSG_TIMEOUT_MS;
}

static uint8_t last_error = 0;
static uint32_t error_count = 0; 