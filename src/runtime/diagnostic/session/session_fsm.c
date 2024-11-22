#include "session_fsm.h"
#include "../logging/diag_logger.h"
#include "../os/critical.h"
#include "../os/timer.h"
#include <string.h>

#define MAX_SESSIONS 16
#define SESSION_TIMEOUT_CHECK_INTERVAL_MS 100

typedef struct {
    SessionContext sessions[MAX_SESSIONS];
    SessionFSMConfig config;
    uint32_t next_session_id;
    uint32_t active_sessions;
    uint32_t last_timeout_check;
    bool initialized;
} SessionFSMContext;

static SessionFSMContext fsm_ctx;

// State transition table
typedef struct {
    SessionState next_state;
    bool (*handler)(SessionContext* ctx, const void* data);
} StateTransition;

static StateTransition state_transitions[SESSION_STATE_COUNT][SESSION_EVENT_COUNT];

// Forward declarations of state handlers
static bool handle_default_timeout(SessionContext* ctx, const void* data);
static bool handle_default_request(SessionContext* ctx, const void* data);
static bool handle_programming_security(SessionContext* ctx, const void* data);
static bool handle_extended_request(SessionContext* ctx, const void* data);
static bool handle_safety_error(SessionContext* ctx, const void* data);
static bool handle_supplier_response(SessionContext* ctx, const void* data);
static bool handle_eol_reset(SessionContext* ctx, const void* data);

static void initialize_state_transitions(void) {
    memset(state_transitions, 0, sizeof(state_transitions));

    // Default state transitions
    state_transitions[SESSION_STATE_DEFAULT][SESSION_EVENT_TIMEOUT].next_state = SESSION_STATE_DEFAULT;
    state_transitions[SESSION_STATE_DEFAULT][SESSION_EVENT_TIMEOUT].handler = handle_default_timeout;
    state_transitions[SESSION_STATE_DEFAULT][SESSION_EVENT_REQUEST].next_state = SESSION_STATE_EXTENDED;
    state_transitions[SESSION_STATE_DEFAULT][SESSION_EVENT_REQUEST].handler = handle_default_request;

    // Programming state transitions
    state_transitions[SESSION_STATE_PROGRAMMING][SESSION_EVENT_SECURITY_ACCESS].next_state = SESSION_STATE_PROGRAMMING;
    state_transitions[SESSION_STATE_PROGRAMMING][SESSION_EVENT_SECURITY_ACCESS].handler = handle_programming_security;

    // Extended state transitions
    state_transitions[SESSION_STATE_EXTENDED][SESSION_EVENT_REQUEST].next_state = SESSION_STATE_EXTENDED;
    state_transitions[SESSION_STATE_EXTENDED][SESSION_EVENT_REQUEST].handler = handle_extended_request;

    // Safety state transitions
    state_transitions[SESSION_STATE_SAFETY][SESSION_EVENT_ERROR].next_state = SESSION_STATE_DEFAULT;
    state_transitions[SESSION_STATE_SAFETY][SESSION_EVENT_ERROR].handler = handle_safety_error;

    // Supplier state transitions
    state_transitions[SESSION_STATE_SUPPLIER][SESSION_EVENT_RESPONSE].next_state = SESSION_STATE_SUPPLIER;
    state_transitions[SESSION_STATE_SUPPLIER][SESSION_EVENT_RESPONSE].handler = handle_supplier_response;

    // EOL state transitions
    state_transitions[SESSION_STATE_EOL][SESSION_EVENT_RESET].next_state = SESSION_STATE_DEFAULT;
    state_transitions[SESSION_STATE_EOL][SESSION_EVENT_RESET].handler = handle_eol_reset;
}

static bool handle_default_timeout(SessionContext* ctx, const void* data) {
    (void)data;
    Logger_LogSession(ctx->session_id, LOG_LEVEL_INFO, "SESSION", 
                     "Default session timeout, resetting state");
    ctx->security_level = 0;
    ctx->pending_did = 0;
    ctx->pending_routine = 0;
    ctx->routine_active = false;
    ctx->error_counter = 0;
    return true;
}

static bool handle_default_request(SessionContext* ctx, const void* data) {
    const uint8_t* request = (const uint8_t*)data;
    Logger_LogSession(ctx->session_id, LOG_LEVEL_DEBUG, "SESSION", 
                     "Processing request in default session");
    
    if (request[0] == 0x10) { // Diagnostic Session Control
        ctx->last_activity_time = Timer_GetMilliseconds();
        return true;
    }
    return false;
}

