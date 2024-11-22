#ifndef CANT_RTE_TYPES_H
#define CANT_RTE_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// RTE Status Types
typedef enum {
    RTE_E_OK = 0,
    RTE_E_INVALID,
    RTE_E_TIMEOUT,
    RTE_E_MAX_AGE_EXCEEDED,
    RTE_E_NO_DATA,
    RTE_E_LOST_DATA,
    RTE_E_LIMIT,
    RTE_E_NOT_OK
} Rte_StatusType;

// Port Interface Types
typedef enum {
    RTE_PORT_SENDER_RECEIVER,
    RTE_PORT_CLIENT_SERVER,
    RTE_PORT_PARAMETER,
    RTE_PORT_MODE_SWITCH
} Rte_PortType;

// Data Element Properties
typedef struct {
    void* data;
    uint32_t size;
    uint32_t init_value;
    bool is_array;
    bool has_getter;
    bool has_setter;
} Rte_DEProperty;

// Port Interface Properties
typedef struct {
    Rte_PortType type;
    uint32_t element_count;
    Rte_DEProperty* elements;
    void* impl_ref;
} Rte_PortProperty;

// Runnable Properties
typedef struct {
    void (*function)(void);
    uint32_t* trigger_ports;
    uint32_t trigger_count;
    uint32_t* data_read_ports;
    uint32_t read_count;
    uint32_t* data_write_ports;
    uint32_t write_count;
    bool can_be_invoked;
    bool minimum_start_interval;
} Rte_RunnableProperty;

// Component Properties
typedef struct {
    uint32_t* required_ports;
    uint32_t required_count;
    uint32_t* provided_ports;
    uint32_t provided_count;
    Rte_RunnableProperty* runnables;
    uint32_t runnable_count;
} Rte_ComponentProperty;

#endif // CANT_RTE_TYPES_H 