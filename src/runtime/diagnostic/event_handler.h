#ifndef CANT_EVENT_HANDLER_H
#define CANT_EVENT_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "dtc_manager.h"

// Event Types
typedef enum {
    DIAG_EVENT_ERROR,
    DIAG_EVENT_WARNING,
    DIAG_EVENT_INFO,
    DIAG_EVENT_DEBUG
} DiagEventType;

// Event Priority
typedef enum {
    DIAG_PRIORITY_HIGH,
    DIAG_PRIORITY_MEDIUM,
    DIAG_PRIORITY_LOW
} DiagEventPriority;

// Event Data
typedef struct {
    uint32_t event_id;
    DiagEventType type;
    DiagEventPriority priority;
    uint32_t timestamp;
    uint32_t dtc;
    uint8_t* data;
    uint16_t data_size;
    char description[128];
} DiagEventData;

// Event Handler Configuration
typedef struct {
    uint32_t max_events;
    uint32_t max_event_data_size;
    bool enable_event_logging;
    bool enable_auto_dtc;
    void (*event_callback)(const DiagEventData* event);
} DiagEventConfig;

// Event Handler API
bool Event_Handler_Init(const DiagEventConfig* config);
void Event_Handler_DeInit(void);
bool Event_Handler_ReportEvent(const DiagEventData* event);
bool Event_Handler_GetEvent(uint32_t event_id, DiagEventData* event);
uint32_t Event_Handler_GetEventCount(void);
void Event_Handler_ClearEvents(void);
bool Event_Handler_ProcessEvent(uint32_t event_id);
DiagEventType Event_Handler_GetEventType(uint32_t event_id);
bool Event_Handler_SetEventPriority(uint32_t event_id, DiagEventPriority priority);
bool Event_Handler_IsEventActive(uint32_t event_id);
uint32_t Event_Handler_GetActiveEvents(DiagEventData* events, uint32_t max_events);
void Event_Handler_ProcessAllEvents(void);

#endif // CANT_EVENT_HANDLER_H 