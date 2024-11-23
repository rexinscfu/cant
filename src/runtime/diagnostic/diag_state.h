#ifndef CANT_DIAG_STATE_H
#define DIAG_STATE_H

#include "diag_core.h"
#include <stdint.h>
#include <stdbool.h>

// Maximum number of state callbacks
#define MAX_STATE_CALLBACKS 16

// Maximum number of custom states
#define MAX_CUSTOM_STATES 8

// State transition timeout (ms)
#define STATE_TRANSITION_TIMEOUT 5000

// State machine events
typedef enum {
    STATE_EVENT_NONE = 0,
    STATE_EVENT_INIT,                // System initialization
    STATE_EVENT_DEINIT,             // System shutdown
    STATE_EVENT_SESSION_START,       // Start diagnostic session
    STATE_EVENT_SESSION_END,        // End diagnostic session
    STATE_EVENT_SECURITY_ACCESS,    // Security access request
    STATE_EVENT_SECURITY_LOCK,      // Security lock request
    STATE_EVENT_MESSAGE_RECEIVED,   // New diagnostic message
    STATE_EVENT_RESPONSE_SENT,      // Response transmission complete
    STATE_EVENT_TIMEOUT,            // Operation timeout
    STATE_EVENT_ERROR,              // Error occurred
    STATE_EVENT_RESET,              // System reset request
    STATE_EVENT_TESTER_PRESENT,     // Tester present message
    STATE_EVENT_CUSTOM_START = 0x80 // Start of custom events
} DiagStateEvent;

// State machine states
typedef enum {
    STATE_UNINIT = 0,              // Not initialized
    STATE_IDLE,                    // Ready for session
    STATE_SESSION_STARTING,        // Session startup in progress
    STATE_SESSION_ACTIVE,          // Session running
    STATE_SESSION_ENDING,          // Session shutdown in progress
    STATE_SECURITY_PENDING,        // Security access in progress
    STATE_SECURITY_ACTIVE,         // Security access granted
    STATE_ERROR,                   // Error condition
    STATE_RESET_PENDING,          // System reset in progress
    STATE_SUSPENDED,              // Temporarily suspended
    STATE_CUSTOM_START = 0x80     // Start of custom states
} DiagState;

// State transition result
typedef enum {
    STATE_RESULT_OK,              // Transition successful
    STATE_RESULT_INVALID_STATE,   // Current state doesn't allow transition
    STATE_RESULT_INVALID_EVENT,   // Event not valid for current state
    STATE_RESULT_TIMEOUT,         // Transition timeout
    STATE_RESULT_ERROR,           // Error during transition
    STATE_RESULT_BUSY            // System busy, try later
} DiagStateResult;

// State transition data
typedef struct {
    DiagState from_state;         // Source state
    DiagState to_state;           // Target state
    DiagStateEvent event;         // Triggering event
    uint32_t timestamp;           // Transition time
    void* data;                   // Event-specific data
} DiagStateTransition;

// State change callback
typedef void (*DiagStateCallback)(const DiagStateTransition* transition, void* context);

// Custom state handler
typedef struct {
    DiagState state;
    bool (*enter)(void* data);
    bool (*exit)(void* data);
    DiagStateResult (*handle_event)(DiagStateEvent event, void* data);
} DiagCustomStateHandler;

// State machine functions
bool DiagState_Init(void);
void DiagState_Deinit(void);

// Event handling
DiagStateResult DiagState_HandleEvent(DiagStateEvent event, void* data);
DiagStateResult DiagState_ForceState(DiagState state, void* data);

// State information
DiagState DiagState_GetCurrent(void);
uint32_t DiagState_GetTimeInState(void);
bool DiagState_IsTransitionAllowed(DiagState from_state, DiagState to_state);

// Callback management
bool DiagState_RegisterCallback(DiagStateCallback callback, void* context);
void DiagState_UnregisterCallback(DiagStateCallback callback);

// Custom state management
bool DiagState_RegisterCustomState(const DiagCustomStateHandler* handler);
void DiagState_UnregisterCustomState(DiagState state);

// Debug/Development helpers
#ifdef DEVELOPMENT_BUILD
void DiagState_DumpTransitionHistory(void);
const char* DiagState_GetStateString(DiagState state);
const char* DiagState_GetEventString(DiagStateEvent event);
#endif

// Error handling
uint32_t DiagState_GetLastError(void);
const char* DiagState_GetErrorString(uint32_t error_code);

#endif // CANT_DIAG_STATE_H 