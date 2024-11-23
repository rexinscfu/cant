#ifndef CANT_DIAG_SESSION_H
#define DIAG_SESSION_H

#include "diag_core.h"

// Session states
typedef enum {
    SESSION_STATE_IDLE,
    SESSION_STATE_STARTING,
    SESSION_STATE_ACTIVE,
    SESSION_STATE_ENDING,
    SESSION_STATE_ERROR
} DiagSessionState;

// Session response handler
typedef void (*DiagSessionResponseHandler)(uint32_t msg_id, const DiagResponse* response, void* context);

// Session management functions
bool DiagSession_Init(uint32_t timeout_ms);
void DiagSession_Deinit(void);

bool DiagSession_Start(DiagSessionType session_type);
bool DiagSession_End(void);

bool DiagSession_RegisterResponseHandler(uint32_t msg_id, DiagSessionResponseHandler handler, void* context);
void DiagSession_UnregisterResponseHandler(uint32_t msg_id);

DiagSessionState DiagSession_GetState(void);
uint32_t DiagSession_GetTimeout(void);
void DiagSession_HandleTimeout(void);

#endif // CANT_DIAG_SESSION_H 