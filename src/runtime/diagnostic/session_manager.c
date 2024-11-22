#include "session_manager.h"
#include <string.h>
#include "../utils/timer.h"
#include "../os/critical.h"

// Service access matrix
typedef struct {
    UdsServiceId service_id;
    uint16_t allowed_sessions;  // Bitmap of allowed sessions
    uint8_t required_security;  // Required security level
} ServiceAccessEntry;

static const ServiceAccessEntry service_access_matrix[] = {
    {UDS_SID_DIAGNOSTIC_SESSION_CONTROL,     0xFFFF, 0x00},
    {UDS_SID_ECU_RESET,                     0xFFFE, 0x01},
    {UDS_SID_SECURITY_ACCESS,               0xFFFE, 0x00},
    {UDS_SID_COMMUNICATION_CONTROL,         0x000C, 0x01},
    {UDS_SID_TESTER_PRESENT,               0xFFFF, 0x00},
    {UDS_SID_ACCESS_TIMING_PARAMETER,      0x000C, 0x01},
    {UDS_SID_SECURED_DATA_TRANSMISSION,    0x000C, 0x01},
    {UDS_SID_CONTROL_DTC_SETTING,         0x000C, 0x01},
    {UDS_SID_RESPONSE_ON_EVENT,           0x000C, 0x01},
    {UDS_SID_LINK_CONTROL,                0x000C, 0x01},
    {UDS_SID_READ_DATA_BY_IDENTIFIER,     0xFFFF, 0x00},
    {UDS_SID_READ_MEMORY_BY_ADDRESS,      0x000C, 0x01},
    {UDS_SID_READ_SCALING_DATA_BY_IDENTIFIER, 0xFFFF, 0x00},
    {UDS_SID_READ_DATA_BY_PERIODIC_IDENTIFIER, 0xFFFF, 0x00},
    {UDS_SID_WRITE_DATA_BY_IDENTIFIER,    0x000C, 0x01},
    {UDS_SID_WRITE_MEMORY_BY_ADDRESS,     0x000C, 0x01},
    {UDS_SID_CLEAR_DIAGNOSTIC_INFORMATION, 0x000C, 0x01},
    {UDS_SID_READ_DTC_INFORMATION,        0xFFFF, 0x00},
    {UDS_SID_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER, 0x000C, 0x01},
    {UDS_SID_ROUTINE_CONTROL,             0x000C, 0x01},
    {UDS_SID_REQUEST_DOWNLOAD,            0x0002, 0x02},
    {UDS_SID_REQUEST_UPLOAD,              0x0002, 0x02},
    {UDS_SID_TRANSFER_DATA,               0x0002, 0x02},
    {UDS_SID_REQUEST_TRANSFER_EXIT,       0x0002, 0x02}
};

// Internal state
typedef struct {
    SessionManagerConfig config;
    SessionState state;
    Timer p2_timer;
    Timer s3_timer;
    bool initialized;
    CriticalSection critical;
} SessionManager;

static SessionManager session_manager;

static bool validate_session_transition(UdsSessionType current, UdsSessionType target) {
    // Session transition matrix
    static const uint16_t transition_matrix[] = {
        0xFFFF,  // From DEFAULT
        0xFFFF,  // From PROGRAMMING
        0xFFFF,  // From EXTENDED_DIAGNOSTIC
        0xFFFF   // From SAFETY_SYSTEM
    };

    return (transition_matrix[current] & (1 << target)) != 0;
}

static const ServiceAccessEntry* find_service_access(UdsServiceId service_id) {
    for (size_t i = 0; i < sizeof(service_access_matrix)/sizeof(ServiceAccessEntry); i++) {
        if (service_access_matrix[i].service_id == service_id) {
            return &service_access_matrix[i];
        }
    }
    return NULL;
}

bool Session_Manager_Init(const SessionManagerConfig* config) {
    if (!config) {
        return false;
    }

    enter_critical(&session_manager.critical);

    memcpy(&session_manager.config, config, sizeof(SessionManagerConfig));
    
    session_manager.state.active_session = UDS_SESSION_DEFAULT;
    session_manager.state.session_start_time = get_system_time_ms();
    session_manager.state.last_activity_time = session_manager.state.session_start_time;
    session_manager.state.security_level = 0;
    session_manager.state.suppress_response = false;
    session_manager.state.session_locked = false;
    
    timer_init();
    init_critical(&session_manager.critical);
    
    if (config->s3_timeout_ms > 0) {
        timer_start(&session_manager.s3_timer, config->s3_timeout_ms);
    }
    
    session_manager.initialized = true;

    exit_critical(&session_manager.critical);
    return true;
}

void Session_Manager_DeInit(void) {
    enter_critical(&session_manager.critical);
    memset(&session_manager, 0, sizeof(SessionManager));
    exit_critical(&session_manager.critical);
}

