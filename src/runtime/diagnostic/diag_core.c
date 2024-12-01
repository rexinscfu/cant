#include "diag_core.h"
#include "diag_protocol.h"
#include "diag_session.h"
#include "diag_security.h"
#include "diag_timer.h"
#include "../memory/memory_manager.h"
#include "../diagnostic/logging/diag_logger.h"
#include <string.h>

typedef struct {
    DiagConfig config;
    DiagSessionType current_session;
    DiagSecurityLevel security_level;
    DiagEventCallback event_callback;
    DiagErrorCallback error_callback;
    void* event_context;
    void* error_context;
    uint32_t last_error;
    bool initialized;
    bool session_active;
    bool security_active;
} DiagManager;

static DiagManager diag_mgr;

static const char* error_strings[] = {
    "No error",
    "Not initialized",
    "Invalid parameter",
    "Protocol error",
    "Session error",
    "Security error",
    "Timeout error",
    "Communication error",
    "Memory error",
    "Internal error"
};

bool Diag_Init(const DiagConfig* config) {
    if (!config) {
        return false;
    }
    
    memset(&diag_mgr, 0, sizeof(DiagManager));
    memcpy(&diag_mgr.config, config, sizeof(DiagConfig));
    
    if (!DiagProtocol_Init(config->protocol)) {
        Logger_Log(LOG_LEVEL_ERROR, "DIAG", "Failed to initialize protocol handler");
        return false;
    }
    
    if (!DiagSession_Init(config->session_timeout_ms)) {
        DiagProtocol_Deinit();
        Logger_Log(LOG_LEVEL_ERROR, "DIAG", "Failed to initialize session manager");
        return false;
    }
    
    if (config->enable_security) {
        if (!DiagSecurity_Init(config->security_timeout_ms)) {
            DiagSession_Deinit();
            DiagProtocol_Deinit();
            Logger_Log(LOG_LEVEL_ERROR, "DIAG", "Failed to initialize security manager");
            return false;
        }
    }
    
    if (!DiagTimer_Init()) {
        if (config->enable_security) DiagSecurity_Deinit();
        DiagSession_Deinit();
        DiagProtocol_Deinit();
        Logger_Log(LOG_LEVEL_ERROR, "DIAG", "Failed to initialize timer manager");
        return false;
    }
    
    if (!DiagLogger_Init()) {
        return false;
    }
    
    diag_mgr.initialized = true;
    diag_mgr.current_session = DIAG_SESSION_DEFAULT;
    diag_mgr.security_level = DIAG_SEC_LOCKED;
    
    Logger_Log(LOG_LEVEL_INFO, "DIAG", "Diagnostic system initialized");
    return true;
}

void Diag_Deinit(void) {
    if (!diag_mgr.initialized) {
        return;
    }
    
    DiagTimer_Deinit();
    if (diag_mgr.config.enable_security) {
        DiagSecurity_Deinit();
    }
    DiagSession_Deinit();
    DiagProtocol_Deinit();
    DiagLogger_Deinit();
    
    memset(&diag_mgr, 0, sizeof(DiagManager));
    Logger_Log(LOG_LEVEL_INFO, "DIAG", "Diagnostic system deinitialized");
}

bool Diag_StartSession(DiagSessionType session_type) {
    if (!diag_mgr.initialized) {
        diag_mgr.last_error = DIAG_ERR_NOT_INITIALIZED;
        return false;
    }
    
    if (!DiagSession_Start(session_type)) {
        Logger_Log(LOG_LEVEL_ERROR, "DIAG", "Failed to start diagnostic session");
        return false;
    }
    
    if (!DiagProtocol_StartSession(session_type)) {
        DiagSession_End();
        Logger_Log(LOG_LEVEL_ERROR, "DIAG", "Protocol failed to start session");
        return false;
    }
    
    diag_mgr.current_session = session_type;
    diag_mgr.session_active = true;
    
    Logger_Log(LOG_LEVEL_INFO, "DIAG", "Started diagnostic session: %d", session_type);
    return true;
}

bool Diag_EndSession(void) {
    if (!diag_mgr.initialized || !diag_mgr.session_active) {
        return false;
    }
    
    DiagProtocol_EndSession();
    DiagSession_End();
    
    diag_mgr.current_session = DIAG_SESSION_DEFAULT;
    diag_mgr.session_active = false;
    
    if (diag_mgr.security_active) {
        DiagSecurity_Lock();
        diag_mgr.security_level = DIAG_SEC_LOCKED;
        diag_mgr.security_active = false;
    }
    
    Logger_Log(LOG_LEVEL_INFO, "DIAG", "Ended diagnostic session");
    return true;
}

bool Diag_SecurityAccess(DiagSecurityLevel level, const uint8_t* key, uint32_t key_length) {
    if (!diag_mgr.initialized || !diag_mgr.session_active) {
        return false;
    }
    
    if (!diag_mgr.config.enable_security) {
        Logger_Log(LOG_LEVEL_ERROR, "DIAG", "Security access not enabled");
        return false;
    }
    
    if (!DiagSecurity_Access(level, key, key_length)) {
        Logger_Log(LOG_LEVEL_ERROR, "DIAG", "Security access denied");
        return false;
    }
    
    if (!DiagProtocol_SecurityAccess(level, key, key_length)) {
        DiagSecurity_Lock();
        Logger_Log(LOG_LEVEL_ERROR, "DIAG", "Protocol security access failed");
        return false;
    }
    
    diag_mgr.security_level = level;
    diag_mgr.security_active = true;
    
    Logger_Log(LOG_LEVEL_INFO, "DIAG", "Security access granted: level %d", level);
    return true;
}

static void handle_response(const DiagResponse* response, void* context) {
    DiagResponseCallback callback = (DiagResponseCallback)context;
    
    if (response->success) {
        if (diag_mgr.event_callback) {
            diag_mgr.event_callback(response->id, response->data, response->length, 
                                  diag_mgr.event_context);
        }
    } else {
        if (diag_mgr.error_callback) {
            diag_mgr.error_callback(response->response_code, 
                                  Diag_GetErrorString(response->response_code),
                                  diag_mgr.error_context);
        }
    }
    
    if (callback) {
        callback(response, NULL);
    }
}

bool Diag_SendRequest(const DiagMessage* request, DiagResponseCallback callback, void* context) {
    if (!diag_mgr.initialized || !diag_mgr.session_active || !request) {
        return false;
    }
    
    DiagMessage* msg = (DiagMessage*)MEMORY_ALLOC(sizeof(DiagMessage));
    if (!msg) {
        Logger_Log(LOG_LEVEL_ERROR, "DIAG", "Failed to allocate message");
        return false;
    }
    
    memcpy(msg, request, sizeof(DiagMessage));
    msg->session = diag_mgr.current_session;
    msg->security = diag_mgr.security_level;
    
    if (!DiagProtocol_SendMessage(msg)) {
        MEMORY_FREE(msg);
        Logger_Log(LOG_LEVEL_ERROR, "DIAG", "Failed to send diagnostic message");
        return false;
    }
    
    DiagTimer_StartRequest(msg->id, diag_mgr.config.request_timeout_ms);
    
    if (callback) {
        // Register response handler
        DiagSession_RegisterResponseHandler(msg->id, handle_response, callback);
    }
    
    MEMORY_FREE(msg);
    return true;
} 