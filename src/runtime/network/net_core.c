#include "net_core.h"
#include "net_buffer.h"
#include "net_protocol.h"
#include "net_interface.h"
#include "../diagnostic/logging/diag_logger.h"
#include "../diagnostic/os/critical.h"
#include "../diagnostic/os/timer.h"
#include <string.h>

#define MAX_INTERFACES 8
#define MAX_CALLBACKS_PER_EVENT 8

typedef struct {
    NetEventCallback callback;
    void* context;
    bool active;
} EventCallback;

typedef struct {
    NetInterfaceConfig config;
    NetConnectionState state;
    NetStatistics stats;
    uint32_t last_heartbeat;
    bool active;
} InterfaceContext;

typedef struct {
    NetManagerConfig config;
    InterfaceContext interfaces[MAX_INTERFACES];
    EventCallback callbacks[NET_EVENT_ERROR + 1][MAX_CALLBACKS_PER_EVENT];
    NetBuffer rx_buffer;
    NetBuffer tx_buffer;
    bool initialized;
} NetworkManager;

static NetworkManager net_mgr;

static void trigger_event(NetEventType event, void* data) {
    if (!net_mgr.initialized) {
        return;
    }

    enter_critical();
    
    for (uint32_t i = 0; i < MAX_CALLBACKS_PER_EVENT; i++) {
        EventCallback* cb = &net_mgr.callbacks[event][i];
        if (cb->active && cb->callback) {
            cb->callback(event, data, cb->context);
        }
    }
    
    exit_critical();
}

bool Net_Init(const NetManagerConfig* config) {
    if (!config || !config->rx_buffer_size || !config->tx_buffer_size) {
        return false;
    }

    memset(&net_mgr, 0, sizeof(NetworkManager));
    memcpy(&net_mgr.config, config, sizeof(NetManagerConfig));

    // Initialize buffers
    if (!NetBuffer_Init(&net_mgr.rx_buffer, config->rx_buffer_size) ||
        !NetBuffer_Init(&net_mgr.tx_buffer, config->tx_buffer_size)) {
        Net_Deinit();
        return false;
    }

    net_mgr.initialized = true;
    Logger_Log(LOG_LEVEL_INFO, "NETWORK", "Network manager initialized");
    return true;
}

void Net_Deinit(void) {
    if (!net_mgr.initialized) {
        return;
    }

    // Disconnect all interfaces
    for (uint32_t i = 0; i < MAX_INTERFACES; i++) {
        if (net_mgr.interfaces[i].active) {
            Net_Disconnect(net_mgr.interfaces[i].config.type);
        }
    }

    // Free buffers
    NetBuffer_Deinit(&net_mgr.rx_buffer);
    NetBuffer_Deinit(&net_mgr.tx_buffer);

    Logger_Log(LOG_LEVEL_INFO, "NETWORK", "Network manager deinitialized");
    memset(&net_mgr, 0, sizeof(NetworkManager));
}

bool Net_AddInterface(const NetInterfaceConfig* config) {
    if (!net_mgr.initialized || !config) {
        return false;
    }

    enter_critical();

    // Find free slot
    InterfaceContext* ctx = NULL;
    for (uint32_t i = 0; i < MAX_INTERFACES; i++) {
        if (!net_mgr.interfaces[i].active) {
            ctx = &net_mgr.interfaces[i];
            break;
        }
    }

    if (!ctx) {
        exit_critical();
        Logger_Log(LOG_LEVEL_ERROR, "NETWORK", "No free interface slots");
        return false;
    }

    // Initialize interface
    memset(ctx, 0, sizeof(InterfaceContext));
    memcpy(&ctx->config, config, sizeof(NetInterfaceConfig));
    ctx->state = NET_STATE_DISCONNECTED;
    ctx->active = true;

    Logger_Log(LOG_LEVEL_INFO, "NETWORK", "Added interface: %s", config->name);

    exit_critical();
    return true;
}

bool Net_RemoveInterface(NetInterfaceType type) {
    if (!net_mgr.initialized) {
        return false;
    }

    enter_critical();

    InterfaceContext* ctx = NULL;
    for (uint32_t i = 0; i < MAX_INTERFACES; i++) {
        if (net_mgr.interfaces[i].active && 
            net_mgr.interfaces[i].config.type == type) {
            ctx = &net_mgr.interfaces[i];
            break;
        }
    }

    if (!ctx) {
        exit_critical();
        return false;
    }

    // Disconnect if connected
    if (ctx->state == NET_STATE_CONNECTED) {
        Net_Disconnect(type);
    }

    Logger_Log(LOG_LEVEL_INFO, "NETWORK", "Removed interface: %s", 
               ctx->config.name);
    memset(ctx, 0, sizeof(InterfaceContext));

    exit_critical();
    return true;
}

