#ifndef CANT_NET_CORE_H
#define CANT_NET_CORE_H

#include <stdint.h>
#include <stdbool.h>

// Network interface types
typedef enum {
    NET_IF_LOOPBACK = 0,
    NET_IF_ETHERNET,
    NET_IF_WIFI,
    NET_IF_CELLULAR,
    NET_IF_CAN,
    NET_IF_COUNT
} NetInterfaceType;

// Network protocol types
typedef enum {
    NET_PROTO_TCP = 0,
    NET_PROTO_UDP,
    NET_PROTO_CAN,
    NET_PROTO_MQTT,
    NET_PROTO_COUNT
} NetProtocolType;

// Network connection states
typedef enum {
    NET_STATE_DISCONNECTED = 0,
    NET_STATE_CONNECTING,
    NET_STATE_CONNECTED,
    NET_STATE_DISCONNECTING,
    NET_STATE_ERROR
} NetConnectionState;

// Network interface configuration
typedef struct {
    NetInterfaceType type;
    const char* name;
    const char* address;
    uint16_t port;
    bool auto_connect;
    uint32_t reconnect_interval_ms;
    uint32_t timeout_ms;
    void* interface_config;  // Interface specific configuration
} NetInterfaceConfig;

// Network statistics
typedef struct {
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t errors;
    uint32_t connection_attempts;
    uint32_t successful_connections;
    uint32_t disconnections;
} NetStatistics;

// Network manager configuration
typedef struct {
    uint32_t max_interfaces;
    uint32_t max_connections;
    uint32_t rx_buffer_size;
    uint32_t tx_buffer_size;
    bool enable_statistics;
    bool auto_reconnect;
    uint32_t heartbeat_interval_ms;
} NetManagerConfig;

// Network message structure
typedef struct {
    uint32_t id;
    uint8_t* data;
    uint32_t length;
    NetProtocolType protocol;
    uint32_t timestamp;
    uint32_t flags;
} NetMessage;

// Network event types
typedef enum {
    NET_EVENT_CONNECTED = 0,
    NET_EVENT_DISCONNECTED,
    NET_EVENT_DATA_RECEIVED,
    NET_EVENT_DATA_SENT,
    NET_EVENT_ERROR
} NetEventType;

// Network event callback
typedef void (*NetEventCallback)(NetEventType event, void* data, void* context);

// Network interface operations
bool Net_Init(const NetManagerConfig* config);
void Net_Deinit(void);

bool Net_AddInterface(const NetInterfaceConfig* config);
bool Net_RemoveInterface(NetInterfaceType type);

bool Net_Connect(NetInterfaceType type);
bool Net_Disconnect(NetInterfaceType type);

bool Net_SendMessage(const NetMessage* message);
bool Net_ReceiveMessage(NetMessage* message);

void Net_RegisterCallback(NetEventType event, NetEventCallback callback, void* context);
void Net_UnregisterCallback(NetEventType event, NetEventCallback callback);

NetConnectionState Net_GetState(NetInterfaceType type);
void Net_GetStatistics(NetInterfaceType type, NetStatistics* stats);

void Net_Process(void);

#endif // CANT_NET_CORE_H 