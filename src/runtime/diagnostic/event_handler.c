#include "event_handler.h"
#include <string.h>
#include "../utils/timer.h"
#include "../os/critical.h"
#include "../memory/memory_manager.h"

#define MAX_EVENTS 1000
#define MAX_EVENT_DATA_SIZE 512

// Internal event storage
typedef struct {
    DiagEventData* events;
    uint32_t event_count;
    uint32_t max_events;
    uint8_t* event_data_buffer;
    uint32_t event_data_buffer_size;
    DiagEventConfig config;
    CriticalSection critical;
    bool initialized;
} EventStorage;

static EventStorage event_storage;

static bool validate_event_id(uint32_t event_id) {
    for (uint32_t i = 0; i < event_storage.event_count; i++) {
        if (event_storage.events[i].event_id == event_id) {
            return true;
        }
    }
    return false;
}

static DiagEventData* find_event(uint32_t event_id) {
    for (uint32_t i = 0; i < event_storage.event_count; i++) {
        if (event_storage.events[i].event_id == event_id) {
            return &event_storage.events[i];
        }
    }
    return NULL;
}

static bool allocate_event_data(DiagEventData* event, uint16_t size) {
    if (size > event_storage.config.max_event_data_size) {
        return false;
    }

    uint32_t offset = 0;
    for (uint32_t i = 0; i < event_storage.event_count; i++) {
        offset += event_storage.events[i].data_size;
    }

    if (offset + size > event_storage.event_data_buffer_size) {
        return false;
    }

    event->data = &event_storage.event_data_buffer[offset];
    event->data_size = size;
    return true;
}

bool Event_Handler_Init(const DiagEventConfig* config) {
    if (!config || config->max_events == 0 || 
        config->max_events > MAX_EVENTS ||
        config->max_event_data_size > MAX_EVENT_DATA_SIZE) {
        return false;
    }

    enter_critical(&event_storage.critical);

    // Allocate memory for events
    event_storage.events = (DiagEventData*)memory_allocate(
        sizeof(DiagEventData) * config->max_events);
    if (!event_storage.events) {
        exit_critical(&event_storage.critical);
        return false;
    }

    // Allocate memory for event data buffer
    uint32_t total_data_size = config->max_events * config->max_event_data_size;
    event_storage.event_data_buffer = (uint8_t*)memory_allocate(total_data_size);
    if (!event_storage.event_data_buffer) {
        memory_free(event_storage.events);
        exit_critical(&event_storage.critical);
        return false;
    }

    memset(event_storage.events, 0, sizeof(DiagEventData) * config->max_events);
    memset(event_storage.event_data_buffer, 0, total_data_size);

    memcpy(&event_storage.config, config, sizeof(DiagEventConfig));
    event_storage.event_count = 0;
    event_storage.max_events = config->max_events;
    event_storage.event_data_buffer_size = total_data_size;
    event_storage.initialized = true;

    exit_critical(&event_storage.critical);
    return true;
}

void Event_Handler_DeInit(void) {
    enter_critical(&event_storage.critical);

    if (event_storage.events) {
        memory_free(event_storage.events);
    }
    if (event_storage.event_data_buffer) {
        memory_free(event_storage.event_data_buffer);
    }

    memset(&event_storage, 0, sizeof(EventStorage));

    exit_critical(&event_storage.critical);
}

bool Event_Handler_ReportEvent(const DiagEventData* event) {
    if (!event_storage.initialized || !event) {
        return false;
    }

    enter_critical(&event_storage.critical);

    // Check if event already exists
    DiagEventData* existing = find_event(event->event_id);
    if (existing) {
        // Update existing event
        existing->type = event->type;
        existing->priority = event->priority;
        existing->timestamp = get_system_time_ms();
        
        if (event->data && event->data_size > 0) {
            if (existing->data_size >= event->data_size) {
                memcpy(existing->data, event->data, event->data_size);
            } else {
                if (!allocate_event_data(existing, event->data_size)) {
                    exit_critical(&event_storage.critical);
                    return false;
                }
                memcpy(existing->data, event->data, event->data_size);
            }
        }
        
        strncpy(existing->description, event->description, sizeof(existing->description) - 1);
    } else {
        // Add new event
        if (event_storage.event_count >= event_storage.max_events) {
            exit_critical(&event_storage.critical);
            return false;
        }

        DiagEventData* new_event = &event_storage.events[event_storage.event_count];
        memcpy(new_event, event, sizeof(DiagEventData));
        new_event->timestamp = get_system_time_ms();

        if (event->data && event->data_size > 0) {
            if (!allocate_event_data(new_event, event->data_size)) {
                exit_critical(&event_storage.critical);
                return false;
            }
            memcpy(new_event->data, event->data, event->data_size);
        }

        event_storage.event_count++;
    }

    // Handle automatic DTC creation if enabled
    if (event_storage.config.enable_auto_dtc && event->type == DIAG_EVENT_ERROR) {
        DTC_SetStatus(event->dtc, DTC_STATUS_TEST_FAILED | DTC_STATUS_CONFIRMED);
        if (event->data && event->data_size > 0) {
            DTC_AddFreezeFrame(event->dtc, event->data, event->data_size);
        }
    }

    // Notify callback if registered
    if (event_storage.config.event_callback) {
        event_storage.config.event_callback(event);
    }

    exit_critical(&event_storage.critical);
    return true;
}

