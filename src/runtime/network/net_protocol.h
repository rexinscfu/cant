#ifndef CANT_NET_PROTOCOL_H
#define CANT_NET_PROTOCOL_H

#include "net_core.h"
#include <stdint.h>
#include <stdbool.h>

// Protocol-specific configuration structures
typedef struct {
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t timeout_ms;
    bool use_keepalive;
    uint32_t keepalive_interval_ms;
    uint32_t max_retries;
} TCPConfig;

typedef struct {
    uint16_t local_port;
    uint16_t remote_port;
    bool broadcast_enabled;
    bool multicast_enabled;
    char multicast_group[16];
} UDPConfig;

typedef struct {
    uint32_t bitrate;
    bool extended_id;
    bool fd_mode;
    bool brs_enabled;
    uint8_t sample_point;
} CANConfig;

typedef struct {
    char broker_url[128];
    uint16_t broker_port;
    char client_id[64];
    char username[32];
    char password[32];
    bool use_ssl;
    uint16_t keep_alive_interval;
    bool clean_session;
} MQTTConfig;

// Protocol handler functions
bool NetProtocol_InitTCP(const TCPConfig* config);
bool NetProtocol_InitUDP(const UDPConfig* config);
bool NetProtocol_InitCAN(const CANConfig* config);
bool NetProtocol_InitMQTT(const MQTTConfig* config);

bool NetProtocol_SendMessage(const NetMessage* message, void* interface_context);
bool NetProtocol_ProcessReceived(void* interface_context);

bool NetProtocol_HandleTCP(const NetMessage* message, void* interface_context);
bool NetProtocol_HandleUDP(const NetMessage* message, void* interface_context);
bool NetProtocol_HandleCAN(const NetMessage* message, void* interface_context);
bool NetProtocol_HandleMQTT(const NetMessage* message, void* interface_context);

#endif // CANT_NET_PROTOCOL_H 