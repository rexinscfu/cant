#ifndef CANT_SESSION_MANAGER_H
#define CANT_SESSION_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "uds_services.h"
#include "network_manager.h"
#include "dtc_handler.h"

// Session manager configuration
typedef struct {
    uint32_t session_timeout_ms;
    bool require_security_access;
    uint8_t max_security_level;
    struct {
        bool enable_programming;
        bool enable_extended_session;
        bool enable_safety_system;
    } features;
} SessionConfig;

typedef struct SessionManager SessionManager;

// Session Manager API
SessionManager* session_manager_create(UDSHandler* uds, 
                                     NetworkManager* network,
                                     DTCHandler* dtc,
                                     const SessionConfig* config);
void session_manager_destroy(SessionManager* manager);
void session_manager_process(SessionManager* manager);
bool session_manager_handle_request(SessionManager* manager, 
                                  const uint8_t* data, 
                                  size_t length);
UDSSessionType session_manager_get_current_session(const SessionManager* manager);
bool session_manager_is_locked(const SessionManager* manager);

#endif // CANT_SESSION_MANAGER_H 