#ifndef CANT_OS_TYPES_H
#define CANT_OS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// AUTOSAR OS Status Types
typedef enum {
    E_OK = 0,
    E_OS_ACCESS,
    E_OS_CALLEVEL,
    E_OS_ID,
    E_OS_LIMIT,
    E_OS_NOFUNC,
    E_OS_RESOURCE,
    E_OS_STATE,
    E_OS_VALUE
} StatusType;

// Task States
typedef enum {
    SUSPENDED,
    READY,
    RUNNING,
    WAITING
} TaskStateType;

// Event Masks
typedef uint32_t EventMaskType;
typedef uint32_t TaskType;
typedef uint32_t ResourceType;
typedef uint32_t AlarmType;
typedef uint32_t CounterType;
typedef uint32_t AppModeType;

// Task Priorities
typedef uint8_t TaskPrioType;

// Resource Properties
typedef struct {
    TaskPrioType ceiling_priority;
    bool is_internal;
} ResourceConfigType;

// Task Properties
typedef struct {
    void (*entry)(void);
    TaskPrioType priority;
    uint32_t stack_size;
    bool is_extended;
    bool is_autostart;
    EventMaskType events;
    ResourceType* resources;
    uint8_t resource_count;
} TaskConfigType;

// Alarm Properties
typedef struct {
    CounterType counter_id;
    uint32_t cycle_time;
    uint32_t offset;
    void (*action)(void);
    TaskType task_id;
    EventMaskType event;
} AlarmConfigType;

// Counter Properties
typedef struct {
    uint32_t ticks_per_base;
    uint32_t max_allowed_value;
    uint32_t min_cycle;
} CounterConfigType;

#endif // CANT_OS_TYPES_H 