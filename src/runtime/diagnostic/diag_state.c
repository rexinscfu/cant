#include "diag_state.h"
#include "diag_timer.h"
#include "diag_error.h"
#include "../memory/memory_manager.h"
#include "../diagnostic/logging/diag_logger.h"
#include <string.h>

// Maximum number of transitions to keep in history
#define MAX_TRANSITION_HISTORY 32

// State transition validation matrix
// -1 = invalid transition, 1 = valid transition
static const int8_t state_transition_matrix[][8] = {
    //UNINIT IDLE  SSTART SACTIV SEND  SPEND  SACTIV ERROR
    { 0,     1,    -1,    -1,    -1,    -1,    -1,    1  }, // UNINIT
    { -1,    0,     1,    -1,    -1,     1,    -1,    1  }, // IDLE
    { -1,    1,     0,     1,    -1,    -1,    -1,    1  }, // SESSION_STARTING
    { -1,    -1,    -1,    0,     1,     1,    -1,    1  }, // SESSION_ACTIVE
    { -1,    1,    -1,    -1,     0,    -1,    -1,    1  }, // SESSION_ENDING
    { -1,    1,    -1,    -1,    -1,     0,     1,    1  }, // SECURITY_PENDING
    { -1,    -1,    -1,    -1,    -1,    -1,     0,    1  }, // SECURITY_ACTIVE
    { -1,    1,    -1,    -1,    -1,    -1,    -1,    0  }  // ERROR
};

typedef struct {
    DiagStateCallback callback;
    void* context;
    bool active;
} StateCallback;

typedef struct {
    DiagState current_state;
    DiagStateTransition history[MAX_TRANSITION_HISTORY];
    uint32_t history_index;
    StateCallback callbacks[MAX_STATE_CALLBACKS];
    uint32_t callback_count;
    DiagCustomStateHandler custom_states[MAX_CUSTOM_STATES];
    uint32_t custom_state_count;
    uint32_t state_entry_time;
    uint32_t last_error;
    bool transition_in_progress;
    bool initialized;
} StateMachine;

static StateMachine state_machine;

// State strings for debugging
#ifdef DEVELOPMENT_BUILD
static const char* state_strings[] = {
    "UNINIT", "IDLE", "SESSION_STARTING", "SESSION_ACTIVE",
    "SESSION_ENDING", "SECURITY_PENDING", "SECURITY_ACTIVE", "ERROR"
};

static const char* event_strings[] = {
    "NONE", "INIT", "DEINIT", "SESSION_START", "SESSION_END",
    "SECURITY_ACCESS", "SECURITY_LOCK", "MESSAGE_RECEIVED",
    "RESPONSE_SENT", "TIMEOUT", "ERROR", "RESET", "TESTER_PRESENT"
};
#endif

// Forward declarations
static DiagStateResult validate_transition(DiagState from_state, DiagState to_state);
static void notify_state_change(const DiagStateTransition* transition);
static void handle_transition_timeout(uint32_t timer_id, void* context);
static DiagState determine_next_state(DiagState current, DiagStateEvent event);

bool DiagState_Init(void) {
    if (state_machine.initialized) {
        DIAG_ERROR_SET(ERROR_SYSTEM_ALREADY_INITIALIZED, 
                      "State machine already initialized");
        return false;
    }
    
    memset(&state_machine, 0, sizeof(StateMachine));
    state_machine.current_state = STATE_UNINIT;
    state_machine.state_entry_time = DiagTimer_GetTimestamp();
    state_machine.initialized = true;
    
    Logger_Log(LOG_LEVEL_INFO, "STATE", "State machine initialized");
    return true;
}

