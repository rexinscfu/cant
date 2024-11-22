#include "network_manager.h"
#include <stdlib.h>
#include <string.h>
#include "../os/critical.h"
#include "../utils/timer.h"

struct NetworkManager {
    J1939Handler* j1939;
    NetworkConfig config;
    
    struct {
        NodeState current;
        NodeState requested;
        Timer state_timer;
        void (*state_change_callback)(NodeState);
    } state_mgmt;
    
    struct {
        bool sleep_requested;
        Timer sleep_timer;
    } power_mgmt;
    
    CriticalSection critical;
};

static void handle_state_change(NetworkManager* manager, NodeState new_state) {
    if (manager->state_mgmt.current != new_state) {
        manager->state_mgmt.current = new_state;
        if (manager->state_mgmt.state_change_callback) {
            manager->state_mgmt.state_change_callback(new_state);
        }
    }
}

static void process_sleep_request(NetworkManager* manager) {
    if (manager->power_mgmt.sleep_requested) {
        if (timer_expired(&manager->power_mgmt.sleep_timer)) {
            handle_state_change(manager, NODE_STATE_SLEEP);
        }
    }
}

static void process_wakeup_sources(NetworkManager* manager) {
    if (manager->config.support_partial_networking) {
        // Check wakeup pattern
        uint32_t wakeup_pattern = 0; // Get from hardware
        if ((wakeup_pattern & manager->config.pn_config.wakeup_mask) == 
            manager->config.pn_config.wakeup_source) {
            handle_state_change(manager, NODE_STATE_WAKEUP);
        }
    }
}

NetworkManager* network_manager_create(J1939Handler* j1939, 
                                    const NetworkConfig* config) {
    if (!j1939 || !config) return NULL;
    
    NetworkManager* manager = calloc(1, sizeof(NetworkManager));
    if (!manager) return NULL;
    
    manager->j1939 = j1939;
    memcpy(&manager->config, config, sizeof(NetworkConfig));
    
    manager->state_mgmt.current = NODE_STATE_NORMAL;
    init_critical(&manager->critical);
    
    return manager;
}

void network_manager_destroy(NetworkManager* manager) {
    if (!manager) return;
    destroy_critical(&manager->critical);
    free(manager);
}

void network_manager_process(NetworkManager* manager) {
    if (!manager) return;
    
    enter_critical(&manager->critical);
    
    switch (manager->state_mgmt.current) {
        case NODE_STATE_NORMAL:
            process_sleep_request(manager);
            break;
            
        case NODE_STATE_PREPARE_SLEEP:
            if (timer_expired(&manager->state_mgmt.state_timer)) {
                handle_state_change(manager, NODE_STATE_SLEEP);
            }
            break;
            
        case NODE_STATE_SLEEP:
            process_wakeup_sources(manager);
            break;
            
        case NODE_STATE_WAKEUP:
            if (timer_expired(&manager->state_mgmt.state_timer)) {
                handle_state_change(manager, NODE_STATE_NORMAL);
            }
            break;
    }
    
    exit_critical(&manager->critical);
}

NodeState network_manager_get_state(const NetworkManager* manager) {
    return manager ? manager->state_mgmt.current : NODE_STATE_NORMAL;
}

bool network_manager_request_sleep(NetworkManager* manager) {
    if (!manager) return false;
    
    enter_critical(&manager->critical);
    
    if (manager->state_mgmt.current == NODE_STATE_NORMAL) {
        manager->power_mgmt.sleep_requested = true;
        timer_start(&manager->power_mgmt.sleep_timer, 
                   manager->config.sleep_delay_ms);
        handle_state_change(manager, NODE_STATE_PREPARE_SLEEP);
        exit_critical(&manager->critical);
        return true;
    }
    
    exit_critical(&manager->critical);
    return false;
}

bool network_manager_request_wakeup(NetworkManager* manager) {
    if (!manager) return false;
    
    enter_critical(&manager->critical);
    
    if (manager->state_mgmt.current == NODE_STATE_SLEEP) {
        manager->power_mgmt.sleep_requested = false;
        timer_start(&manager->state_mgmt.state_timer, 100);  // 100ms wakeup time
        handle_state_change(manager, NODE_STATE_WAKEUP);
        exit_critical(&manager->critical);
        return true;
    }
    
    exit_critical(&manager->critical);
    return false;
}

void network_manager_set_state_change_callback(NetworkManager* manager,
                                             void (*callback)(NodeState)) {
    if (manager) {
        manager->state_mgmt.state_change_callback = callback;
    }
} 