bool Net_Connect(NetInterfaceType type) {
    if (!net_mgr.initialized) {
        return false;
    }

    enter_critical();

    InterfaceContext* ctx = NULL;
    for (uint32_t i = 0; i < MAX_INTERFACES; i++) {
        if (net_mgr.interfaces[i].active && 
            net_mgr.interfaces[i].config.type == type) {
            ctx = &net_mgr.interfaces[i];
            break;
        }
    }

    if (!ctx) {
        exit_critical();
        return false;
    }

    if (ctx->state == NET_STATE_CONNECTED) {
        exit_critical();
        return true;
    }

    // Initialize interface-specific connection
    bool success = false;
    switch (type) {
        case NET_IF_ETHERNET:
            success = NetInterface_ConnectEthernet(&ctx->config);
            break;
        case NET_IF_WIFI:
            success = NetInterface_ConnectWiFi(&ctx->config);
            break;
        case NET_IF_CELLULAR:
            success = NetInterface_ConnectCellular(&ctx->config);
            break;
        case NET_IF_CAN:
            success = NetInterface_ConnectCAN(&ctx->config);
            break;
        default:
            break;
    }

    if (success) {
        ctx->state = NET_STATE_CONNECTED;
        ctx->stats.successful_connections++;
        ctx->last_heartbeat = Timer_GetMilliseconds();
        trigger_event(NET_EVENT_CONNECTED, ctx);
        Logger_Log(LOG_LEVEL_INFO, "NETWORK", "Connected interface: %s", 
                  ctx->config.name);
    } else {
        ctx->state = NET_STATE_ERROR;
        ctx->stats.errors++;
        Logger_Log(LOG_LEVEL_ERROR, "NETWORK", "Failed to connect interface: %s", 
                  ctx->config.name);
    }

    exit_critical();
    return success;
}

bool Net_Disconnect(NetInterfaceType type) {
    if (!net_mgr.initialized) {
        return false;
    }

    enter_critical();

    InterfaceContext* ctx = NULL;
    for (uint32_t i = 0; i < MAX_INTERFACES; i++) {
        if (net_mgr.interfaces[i].active && 
            net_mgr.interfaces[i].config.type == type) {
            ctx = &net_mgr.interfaces[i];
            break;
        }
    }

    if (!ctx || ctx->state != NET_STATE_CONNECTED) {
        exit_critical();
        return false;
    }

    // Perform interface-specific disconnection
    bool success = false;
    switch (type) {
        case NET_IF_ETHERNET:
            success = NetInterface_DisconnectEthernet(&ctx->config);
            break;
        case NET_IF_WIFI:
            success = NetInterface_DisconnectWiFi(&ctx->config);
            break;
        case NET_IF_CELLULAR:
            success = NetInterface_DisconnectCellular(&ctx->config);
            break;
        case NET_IF_CAN:
            success = NetInterface_DisconnectCAN(&ctx->config);
            break;
        default:
            break;
    }

    if (success) {
        ctx->state = NET_STATE_DISCONNECTED;
        ctx->stats.disconnections++;
        trigger_event(NET_EVENT_DISCONNECTED, ctx);
        Logger_Log(LOG_LEVEL_INFO, "NETWORK", "Disconnected interface: %s", 
                  ctx->config.name);
    } else {
        ctx->state = NET_STATE_ERROR;
        ctx->stats.errors++;
        Logger_Log(LOG_LEVEL_ERROR, "NETWORK", 
                  "Failed to disconnect interface: %s", ctx->config.name);
    }

    exit_critical();
    return success;
}

bool Net_SendMessage(const NetMessage* message) {
    if (!net_mgr.initialized || !message || !message->data || !message->length) {
        return false;
    }

    enter_critical();

    // Find appropriate interface for protocol
    InterfaceContext* ctx = NULL;
    for (uint32_t i = 0; i < MAX_INTERFACES; i++) {
        if (net_mgr.interfaces[i].active && 
            net_mgr.interfaces[i].state == NET_STATE_CONNECTED) {
            // Match protocol to interface type
            switch (message->protocol) {
                case NET_PROTO_TCP:
                case NET_PROTO_UDP:
                    if (net_mgr.interfaces[i].config.type == NET_IF_ETHERNET ||
                        net_mgr.interfaces[i].config.type == NET_IF_WIFI) {
                        ctx = &net_mgr.interfaces[i];
                    }
                    break;
                case NET_PROTO_CAN:
                    if (net_mgr.interfaces[i].config.type == NET_IF_CAN) {
                        ctx = &net_mgr.interfaces[i];
                    }
                    break;
                default:
                    break;
            }
            if (ctx) break;
        }
    }

    if (!ctx) {
        exit_critical();
        return false;
    }

    // Add message to TX buffer
    if (!NetBuffer_Write(&net_mgr.tx_buffer, message->data, message->length)) {
        exit_critical();
        return false;
    }

    // Send using protocol-specific handler
    bool success = NetProtocol_SendMessage(message, ctx);
    if (success) {
        ctx->stats.bytes_sent += message->length;
        ctx->stats.packets_sent++;
        trigger_event(NET_EVENT_DATA_SENT, (void*)message);
    } else {
        ctx->stats.errors++;
    }

    exit_critical();
    return success;
}