void DiagState_Deinit(void) {
    if (!state_machine.initialized) return;
    
    // Force transition to UNINIT state
    DiagStateTransition transition = {
        .from_state = state_machine.current_state,
        .to_state = STATE_UNINIT,
        .event = STATE_EVENT_DEINIT,
        .timestamp = DiagTimer_GetTimestamp(),
        .data = NULL
    };
    
    notify_state_change(&transition);
    
    // Clear all callbacks and custom states
    memset(state_machine.callbacks, 0, sizeof(state_machine.callbacks));
    memset(state_machine.custom_states, 0, sizeof(state_machine.custom_states));
    
    state_machine.callback_count = 0;
    state_machine.custom_state_count = 0;
    state_machine.initialized = false;
    
    Logger_Log(LOG_LEVEL_INFO, "STATE", "State machine deinitialized");
}

DiagStateResult DiagState_HandleEvent(DiagStateEvent event, void* data) {
    if (!state_machine.initialized) {
        DIAG_ERROR_SET(ERROR_SYSTEM_NOT_INITIALIZED, 
                      "State machine not initialized");
        return STATE_RESULT_ERROR;
    }
    
    if (state_machine.transition_in_progress) {
        Logger_Log(LOG_LEVEL_WARNING, "STATE", 
                  "State transition already in progress");
        return STATE_RESULT_BUSY;
    }
    
    // Handle custom states first
    if (state_machine.current_state >= STATE_CUSTOM_START) {
        for (uint32_t i = 0; i < state_machine.custom_state_count; i++) {
            if (state_machine.custom_states[i].state == state_machine.current_state) {
                return state_machine.custom_states[i].handle_event(event, data);
            }
        }
    }
    
    DiagState next_state = determine_next_state(state_machine.current_state, event);
    if (next_state == state_machine.current_state) {
        return STATE_RESULT_OK;  // No transition needed
    }
    
    DiagStateResult result = validate_transition(state_machine.current_state, next_state);
    if (result != STATE_RESULT_OK) {
        DIAG_ERROR_SET(ERROR_SYSTEM_RESOURCE_BUSY, 
                      "Invalid state transition: %s -> %s", 
                      DiagState_GetStateString(state_machine.current_state),
                      DiagState_GetStateString(next_state));
        return result;
    }
    
    // Start transition
    state_machine.transition_in_progress = true;
    
    DiagStateTransition transition = {
        .from_state = state_machine.current_state,
        .to_state = next_state,
        .event = event,
        .timestamp = DiagTimer_GetTimestamp(),
        .data = data
    };
    
    // Start transition timeout timer
    uint32_t timer_id = DiagTimer_Start(TIMER_TYPE_REQUEST, 
                                      STATE_TRANSITION_TIMEOUT,
                                      handle_transition_timeout,
                                      &transition);
    
    if (!timer_id) {
        state_machine.transition_in_progress = false;
        DIAG_ERROR_SET(ERROR_TIMING_INVALID, "Failed to start transition timer");
        return STATE_RESULT_ERROR;
    }
    
    // Notify state change
    notify_state_change(&transition);
    
    // Update state
    state_machine.current_state = next_state;
    state_machine.state_entry_time = transition.timestamp;
    state_machine.transition_in_progress = false;
    
    // Add to history
    memcpy(&state_machine.history[state_machine.history_index], 
           &transition, sizeof(DiagStateTransition));
           
    state_machine.history_index = 
        (state_machine.history_index + 1) % MAX_TRANSITION_HISTORY;
    
    DiagTimer_Stop(timer_id);
    
    Logger_Log(LOG_LEVEL_INFO, "STATE", 
              "State transition: %s -> %s (Event: %s)",
              DiagState_GetStateString(transition.from_state),
              DiagState_GetStateString(transition.to_state),
              DiagState_GetEventString(event));
    
    return STATE_RESULT_OK;
}

