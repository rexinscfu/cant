#include "diag_transport.h"
#include <string.h>
#include "../utils/timer.h"
#include "../os/critical.h"

#define MAX_TRANSPORT_BUFFER_SIZE 4096
#define MAX_CONSECUTIVE_FRAME_TIMEOUT 1000
#define MAX_FLOW_CONTROL_TIMEOUT 1000

// Transport Protocol States
typedef enum {
    TP_STATE_IDLE,
    TP_STATE_RECEIVING_MULTI_FRAME,
    TP_STATE_SENDING_MULTI_FRAME,
    TP_STATE_WAITING_FLOW_CONTROL,
    TP_STATE_SENDING_FLOW_CONTROL
} TransportState;

// Frame Types
typedef enum {
    TP_FRAME_SINGLE = 0x00,
    TP_FRAME_FIRST = 0x01,
    TP_FRAME_CONSECUTIVE = 0x02,
    TP_FRAME_FLOW_CONTROL = 0x03
} TransportFrameType;

// Flow Control Status
typedef enum {
    FLOW_STATUS_CONTINUE = 0x00,
    FLOW_STATUS_WAIT = 0x01,
    FLOW_STATUS_OVERFLOW = 0x02
} FlowControlStatus;

// Internal transport state
typedef struct {
    DiagTransportConfig config;
    struct {
        TransportState state;
        uint8_t buffer[MAX_TRANSPORT_BUFFER_SIZE];
        uint16_t buffer_index;
        uint16_t expected_length;
        uint8_t sequence_number;
        uint8_t block_counter;
        Timer timeout_timer;
        Timer stmin_timer;
        bool initialized;
        uint32_t last_error;
    } rx_state;
    struct {
        TransportState state;
        uint8_t buffer[MAX_TRANSPORT_BUFFER_SIZE];
        uint16_t buffer_length;
        uint16_t buffer_index;
        uint8_t sequence_number;
        uint8_t block_counter;
        Timer timeout_timer;
        Timer stmin_timer;
    } tx_state;
    CriticalSection critical;
} DiagTransport;

static DiagTransport transport;

// Error codes
#define TP_ERROR_NONE 0
#define TP_ERROR_TIMEOUT 1
#define TP_ERROR_SEQUENCE 2
#define TP_ERROR_OVERFLOW 3
#define TP_ERROR_INVALID_FRAME 4
#define TP_ERROR_BUSY 5

static void reset_rx_state(void) {
    transport.rx_state.state = TP_STATE_IDLE;
    transport.rx_state.buffer_index = 0;
    transport.rx_state.expected_length = 0;
    transport.rx_state.sequence_number = 0;
    transport.rx_state.block_counter = 0;
    transport.rx_state.last_error = TP_ERROR_NONE;
}

static void reset_tx_state(void) {
    transport.tx_state.state = TP_STATE_IDLE;
    transport.tx_state.buffer_length = 0;
    transport.tx_state.buffer_index = 0;
    transport.tx_state.sequence_number = 0;
    transport.tx_state.block_counter = 0;
}

static bool send_single_frame(const uint8_t* data, uint16_t length) {
    if (length > 7) return false;  // Single frame limited to 7 bytes for CAN

    uint8_t frame[8];
    frame[0] = (TP_FRAME_SINGLE << 4) | length;
    memcpy(&frame[1], data, length);

    // Platform specific send implementation
    // return platform_send_can_frame(transport.config.tx_id, frame, length + 1);
    return true; // Placeholder
}

static bool send_first_frame(const uint8_t* data, uint16_t total_length) {
    uint8_t frame[8];
    frame[0] = (TP_FRAME_FIRST << 4) | ((total_length >> 8) & 0x0F);
    frame[1] = total_length & 0xFF;
    memcpy(&frame[2], data, 6);  // First frame can contain 6 bytes of data

    // Platform specific send implementation
    // return platform_send_can_frame(transport.config.tx_id, frame, 8);
    return true; // Placeholder
}

