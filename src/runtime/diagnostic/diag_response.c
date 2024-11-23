#include "diag_response.h"
#include "diag_timer.h"
#include "../memory/memory_manager.h"
#include "../diagnostic/logging/diag_logger.h"
#include <string.h>

// Maximum pending responses
// TODO: Make this configurable
#define MAX_PENDING_RESPONSES 16

// Response timeout (ms)
// FIXME: Some ECUs are slow to respond, might need to increase
#define RESPONSE_TIMEOUT 1000

typedef struct {
    ResponseQueueEntry queue[MAX_PENDING_RESPONSES];
    uint32_t queue_count;
    bool initialized;
} ResponseHandler;

static ResponseHandler resp_handler;

// Forward declarations
static ResponseQueueEntry* find_queue_entry(uint32_t msg_id);
static void cleanup_queue_entry(ResponseQueueEntry* entry);
static void handle_response_timeout(uint32_t timer_id, void* context);

// Debug counter for tracking response issues
#ifdef DEBUG_RESPONSES
static struct {
    uint32_t total_responses;
    uint32_t timeouts;
    uint32_t errors;
    uint32_t unexpected;
} response_stats;
#endif

bool DiagResponse_Init(void) {
    if (resp_handler.initialized) {
        return false;
    }
    
    memset(&resp_handler, 0, sizeof(ResponseHandler));
    resp_handler.initialized = true;
    
    #ifdef DEBUG_RESPONSES
    memset(&response_stats, 0, sizeof(response_stats));
    #endif
    
    return true;
}

void DiagResponse_Deinit(void) {
    if (!resp_handler.initialized) return;
    
    // Clean up any pending responses
    for (uint32_t i = 0; i < resp_handler.queue_count; i++) {
        ResponseQueueEntry* entry = &resp_handler.queue[i];
        if (entry->active) {
            cleanup_queue_entry(entry);
        }
    }
    
    #ifdef DEBUG_RESPONSES
    // Log response statistics
    Logger_Log(LOG_LEVEL_INFO, "RESPONSE", 
              "Response stats - Total: %u, Timeouts: %u, Errors: %u, Unexpected: %u",
              response_stats.total_responses,
              response_stats.timeouts,
              response_stats.errors,
              response_stats.unexpected);
    #endif
    
    memset(&resp_handler, 0, sizeof(ResponseHandler));
}

bool DiagResponse_QueueResponse(uint32_t msg_id, DiagResponseCallback callback,
                              void* context) 
{
    if (!resp_handler.initialized || !callback) {
        return false;
    }
    
    // Check if response is already queued
    if (find_queue_entry(msg_id)) {
        Logger_Log(LOG_LEVEL_WARNING, "RESPONSE", 
                  "Response already queued for message %u", msg_id);
        return false;
    }
    
    // Find free queue slot
    ResponseQueueEntry* entry = NULL;
    uint32_t slot = MAX_PENDING_RESPONSES;
    
    for (uint32_t i = 0; i < MAX_PENDING_RESPONSES; i++) {
        if (!resp_handler.queue[i].active) {
            entry = &resp_handler.queue[i];
            slot = i;
            break;
        }
    }
    
    if (!entry) {
        Logger_Log(LOG_LEVEL_ERROR, "RESPONSE", 
                  "Response queue full (%d entries)", MAX_PENDING_RESPONSES);
        return false;
    }
    
    // Initialize queue entry
    entry->msg_id = msg_id;
    entry->timestamp = DiagTimer_GetTimestamp();
    entry->callback = callback;
    entry->context = context;
    entry->state = RESP_STATE_WAITING;
    entry->active = true;
    
    // Start response timer
    uint32_t timer_id = DiagTimer_Start(TIMER_TYPE_REQUEST, 
                                      RESPONSE_TIMEOUT,
                                      handle_response_timeout,
                                      (void*)(uintptr_t)msg_id);
    
    if (!timer_id) {
        Logger_Log(LOG_LEVEL_ERROR, "RESPONSE", 
                  "Failed to start response timer");
        entry->active = false;
        return false;
    }
    
    if (slot >= resp_handler.queue_count) {
        resp_handler.queue_count = slot + 1;
    }
    
    return true;
}

bool DiagResponse_HandleResponse(const DiagResponse* response) {
    if (!resp_handler.initialized || !response) {
        return false;
    }
    
    #ifdef DEBUG_RESPONSES
    response_stats.total_responses++;
    #endif
    
    ResponseQueueEntry* entry = find_queue_entry(response->id);
    if (!entry) {
        #ifdef DEBUG_RESPONSES
        response_stats.unexpected++;
        #endif
        Logger_Log(LOG_LEVEL_WARNING, "RESPONSE", 
                  "Unexpected response for message %u", response->id);
        return false;
    }
    
    // Copy response data
    memcpy(&entry->response, response, sizeof(DiagResponse));
    
    // Update state
    entry->state = response->success ? RESP_STATE_RECEIVED : RESP_STATE_ERROR;
    
    #ifdef DEBUG_RESPONSES
    if (!response->success) {
        response_stats.errors++;
    }
    #endif
    
    // Call callback
    if (entry->callback) {
        entry->callback(&entry->response, entry->context);
    }
    
    // Cleanup entry
    cleanup_queue_entry(entry);
    
    return true;
}

static void handle_response_timeout(uint32_t timer_id, void* context) {
    uint32_t msg_id = (uint32_t)(uintptr_t)context;
    DiagResponse_HandleTimeout(msg_id);
}

void DiagResponse_HandleTimeout(uint32_t msg_id) {
    if (!resp_handler.initialized) return;
    
    ResponseQueueEntry* entry = find_queue_entry(msg_id);
    if (!entry) return;
    
    #ifdef DEBUG_RESPONSES
    response_stats.timeouts++;
    #endif
    
    Logger_Log(LOG_LEVEL_WARNING, "RESPONSE", 
              "Response timeout for message %u", msg_id);
    
    entry->state = RESP_STATE_TIMEOUT;
    
    // Create timeout response
    DiagResponse timeout_response = {
        .id = msg_id,
        .service_id = entry->response.service_id,
        .response_code = DIAG_RESP_GENERAL_REJECT,
        .success = false,
        .timestamp = DiagTimer_GetTimestamp()
    };
    
    // Call callback with timeout response
    if (entry->callback) {
        entry->callback(&timeout_response, entry->context);
    }
    
    cleanup_queue_entry(entry);
}

static ResponseQueueEntry* find_queue_entry(uint32_t msg_id) {
    for (uint32_t i = 0; i < resp_handler.queue_count; i++) {
        if (resp_handler.queue[i].active && 
            resp_handler.queue[i].msg_id == msg_id) {
            return &resp_handler.queue[i];
        }
    }
    return NULL;
}

static void cleanup_queue_entry(ResponseQueueEntry* entry) {
    if (!entry) return;
    
    // Free response data if allocated
    if (entry->response.data) {
        MEMORY_FREE(entry->response.data);
    }
    
    memset(entry, 0, sizeof(ResponseQueueEntry));
    
    // Update queue count if possible
    while (resp_handler.queue_count > 0 && 
           !resp_handler.queue[resp_handler.queue_count - 1].active) {
        resp_handler.queue_count--;
    }
} 