static bool handle_programming_security(SessionContext* ctx, const void* data) {
    const uint8_t* security_data = (const uint8_t*)data;
    Logger_LogSession(ctx->session_id, LOG_LEVEL_INFO, "SESSION", 
                     "Processing security access in programming session");
    
    if (ctx->error_counter >= fsm_ctx.config.max_error_count) {
        Logger_LogSession(ctx->session_id, LOG_LEVEL_WARNING, "SESSION", 
                         "Security access denied - max attempts exceeded");
        return false;
    }
    
    // Validate security access
    if (security_data[0] == 0x27) {
        ctx->security_level = security_data[1];
        ctx->error_counter = 0;
        return true;
    }
    
    ctx->error_counter++;
    return false;
}

static bool handle_extended_request(SessionContext* ctx, const void* data) {
    const uint8_t* request = (const uint8_t*)data;
    Logger_LogSession(ctx->session_id, LOG_LEVEL_DEBUG, "SESSION", 
                     "Processing request in extended session");
    
    if (fsm_ctx.config.require_security_access && ctx->security_level == 0) {
        Logger_LogSession(ctx->session_id, LOG_LEVEL_WARNING, "SESSION", 
                         "Security access required but not granted");
        return false;
    }
    
    ctx->last_activity_time = Timer_GetMilliseconds();
    return true;
}

static bool handle_safety_error(SessionContext* ctx, const void* data) {
    const uint8_t* error = (const uint8_t*)data;
    Logger_LogSession(ctx->session_id, LOG_LEVEL_ERROR, "SESSION", 
                     "Safety error occurred, error code: 0x%02X", error[0]);
    
    ctx->error_counter++;
    if (ctx->error_counter >= fsm_ctx.config.max_error_count) {
        Logger_LogSession(ctx->session_id, LOG_LEVEL_CRITICAL, "SESSION", 
                         "Max error count exceeded, forcing default session");
        return true;
    }
    return false;
}

static bool handle_supplier_response(SessionContext* ctx, const void* data) {
    const uint8_t* response = (const uint8_t*)data;
    Logger_LogSession(ctx->session_id, LOG_LEVEL_DEBUG, "SESSION", 
                     "Processing supplier specific response");
    
    if (!fsm_ctx.config.allow_nested_response && ctx->pending_did != 0) {
        Logger_LogSession(ctx->session_id, LOG_LEVEL_WARNING, "SESSION", 
                         "Nested response not allowed");
        return false;
    }
    
    ctx->last_activity_time = Timer_GetMilliseconds();
    return true;
}

static bool handle_eol_reset(SessionContext* ctx, const void* data) {
    (void)data;
    Logger_LogSession(ctx->session_id, LOG_LEVEL_INFO, "SESSION", 
                     "EOL session reset requested");
    
    ctx->security_level = 0;
    ctx->pending_did = 0;
    ctx->pending_routine = 0;
    ctx->routine_active = false;
    ctx->error_counter = 0;
    return true;
}

bool Session_FSM_Init(const SessionFSMConfig* config) {
    if (!config) {
        return false;
    }

    memset(&fsm_ctx, 0, sizeof(SessionFSMContext));
    memcpy(&fsm_ctx.config, config, sizeof(SessionFSMConfig));
    
    initialize_state_transitions();
    
    fsm_ctx.next_session_id = 1;
    fsm_ctx.last_timeout_check = Timer_GetMilliseconds();
    fsm_ctx.initialized = true;
    
    Logger_Log(LOG_LEVEL_INFO, "SESSION", "Session FSM initialized");
    return true;
}

void Session_FSM_Deinit(void) {
    Logger_Log(LOG_LEVEL_INFO, "SESSION", "Session FSM deinitialized");
    memset(&fsm_ctx, 0, sizeof(SessionFSMContext));
}

bool Session_FSM_CreateSession(uint32_t* session_id) {
    if (!fsm_ctx.initialized || !session_id) {
        return false;
    }

    enter_critical();
    
    if (fsm_ctx.active_sessions >= MAX_SESSIONS) {
        exit_critical();
        Logger_Log(LOG_LEVEL_ERROR, "SESSION", "Max sessions limit reached");
        return false;
    }

    // Find free session slot
    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        if (fsm_ctx.sessions[i].session_id == 0) {
            fsm_ctx.sessions[i].session_id = fsm_ctx.next_session_id++;
            fsm_ctx.sessions[i].current_state = SESSION_STATE_DEFAULT;
            fsm_ctx.sessions[i].state_entry_time = Timer_GetMilliseconds();
            fsm_ctx.sessions[i].last_activity_time = Timer_GetMilliseconds();
            
            *session_id = fsm_ctx.sessions[i].session_id;
            fsm_ctx.active_sessions++;
            
            exit_critical();
            Logger_LogSession(*session_id, LOG_LEVEL_INFO, "SESSION", "New session created");
            return true;
        }
    }

    exit_critical();
    return false;
}

