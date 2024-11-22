#include "security_manager.h"
#include "../logging/diag_logger.h"
#include "../os/critical.h"
#include "../os/timer.h"
#include "../crypto/crypto_utils.h"
#include <string.h>

#define MAX_SECURITY_CONTEXTS 16
#define SECURITY_CHECK_INTERVAL_MS 100

typedef struct {
    SecurityContext contexts[MAX_SECURITY_CONTEXTS];
    SecurityConfig config;
    uint32_t active_contexts;
    uint32_t last_check_time;
    bool initialized;
} SecurityManager;

static SecurityManager security_mgr;

// Internal helper functions
static SecurityContext* find_context(uint32_t session_id) {
    for (uint32_t i = 0; i < MAX_SECURITY_CONTEXTS; i++) {
        if (security_mgr.contexts[i].session_id == session_id) {
            return &security_mgr.contexts[i];
        }
    }
    return NULL;
}

static uint32_t generate_seed(void) {
    uint32_t seed;
    Crypto_GenerateRandom((uint8_t*)&seed, sizeof(seed));
    return seed;
}

static bool validate_security_key(uint32_t seed, const uint8_t* key, uint16_t length) {
    if (length < security_mgr.config.min_key_length || 
        length > security_mgr.config.max_key_length) {
        return false;
    }

    uint32_t expected_key = Crypto_CalculateKey(seed);
    uint32_t received_key = 0;
    memcpy(&received_key, key, sizeof(uint32_t));
    
    return expected_key == received_key;
}

bool Security_Init(const SecurityConfig* config) {
    if (!config) {
        return false;
    }

    memset(&security_mgr, 0, sizeof(SecurityManager));
    memcpy(&security_mgr.config, config, sizeof(SecurityConfig));
    
    security_mgr.last_check_time = Timer_GetMilliseconds();
    security_mgr.initialized = true;
    
    Logger_Log(LOG_LEVEL_INFO, "SECURITY", "Security manager initialized");
    return true;
}

void Security_Deinit(void) {
    Logger_Log(LOG_LEVEL_INFO, "SECURITY", "Security manager deinitialized");
    memset(&security_mgr, 0, sizeof(SecurityManager));
}

bool Security_RequestAccess(uint32_t session_id, SecurityLevel level) {
    if (!security_mgr.initialized || session_id == 0) {
        return false;
    }

    enter_critical();
    
    SecurityContext* ctx = find_context(session_id);
    if (!ctx) {
        // Create new context if slot available
        if (security_mgr.active_contexts >= MAX_SECURITY_CONTEXTS) {
            exit_critical();
            Logger_Log(LOG_LEVEL_ERROR, "SECURITY", "Max security contexts reached");
            return false;
        }

        for (uint32_t i = 0; i < MAX_SECURITY_CONTEXTS; i++) {
            if (security_mgr.contexts[i].session_id == 0) {
                ctx = &security_mgr.contexts[i];
                ctx->session_id = session_id;
                security_mgr.active_contexts++;
                break;
            }
        }
    }

    if (ctx->delay_active) {
        exit_critical();
        Logger_LogSession(session_id, LOG_LEVEL_WARNING, "SECURITY", 
                         "Security access delayed due to previous failures");
        return false;
    }

    ctx->requested_level = level;
    ctx->seed = generate_seed();
    ctx->last_attempt_time = Timer_GetMilliseconds();
    
    Logger_LogSession(session_id, LOG_LEVEL_INFO, "SECURITY", 
                     "Security access requested for level %d", level);
    
    exit_critical();
    return true;
}

bool Security_GetSeed(uint32_t session_id, uint32_t* seed) {
    if (!security_mgr.initialized || !seed) {
        return false;
    }

    enter_critical();
    
    SecurityContext* ctx = find_context(session_id);
    if (!ctx || ctx->delay_active) {
        exit_critical();
        return false;
    }

    *seed = ctx->seed;
    
    Logger_LogSession(session_id, LOG_LEVEL_DEBUG, "SECURITY", 
                     "Seed provided: 0x%08X", ctx->seed);
    
    exit_critical();
    return true;
}