static DiagState determine_next_state(DiagState current, DiagStateEvent event) {
    // State transition logic based on current state and event
    switch (current) {
        case STATE_UNINIT:
            if (event == STATE_EVENT_INIT) return STATE_IDLE;
            break;
            
        case STATE_IDLE:
            switch (event) {
                case STATE_EVENT_SESSION_START:
                    return STATE_SESSION_STARTING;
                case STATE_EVENT_SECURITY_ACCESS:
                    return STATE_SECURITY_PENDING;
                case STATE_EVENT_ERROR:
                    return STATE_ERROR;
                case STATE_EVENT_DEINIT:
                    return STATE_UNINIT;
                default:
                    break;
            }
            break;
            
        case STATE_SESSION_STARTING:
            switch (event) {
                case STATE_EVENT_MESSAGE_RECEIVED:
                    return STATE_SESSION_ACTIVE;
                case STATE_EVENT_TIMEOUT:
                    return STATE_ERROR;
                case STATE_EVENT_ERROR:
                    return STATE_ERROR;
                default:
                    break;
            }
            break;
            
        case STATE_SESSION_ACTIVE:
            switch (event) {
                case STATE_EVENT_SESSION_END:
                    return STATE_SESSION_ENDING;
                case STATE_EVENT_SECURITY_ACCESS:
                    return STATE_SECURITY_PENDING;
                case STATE_EVENT_ERROR:
                    return STATE_ERROR;
                case STATE_EVENT_TIMEOUT:
                    return STATE_SESSION_ENDING;
                default:
                    break;
            }
            break;
            
        case STATE_SECURITY_PENDING:
            switch (event) {
                case STATE_EVENT_MESSAGE_RECEIVED:
                    return STATE_SECURITY_ACTIVE;
                case STATE_EVENT_TIMEOUT:
                    return STATE_IDLE;
                case STATE_EVENT_ERROR:
                    return STATE_ERROR;
                default:
                    break;
            }
            break;
            
        case STATE_ERROR:
            if (event == STATE_EVENT_RESET) return STATE_IDLE;
            break;
            
        default:
            // Handle custom states
            if (current >= STATE_CUSTOM_START) {
                for (uint32_t i = 0; i < state_machine.custom_state_count; i++) {
                    if (state_machine.custom_states[i].state == current) {
                        // Let custom handler determine next state
                        DiagStateResult result = 
                            state_machine.custom_states[i].handle_event(event, NULL);
                        if (result >= 0) return (DiagState)result;
                        break;
                    }
                }
            }
            break;
    }
    
    // No state change
    return current;
}

static void notify_state_change(const DiagStateTransition* transition) {
    for (uint32_t i = 0; i < state_machine.callback_count; i++) {
        if (state_machine.callbacks[i].active && 
            state_machine.callbacks[i].callback) {
            state_machine.callbacks[i].callback(transition, 
                                             state_machine.callbacks[i].context);
        }
    }
}

bool DiagState_RegisterCallback(DiagStateCallback callback, void* context) {
    if (!state_machine.initialized || !callback) {
        return false;
    }
    
    // Check if callback already registered
    for (uint32_t i = 0; i < state_machine.callback_count; i++) {
        if (state_machine.callbacks[i].callback == callback) {
            state_machine.callbacks[i].context = context;
            state_machine.callbacks[i].active = true;
            return true;
        }
    }
    
    // Find free slot
    for (uint32_t i = 0; i < MAX_STATE_CALLBACKS; i++) {
        if (!state_machine.callbacks[i].active) {
            state_machine.callbacks[i].callback = callback;
            state_machine.callbacks[i].context = context;
            state_machine.callbacks[i].active = true;
            
            if (i >= state_machine.callback_count) {
                state_machine.callback_count = i + 1;
            }
            return true;
        }
    }
    
    DIAG_ERROR_SET(ERROR_SYSTEM_RESOURCE_BUSY, 
                  "Maximum number of state callbacks reached (%d)", 
                  MAX_STATE_CALLBACKS);
    return false;
}

