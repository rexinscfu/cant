#ifndef CANT_SECURITY_MANAGER_H
#define CANT_SECURITY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SECURITY_LEVEL_LOCKED = 0,
    SECURITY_LEVEL_1 = 1,
    SECURITY_LEVEL_2 = 2,
    SECURITY_LEVEL_3 = 3,
    SECURITY_LEVEL_SUPPLIER = 0xF0,
    SECURITY_LEVEL_DEBUG = 0xFF
} SecurityLevel;

typedef struct {
    uint32_t delay_time_ms;
    uint8_t max_attempts;
    bool enable_delay_on_error;
    bool require_seed_on_reset;
    uint16_t min_key_length;
    uint16_t max_key_length;
} SecurityConfig;

typedef struct {
    uint32_t session_id;
    SecurityLevel requested_level;
    uint32_t seed;
    uint32_t attempt_count;
    uint32_t last_attempt_time;
    bool delay_active;
} SecurityContext;

bool Security_Init(const SecurityConfig* config);
void Security_Deinit(void);

bool Security_RequestAccess(uint32_t session_id, SecurityLevel level);
bool Security_GetSeed(uint32_t session_id, uint32_t* seed);
bool Security_ValidateKey(uint32_t session_id, const uint8_t* key, uint16_t length);
bool Security_RevokeAccess(uint32_t session_id);

SecurityLevel Security_GetCurrentLevel(uint32_t session_id);
bool Security_IsLevelAllowed(uint32_t session_id, SecurityLevel level);
void Security_ProcessTimeouts(void);

#endif // CANT_SECURITY_MANAGER_H 