static bool send_consecutive_frame(const uint8_t* data, uint16_t length) {
    uint8_t frame[8];
    frame[0] = (TP_FRAME_CONSECUTIVE << 4) | (transport.tx_state.sequence_number & 0x0F);
    memcpy(&frame[1], data, length);

    // Platform specific send implementation
    // return platform_send_can_frame(transport.config.tx_id, frame, length + 1);
    return true; // Placeholder
}

static bool send_flow_control(FlowControlStatus status) {
    uint8_t frame[8];
    frame[0] = (TP_FRAME_FLOW_CONTROL << 4) | status;
    frame[1] = transport.config.block_size;
    frame[2] = transport.config.stmin_ms;
    memset(&frame[3], 0xFF, 5);  // Padding

    // Platform specific send implementation
    // return platform_send_can_frame(transport.config.tx_id, frame, 8);
    return true; // Placeholder
}

bool Diag_Transport_Init(const DiagTransportConfig* config) {
    if (!config) return false;

    enter_critical(&transport.critical);

    memcpy(&transport.config, config, sizeof(DiagTransportConfig));
    
    reset_rx_state();
    reset_tx_state();
    
    timer_init();
    init_critical(&transport.critical);
    
    transport.rx_state.initialized = true;

    exit_critical(&transport.critical);
    return true;
}

void Diag_Transport_DeInit(void) {
    enter_critical(&transport.critical);
    memset(&transport, 0, sizeof(DiagTransport));
    exit_critical(&transport.critical);
}

bool Diag_Transport_SendResponse(const uint8_t* data, uint16_t length) {
    if (!transport.rx_state.initialized || !data || length == 0) {
        return false;
    }

    enter_critical(&transport.critical);

    if (transport.tx_state.state != TP_STATE_IDLE) {
        transport.rx_state.last_error = TP_ERROR_BUSY;
        exit_critical(&transport.critical);
        return false;
    }

    if (length <= 7) {
        // Single frame transmission
        bool result = send_single_frame(data, length);
        exit_critical(&transport.critical);
        return result;
    }

    // Multi frame transmission
    memcpy(transport.tx_state.buffer, data, length);
    transport.tx_state.buffer_length = length;
    transport.tx_state.buffer_index = 6;  // First frame contains 6 bytes
    transport.tx_state.sequence_number = 1;
    transport.tx_state.state = TP_STATE_SENDING_MULTI_FRAME;

    bool result = send_first_frame(data, length);
    if (result) {
        timer_start(&transport.tx_state.timeout_timer, MAX_FLOW_CONTROL_TIMEOUT);
        transport.tx_state.state = TP_STATE_WAITING_FLOW_CONTROL;
    }

    exit_critical(&transport.critical);
    return result;
}