bool Session_FSM_DestroySession(uint32_t session_id) {
    if (!fsm_ctx.initialized || session_id == 0) {
        return false;
    }

    enter_critical();
    
    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        if (fsm_ctx.sessions[i].session_id == session_id) {
            Logger_LogSession(session_id, LOG_LEVEL_INFO, "SESSION", "Session destroyed");
            memset(&fsm_ctx.sessions[i], 0, sizeof(SessionContext));
            fsm_ctx.active_sessions--;
            exit_critical();
            return true;
        }
    }

    exit_critical();
    return false;
}

bool Session_FSM_HandleEvent(uint32_t session_id, SessionEvent event, const void* event_data) {
    if (!fsm_ctx.initialized || event >= SESSION_EVENT_COUNT) {
        return false;
    }

    enter_critical();
    
    SessionContext* ctx = NULL;
    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        if (fsm_ctx.sessions[i].session_id == session_id) {
            ctx = &fsm_ctx.sessions[i];
            break;
        }
    }

    if (!ctx) {
        exit_critical();
        return false;
    }

    StateTransition* transition = &state_transitions[ctx->current_state][event];
    if (!transition->handler) {
        Logger_LogSession(session_id, LOG_LEVEL_WARNING, "SESSION", 
                         "No handler for event %d in state %d", event, ctx->current_state);
        exit_critical();
        return false;
    }

    bool result = transition->handler(ctx, event_data);
    if (result) {
        SessionState old_state = ctx->current_state;
        ctx->current_state = transition->next_state;
        ctx->state_entry_time = Timer_GetMilliseconds();
        
        Logger_LogSession(session_id, LOG_LEVEL_INFO, "SESSION", 
                         "State transition: %d -> %d", old_state, ctx->current_state);
    }

    exit_critical();
    return result;
}

void Session_FSM_ProcessTimeouts(void) {
    if (!fsm_ctx.initialized) {
        return;
    }

    uint32_t current_time = Timer_GetMilliseconds();
    if (current_time - fsm_ctx.last_timeout_check < SESSION_TIMEOUT_CHECK_INTERVAL_MS) {
        return;
    }

    enter_critical();
    fsm_ctx.last_timeout_check = current_time;

    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        SessionContext* ctx = &fsm_ctx.sessions[i];
        if (ctx->session_id == 0) {
            continue;
        }

        // Check S3 timeout
        if (current_time - ctx->last_activity_time > fsm_ctx.config.s3_timeout_ms) {
            Logger_LogSession(ctx->session_id, LOG_LEVEL_WARNING, "SESSION", 
                            "S3 timeout occurred");
            Session_FSM_HandleEvent(ctx->session_id, SESSION_EVENT_TIMEOUT, NULL);
            if (fsm_ctx.config.auto_session_cleanup) {
                Session_FSM_DestroySession(ctx->session_id);
            }
            continue;
        }

        // Check P2 timeout for pending operations
        if (ctx->pending_did != 0 || ctx->pending_routine != 0) {
            uint32_t timeout = ctx->routine_active ? 
                             fsm_ctx.config.p2_star_timeout_ms : 
                             fsm_ctx.config.p2_timeout_ms;
            
            if (current_time - ctx->state_entry_time > timeout) {
                Logger_LogSession(ctx->session_id, LOG_LEVEL_WARNING, "SESSION", 
                                "P2 timeout occurred");
                Session_FSM_HandleEvent(ctx->session_id, SESSION_EVENT_TIMEOUT, NULL);
            }
        }
    }

    exit_critical();
}

bool Session_FSM_UpdateActivity(uint32_t session_id) {
    if (!fsm_ctx.initialized) {
        return false;
    }

    enter_critical();
    
    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        if (fsm_ctx.sessions[i].session_id == session_id) {
            fsm_ctx.sessions[i].last_activity_time = Timer_GetMilliseconds();
            exit_critical();
            return true;
        }
    }

    exit_critical();
    return false;
}

uint32_t Session_FSM_GetActiveSessionCount(void) {
    return fsm_ctx.active_sessions;
}

