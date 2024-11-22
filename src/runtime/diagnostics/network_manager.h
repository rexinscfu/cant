#ifndef CANT_NETWORK_MANAGER_H
#define CANT_NETWORK_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "../protocols/j1939.h"

// Network node states
typedef enum {
    NODE_STATE_SLEEP,
    NODE_STATE_PREPARE_SLEEP,
    NODE_STATE_WAKEUP,
    NODE_STATE_NORMAL
} NodeState;

// Network configuration
typedef struct {
    uint32_t node_timeout_ms;
    uint32_t sleep_delay_ms;
    bool support_partial_networking;
    struct {
        uint32_t wakeup_source;
        uint32_t wakeup_mask;
    } pn_config;
} NetworkConfig;

typedef struct NetworkManager NetworkManager;

// Network Manager API
NetworkManager* network_manager_create(J1939Handler* j1939, const NetworkConfig* config);
void network_manager_destroy(NetworkManager* manager);
void network_manager_process(NetworkManager* manager);
NodeState network_manager_get_state(const NetworkManager* manager);
bool network_manager_request_sleep(NetworkManager* manager);
bool network_manager_request_wakeup(NetworkManager* manager);
void network_manager_set_state_change_callback(NetworkManager* manager, 
                                             void (*callback)(NodeState));

#endif // CANT_NETWORK_MANAGER_H 