void Diag_Transport_ProcessReceived(const uint8_t* data, uint16_t length) {
    if (!transport.rx_state.initialized || !data || length == 0) {
        return;
    }

    enter_critical(&transport.critical);

    uint8_t frame_type = (data[0] >> 4) & 0x0F;
    
    switch (frame_type) {
        case TP_FRAME_SINGLE: {
            uint8_t data_length = data[0] & 0x0F;
            if (data_length > 0 && data_length <= 7) {
                UdsMessage request = {0};
                memcpy(&request.data[0], &data[1], data_length);
                request.length = data_length;
                
                UdsMessage response = {0};
                UdsResponseCode result = UDS_Handler_ProcessRequest(&request, &response);
                
                if (result == UDS_RESPONSE_POSITIVE) {
                    Diag_Transport_SendResponse(response.data, response.length);
                } else {
                    UDS_Handler_SendNegativeResponse(request.service_id, result);
                }
            }
            break;
        }
        
        case TP_FRAME_FIRST: {
            uint16_t total_length = ((data[0] & 0x0F) << 8) | data[1];
            if (total_length > MAX_TRANSPORT_BUFFER_SIZE) {
                send_flow_control(FLOW_STATUS_OVERFLOW);
                transport.rx_state.last_error = TP_ERROR_OVERFLOW;
                reset_rx_state();
            } else {
                memcpy(transport.rx_state.buffer, &data[2], 6);
                transport.rx_state.buffer_index = 6;
                transport.rx_state.expected_length = total_length;
                transport.rx_state.sequence_number = 1;
                transport.rx_state.state = TP_STATE_RECEIVING_MULTI_FRAME;
                
                send_flow_control(FLOW_STATUS_CONTINUE);
                timer_start(&transport.rx_state.timeout_timer, MAX_CONSECUTIVE_FRAME_TIMEOUT);
            }
            break;
        }
        
        case TP_FRAME_CONSECUTIVE: {
            if (transport.rx_state.state != TP_STATE_RECEIVING_MULTI_FRAME) {
                transport.rx_state.last_error = TP_ERROR_SEQUENCE;
                reset_rx_state();
                break;
            }

            uint8_t sequence = data[0] & 0x0F;
            if (sequence != transport.rx_state.sequence_number) {
                transport.rx_state.last_error = TP_ERROR_SEQUENCE;
                reset_rx_state();
                break;
            }

            uint16_t remaining = transport.rx_state.expected_length - transport.rx_state.buffer_index;
            uint8_t copy_length = (remaining > 7) ? 7 : remaining;
            
            memcpy(&transport.rx_state.buffer[transport.rx_state.buffer_index], 
                   &data[1], copy_length);
            transport.rx_state.buffer_index += copy_length;
            
            if (transport.rx_state.buffer_index >= transport.rx_state.expected_length) {
                // Complete message received
                UdsMessage request = {0};
                memcpy(request.data, transport.rx_state.buffer, 
                       transport.rx_state.expected_length);
                request.length = transport.rx_state.expected_length;
                
                UdsMessage response = {0};
                UdsResponseCode result = UDS_Handler_ProcessRequest(&request, &response);
                
                if (result == UDS_RESPONSE_POSITIVE) {
                    Diag_Transport_SendResponse(response.data, response.length);
                } else {
                    UDS_Handler_SendNegativeResponse(request.service_id, result);
                }
                
                reset_rx_state();
            } else {
                transport.rx_state.sequence_number = (transport.rx_state.sequence_number + 1) & 0x0F;
                transport.rx_state.block_counter++;
                
                if (transport.rx_state.block_counter >= transport.config.block_size) {
                    transport.rx_state.block_counter = 0;
                    send_flow_control(FLOW_STATUS_CONTINUE);
                }
                
                timer_start(&transport.rx_state.timeout_timer, MAX_CONSECUTIVE_FRAME_TIMEOUT);
            }
            break;
        }
        
        case TP_FRAME_FLOW_CONTROL: {
            if (transport.tx_state.state != TP_STATE_WAITING_FLOW_CONTROL) {
                break;
            }

            FlowControlStatus status = data[0] & 0x0F;
            uint8_t block_size = data[1];
            uint8_t stmin = data[2];
            
            switch (status) {
                case FLOW_STATUS_CONTINUE:
                    transport.tx_state.state = TP_STATE_SENDING_MULTI_FRAME;
                    timer_start(&transport.tx_state.stmin_timer, stmin);
                    break;
                    
                case FLOW_STATUS_WAIT:
                    timer_start(&transport.tx_state.timeout_timer, MAX_FLOW_CONTROL_TIMEOUT);
                    break;
                    
                case FLOW_STATUS_OVERFLOW:
                    transport.rx_state.last_error = TP_ERROR_OVERFLOW;
                    reset_tx_state();
                    break;
            }
            break;
        }
    }

    exit_critical(&transport.critical);
}

bool Diag_Transport_IsBusy(void) {
    return transport.tx_state.state != TP_STATE_IDLE;
}

void Diag_Transport_AbortTransmission(void) {
    enter_critical(&transport.critical);
    reset_tx_state();
    exit_critical(&transport.critical);
}

uint32_t Diag_Transport_GetLastError(void) {
    return transport.rx_state.last_error;
} 