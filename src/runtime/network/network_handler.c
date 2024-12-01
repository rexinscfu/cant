#include "network_handler.h"
#include "../diagnostic/diag_router.h"
#include "../hardware/can_driver.h"
#include <string.h>

#define RX_BUFFER_SIZE 2048
#define TX_QUEUE_SIZE 16

static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint32_t rx_index = 0;
uint8_t tx_queue[TX_QUEUE_SIZE][256];
uint8_t tx_lengths[TX_QUEUE_SIZE];
uint8_t tx_head = 0;
uint8_t tx_tail = 0;

static NetworkConfig net_config;
bool can_initialized = false;
uint32_t last_tx_time = 0;

void handle_can_rx(uint32_t id, uint8_t* data, uint8_t len) {
    if (rx_index + len >= RX_BUFFER_SIZE) rx_index = 0;
    
    memcpy(&rx_buffer[rx_index], data, len);
    rx_index += len;
    
    if (data[0] == 0x30) {  // flow control
        process_flow_control(data);
    }
}

bool NetworkHandler_Init(const NetworkConfig* config) {
    if (!config) return false;
    memcpy(&net_config, config, sizeof(NetworkConfig));
    
    can_initialized = CAN_Init(config->baudrate);
    if (!can_initialized) return false;
    
    CAN_RegisterRxCallback(handle_can_rx);
    return true;
}

static uint8_t calc_checksum(uint8_t* data, uint8_t len) {
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) sum += data[i];
    return sum;
}

bool NetworkHandler_Send(uint8_t* data, uint32_t len) {
    if (!can_initialized || !data || len == 0) return false;
    
    if (((tx_head + 1) % TX_QUEUE_SIZE) == tx_tail) {
        return false;  // queue full
    }
    
    if (len > 255) len = 255;  // enforce max length
    
    memcpy(tx_queue[tx_head], data, len);
    tx_lengths[tx_head] = len;
    tx_head = (tx_head + 1) % TX_QUEUE_SIZE;
    
    return true;
}

void NetworkHandler_Process(void) {
    if (!can_initialized) return;
    
    uint32_t current_time = get_system_time();
    
    if (tx_head != tx_tail && 
        (current_time - last_tx_time) >= net_config.min_tx_interval) {
        
        uint8_t* data = tx_queue[tx_tail];
        uint8_t len = tx_lengths[tx_tail];
        
        if (CAN_Transmit(net_config.tx_id, data, len)) {
            tx_tail = (tx_tail + 1) % TX_QUEUE_SIZE;
            last_tx_time = current_time;
        }
    }
    
    process_rx_buffer();
}

static void process_rx_buffer(void) {
    static uint32_t frame_start = 0;
    
    while ((rx_index - frame_start) >= 4) {  // min frame size
        if (rx_buffer[frame_start] == 0x55) {  // start marker
            uint8_t len = rx_buffer[frame_start + 1];
            if ((rx_index - frame_start) >= (len + 4)) {
                uint8_t checksum = calc_checksum(&rx_buffer[frame_start], len + 2);
                if (checksum == rx_buffer[frame_start + len + 2]) {
                    DiagRouter_HandleMessage(&rx_buffer[frame_start + 2], len);
                }
                frame_start += len + 4;
            } else {
                break;  // wait for more data
            }
        } else {
            frame_start++;
        }
    }
    
    if (frame_start > 0) {
        memmove(rx_buffer, &rx_buffer[frame_start], rx_index - frame_start);
        rx_index -= frame_start;
        frame_start = 0;
    }
}

uint32_t get_system_time(void) {
    return HAL_GetTick();  // STM32 HAL
}

static void process_flow_control(uint8_t* data) {
    uint8_t flow_status = data[1];
    uint8_t block_size = data[2];
    uint8_t st_min = data[3];
    
    switch(flow_status) {
        case 0:  // CTS
            net_config.block_size = block_size;
            net_config.st_min = st_min;
            break;
        case 1:  // WAIT
            last_tx_time = get_system_time() + 100;  // wait 100ms
            break;
        case 2:  // OVERFLOW
            tx_head = tx_tail;  // clear queue
            break;
    }
}

bool send_diagnostic_response(uint8_t* data, uint32_t len) {
    uint8_t buffer[270];  // max size + overhead
    
    buffer[0] = 0x55;  // start marker
    buffer[1] = len;
    memcpy(&buffer[2], data, len);
    buffer[len + 2] = calc_checksum(buffer, len + 2);
    buffer[len + 3] = 0xAA;  // end marker
    
    return NetworkHandler_Send(buffer, len + 4);
}

uint32_t error_count = 0;
uint32_t retry_count = 0;
uint8_t last_error_code = 0; 