void DiagState_UnregisterCallback(DiagStateCallback callback) {
    if (!state_machine.initialized || !callback) {
        return;
    }
    
    for (uint32_t i = 0; i < state_machine.callback_count; i++) {
        if (state_machine.callbacks[i].callback == callback) {
            state_machine.callbacks[i].callback = NULL;
            state_machine.callbacks[i].context = NULL;
            state_machine.callbacks[i].active = false;
            
            // Update callback count if possible
            while (state_machine.callback_count > 0 && 
                   !state_machine.callbacks[state_machine.callback_count - 1].active) {
                state_machine.callback_count--;
            }
            break;
        }
    }
}

bool DiagState_RegisterCustomState(const DiagCustomStateHandler* handler) {
    if (!state_machine.initialized || !handler || 
        handler->state < STATE_CUSTOM_START) {
        return false;
    }
    
    // Check if state already registered
    for (uint32_t i = 0; i < state_machine.custom_state_count; i++) {
        if (state_machine.custom_states[i].state == handler->state) {
            memcpy(&state_machine.custom_states[i], handler, 
                   sizeof(DiagCustomStateHandler));
            return true;
        }
    }
    
    // Find free slot
    if (state_machine.custom_state_count >= MAX_CUSTOM_STATES) {
        DIAG_ERROR_SET(ERROR_SYSTEM_RESOURCE_BUSY, 
                      "Maximum number of custom states reached (%d)", 
                      MAX_CUSTOM_STATES);
        return false;
    }
    
    memcpy(&state_machine.custom_states[state_machine.custom_state_count], 
           handler, sizeof(DiagCustomStateHandler));
    state_machine.custom_state_count++;
    
    return true;
}

void DiagState_UnregisterCustomState(DiagState state) {
    if (!state_machine.initialized || state < STATE_CUSTOM_START) {
        return;
    }
    
    for (uint32_t i = 0; i < state_machine.custom_state_count; i++) {
        if (state_machine.custom_states[i].state == state) {
            // Call exit handler if state is current
            if (state_machine.current_state == state && 
                state_machine.custom_states[i].exit) {
                state_machine.custom_states[i].exit(NULL);
            }
            
            // Remove state by shifting remaining entries
            if (i < state_machine.custom_state_count - 1) {
                memmove(&state_machine.custom_states[i],
                       &state_machine.custom_states[i + 1],
                       (state_machine.custom_state_count - i - 1) * 
                       sizeof(DiagCustomStateHandler));
            }
            
            state_machine.custom_state_count--;
            break;
        }
    }
}

DiagState DiagState_GetCurrent(void) {
    return state_machine.initialized ? state_machine.current_state : STATE_UNINIT;
}

uint32_t DiagState_GetTimeInState(void) {
    if (!state_machine.initialized) return 0;
    
    return DiagTimer_GetTimestamp() - state_machine.state_entry_time;
}

bool DiagState_IsTransitionAllowed(DiagState from_state, DiagState to_state) {
    // Handle custom states
    if (from_state >= STATE_CUSTOM_START || to_state >= STATE_CUSTOM_START) {
        for (uint32_t i = 0; i < state_machine.custom_state_count; i++) {
            if (state_machine.custom_states[i].state == from_state) {
                // Custom states can transition to any state unless explicitly handled
                return true;
            }
        }
        return false;
    }
    
    // Check transition matrix
    if (from_state >= sizeof(state_transition_matrix) / sizeof(state_transition_matrix[0]) ||
        to_state >= sizeof(state_transition_matrix[0]) / sizeof(int8_t)) {
        return false;
    }
    
    return state_transition_matrix[from_state][to_state] > 0;
}

