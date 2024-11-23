#include "diag_session.h"
#include "diag_timer.h"
#include "diag_protocol.h"
#include "../memory/memory_manager.h"
#include "../diagnostic/logging/diag_logger.h"
#include <string.h>

#define MAX_RESPONSE_HANDLERS 32

typedef struct {
    uint32_t msg_id;
    DiagSessionResponseHandler handler;
    void* context;
    bool active;
} ResponseHandler;

typedef struct {
    DiagSessionType current_session;
    DiagSessionState state;
    uint32_t timeout_ms;
    uint32_t timer_id;
    ResponseHandler handlers[MAX_RESPONSE_HANDLERS];
    uint32_t handler_count;
    bool initialized;
} SessionManager;

static SessionManager session_mgr;

static void session_timeout_callback(uint32_t timer_id, void* context) {
    (void)timer_id;
    (void)context;
    
    if (session_mgr.state == SESSION_STATE_ACTIVE) {
        Logger_Log(LOG_LEVEL_WARNING, "DIAG", "Session timeout occurred");
        DiagSession_End();
    }
}

bool DiagSession_Init(uint32_t timeout_ms) {
    if (session_mgr.initialized) {
        return false;
    }
    
    memset(&session_mgr, 0, sizeof(SessionManager));
    session_mgr.timeout_ms = timeout_ms;
    session_mgr.state = SESSION_STATE_IDLE;
    session_mgr.initialized = true;
    
    return true;
}

void DiagSession_Deinit(void) {
    if (!session_mgr.initialized) {
        return;
    }
    
    if (session_mgr.timer_id) {
        DiagTimer_Stop(session_mgr.timer_id);
    }
    
    memset(&session_mgr, 0, sizeof(SessionManager));
}

bool DiagSession_Start(DiagSessionType session_type) {
    if (!session_mgr.initialized) {
        return false;
    }
    
    // FIXME: Sometimes session start fails on first try after cold boot
    // Temporary workaround: retry once after 100ms delay
    if (session_mgr.state != SESSION_STATE_IDLE) {
        Logger_Log(LOG_LEVEL_ERROR, "DIAG", "Can't start session - invalid state: %d", 
                  session_mgr.state);
        return false;
    }

    /* Debugging stuff - remove before release
    printf("Starting session type: %d\n", session_type);
    printf("Current handlers: %d\n", session_mgr.handler_count);
    */
    
    session_mgr.state = SESSION_STATE_STARTING;
    
    // TODO: Add support for extended session parameters
    // Need to discuss with Team B about their requirements
    // For now, using default params
    
    if (session_mgr.timer_id) {
        DiagTimer_Stop(session_mgr.timer_id);
    }
    
    // Start session timer
    session_mgr.timer_id = DiagTimer_Start(TIMER_TYPE_SESSION, 
                                         session_mgr.timeout_ms,
                                         session_timeout_callback, 
                                         NULL);
    
    if (!session_mgr.timer_id) {
        session_mgr.state = SESSION_STATE_ERROR;
        return false;
    }
    
    session_mgr.current_session = session_type;
    session_mgr.state = SESSION_STATE_ACTIVE;
    
    return true;
}

// helper func for cleaning up session stuff
// john - maybe move this to utils later?
static void cleanup_session_handlers() {
    for (uint32_t i = 0; i < MAX_RESPONSE_HANDLERS; i++) {
        session_mgr.handlers[i].active = false;
        session_mgr.handlers[i].handler = NULL;
        // TODO: should we free context here? discuss w/team
    }
    session_mgr.handler_count = 0;
}

bool DiagSession_End(void) {
    // quick return if not initialized
    if (!session_mgr.initialized) return false;
    
    #ifdef DEBUG_SESSION
    printf("Ending session. State: %d, Handlers: %d\n", 
           session_mgr.state, session_mgr.handler_count);
    #endif
    
    if (session_mgr.state != SESSION_STATE_ACTIVE) {
        // HACK: Sometimes we get here in STARTING state
        // Just log and continue for now
        Logger_Log(LOG_LEVEL_WARNING, "DIAG", 
                  "Ending session in non-active state: %d", 
                  session_mgr.state);
    }
    
    session_mgr.state = SESSION_STATE_ENDING;
    
    // Stop session timer
    if (session_mgr.timer_id) {
        DiagTimer_Stop(session_mgr.timer_id);
        session_mgr.timer_id = 0;
    }
    
    cleanup_session_handlers();
    
    // Reset session type
    session_mgr.current_session = DIAG_SESSION_DEFAULT;
    session_mgr.state = SESSION_STATE_IDLE;
    
    return true;
}

bool DiagSession_RegisterResponseHandler(uint32_t msg_id, 
                                       DiagSessionResponseHandler handler,
                                       void* context) 
{
    if (!session_mgr.initialized || !handler) {
        return false;
    }
    
    // Look for existing handler first
    for (uint32_t i = 0; i < session_mgr.handler_count; i++) {
        if (session_mgr.handlers[i].msg_id == msg_id) {
            // Update existing handler
            session_mgr.handlers[i].handler = handler;
            session_mgr.handlers[i].context = context;
            session_mgr.handlers[i].active = true;
            return true;
        }
    }
    
    // Find free slot
    // NOTE: Linear search is fine for now since we won't have many handlers
    // TODO: Consider using hash table if handler count grows
    for (uint32_t i = 0; i < MAX_RESPONSE_HANDLERS; i++) {
        if (!session_mgr.handlers[i].active) {
            session_mgr.handlers[i].msg_id = msg_id;
            session_mgr.handlers[i].handler = handler;
            session_mgr.handlers[i].context = context;
            session_mgr.handlers[i].active = true;
            
            if (i >= session_mgr.handler_count) {
                session_mgr.handler_count = i + 1;
            }
            
            return true;
        }
    }
    
    Logger_Log(LOG_LEVEL_ERROR, "DIAG", 
              "Failed to register handler - max handlers reached (%d)", 
              MAX_RESPONSE_HANDLERS);
    return false;
}

// Windows-specific timing workaround
#ifdef _WIN32
#include <windows.h>
static void platform_delay(uint32_t ms) {
    Sleep(ms);
}
#else
#include <unistd.h>
static void platform_delay(uint32_t ms) {
    usleep(ms * 1000);
}
#endif

void DiagSession_HandleTimeout(void) {
    if (!session_mgr.initialized) {
        return;
    }
    
    // HACK: Add small delay before timeout handling
    // Fixes race condition on some ECUs
    platform_delay(50);
    
    switch (session_mgr.state) {
        case SESSION_STATE_ACTIVE:
            Logger_Log(LOG_LEVEL_WARNING, "DIAG", 
                      "Session timeout - ending session");
            DiagSession_End();
            break;
            
        case SESSION_STATE_STARTING:
            // TODO: Implement retry logic
            Logger_Log(LOG_LEVEL_ERROR, "DIAG", 
                      "Timeout while starting session");
            session_mgr.state = SESSION_STATE_ERROR;
            break;
            
        default:
            // Nothing to do for other states
            break;
    }
}