bool Session_Manager_ChangeSession(UdsSessionType new_session) {
    if (!session_manager.initialized) {
        return false;
    }

    enter_critical(&session_manager.critical);

    if (session_manager.state.session_locked) {
        exit_critical(&session_manager.critical);
        return false;
    }

    if (!validate_session_transition(session_manager.state.active_session, new_session)) {
        exit_critical(&session_manager.critical);
        return false;
    }

    UdsSessionType old_session = session_manager.state.active_session;
    session_manager.state.active_session = new_session;
    session_manager.state.session_start_time = get_system_time_ms();
    session_manager.state.last_activity_time = session_manager.state.session_start_time;
    
    // Reset security level on session change
    uint8_t old_security = session_manager.state.security_level;
    session_manager.state.security_level = 0;

    if (session_manager.config.session_change_callback) {
        session_manager.config.session_change_callback(old_session, new_session);
    }

    if (session_manager.config.security_change_callback) {
        session_manager.config.security_change_callback(old_security, 0);
    }

    // Reset timers
    if (session_manager.config.s3_timeout_ms > 0) {
        timer_start(&session_manager.s3_timer, session_manager.config.s3_timeout_ms);
    }

    exit_critical(&session_manager.critical);
    return true;
}

bool Session_Manager_UpdateSecurity(uint8_t security_level) {
    if (!session_manager.initialized) {
        return false;
    }

    enter_critical(&session_manager.critical);

    if (session_manager.state.session_locked) {
        exit_critical(&session_manager.critical);
        return false;
    }

    uint8_t old_level = session_manager.state.security_level;
    session_manager.state.security_level = security_level;

    if (session_manager.config.security_change_callback) {
        session_manager.config.security_change_callback(old_level, security_level);
    }

    exit_critical(&session_manager.critical);
    return true;
}

void Session_Manager_ProcessTimeout(void) {
    if (!session_manager.initialized) {
        return;
    }

    enter_critical(&session_manager.critical);

    uint32_t current_time = get_system_time_ms();

    // Check S3 timeout
    if (session_manager.config.s3_timeout_ms > 0 && 
        timer_expired(&session_manager.s3_timer)) {
        
        if (session_manager.state.active_session != UDS_SESSION_DEFAULT) {
            UdsSessionType old_session = session_manager.state.active_session;
            session_manager.state.active_session = UDS_SESSION_DEFAULT;
            session_manager.state.security_level = 0;

            if (session_manager.config.session_change_callback) {
                session_manager.config.session_change_callback(old_session, UDS_SESSION_DEFAULT);
            }

            if (session_manager.config.timeout_callback) {
                session_manager.config.timeout_callback();
            }
        }
    }

    // Check P2 timeout
    if (timer_expired(&session_manager.p2_timer)) {
        if (session_manager.config.timeout_callback) {
            session_manager.config.timeout_callback();
        }
    }

    exit_critical(&session_manager.critical);
}

bool Session_Manager_IsSessionAllowed(UdsSessionType session) {
    if (!session_manager.initialized) {
        return false;
    }

    return validate_session_transition(session_manager.state.active_session, session);
}

bool Session_Manager_IsServiceAllowed(UdsServiceId service_id) {
    if (!session_manager.initialized) {
        return false;
    }

    const ServiceAccessEntry* access = find_service_access(service_id);
    if (!access) {
        return false;
    }

    bool session_allowed = (access->allowed_sessions & 
                          (1 << session_manager.state.active_session)) != 0;
    bool security_allowed = session_manager.state.security_level >= access->required_security;

    return session_allowed && security_allowed;
}

void Session_Manager_UpdateActivity(void) {
    if (!session_manager.initialized) {
        return;
    }

    enter_critical(&session_manager.critical);
    
    session_manager.state.last_activity_time = get_system_time_ms();
    
    if (session_manager.config.s3_timeout_ms > 0) {
        timer_start(&session_manager.s3_timer, session_manager.config.s3_timeout_ms);
    }

    exit_critical(&session_manager.critical);
}

bool Session_Manager_LockSession(void) {
    if (!session_manager.initialized || !session_manager.config.enable_session_lock) {
        return false;
    }

    enter_critical(&session_manager.critical);
    session_manager.state.session_locked = true;
    exit_critical(&session_manager.critical);
    return true;
}

bool Session_Manager_UnlockSession(void) {
    if (!session_manager.initialized || !session_manager.config.enable_session_lock) {
        return false;
    }

    enter_critical(&session_manager.critical);
    session_manager.state.session_locked = false;
    exit_critical(&session_manager.critical);
    return true;
}

SessionState Session_Manager_GetState(void) {
    SessionState state = {0};
    
    if (session_manager.initialized) {
        enter_critical(&session_manager.critical);
        memcpy(&state, &session_manager.state, sizeof(SessionState));
        exit_critical(&session_manager.critical);
    }
    
    return state;
}

uint32_t Session_Manager_GetP2Timeout(void) {
    if (!session_manager.initialized) {
        return 0;
    }

    if (session_manager.state.active_session == UDS_SESSION_DEFAULT) {
        return session_manager.config.default_p2_timeout_ms;
    } else {
        return session_manager.config.extended_p2_timeout_ms;
    }
}

uint32_t Session_Manager_GetS3Timeout(void) {
    if (!session_manager.initialized) {
        return 0;
    }
    return session_manager.config.s3_timeout_ms;
}

bool Session_Manager_IsSuppressResponse(void) {
    if (!session_manager.initialized) {
        return false;
    }
    return session_manager.state.suppress_response;
}

void Session_Manager_SetSuppressResponse(bool suppress) {
    if (!session_manager.initialized) {
        return;
    }

    enter_critical(&session_manager.critical);
    session_manager.state.suppress_response = suppress;
    exit_critical(&session_manager.critical);
}