static void handle_transition_timeout(uint32_t timer_id, void* context) {
    (void)timer_id;
    DiagStateTransition* transition = (DiagStateTransition*)context;
    
    if (!transition || !state_machine.transition_in_progress) {
        return;
    }
    
    Logger_Log(LOG_LEVEL_ERROR, "STATE", 
              "State transition timeout: %s -> %s",
              DiagState_GetStateString(transition->from_state),
              DiagState_GetStateString(transition->to_state));
    
    // Force error state on timeout
    DiagStateTransition error_transition = {
        .from_state = state_machine.current_state,
        .to_state = STATE_ERROR,
        .event = STATE_EVENT_TIMEOUT,
        .timestamp = DiagTimer_GetTimestamp(),
        .data = NULL
    };
    
    notify_state_change(&error_transition);
    state_machine.current_state = STATE_ERROR;
    state_machine.transition_in_progress = false;
    
    DIAG_ERROR_SET(ERROR_TIMING_INVALID, 
                  "State transition timeout after %d ms",
                  STATE_TRANSITION_TIMEOUT);
}

DiagStateResult DiagState_ForceState(DiagState state, void* data) {
    if (!state_machine.initialized) {
        return STATE_RESULT_ERROR;
    }
    
    if (state_machine.transition_in_progress) {
        return STATE_RESULT_BUSY;
    }
    
    // Create forced transition
    DiagStateTransition transition = {
        .from_state = state_machine.current_state,
        .to_state = state,
        .event = STATE_EVENT_CUSTOM_START,  // Use custom event for forced transitions
        .timestamp = DiagTimer_GetTimestamp(),
        .data = data
    };
    
    // Log forced transition
    Logger_Log(LOG_LEVEL_WARNING, "STATE", 
              "Forcing state transition: %s -> %s",
              DiagState_GetStateString(transition.from_state),
              DiagState_GetStateString(transition.to_state));
    
    notify_state_change(&transition);
    
    // Update state
    state_machine.current_state = state;
    state_machine.state_entry_time = transition.timestamp;
    
    // Add to history
    memcpy(&state_machine.history[state_machine.history_index],
           &transition, sizeof(DiagStateTransition));
           
    state_machine.history_index = 
        (state_machine.history_index + 1) % MAX_TRANSITION_HISTORY;
    
    return STATE_RESULT_OK;
}

#ifdef DEVELOPMENT_BUILD

void DiagState_DumpTransitionHistory(void) {
    if (!state_machine.initialized) {
        printf("State machine not initialized\n");
        return;
    }
    
    printf("\nState Transition History:\n");
    printf("========================\n");
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_TRANSITION_HISTORY; i++) {
        uint32_t idx = (state_machine.history_index - 1 - i) % MAX_TRANSITION_HISTORY;
        DiagStateTransition* trans = &state_machine.history[idx];
        
        if (trans->timestamp == 0) continue;
        
        printf("\nTransition #%d:\n", ++count);
        printf("From State: %s\n", DiagState_GetStateString(trans->from_state));
        printf("To State: %s\n", DiagState_GetStateString(trans->to_state));
        printf("Event: %s\n", DiagState_GetEventString(trans->event));
        printf("Timestamp: %u ms\n", trans->timestamp);
        
        if (trans->data) {
            printf("Data: 0x%p\n", trans->data);
        }
    }
    
    if (count == 0) {
        printf("No transitions in history.\n");
    }
}

const char* DiagState_GetStateString(DiagState state) {
    if (state >= STATE_CUSTOM_START) {
        static char custom_state[32];
        snprintf(custom_state, sizeof(custom_state), "CUSTOM_STATE_%d", 
                state - STATE_CUSTOM_START);
        return custom_state;
    }
    
    if (state >= sizeof(state_strings) / sizeof(state_strings[0])) {
        return "UNKNOWN_STATE";
    }
    
    return state_strings[state];
}

const char* DiagState_GetEventString(DiagStateEvent event) {
    if (event >= STATE_EVENT_CUSTOM_START) {
        static char custom_event[32];
        snprintf(custom_event, sizeof(custom_event), "CUSTOM_EVENT_%d", 
                event - STATE_EVENT_CUSTOM_START);
        return custom_event;
    }
    
    if (event >= sizeof(event_strings) / sizeof(event_strings[0])) {
        return "UNKNOWN_EVENT";
    }
    
    return event_strings[event];
}

#endif // DEVELOPMENT_BUILD

