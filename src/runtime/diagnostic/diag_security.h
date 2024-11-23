#ifndef CANT_DIAG_SECURITY_H
#define DIAG_SECURITY_H

#include "diag_core.h"

// Security states
typedef enum {
    SECURITY_STATE_LOCKED,
    SECURITY_STATE_SEED_REQUESTED,
    SECURITY_STATE_KEY_PENDING,
    SECURITY_STATE_UNLOCKED,
    SECURITY_STATE_ERROR
} DiagSecurityState;

// Security functions
bool DiagSecurity_Init(uint32_t timeout_ms);
void DiagSecurity_Deinit(void);

bool DiagSecurity_Access(DiagSecurityLevel level, const uint8_t* key, uint32_t length);
bool DiagSecurity_Lock(void);

bool DiagSecurity_GenerateSeed(uint8_t* seed, uint32_t* length);
bool DiagSecurity_ValidateKey(const uint8_t* key, uint32_t length);

DiagSecurityState DiagSecurity_GetState(void);
uint32_t DiagSecurity_GetTimeout(void);
void DiagSecurity_HandleTimeout(void);

#endif // CANT_DIAG_SECURITY_H 