bool Net_ReceiveMessage(NetMessage* message) {
    if (!net_mgr.initialized || !message) {
        return false;
    }

    enter_critical();

    // Check RX buffer for data
    uint32_t available = NetBuffer_GetAvailable(&net_mgr.rx_buffer);
    if (!available) {
        exit_critical();
        return false;
    }

    // Read message from buffer
    if (!NetBuffer_Read(&net_mgr.rx_buffer, message->data, available)) {
        exit_critical();
        return false;
    }

    message->length = available;
    message->timestamp = Timer_GetMilliseconds();

    // Update statistics for the appropriate interface
    for (uint32_t i = 0; i < MAX_INTERFACES; i++) {
        if (net_mgr.interfaces[i].active && 
            net_mgr.interfaces[i].state == NET_STATE_CONNECTED) {
            net_mgr.interfaces[i].stats.bytes_received += available;
            net_mgr.interfaces[i].stats.packets_received++;
            break;
        }
    }

    trigger_event(NET_EVENT_DATA_RECEIVED, (void*)message);

    exit_critical();
    return true;
}

void Net_RegisterCallback(NetEventType event, NetEventCallback callback, 
                         void* context) {
    if (!net_mgr.initialized || !callback || event > NET_EVENT_ERROR) {
        return;
    }

    enter_critical();

    // Find free callback slot
    for (uint32_t i = 0; i < MAX_CALLBACKS_PER_EVENT; i++) {
        EventCallback* cb = &net_mgr.callbacks[event][i];
        if (!cb->active) {
            cb->callback = callback;
            cb->context = context;
            cb->active = true;
            break;
        }
    }

    exit_critical();
}

void Net_UnregisterCallback(NetEventType event, NetEventCallback callback) {
    if (!net_mgr.initialized || !callback || event > NET_EVENT_ERROR) {
        return;
    }

    enter_critical();

    for (uint32_t i = 0; i < MAX_CALLBACKS_PER_EVENT; i++) {
        EventCallback* cb = &net_mgr.callbacks[event][i];
        if (cb->active && cb->callback == callback) {
            memset(cb, 0, sizeof(EventCallback));
            break;
        }
    }

    exit_critical();
}

NetConnectionState Net_GetState(NetInterfaceType type) {
    if (!net_mgr.initialized) {
        return NET_STATE_ERROR;
    }

    for (uint32_t i = 0; i < MAX_INTERFACES; i++) {
        if (net_mgr.interfaces[i].active && 
            net_mgr.interfaces[i].config.type == type) {
            return net_mgr.interfaces[i].state;
        }
    }

    return NET_STATE_ERROR;
}

void Net_GetStatistics(NetInterfaceType type, NetStatistics* stats) {
    if (!net_mgr.initialized || !stats) {
        return;
    }

    enter_critical();

    for (uint32_t i = 0; i < MAX_INTERFACES; i++) {
        if (net_mgr.interfaces[i].active && 
            net_mgr.interfaces[i].config.type == type) {
            memcpy(stats, &net_mgr.interfaces[i].stats, sizeof(NetStatistics));
            break;
        }
    }

    exit_critical();
}

void Net_Process(void) {
    if (!net_mgr.initialized) {
        return;
    }

    enter_critical();

    uint32_t current_time = Timer_GetMilliseconds();

    // Process each active interface
    for (uint32_t i = 0; i < MAX_INTERFACES; i++) {
        InterfaceContext* ctx = &net_mgr.interfaces[i];
        if (!ctx->active) {
            continue;
        }

        // Check connection state
        if (ctx->state == NET_STATE_CONNECTED) {
            // Process heartbeat
            if (net_mgr.config.heartbeat_interval_ms > 0 &&
                current_time - ctx->last_heartbeat >= 
                net_mgr.config.heartbeat_interval_ms) {
                
                // Send heartbeat message
                NetMessage heartbeat = {
                    .id = 0,
                    .protocol = NET_PROTO_TCP,
                    .length = 0
                };
                Net_SendMessage(&heartbeat);
                ctx->last_heartbeat = current_time;
            }

            // Process received data
            NetProtocol_ProcessReceived(ctx);
        }
        // Handle auto-reconnect
        else if (ctx->state == NET_STATE_DISCONNECTED && 
                 ctx->config.auto_connect &&
                 current_time - ctx->last_heartbeat >= 
                 ctx->config.reconnect_interval_ms) {
            Net_Connect(ctx->config.type);
        }
    }

    exit_critical();
} 