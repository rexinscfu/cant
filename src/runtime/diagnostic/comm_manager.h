#ifndef CANT_COMM_MANAGER_H
#define CANT_COMM_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "uds_handler.h"

// Communication Channel Types
typedef enum {
    COMM_CHANNEL_CAN,
    COMM_CHANNEL_ETHERNET,
    COMM_CHANNEL_K_LINE,
    COMM_CHANNEL_FLEXRAY,
    COMM_CHANNEL_DOIP
} CommChannelType;

// Communication Control Types
typedef enum {
    COMM_CONTROL_ENABLE_RX_TX = 0x00,
    COMM_CONTROL_ENABLE_RX_DISABLE_TX = 0x01,
    COMM_CONTROL_DISABLE_RX_ENABLE_TX = 0x02,
    COMM_CONTROL_DISABLE_RX_TX = 0x03
} CommControlType;

// Communication Channel Configuration
typedef struct {
    CommChannelType type;
    uint32_t channel_id;
    uint32_t rx_id;
    uint32_t tx_id;
    uint32_t baud_rate;
    uint16_t block_size;
    uint16_t stmin;
    uint32_t timeout_ms;
    bool (*transmit)(const uint8_t* data, uint16_t length);
    void (*receive_callback)(const uint8_t* data, uint16_t length);
} CommChannelConfig;

// Communication Manager Configuration
typedef struct {
    CommChannelConfig* channels;
    uint32_t channel_count;
    void (*error_callback)(uint32_t channel_id, uint32_t error_code);
    void (*state_change_callback)(uint32_t channel_id, CommControlType state);
} CommManagerConfig;

// Communication Manager API
bool Comm_Manager_Init(const CommManagerConfig* config);
void Comm_Manager_DeInit(void);
bool Comm_Manager_TransmitMessage(uint32_t channel_id, const uint8_t* data, uint16_t length);
void Comm_Manager_ProcessReceived(uint32_t channel_id, const uint8_t* data, uint16_t length);
bool Comm_Manager_ControlCommunication(uint32_t channel_id, CommControlType control_type);
bool Comm_Manager_AddChannel(const CommChannelConfig* channel);
bool Comm_Manager_RemoveChannel(uint32_t channel_id);
CommChannelConfig* Comm_Manager_GetChannel(uint32_t channel_id);
bool Comm_Manager_IsChannelEnabled(uint32_t channel_id);
CommControlType Comm_Manager_GetChannelState(uint32_t channel_id);
void Comm_Manager_ProcessTimeout(void);
uint32_t Comm_Manager_GetActiveChannels(void);
bool Comm_Manager_ResetChannel(uint32_t channel_id);
uint32_t Comm_Manager_GetLastError(uint32_t channel_id);

// Network Management Functions
bool Comm_Manager_WakeupNetwork(uint32_t channel_id);
bool Comm_Manager_SleepNetwork(uint32_t channel_id);
bool Comm_Manager_IsNetworkActive(uint32_t channel_id);

#endif // CANT_COMM_MANAGER_H 