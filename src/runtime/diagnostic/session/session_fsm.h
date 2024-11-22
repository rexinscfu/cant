#ifndef CANT_SESSION_FSM_H
#define CANT_SESSION_FSM_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SESSION_STATE_DEFAULT = 0,
    SESSION_STATE_PROGRAMMING,
    SESSION_STATE_EXTENDED,
    SESSION_STATE_SAFETY,
    SESSION_STATE_SUPPLIER,
    SESSION_STATE_EOL,
    SESSION_STATE_COUNT
} SessionState;

typedef enum {
    SESSION_EVENT_TIMEOUT = 0,
    SESSION_EVENT_REQUEST,
    SESSION_EVENT_RESPONSE,
    SESSION_EVENT_SECURITY_ACCESS,
    SESSION_EVENT_SECURITY_DENIED,
    SESSION_EVENT_ERROR,
    SESSION_EVENT_RESET,
    SESSION_EVENT_COUNT
} SessionEvent;

typedef struct {
    uint32_t session_id;
    SessionState current_state;
    uint32_t state_entry_time;
    uint32_t last_activity_time;
    uint8_t security_level;
    uint16_t pending_did;
    uint16_t pending_routine;
    bool routine_active;
    uint8_t error_counter;
} SessionContext;

typedef struct {
    uint32_t p2_timeout_ms;
    uint32_t p2_star_timeout_ms;
    uint32_t s3_timeout_ms;
    uint8_t max_error_count;
    bool require_security_access;
    bool allow_nested_response;
    bool auto_session_cleanup;
} SessionFSMConfig;

bool Session_FSM_Init(const SessionFSMConfig* config);
void Session_FSM_Deinit(void);

bool Session_FSM_CreateSession(uint32_t* session_id);
bool Session_FSM_DestroySession(uint32_t session_id);

bool Session_FSM_HandleEvent(uint32_t session_id, SessionEvent event, const void* event_data);
bool Session_FSM_GetSessionContext(uint32_t session_id, SessionContext* context);
bool Session_FSM_UpdateActivity(uint32_t session_id);

void Session_FSM_ProcessTimeouts(void);
uint32_t Session_FSM_GetActiveSessionCount(void);

#endif // CANT_SESSION_FSM_H 