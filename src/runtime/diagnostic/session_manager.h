#ifndef CANT_SESSION_MANAGER_H
#define CANT_SESSION_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "uds_handler.h"

// Session State
typedef struct {
    UdsSessionType active_session;
    uint32_t session_start_time;
    uint32_t last_activity_time;
    uint8_t security_level;
    bool suppress_response;
    bool session_locked;
} SessionState;

// Session Manager Configuration
typedef struct {
    uint32_t default_p2_timeout_ms;
    uint32_t extended_p2_timeout_ms;
    uint32_t s3_timeout_ms;
    bool enable_session_lock;
    void (*session_change_callback)(UdsSessionType old_session, UdsSessionType new_session);
    void (*security_change_callback)(uint8_t old_level, uint8_t new_level);
    void (*timeout_callback)(void);
} SessionManagerConfig;

// Session Manager API
bool Session_Manager_Init(const SessionManagerConfig* config);
void Session_Manager_DeInit(void);
bool Session_Manager_ChangeSession(UdsSessionType new_session);
bool Session_Manager_UpdateSecurity(uint8_t security_level);
void Session_Manager_ProcessTimeout(void);
bool Session_Manager_IsSessionAllowed(UdsSessionType session);
bool Session_Manager_IsServiceAllowed(UdsServiceId service_id);
void Session_Manager_UpdateActivity(void);
bool Session_Manager_LockSession(void);
bool Session_Manager_UnlockSession(void);
SessionState Session_Manager_GetState(void);
uint32_t Session_Manager_GetP2Timeout(void);
uint32_t Session_Manager_GetS3Timeout(void);
bool Session_Manager_IsSuppressResponse(void);
void Session_Manager_SetSuppressResponse(bool suppress);

#endif // CANT_SESSION_MANAGER_H 