bool Event_Handler_GetEvent(uint32_t event_id, DiagEventData* event) {
    if (!event_storage.initialized || !event || !validate_event_id(event_id)) {
        return false;
    }

    enter_critical(&event_storage.critical);
    DiagEventData* found = find_event(event_id);
    if (found) {
        memcpy(event, found, sizeof(DiagEventData));
    }
    exit_critical(&event_storage.critical);

    return found != NULL;
}

uint32_t Event_Handler_GetEventCount(void) {
    return event_storage.event_count;
}

void Event_Handler_ClearEvents(void) {
    if (!event_storage.initialized) {
        return;
    }

    enter_critical(&event_storage.critical);
    memset(event_storage.events, 0, sizeof(DiagEventData) * event_storage.max_events);
    memset(event_storage.event_data_buffer, 0, event_storage.event_data_buffer_size);
    event_storage.event_count = 0;
    exit_critical(&event_storage.critical);
}

bool Event_Handler_ProcessEvent(uint32_t event_id) {
    if (!event_storage.initialized || !validate_event_id(event_id)) {
        return false;
    }

    enter_critical(&event_storage.critical);
    DiagEventData* event = find_event(event_id);
    
    if (event) {
        // Process based on event type
        switch (event->type) {
            case DIAG_EVENT_ERROR:
                // Update DTC if auto-DTC is enabled
                if (event_storage.config.enable_auto_dtc) {
                    DTC_SetStatus(event->dtc, DTC_STATUS_TEST_FAILED | DTC_STATUS_CONFIRMED);
                }
                break;

            case DIAG_EVENT_WARNING:
                // Set warning indicator if applicable
                if (event->dtc) {
                    DTC_SetStatus(event->dtc, DTC_STATUS_WARNING_INDICATOR_REQUESTED);
                }
                break;

            case DIAG_EVENT_INFO:
            case DIAG_EVENT_DEBUG:
                // Log event if logging is enabled
                if (event_storage.config.enable_event_logging) {
                    // Implement logging mechanism here
                }
                break;
        }
    }

    exit_critical(&event_storage.critical);
    return event != NULL;
}

DiagEventType Event_Handler_GetEventType(uint32_t event_id) {
    if (!event_storage.initialized || !validate_event_id(event_id)) {
        return DIAG_EVENT_DEBUG; // Return lowest severity as default
    }

    DiagEventData* event = find_event(event_id);
    return event ? event->type : DIAG_EVENT_DEBUG;
}

bool Event_Handler_SetEventPriority(uint32_t event_id, DiagEventPriority priority) {
    if (!event_storage.initialized || !validate_event_id(event_id)) {
        return false;
    }

    enter_critical(&event_storage.critical);
    DiagEventData* event = find_event(event_id);
    if (event) {
        event->priority = priority;
    }
    exit_critical(&event_storage.critical);

    return event != NULL;
}

bool Event_Handler_IsEventActive(uint32_t event_id) {
    if (!event_storage.initialized || !validate_event_id(event_id)) {
        return false;
    }

    DiagEventData* event = find_event(event_id);
    if (!event) {
        return false;
    }

    // Check if event is still relevant based on type
    uint32_t current_time = get_system_time_ms();
    uint32_t age = current_time - event->timestamp;

    switch (event->type) {
        case DIAG_EVENT_ERROR:
            // Errors are active until cleared
            return true;

        case DIAG_EVENT_WARNING:
            // Warnings are active for 1 hour
            return age < 3600000;

        case DIAG_EVENT_INFO:
            // Info events are active for 10 minutes
            return age < 600000;

        case DIAG_EVENT_DEBUG:
            // Debug events are active for 1 minute
            return age < 60000;

        default:
            return false;
    }
}

uint32_t Event_Handler_GetActiveEvents(DiagEventData* events, uint32_t max_events) {
    if (!event_storage.initialized || !events || max_events == 0) {
        return 0;
    }

    uint32_t active_count = 0;
    enter_critical(&event_storage.critical);

    for (uint32_t i = 0; i < event_storage.event_count && active_count < max_events; i++) {
        if (Event_Handler_IsEventActive(event_storage.events[i].event_id)) {
            memcpy(&events[active_count], &event_storage.events[i], sizeof(DiagEventData));
            active_count++;
        }
    }

    exit_critical(&event_storage.critical);
    return active_count;
}

void Event_Handler_ProcessAllEvents(void) {
    if (!event_storage.initialized) {
        return;
    }

    enter_critical(&event_storage.critical);

    // Process events in priority order
    for (DiagEventPriority priority = DIAG_PRIORITY_HIGH; 
         priority <= DIAG_PRIORITY_LOW; 
         priority++) {
        
        for (uint32_t i = 0; i < event_storage.event_count; i++) {
            if (event_storage.events[i].priority == priority) {
                Event_Handler_ProcessEvent(event_storage.events[i].event_id);
            }
        }
    }

    // Clean up old events
    uint32_t current_time = get_system_time_ms();
    for (uint32_t i = 0; i < event_storage.event_count; i++) {
        if (!Event_Handler_IsEventActive(event_storage.events[i].event_id)) {
            // Remove inactive event
            if (i < event_storage.event_count - 1) {
                memmove(&event_storage.events[i], 
                       &event_storage.events[i + 1],
                       sizeof(DiagEventData) * (event_storage.event_count - i - 1));
            }
            event_storage.event_count--;
            i--; // Adjust index after removal
        }
    }

    exit_critical(&event_storage.critical);
}

