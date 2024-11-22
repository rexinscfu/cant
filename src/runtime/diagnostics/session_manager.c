#include "session_manager.h"
#include <stdlib.h>
#include <string.h>
#include "../os/critical.h"
#include "../utils/timer.h"

struct SessionManager {
    UDSHandler* uds;
    NetworkManager* network;
    DTCHandler* dtc;
    SessionConfig config;
    
    struct {
        UDSSessionType current;
        Timer timeout;
        bool session_active;
    } session;
    
    struct {
        uint8_t level;
        bool locked;
        uint32_t seed;
        Timer lockout;
        uint8_t failed_attempts;
    } security;
    
    CriticalSection critical;
};

static void handle_session_timeout(SessionManager* manager) {
    if (manager->session.session_active && 
        timer_expired(&manager->session.timeout)) {
        
        // Return to default session
        manager->session.current = UDS_SESSION_DEFAULT;
        manager->session.session_active = false;
        manager->security.locked = true;
        manager->security.level = 0;
    }
}

static bool validate_security_access(SessionManager* manager, 
                                   const uint8_t* key, 
                                   size_t length) {
    // Implement security access validation
    // This is a simplified example
    if (length != 4) return false;
    
    uint32_t received_key = (key[0] << 24) | (key[1] << 16) | 
                           (key[2] << 8) | key[3];
    
    // Compare with expected key (implement proper security algorithm)
    uint32_t expected_key = manager->security.seed ^ 0x12345678;
    
    return received_key == expected_key;
}

SessionManager* session_manager_create(UDSHandler* uds,
                                     NetworkManager* network,
                                     DTCHandler* dtc,
                                     const SessionConfig* config) {
    if (!uds || !network || !dtc || !config) return NULL;
    
    SessionManager* manager = calloc(1, sizeof(SessionManager));
    if (!manager) return NULL;
    
    manager->uds = uds;
    manager->network = network;
    manager->dtc = dtc;
    memcpy(&manager->config, config, sizeof(SessionConfig));
    
    manager->session.current = UDS_SESSION_DEFAULT;
    manager->security.locked = true;
    
    init_critical(&manager->critical);
    
    return manager;
}

void session_manager_destroy(SessionManager* manager) {
    if (!manager) return;
    destroy_critical(&manager->critical);
    free(manager);
}

void session_manager_process(SessionManager* manager) {
    if (!manager) return;
    
    enter_critical(&manager->critical);
    
    handle_session_timeout(manager);
    
    // Process security lockout
    if (manager->security.failed_attempts > 0 && 
        timer_expired(&manager->security.lockout)) {
        manager->security.failed_attempts = 0;
    }
    
    exit_critical(&manager->critical);
}

bool session_manager_handle_request(SessionManager* manager,
                                  const uint8_t* data,
                                  size_t length) {
    if (!manager || !data || length == 0) return false;
    
    enter_critical(&manager->critical);
    
    bool result = false;
    uint8_t service = data[0];
    
    switch (service) {
        case UDS_DIAGNOSTIC_SESSION_CONTROL:
            if (length == 2) {
                UDSSessionType requested = data[1];
                
                // Validate session change
                bool allowed = true;
                switch (requested) {
                    case UDS_SESSION_PROGRAMMING:
                        allowed = manager->config.features.enable_programming;
                        break;
                    case UDS_SESSION_EXTENDED:
                        allowed = manager->config.features.enable_extended_session;
                        break;
                    case UDS_SESSION_SAFETY:
                        allowed = manager->config.features.enable_safety_system;
                        break;
                }
                
                if (allowed) {
                    manager->session.current = requested;
                    manager->session.session_active = true;
                    timer_start(&manager->session.timeout,
                              manager->config.session_timeout_ms);
                    result = true;
                }
            }
            break;
            
        case UDS_SECURITY_ACCESS:
            if (length >= 2) {
                uint8_t subfunction = data[1];
                if (subfunction % 2 == 1) {  // Request seed
                    if (manager->security.failed_attempts >= 3) {
                        // Security locked out
                        break;
                    }
                    
                    // Generate seed
                    manager->security.seed = rand();  // Use proper random source
                    result = true;
                } else {  // Send key
                    if (length >= 6 && 
                        validate_security_access(manager, &data[2], 4)) {
                        manager->security.locked = false;
                        manager->security.level = subfunction / 2;
                        manager->security.failed_attempts = 0;
                        result = true;
                    } else {
                        manager->security.failed_attempts++;
                        if (manager->security.failed_attempts >= 3) {
                            timer_start(&manager->security.lockout, 10000);  // 10s
                        }
                    }
                }
            }
            break;
    }
    
    exit_critical(&manager->critical);
    return result;
}

UDSSessionType session_manager_get_current_session(const SessionManager* manager) {
    return manager ? manager->session.current : UDS_SESSION_DEFAULT;
}

bool session_manager_is_locked(const SessionManager* manager) {
    return manager ? manager->security.locked : true;
} 