bool Security_ValidateKey(uint32_t session_id, const uint8_t* key, uint16_t length) {
    if (!security_mgr.initialized || !key) {
        return false;
    }

    enter_critical();
    
    SecurityContext* ctx = find_context(session_id);
    if (!ctx || ctx->delay_active) {
        exit_critical();
        return false;
    }

    bool valid = validate_security_key(ctx->seed, key, length);
    
    if (valid) {
        ctx->attempt_count = 0;
        Logger_LogSession(session_id, LOG_LEVEL_INFO, "SECURITY", 
                         "Security access granted for level %d", ctx->requested_level);
    } else {
        ctx->attempt_count++;
        Logger_LogSession(session_id, LOG_LEVEL_WARNING, "SECURITY", 
                         "Invalid security key, attempt %d/%d", 
                         ctx->attempt_count, security_mgr.config.max_attempts);
        
        if (ctx->attempt_count >= security_mgr.config.max_attempts) {
            ctx->delay_active = true;
            Logger_LogSession(session_id, LOG_LEVEL_ERROR, "SECURITY", 
                            "Max attempts exceeded, enforcing delay");
        }
    }

    ctx->last_attempt_time = Timer_GetMilliseconds();
    
    exit_critical();
    return valid;
}

bool Security_RevokeAccess(uint32_t session_id) {
    if (!security_mgr.initialized) {
        return false;
    }

    enter_critical();
    
    SecurityContext* ctx = find_context(session_id);
    if (!ctx) {
        exit_critical();
        return false;
    }

    Logger_LogSession(session_id, LOG_LEVEL_INFO, "SECURITY", 
                     "Security access revoked for level %d", ctx->requested_level);
    
    memset(ctx, 0, sizeof(SecurityContext));
    security_mgr.active_contexts--;
    
    exit_critical();
    return true;
}

SecurityLevel Security_GetCurrentLevel(uint32_t session_id) {
    if (!security_mgr.initialized) {
        return SECURITY_LEVEL_LOCKED;
    }

    enter_critical();
    
    SecurityContext* ctx = find_context(session_id);
    if (!ctx || ctx->delay_active) {
        exit_critical();
        return SECURITY_LEVEL_LOCKED;
    }

    SecurityLevel level = ctx->requested_level;
    exit_critical();
    return level;
}

bool Security_IsLevelAllowed(uint32_t session_id, SecurityLevel level) {
    if (!security_mgr.initialized) {
        return false;
    }

    enter_critical();
    
    SecurityContext* ctx = find_context(session_id);
    if (!ctx || ctx->delay_active) {
        exit_critical();
        return false;
    }

    bool allowed = (ctx->requested_level >= level);
    exit_critical();
    return allowed;
}

void Security_ProcessTimeouts(void) {
    if (!security_mgr.initialized) {
        return;
    }

    uint32_t current_time = Timer_GetMilliseconds();
    if (current_time - security_mgr.last_check_time < SECURITY_CHECK_INTERVAL_MS) {
        return;
    }

    enter_critical();
    security_mgr.last_check_time = current_time;

    for (uint32_t i = 0; i < MAX_SECURITY_CONTEXTS; i++) {
        SecurityContext* ctx = &security_mgr.contexts[i];
        if (ctx->session_id == 0) {
            continue;
        }

        if (ctx->delay_active) {
            if (current_time - ctx->last_attempt_time >= security_mgr.config.delay_time_ms) {
                ctx->delay_active = false;
                ctx->attempt_count = 0;
                Logger_LogSession(ctx->session_id, LOG_LEVEL_INFO, "SECURITY", 
                                "Security delay period ended");
            }
        }
    }

    exit_critical();
}
