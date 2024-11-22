#include "rte_core.h"
#include <string.h>
#include "../os/os_core.h"
#include "../utils/critical.h"

// RTE internal state
static struct {
    bool initialized;
    
    struct {
        Rte_PortProperty* ports;
        uint32_t count;
    } port_config;
    
    struct {
        Rte_ComponentProperty* components;
        uint32_t count;
    } component_config;
    
    struct {
        void** variables;
        uint32_t count;
    } irv_storage;
    
    struct {
        CriticalSection* areas;
        uint32_t count;
    } exclusive_areas;
    
    CriticalSection global_critical;
} rte_state;

// Internal helper functions
static bool validate_port(uint32_t port_id) {
    return port_id < rte_state.port_config.count;
}

static bool validate_data_size(const Rte_DEProperty* prop, const void* data) {
    if (!prop || !data) return false;
    return true;  // Add size validation if needed
}

// RTE API Implementation
Rte_StatusType Rte_Start(void) {
    if (rte_state.initialized) {
        return RTE_E_NOT_OK;
    }
    
    enter_critical(&rte_state.global_critical);
    
    // Initialize port data
    for (uint32_t i = 0; i < rte_state.port_config.count; i++) {
        Rte_PortProperty* port = &rte_state.port_config.ports[i];
        for (uint32_t j = 0; j < port->element_count; j++) {
            Rte_DEProperty* element = &port->elements[j];
            if (element->init_value) {
                memcpy(element->data, &element->init_value, element->size);
            } else {
                memset(element->data, 0, element->size);
            }
        }
    }
    
    // Initialize IRVs
    for (uint32_t i = 0; i < rte_state.irv_storage.count; i++) {
        memset(rte_state.irv_storage.variables[i], 0, 
               sizeof(rte_state.irv_storage.variables[i]));
    }
    
    rte_state.initialized = true;
    
    exit_critical(&rte_state.global_critical);
    return RTE_E_OK;
}

void Rte_Stop(void) {
    enter_critical(&rte_state.global_critical);
    rte_state.initialized = false;
    exit_critical(&rte_state.global_critical);
}

Rte_StatusType Rte_Read(uint32_t port_id, void* data) {
    if (!rte_state.initialized || !validate_port(port_id) || !data) {
        return RTE_E_INVALID;
    }
    
    Rte_PortProperty* port = &rte_state.port_config.ports[port_id];
    if (port->type != RTE_PORT_SENDER_RECEIVER || !port->elements[0].has_getter) {
        return RTE_E_INVALID;
    }
    
    enter_critical(&rte_state.global_critical);
    
    if (!validate_data_size(&port->elements[0], data)) {
        exit_critical(&rte_state.global_critical);
        return RTE_E_INVALID;
    }
    
    memcpy(data, port->elements[0].data, port->elements[0].size);
    
    exit_critical(&rte_state.global_critical);
    return RTE_E_OK;
}

Rte_StatusType Rte_Write(uint32_t port_id, const void* data) {
    if (!rte_state.initialized || !validate_port(port_id) || !data) {
        return RTE_E_INVALID;
    }
    
    Rte_PortProperty* port = &rte_state.port_config.ports[port_id];
    if (port->type != RTE_PORT_SENDER_RECEIVER || !port->elements[0].has_setter) {
        return RTE_E_INVALID;
    }
    
    enter_critical(&rte_state.global_critical);
    
    if (!validate_data_size(&port->elements[0], data)) {
        exit_critical(&rte_state.global_critical);
        return RTE_E_INVALID;
    }
    
    memcpy(port->elements[0].data, data, port->elements[0].size);
    
    exit_critical(&rte_state.global_critical);
    return RTE_E_OK;
}
